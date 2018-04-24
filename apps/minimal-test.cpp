#include <libwebsockets.h>
#include <signal.h>
#include <string.h>

static int interrupted;

const char *htmlPage{
    R"HTMLCONTENT(<html>
    <head>
        <meta charset="UTF-8">
        <title>Minimal Page</title>
    </head>
    <body>
        <h1>Minimal Page</h1>
    </body>
</html>
)HTMLCONTENT"};

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_HTTP:
    {
        uint8_t buf[LWS_PRE + 256];
        uint8_t *start = &buf[LWS_PRE];
        uint8_t *p = start;
        uint8_t *end = &buf[sizeof(buf) - 1];

        lwsl_user("lws_http_serve: %s\n", (const char *)in);

        if (!lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
            /* not a GET */
            break;

        if (strcmp((const char *)in, "/"))
            /* not our URL, break to return 404 */
            break;

        /* Write headers */
        if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
            return 1;

        if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                                         (unsigned char *)"text/html", 9, &p,
                                         end))
            return 1;

        if (lws_add_http_header_content_length(wsi, strlen(htmlPage), &p, end))
            return 1;

        if (lws_finalize_http_header(wsi, &p, end))
            return -1;

        const int n =
            lws_write(wsi, start, p - start,
                      LWS_WRITE_HTTP_HEADERS /*| LWS_WRITE_H2_STREAM_END*/);
        if (n < 0)
            return -1;

        lws_callback_on_writable(wsi);
        return 0;
    }
    break;

    case LWS_CALLBACK_HTTP_WRITEABLE:
    {
        uint8_t buffer[LWS_PRE + 4096];
        uint8_t *start = &buffer[LWS_PRE];
        memcpy(start, htmlPage, strlen(htmlPage));

        /* Write body */
        const int n =
            lws_write(wsi, start, strlen(htmlPage), LWS_WRITE_HTTP);
        if (n < 0)
        {
            lwsl_err("write failed\n");
            /* write failed, close conn */
            return -1;
        }
        if (lws_http_transaction_completed(wsi))
            return -1;
    }
    break;

    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
    {"http", callback_http, 0, 0, 0, 0, 0},
    {NULL, NULL, 0, 0, 0, 0, 0} /* terminator */
};

void sigint_handler(int /*sig*/)
{
    interrupted = 1;
}

int main(int /*argc*/, const char ** /*argv*/)
{
    struct lws_context_creation_info info;
    struct lws_context *context;
    int n = 0;
    int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO;

    signal(SIGINT, sigint_handler);

    lws_set_log_level(logs, NULL);
    lwsl_user("LWS minimal http server GET | visit http://localhost:7681\n");

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = 7681;
    info.protocols = protocols;
    info.max_http_header_pool = 1;

    context = lws_create_context(&info);
    if (!context)
    {
        lwsl_err("lws init failed\n");
        return 1;
    }

    while (n >= 0 && !interrupted)
        n = lws_service(context, 1000);

    lws_context_destroy(context);

    return 0;
}
