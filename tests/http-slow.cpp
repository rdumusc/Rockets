/* Copyright (c) 2017-2018, EPFL/Blue Brain Project
 *                          Raphael.Dumusc@epfl.ch
 *                          Stefan.Eilemann@epfl.ch
 *                          Daniel.Nachbaur@epfl.ch
 *                          Pawel.Podhajski@epfl.ch
 *
 * This file is part of Rockets <https://github.com/BlueBrain/Rockets>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define BOOST_TEST_MODULE rockets_http

#include "json_utils.h"

#include <rockets/helpers.h>
#include <rockets/http/client.h>
#include <rockets/http/helpers.h>
#include <rockets/http/request.h>
#include <rockets/http/response.h>
#include <rockets/http/utils.h>
#include <rockets/server.h>

#include <libwebsockets.h>

#include <iostream>
#include <map>

#include <boost/mpl/vector.hpp>
#include <boost/test/unit_test.hpp>

#define CLIENT_SUPPORTS_REP_PAYLOAD (LWS_LIBRARY_VERSION_NUMBER >= 2000000)
#define CLIENT_SUPPORTS_REQ_PAYLOAD (LWS_LIBRARY_VERSION_NUMBER >= 2001000)
#define CLIENT_SUPPORTS_REP_ERRORS (LWS_LIBRARY_VERSION_NUMBER >= 2002000)

using namespace rockets;

namespace
{
const auto echoFunc = [](const http::Request& request) {
    auto body = request.path;

    for (auto kv : request.query)
        body.append("?" + kv.first + "=" + kv.second);

    if (!request.body.empty())
    {
        if (!body.empty())
            body.append(":");
        body.append(request.body);
    }
    return http::make_ready_response(http::Code::OK, body);
};

const std::string JSON_TYPE = "application/json";

const http::Response response200{http::Code::OK};
const http::Response response204{http::Code::NO_CONTENT};
const http::Response error400{http::Code::BAD_REQUEST};
const http::Response error404{http::Code::NOT_FOUND};
const http::Response error405{http::Code::NOT_SUPPORTED};

const std::string jsonGet("{\"json\": \"yes\", \"value\": 42}");
const std::string jsonPut("{\"foo\": \"no\", \"bar\": true}");

const http::Response responseJsonGet(http::Code::OK, jsonGet, JSON_TYPE);

class Foo
{
public:
    std::string toJson() const
    {
        _called = true;
        return jsonGet;
    }
    bool fromJson(const std::string& json)
    {
        if (jsonPut == json)
        {
            _notified = true;
            return true;
        }
        return false;
    }

    bool getNotified() const { return _notified; }
    void setNotified(const bool notified = true) { _notified = notified; }
    bool getCalled() const { return _called; }
    void setCalled(const bool called = true) { _called = called; }
    std::string getEndpoint() const { return "test/foo"; }
private:
    bool _notified = false;
    mutable bool _called = false;
};

template <typename T>
std::string to_json(const T& obj)
{
    return obj.toJson();
}
template <typename T>
bool from_json(T& obj, const std::string& json)
{
    return obj.fromJson(json);
}

class MockClient : public http::Client
{
public:
    using Client::Client;

    http::Response checkGET(Server& server, const std::string& uri)
    {
        return check(server, uri, http::Method::GET, "");
    }

    http::Response checkPUT(Server& server, const std::string& uri,
                            const std::string& body)
    {
        return check(server, uri, http::Method::PUT, body);
    }

    http::Response checkPOST(Server& server, const std::string& uri,
                             const std::string& body)
    {
        return check(server, uri, http::Method::POST, body);
    }

    http::Response check(Server& server, const std::string& uri,
                         const http::Method method, const std::string& body)
    {
        std::cerr << http::to_cstring(method) << " " << uri << " " << body
                  << std::endl;

        auto response = request(server.getURI() + uri, method, body);
        while (!is_ready(response))
        {
            process(0);
            if (server.getThreadCount() == 0)
                server.process(0);
        }
        return response.get();
    }

private:
};
} // anonymous namespace

namespace rockets
{
namespace http
{
template <typename Map>
bool operator==(const Map& a, const Map& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}
bool operator==(const Response& a, const Response& b)
{
    return a.code == b.code && a.body == b.body && a.headers == b.headers;
}
std::ostream& operator<<(std::ostream& oss, const http::Header& header)
{
    switch (header)
    {
    case Header::ALLOW:
        oss << "Allow";
        break;
    case Header::CONTENT_TYPE:
        oss << "Content-Type";
        break;
    case Header::LAST_MODIFIED:
        oss << "Last-Modified";
        break;
    case Header::LOCATION:
        oss << "Location";
        break;
    case Header::RETRY_AFTER:
        oss << "Retry-After";
        break;
    default:
        oss << "UNDEFINED";
        break;
    }
    return oss;
}

std::ostream& operator<<(std::ostream& oss, const Response& response)
{
    oss << "Code: " << (int)response.code << ", body: '" << response.body;
    oss << "', headers: [";
    for (auto kv : response.headers)
        oss << "(" << kv.first << ": " << kv.second << ")";
    oss << "]" << std::endl;
    return oss;
}
}
}

/**
 * Fixtures to run all test cases with {0, 1, 2} server worker threads.
 */
