#include <libwebsockets.h>
#include <signal.h>
#include <string.h>

static int interrupted;

const char *htmlPage{
    R"HTMLCONTENT(<html>
    <head>
        <meta charset="UTF-8">
        <title>Minimal Page</title>
        <script>
            function getImage(targetImg, sourceUrl)
            {
                var req = new XMLHttpRequest();
                req.onreadystatechange = function() {
                    if (req.readyState == 4 && req.status == 200) {
                        document.getElementById(targetImg).src = req.responseText;
                    }
                };
                req.open("GET", sourceUrl, true); // true for asynchronous
                req.send(null);
            }
            function getImages()
            {
                getImage("pngImg1", "img1.png");
//                getImage("pngImg2", "img2.png");
//                getImage("pngImg3", "img3.png");
//                getImage("pngImg4", "img1.png");
//                getImage("pngImg5", "img2.png");
//                getImage("pngImg6", "img3.png");
            }
        </script>
    </head>
    <body onload="getImages()">
        <h1>Minimal Page</h1>
        <img id="pngImg1" />
        <img id="pngImg2" />
        <img id="pngImg3" />
        <img id="pngImg4" />
        <img id="pngImg5" />
        <img id="pngImg6" />
    </body>
</html>
)HTMLCONTENT"};

const char *pngImageBlue =
    "data:image/"
    "png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAnUlEQVR42u3RAQ0AAAgDIG1m/"
    "1K3hnNQgZ5KijNaiBCECEGIEIQIQYgQIUIQIgQhQhAiBCFCEIIQIQgRghAhCBGCEIQIQYgQhAh"
    "BiBCEIEQIQoQgRAhChCAEIUIQIgQhQhAiBCEIEYIQIQgRghAhCEGIEIQIQYgQhAhBCEKEIEQIQ"
    "oQgRAhCECIEIUIQIgQhQhAiRIgQhAhBiBCEfLc/reCdqegczgAAAABJRU5ErkJggg==";

const char *pngImageGreen =
    "data:image/"
    "png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAnUlEQVR42u3RAQ0AAAgDIO1q/"
    "wq3hnNQgZ5UijNaiBCECEGIEIQIQYgQIUIQIgQhQhAiBCFCEIIQIQgRghAhCBGCEIQIQYgQhAh"
    "BiBCEIEQIQoQgRAhChCAEIUIQIgQhQhAiBCEIEYIQIQgRghAhCEGIEIQIQYgQhAhBCEKEIEQIQ"
    "oQgRAhCECIEIUIQIgQhQhAiRIgQhAhBiBCEfLc7GO6tdYHtTQAAAABJRU5ErkJggg==";
const char *pngImageRed =
    "data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAApElEQVR42u3RAQ0AAAjDMO5fNC"
    "CDkC5z0HTVrisFCBABASIgQAQEiIAAAQJEQIAICBABASIgQAREQIAICBABASIgQAREQIAICBAB"
    "ASIgQAREQIAICBABASIgQAREQIAICBABASIgQAREQIAICBABASIgQAREQIAICBABASIgQAREQI"
    "AICBABASIgQAREQIAICBABASIgQAQECBAgAgJEQIAIyPcGFY7HnV2aPXoAAAAASUVORK5CYII="
    "";

int getContentIndex(const char *in)
{
    if (!strcmp((const char *)in, "/img1.png"))
        return 1;
    if (!strcmp((const char *)in, "/img2.png"))
        return 2;
    if (!strcmp((const char *)in, "/img3.png"))
        return 3;
    return 0;
}

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

        /* Tell writable callback what content to send. */
        static_cast<int *>(user)[0] = getContentIndex((const char *)in);

        if (!strcmp((const char *)in, "/"))
        {
            /* Write headers */
            if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
                return 1;

            if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                                             (unsigned char *)"text/html", 9,
                                             &p, end))
                return 1;

            if (lws_add_http_header_content_length(wsi, strlen(htmlPage), &p,
                                                   end))
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
        else if (static_cast<int *>(user)[0])
        {
            /* Write headers */
            if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
                return 1;

            if (lws_add_http_header_by_token(
                    wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
                    (unsigned char *)"application/base64", 18, &p, end))
                return 1;

            if (lws_add_http_header_content_length(wsi, strlen(pngImageBlue),
                                                   &p, end))
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
        /* not our URL, let lws_callback_http_dummy() return 404 */
    }
    break;

    case LWS_CALLBACK_HTTP_WRITEABLE:
    {
        uint8_t buffer[LWS_PRE + 8192];
        uint8_t *start = &buffer[LWS_PRE];
        size_t length = 0;

        switch (static_cast<int *>(user)[0])
        {
        case 0:
            length = strlen(htmlPage);
            memcpy(start, htmlPage, strlen(htmlPage));
            break;
        case 1:
            length = strlen(pngImageBlue);
            memcpy(start, pngImageBlue, length);
            break;
        case 2:
            length = strlen(pngImageGreen);
            memcpy(start, pngImageGreen, length);
            break;
        case 3:
            length = strlen(pngImageRed);
            memcpy(start, pngImageRed, length);
            break;
        default:
            lwsl_err("invalid content index\n");
            return -1;
        }

        /* Write body */
        const int n = lws_write(wsi, start, length, LWS_WRITE_HTTP);
        if (n < 0)
        {
            lwsl_err("write failed\n");
            /* write failed, close conn */
            return -1;
        }
        if (lws_http_transaction_completed(wsi))
            return -1;
        return 0;
    }
    break;

    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
    {"http", callback_http, 4, 0, 0, 0, 0},
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
