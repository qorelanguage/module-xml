# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

public namespace WebDAV {
#! Abstract WebDAV handler class.
public class WebdavHandler inherits public HttpServer::AbstractHttpRequestHandler {
    private {
        AbstractWebdavInterface m_interface;
    }

    constructor(AbstractWebdavInterface interface, *AbstractAuthenticator n_auth)
        : HttpServer::AbstractHttpRequestHandler(n_auth) {
        m_interface = interface;
    }

    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr, *data body) {
        if (AbstractWebdavInterface::REQUEST_METHODS.hasKey(hdr.method)) {
            #listener.log("cx: %N", cx);
            #listener.log("hdr: %N", hdr);
            hash<HttpResponseInfo> r = call_object_method(m_interface,
                                        AbstractWebdavInterface::REQUEST_METHODS{hdr.method},
                                        cx, hdr, body);
            # MUST have a content-length, even for HTTP responses that cannot return a msg body
            if (!r.body && !r.hdr."Content-Length") {
                r.hdr."Content-Length" = "0";
            }
            #listener.log("RESPONSE: %y", r);
            return r;
        }

        return m_interface.response(405, "Method Not Allowed");
    }

    auto methodGate(string method_name) {
        return call_object_method_args(m_interface, method_name, argv);
    }
} # class WebdavHandler
} # namespace WebDAV

