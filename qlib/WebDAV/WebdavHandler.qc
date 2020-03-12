# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

#! Main public WebDAV namespace
public namespace WebDAV {
#! WebDAV HTTP handler class
public class WebdavHandler inherits public HttpServer::AbstractHttpRequestHandler {
    private {
        #! The actual WebDAV implementation
        AbstractWebdavInterface m_interface;
    }

    #! Creates the object from the arguments passed
    /** @param interface the WebDAV implementation
        @param auth the connection authenticator object
    */
    constructor(AbstractWebdavInterface interface, *AbstractAuthenticator auth) : HttpServer::AbstractHttpRequestHandler(auth) {
        m_interface = interface;
    }

    #! Handles HTTP/WebDAV requests and returns the response
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr, *data body) {
        if (AbstractWebdavInterface::RequestMethods{hdr.method}) {
            #listener.log("CX: %N", cx);
            #listener.log("HDR: %N", hdr);
            #listener.log("BODY: %y", body);
            hash<HttpResponseInfo> r = call_object_method(m_interface,
                                        AbstractWebdavInterface::RequestMethods{hdr.method},
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
} # class WebdavHandler
} # namespace WebDAV

