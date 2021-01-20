# -*- mode: qore; indent-tabs-mode: nil -*-

/*  WebDAV module Copyright 2019 - 2021 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Main public WebDAV namespace
public namespace WebDAV {
#! WebDAV HTTP handler class
public class WebDAVHandler inherits public HttpServer::AbstractHttpRequestHandler {
    private {
        #! The actual WebDAV implementation
        AbstractWebDAVInterface m_interface;
    }

    #! Creates the object from the arguments passed
    /** @param interface the WebDAV implementation
        @param auth the connection authenticator object
    */
    constructor(AbstractWebDAVInterface interface, *AbstractAuthenticator auth) : HttpServer::AbstractHttpRequestHandler(auth) {
        m_interface = interface;
    }

    #! Handles HTTP/WebDAV requests and returns the response
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr, *data body) {
        if (AbstractWebDAVInterface::RequestMethods{hdr.method}) {
            #listener.log("CX: %N", cx);
            #listener.log("HDR: %N", hdr);
            #listener.log("BODY: %y", body);
            hash<HttpResponseInfo> r = call_object_method(m_interface,
                                        AbstractWebDAVInterface::RequestMethods{hdr.method},
                                        cx, hdr, body);
            # MUST have a Content-Length header in the response, even for HTTP responses that cannot return a msg body
            if (!r.body && !r.hdr."Content-Length") {
                r.hdr."Content-Length" = "0";
            }
            #listener.log("RESPONSE: %y", r);
            return r;
        }

        return m_interface.response(405, "Method Not Allowed");
    }

    #! Redirects method calls to the WebDAV implementation object
    auto methodGate(string method_name) {
        return call_object_method_args(m_interface, method_name, argv);
    }
} # class WebDAVHandler
} # namespace WebDAV