struct FixtureBase
{
    Foo foo;
    MockClient client;
    http::Response response;
};
struct Fixture0 : public FixtureBase
{
    Server server;
};
struct Fixture1 : public FixtureBase
{
    Server server{1u};
};
struct Fixture2 : public FixtureBase
{
    Server server{2u};
};
using Fixtures = boost::mpl::vector<Fixture0 /*, Fixture1, Fixture2*/>;

#if CLIENT_SUPPORTS_REQ_PAYLOAD

BOOST_FIXTURE_TEST_CASE_TEMPLATE(handle_all_methods, F, Fixtures, F)
{
    std::cerr << std::endl
              << "TEST: handle_all_methods" << std::endl
              << std::endl;

    // Register "echo" function for all methods
    for (int method = 0; method < int(http::Method::ALL); ++method)
        F::server.handle(http::Method(method), "path", echoFunc);

    // Extra function with no content
    F::server.handle(http::Method::GET, "nocontent", [](const http::Request&) {
        return http::make_ready_response(http::Code::NO_CONTENT);
    });

    const http::Response expectedResponse{http::Code::OK, "?query=:data"};
    const http::Response expectedResponseNoBody{http::Code::OK, "?query="};

    for (int method = 0; method < int(http::Method::ALL); ++method)
    {
        using Method = http::Method;
        const auto m = Method(method);
        // GET and DELETE should receive => return no payload
        if (m == Method::GET || m == Method::DELETE || m == Method::OPTIONS)
        {
            F::response = F::client.check(F::server, "/path?query", m, "");
            BOOST_CHECK_EQUAL(F::response, expectedResponseNoBody);
        }
        else
        {
            F::response = F::client.check(F::server, "/path?query", m, "data");
            BOOST_CHECK_EQUAL(F::response, expectedResponse);
        }
    }

    // Check extra function with no content
    F::response = F::client.checkGET(F::server, "/nocontent");
    BOOST_CHECK_EQUAL(F::response, response204);
}

// BOOST_FIXTURE_TEST_CASE_TEMPLATE(handle_root, F, Fixtures, F)
//{
//    std::cerr << std::endl << "TEST: handle_root" << std::endl << std::endl;

//    F::server.handle(http::Method::GET, "", [](const http::Request&) {
//        return http::make_ready_response(http::Code::OK, "homepage",
//                                         "text/html");
//    });
//    F::server.handle(http::Method::PUT, "", [](const http::Request&) {
//        return http::make_ready_response(http::Code::OK);
//    });

//    const http::Response expectedResponse{http::Code::OK,
//                                          "homepage",
//                                          {{http::Header::CONTENT_TYPE,
//                                            "text/html"}}};

