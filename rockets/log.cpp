/* Copyright (c) 2017, EPFL/Blue Brain Project
 *                     Raphael.Dumusc@epfl.ch
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

#include "proxyConnectionError.h"

#include <libwebsockets.h>

#include <iostream>
#include <stdexcept>

namespace rockets
{
namespace
{
const std::string proxyError = "ERROR proxy: HTTP/1.1 503 \n";

void handleErrorMessage(int level, const char* message)
{
#if PROXY_CONNECTION_ERROR_THROWS
    if (message == proxyError)
        throw proxy_connection_error(message);
#endif

#ifdef NDEBUG
    (void)message;
    (void)level;
#else
    std::cerr << level << " " << message << std::flush;
#endif
}
struct LogInitializer
{
    LogInitializer()
    {
        lws_set_log_level(
            /*LLL_ERR | LLL_WARN|LLL_NOTICE|*/ LLL_INFO /*|LLL_DEBUG|LLL_CLIENT*/,
            handleErrorMessage);
    }
};
static LogInitializer silencer;
}
}