//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, ""), expectedResponse);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/"), expectedResponse);
//    // Note: libwebsockets strips extra '/' so all the following are
//    equivalent:
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "//"), expectedResponse);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "///"), expectedResponse);

//    BOOST_CHECK_EQUAL(F::client.checkPUT(F::server, "", ""), response200);
//}

// BOOST_FIXTURE_TEST_CASE_TEMPLATE(handle_root_path, F, Fixtures, F)
//{
//    std::cerr << std::endl
//              << "TEST: handle_root_path" << std::endl
//              << std::endl;

//    F::server.handle(http::Method::GET, "/", echoFunc);
//    const auto registry = json_reformat(R"({ "/": [ "GET" ] })");
//    const http::Response registryResponse{http::Code::OK, registry,
//    JSON_TYPE};

//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, ""), response200);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/registry"),
//                      registryResponse);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/ABC"),
//                      http::Response(http::Code::OK, "ABC"));
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/"), response200);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "//"), response200);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/abc/def/"),
//                      http::Response(http::Code::OK, "abc/def/"));
//}

// BOOST_FIXTURE_TEST_CASE_TEMPLATE(handle_path, F, Fixtures, F)
//{
//    std::cerr << std::endl << "TEST: handle_path" << std::endl << std::endl;

//    // Register callback function for all methods
//    for (int method = 0; method < int(http::Method::ALL); ++method)
//        F::server.handle(http::Method(method), "test/", echoFunc);

//    const http::Response expectedResponse{http::Code::OK,
//                                          "path/suffix:payload"};
//    const http::Response expectedResponseNoBody{http::Code::OK,
//    "path/suffix"};

//    for (int method = 0; method < int(http::Method::ALL); ++method)
//    {
//        using Method = http::Method;
//        const auto m = Method(method);
//        // GET and DELETE should receive => return no payload
//        if (m == Method::GET || m == Method::DELETE || m == Method::OPTIONS)
//        {
//            F::response =
//                F::client.check(F::server, "/test/path/suffix", m, "");
//            BOOST_CHECK_EQUAL(F::response, expectedResponseNoBody);
//        }
//        else
//        {
//            F::response =
//                F::client.check(F::server, "/test/path/suffix", m, "payload");
//            BOOST_CHECK_EQUAL(F::response, expectedResponse);
//        }
//    }

//    // Test override endpoints
//    const auto get = http::Method::GET;

//    F::server.handle(get, "api/object/", echoFunc);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/api/object/"),
//                      response200);

//    F::server.handle(get, "api/object/properties/", echoFunc);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server,
//                                         "/api/object/properties/color"),
//                      http::Response(http::Code::OK, "color"));

//    F::server.handle(get, "api/object/properties/color/", echoFunc);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server,
//                                         "/api/object/properties/color/rgb"),
//                      http::Response(http::Code::OK, "rgb"));

//    // Test path is not the same as object
//    F::server.handle(get, "api/size/", echoFunc);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/api/size"), error404);

//    F::server.handle(get, "api/size", echoFunc);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/api/size"),
//    response200);
//}

// BOOST_FIXTURE_TEST_CASE_TEMPLATE(urlcasesensitivity, F, Fixtures, F)
//{
//    std::cerr << std::endl
//              << "TEST: urlcasesensitivity" << std::endl
//              << std::endl;

//    F::server.handle(F::foo.getEndpoint(), F::foo);
//    F::server.handle(http::Method::GET, "BlA/CamelCase",
//                     [](const http::Request&) {
//                         return http::make_ready_response(http::Code::OK,
//                         "{}");
//                     });

//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/TEST/FOO"), error404);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/test/foo"),
//                      responseJsonGet);

//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/BlA/CamelCase"),
//                      http::Response(http::Code::OK, "{}"));
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/bla/camelcase"),
//                      error404);
//    BOOST_CHECK_EQUAL(F::client.checkGET(F::server, "/bla/camel-case"),
//                      error404);
//}

#endif // CLIENT_SUPPORTS_REQ_PAYLOAD
