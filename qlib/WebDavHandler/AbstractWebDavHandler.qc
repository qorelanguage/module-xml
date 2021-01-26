# -*- mode: qore; indent-tabs-mode: nil -*-

/*  WebDavHandler module Copyright 2019 - 2021 Qore Technologies, s.r.o.

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

%requires Logger

#! Main public WebDavHandler namespace
public namespace WebDavHandler {
#! PROPPATCH action info
hashdecl PropPatchActionInfo {
    #! Action
    string action;
    #! Namespace
    string ns;
    #! Property
    string prop;
    #! Value
    auto val;
}

#! Abstract WebDavHandler interface class
public class AbstractWebDavHandler inherits public HttpServer::AbstractHttpRequestHandler {
    public {
        #! A hash of all supported WebDavHandler methods mapped to method names
        const RequestMethods = {
            "OPTIONS": "handleOptions",
            "GET": "handleGet",
            "HEAD": "handleHead",
            "POST": "handlePost",
            "PUT": "handlePut",
            "DELETE": "handleDelete",
            "COPY": "handleCopy",
            "MOVE": "handleMove",
            "LOCK": "handleLock",
            "UNLOCK": "handleUnlock",
            "MKCOL": "handleMkcol",
            "PROPFIND": "handlePropfind",
            "PROPPATCH": "handleProppatch",
        };

        #! List of WebDavHandler methods in a string suitable for use in HTTP responses
        const AllowHeaderValue = RequestMethods.keys().join(",");

        #! List of WebDavHandler methods
        const MethodList = keys RequestMethods;

        #! PROPPATCH action: remove
        const PPA_Remove = "R";

        #! PROPPATCH action: set
        const PPA_Set = "S";
    }

    private {
        #! The property handler object
        AbstractWebDavPropertyHandler property_handler;

        #! The virtual base path for resources served by the WebDavHandler handler
        string virtual_base_path;

        #! The Logger for this object
        Logger logger;
    }

    #! Creates the object with an empty logger and an in-memory property handler
    /** @param auth the authentication object to use to authenticate connections
    */
    constructor(*AbstractAuthenticator auth) : AbstractHttpRequestHandler(auth) {
        init(new Logger("WebDAV", LoggerLevel::getLevelInfo()), new InMemoryWebDavPropertyHandler(), "/");
    }

    #! Creates the object with the given argument and an in-memory property handler
    /** @param auth the authentication object to use to authenticate connections
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(*AbstractAuthenticator auth, string virtual_relative_path_base) : AbstractHttpRequestHandler(auth) {
        init(new Logger("WebDAV", LoggerLevel::getLevelInfo()), new InMemoryWebDavPropertyHandler(), virtual_relative_path_base);
    }

    #! Creates the object with the given arguments
    /** @param auth the authentication object to use to authenticate connections
        @param property_handler to handle properties
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(*AbstractAuthenticator auth, AbstractWebDavPropertyHandler property_handler,
            string virtual_relative_path_base = "/") : AbstractHttpRequestHandler(auth) {
        init(new Logger("WebDAV", LoggerLevel::getLevelInfo()), property_handler, virtual_relative_path_base);
    }

    #! Creates the object with the given arguments
    /** @param auth the authentication object to use to authenticate connections
        @param logger the logger
        @param property_handler to handle properties
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(*AbstractAuthenticator auth, Logger logger, AbstractWebDavPropertyHandler property_handler
                = new InMemoryWebDavPropertyHandler(),
            string virtual_relative_path_base = "/") : AbstractHttpRequestHandler(auth) {
        init(logger, property_handler, virtual_relative_path_base);
    }

    #! Returnd a list of WebDavHandler methods
    list<string> getHttpMethods() {
        return MethodList;
    }

    #! Replace the logger
    setLogger(Logger logger) {
        self.logger = logger;
    }

    #! Handles HTTP/WebDavHandler requests and returns the response
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        if (*string method = RequestMethods{hdr.method}) {
            hash<HttpResponseInfo> r = call_object_method(self, method, cx, hdr, body);
            # MUST have a Content-Length header in the response, even for HTTP responses that cannot return a msg body
            if (!r.body && !r.hdr."Content-Length") {
                r.hdr."Content-Length" = "0";
            }
            return r;
        }

        return response(405, "Method Not Allowed");
    }

    #! Returns a relative path from an absolute path
    /** @param raw_path the rar path to process

        @return a relative path generated by stripping any virtual base path (set in the constructor) from the
        argument
    */
    string getRelativePath(string raw_path) {
        return regex_subst(raw_path, "^" + virtual_base_path, "");
    }

    #! Returns a hash that can be used as an HTTP response with the given HTTP response code and response message
    hash<HttpResponseInfo> response(int response_code = 200, data msg = "OK") {
        return new hash<HttpResponseInfo>({
            "code": response_code,
            "body": sprintf(msg, argv),
        });
    }

    #! Returns a hash that can be used as an HTTP response with the given HTTP response code and response headers
    hash<HttpResponseInfo> response(int response_code = 200, hash<auto> hdr) {
        return new hash<HttpResponseInfo>({
            "code": response_code,
            "hdr": hdr,
        });
    }

    #! Returns a hash that can be used as an HTTP response with a 400 response code and response message
    hash<HttpResponseInfo> resp400(data msg = "Bad Request") {
        return response(400, msg);
    }

    #! Returns a hash that can be used as an HTTP response with a 404 response code and response message
    hash<HttpResponseInfo> resp404() {
        return response(404, "Resource Does Not Exist");
    }

    #! Returns a hash that can be used as an HTTP response with a 500 response code and response message
    hash<HttpResponseInfo> resp500(data msg = "General Server Error") {
        return response(500, msg);
    }

    #! Returns a hash that can be used as an HTTP response with a 501 response code and response message
    hash<HttpResponseInfo> resp501() {
        return response(501, "Not Implemented");
    }

    #! Common constructor code
    private init(Logger logger, AbstractWebDavPropertyHandler property_handler,
            string virtual_relative_path_base = "/") {
        self.logger = logger;
        self.property_handler = property_handler;
        virtual_base_path = normalize_dir(virtual_relative_path_base, "/");
    }

    #! Copies all properties from the given source URL to the target URL
    private copyProperties(string source_url, string target_url) {
        property_handler.cp(source_url, target_url);
    }

    #! Moves all properties from the given source URL to the target URL
    private moveProperties(string source_url, string target_url) {
        property_handler.move(source_url, target_url);
    }

    #! Deletes all properties from the given URL
    private deleteProperties(string source_url) {
        property_handler.delAll(source_url);
    }

    #! Returns a hash that can be serialized to an XML string for a PROPFIND request
    private hash<auto> preparePropFindXmlResponse(string local_path, string href, bool is_collection,
            hash<auto> base_props, bool all_props = False, *hash<string, hash<string, bool>> additional_props) {
        # maps from status code -> namespace -> prop
        hash code_map;

        # handle additonal props from the Property Handler
        hash<auto> rv_props;
        if (all_props) {
            rv_props{DavNs}."DAV:resourcetype" = is_collection ? {"DAV:collection": NOTHING} : NOTHING;
            rv_props{DavNs} += base_props; #map {$1.key: $1.value}, base_props.pairIterator();

            # add all user props for the href
            rv_props += property_handler.getAll(local_path);
        } else {
            if (*hash<auto> def_prop_map = remove additional_props{DavNs}) {
                if (remove def_prop_map.resourcetype) {
                    rv_props{DavNs}."DAV:resourcetype" = is_collection ? {"DAV:collection": NOTHING} : NOTHING;
                }
                list<string> def_props = map DavNs + $1, keys def_prop_map;
                rv_props{DavNs} += map {$1.key: $1.value}, base_props{def_props}.pairIterator();

                # add 404 response for not found properties
                if (*list<string> missing = keys (def_prop_map -
                    ((map ($1 =~ x/^DAV:(.*)$/)[0], keys rv_props{DavNs}) ?? ""))) {
                    map code_map."404"{DavNs}{$1} = NOTHING, missing;
                }
            }
            foreach hash<auto> i in (additional_props.pairIterator()) {
                rv_props{i.key} = property_handler.get(local_path, i.key, keys i.value);

                # add 404 response for not found properties
                if (*list<string> missing = keys (i.value - ((keys rv_props{i.key}) ?? ""))) {
                    map code_map."404"{i.key}{$1} = NOTHING, missing;
                }
            }
        }

        code doprop = auto sub (reference<hash<auto>> h, string key, auto value) {
            if (base_props{key}.typeCode() == NT_DATE) {
                h{key} = value.format("YYYY-MM-DDTHH:mm:SS.uuZ");
            } else {
                h{key} = value;
            }
        };

        if (rv_props{DavNs}) {
            hash<auto> ph;
            map doprop(\ph, $1.key, $1.value), rv_props{DavNs}.pairIterator();
            if (ph) {
                code_map."200"{DavNs} = ph;
            }
        }
        foreach hash<auto> i in ((rv_props - DavNs).pairIterator()) {
            hash<auto> ph;
            map doprop(\ph, $1.key, $1.value), i.value.pairIterator();
            if (ph) {
                code_map."200"{i.key} = ph;
            }
        }

        return propStatXmlHash(href, code_map);
    }

    private hash<auto> propStatXmlHash(string href, *hash<auto> code_map) {
        int num = 1;
        hash<string, string> ps_attr;

        # this must be "hash"; cannot be "hash<auto>"
        hash res."DAV:href" = href;

        res."DAV:propstat" = ();
        foreach hash<auto> i in (code_map.pairIterator()) {
            string msg = getStatusMessage(i.key);
            hash<auto> propstat;
            foreach hash<auto> pi in (i.value.pairIterator()) {
                if (pi.key && pi.key != DavNs) {
                    string nspfx = getNsPrefix(pi.key);
                    ps_attr += {("xmlns:" + nspfx[0..(nspfx.length() - 2)]): pi.key};

                    map combineValue(\propstat."DAV:prop"{nspfx + $1.key}, $1.value), pi.value.pairIterator();
                } else {
                    propstat."DAV:prop" += pi.value;
                }
            }
            res."DAV:propstat" += propstat + {
                "DAV:status": msg,
            };
        }
        if (ps_attr) {
            res."^attributes^" += ps_attr;
        }

        return res;
    }

    #! Returns a status message for the given HTTP response code
    /** @throw RESPONSE-ERROR the code is unknown
    */
    private string getStatusMessage(softstring http_code) {
        *string msg = HttpCodes{http_code};
        if (!msg) {
            throw "RESPONSE-ERROR", sprintf("no response for code %y", http_code);
        }
        return sprintf("HTTP/1.1 %s %s", http_code, msg);
    }

    #! Handles a WebDavHandler OPTIONS request
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return a 200 response with headers indicating WebDavHandler support

        @note this base class method calls handleOptionsImpl() to process the request
    */
    hash<HttpResponseInfo> handleOptions(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleOptionsImpl(cx, hdr, body);
    }

    #! Handles HTTP GET requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request

        @note this base class method calls handleGetImpl() to process the request
    */
    hash<HttpResponseInfo> handleGet(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleGetImpl(cx, hdr, body);
    }

    #! Handles HTTP HEAD requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request

        @note this base class method calls handleHeadImpl() to process the request
    */
    hash<HttpResponseInfo> handleHead(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleHeadImpl(cx, hdr, body);
    }

    #! Handles HTTP POST requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request

        @note this base class method calls handlePostImpl() to process the request
    */
    hash<HttpResponseInfo> handlePost(hash<auto> cx, hash<auto> hdr, *data body) {
        return handlePostImpl(cx, hdr, body);
    }

    #! Handles HTTP PUT requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request

        @note this base class method calls handlePutImpl() to process the request
    */
    hash<HttpResponseInfo> handlePut(hash<auto> cx, hash<auto> hdr, *data body) {
        return handlePutImpl(cx, hdr, body);
    }

    #! Handles HTTP DELETE requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request

        @note this base class method calls handleDeleteImpl() to process the request
    */
    hash<HttpResponseInfo> handleDelete(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleDeleteImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler COPY requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handleCopyImpl() to process the request
    */
    hash<HttpResponseInfo> handleCopy(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleCopyImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler MOVE requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handleMoveImpl() to process the request
    */
    hash<HttpResponseInfo> handleMove(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleMoveImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler LOCK requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handleLockImpl() to process the request
    */
    hash<HttpResponseInfo> handleLock(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleLockImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler UNLOCK requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handleUnlockImpl() to process the request
    */
    hash<HttpResponseInfo> handleUnlock(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleUnlockImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler MKCOL requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handleMkcolImpl() to process the request
    */
    hash<HttpResponseInfo> handleMkcol(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleMkcolImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler PROPFIND requests for WebDavHandler resources; reetrieves properties from WebDavHandler resources
    /**
        @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handlePropfindImpl() to process the request
    */
    hash<HttpResponseInfo> handlePropfind(hash<auto> cx, hash<auto> hdr, *data body) {
        return handlePropfindImpl(cx, hdr, body);
    }

    #! Handles WebDavHandler PROPPATCH requests for WebDavHandler resources; sets or removes properties on WebDavHandler resources
    /**
        @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request

        @note this base class method calls handleProppatchImpl() to process the request
    */
    hash<HttpResponseInfo> handleProppatch(hash<auto> cx, hash<auto> hdr, *data body) {
        # parse the mandatory XML body
        hash<auto> request_xml;
        # action list
        list<hash<PropPatchActionInfo>> actions;

        if (body) {
            try {
                request_xml = getXml(body);
            } catch (hash<ExceptionInfo> ex) {
                return response(400, sprintf("%s: %s", ex.err, ex.desc));
            }

            if (!request_xml.hasKey("DAV:propertyupdate")) {
                return response(400, "XML body does not contain the \"propertyupdate\" element");
            }

            # process actions
            foreach hash<auto> i in (request_xml."DAV:propertyupdate".pairIterator()) {
                switch (i.key) {
                    case /^DAV:set(\^[0-9]+)?$/: {
                        foreach hash<auto> elem in (i.value) {
                            foreach hash<auto> pi in (elem."DAV:prop".pairIterator()) {
                                string ns;
                                if (*string pfx = (pi.key =~ x/^([a-z0-9_]+:)/i)[0]) {
                                    *string nsv = nsinfo.nsmap{pfx};
                                    if (!exists nsv) {
                                        return response(400, "property %y references unknown namespace prefix %y\n", pi.key, pfx);
                                    }

                                    ns = nsv;
                                    splice pi.key, 0, pfx.length();
                                } else {
                                    ns = (remove pi.value."^attributes^").xmlns ?? DavNs;
                                }

                                auto value = pi.value."^value^" ?? pi.value;
                                if (value.typeCode() == NT_HASH && value.firstValue()."^attributes^") {
                                    value{value.firstKey()} = value.firstValue() - "^attributes^";
                                }

                                actions += <PropPatchActionInfo>{
                                    "action": PPA_Set,
                                    "ns": ns,
                                    "prop": pi.key,
                                    "val": value,
                                };
                            }
                        }
                        break;
                    }

                    case /^DAV:remove(\^[0-9]+)?$/: {
                        foreach hash<auto> elem in (i.value) {
                            foreach hash<auto> pi in (elem."DAV:prop".pairIterator()) {
                                string ns;
                                if (*string pfx = (pi.key =~ x/^([a-z0-9_]+:)/i)[0]) {
                                    *string nsv = nsinfo.nsmap{pfx};
                                    if (!exists nsv) {
                                        return response(400, "property %y references unknown namespace prefix %y", pi.key, pfx);
                                    }

                                    ns = nsv;
                                    splice pi.key, 0, pfx.length();
                                } else {
                                    ns = (remove pi.value."^attributes^").xmlns ?? DavNs;
                                }

                                actions += <PropPatchActionInfo>{
                                    "action": PPA_Remove,
                                    "ns": ns,
                                    "prop": pi.key,
                                };
                            }
                        }
                        break;
                    }

                    case /^\^DAV:attributes/:
                        break;

                    default:
                        return response(400, "\"propertyupdate\" element has unknown key %y", i.key);
                }
            }
        }

        # do the implementation-specific updates
        string href;
        *hash<string, hash<string, hash<string, bool>>> status_map;
        try {
            status_map = handleProppatchImpl(\href, cx, hdr, request_xml, actions);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "WEBDAVHANDLER-ERROR") {
                return response(ex.arg ?? 500, ex.desc);
            }
            rethrow;
        }

        hash<auto> resp;
        resp."DAV:multistatus"."DAV:response" = propStatXmlHash(href, status_map);
        resp."DAV:multistatus"."^attributes^" = {"xmlns:DAV": "DAV:"};

        string xml = make_xml(resp);
        #string xml = make_xml(resp, XGF_ADD_FORMATTING);

        return <HttpResponseInfo>{
            "code": 207,
            "body": xml,
        };
    }

    #! This method is called with informational log messages
    /** The default implementation is empty; reimplement in subclasses to provide for logging
    */
    logInfo(string fmt) {
        delete argv;
    }

    #! This method is called with error log messages
    /** The default implementation is empty; reimplement in subclasses to provide for logging
    */
    logError(string fmt) {
        delete argv;
    }

    #! This method is called with debug log messages
    /** The default implementation is empty; reimplement in subclasses to provide for logging
    */
    logDebug(string fmt) {
        delete argv;
    }

    #! Parse incoming XML and handle namespaces
    private hash<auto> getXml(string body) {
        nsinfo = hash<NsInfo>{};
        hash<auto> rv = parse_xml(body, XPF_PRESERVE_ORDER);
        return fixDavPrefixes("DAV:", rv);
    }

    #! Handles an HTTP/WebDavHandler OPTIONS request
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return a 200 response with headers indicating WebDavHandler support
    */
    private hash<HttpResponseInfo> handleOptionsImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return <HttpResponseInfo>{
            "code" : 200,
            "hdr": {
                "DAV": "1,2",
                "Depth": "0",
                "Allow": AllowHeaderValue,
            },
        };
    }

    #! Handles HTTP GET requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request
    */
    private abstract hash<HttpResponseInfo> handleGetImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles HTTP HEAD requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request
    */
    private abstract hash<HttpResponseInfo> handleHeadImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles HTTP POST requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request
    */
    private abstract hash<HttpResponseInfo> handlePostImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles HTTP PUT requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request
    */
    private abstract hash<HttpResponseInfo> handlePutImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles HTTP DELETE requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the HTTP request
    */
    private abstract hash<HttpResponseInfo> handleDeleteImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles WebDavHandler COPY requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request
    */
    private abstract hash<HttpResponseInfo> handleCopyImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles WebDavHandler MOVE requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request
    */
    private abstract hash<HttpResponseInfo> handleMoveImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles WebDavHandler LOCK requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request
    */
    private abstract hash<HttpResponseInfo> handleLockImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles WebDavHandler UNLOCK requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request
    */
    private abstract hash<HttpResponseInfo> handleUnlockImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles WebDavHandler MKCOL requests for WebDavHandler resources
    /**  @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request
    */
    private abstract hash<HttpResponseInfo> handleMkcolImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Handles WebDavHandler PROPFIND requests for WebDavHandler resources; reetrieves properties from WebDavHandler resources
    /**
        @param cx call context hash; this hash will have the following keys:
        - \c header-info: a hash of information about the request header with the following keys:
          - \c accept-charset: this key will be set to an appropriate value from any \c "Accept-Charset" header; if
            any of \c "*", \c "utf8", or \c "utf-8" are present, then this will be set to \c "utf8", otherwise it will
            be set to the first requested character encoding in the list
          - \c accept-encoding: a hash where keys are values from any \c "Accept-Encoding" header and the values are
            @ref True
          - \c body-content-type: this is the \c "Content-Type" header without any charset declaration
          - \c charset: if there is a charset declaration in the \c "Content-Type" header, the value is returned in
            this key
          - \c client-cert: if the server is configured to capture remote client certificates, and the client supplied
            a certificate, this key will be populated with the @ref Qore::SSLCertificate "SSLCertificate" for the client
          - \c close: set to @ref True "True" if the connection should be closed after responding,
            @ref False "False" if not (as derived from the request header)
          - \c headers-raw: a hash of raw request headers without any case conversions or other processing
          - \c request-uri: gives the request URI in an HTTP request
        - \c socket: the bind address used to bind the listener (\c "socket-info" provides more detailed information)
        - \c socket-info: a hash of socket information for the listening socket (as returned by
          @ref Qore::Socket::getSocketInfo())
        - \c peer-info: a hash of socket information for the remote socket (as returned by @ref Qore::Socket::getPeerInfo())
        - \c url: a hash of broken-down URL information (as returned from @ref Qore::parse_url())
        - \c id: the unique HTTP connection ID
        - \c ssl: @ref True "True" if the request was encrypted with HTTPS, @ref False "False" if not
        - \c listener-id: the HTTP server listener ID (see HttpServer::getListenerInfo())
        - \c user: the current RBAC username (if any)
        - \c root_path: the root URL path matched if the request was matched by a URL prefix
        @param hdr incoming header hash; all keys will be converted to lower-case, additionally the following keys will be present:
        - \c method: the HTTP method received (ie \c "GET", \c "POST", etc)
        - \c path: the HTTP path given in the request, after processing by @ref Qore::decode_uri_request()
        - \c http_version: the HTTP version number in the request (either \c "1.0" or \c "1.1")
        @param body message body, if any

        @return the response to the WebDavHandler request
    */
    private abstract hash<HttpResponseInfo> handlePropfindImpl(hash<auto> cx, hash<auto> hdr, *data body);

    #! Internal method for handling PROPPATCH requests
    /** @param href the href for the response
        @param cx the HTTP call context
        @param hdr as hash HTTP headers
        @param request_xml the parsed XML in the request
        @param actions a list of set and remove actions to execute on the given properties

        @return a status map of HTTP code -> namespace -> property -> True

        @throw WEBDAVHANDLER-ERROR an error occured
    */
    private abstract *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx, hash<auto> hdr, *hash<auto> request_xml, *list<hash<PropPatchActionInfo>> actions);

    private:internal *hash<auto> fixDavPrefixes(string ns, hash<auto> h, *string pfx) {
        # process namespaces
        foreach hash<auto> i in (h."^attributes^".pairIterator()) {
            *string ns = (i.key =~ x/^xmlns:(.+)$/)[0];
            if (!exists ns) {
                continue;
            }
            ns += ":";
            nsinfo.nsmap{ns} = i.value;
            nsinfo.nsrmap{i.value} = ns;
        }

        *string nns = h."^attributes^".xmlns;
        if (nns) {
            if (!pfx) {
                pfx = getNsPrefix(nns);
            }
            nsinfo.nsmap{pfx} = nns;
            nsinfo.nsrmap{nns} = pfx;
        }

        hash<auto> rv;
        foreach hash<auto> i in (h.pairIterator()) {
            if (i.key == "^attributes^") {
                continue;
            } else if (i.key =~ /\^value/) {
                rv{i.key} = i.value;
                continue;
            }
            *string nsa = (i.key =~ x/^([a-z0-9_]+:)/i)[0];

            if (!nsa) {
                *string n_nns = i.value."^attributes^".xmlns ?? nns;
                if (exists n_nns && n_nns != DavNs) {
                    nsa = getNsPrefix(n_nns);
                    i.key = nsa + i.key;
                }
            }

            foreach auto val in (i.value ?? {}) {
                *string n_nsa = nsa;
                string key = i.key;
                if (!n_nsa) {
                    *string n_nns = val."^attributes^".xmlns ?? nns;
                    if (exists n_nns && n_nns != DavNs) {
                        n_nsa = getNsPrefix(n_nns);
                        key = n_nsa + key;
                    }
                }

                auto rval = fixDavPrefixes(n_nsa ?? ns, val, n_nsa);
                string use_key = getKey(key, n_nsa);

                combineValue(\rv{use_key}, rval);
            }
        }
        return rv;
    }

    #! Combines values recursively
    static combineValue(reference<auto> h, auto v) {
        if (v.typeCode() == NT_HASH && v.size() == 1 && v.hasKey("^value^")) {
            v = v."^value^";
        }

        if (h.typeCode() == NT_HASH && v.typeCode() == NT_HASH) {
            map AbstractWebDavHandler::combineValue(\h{$1.key}, $1.value), v.pairIterator();
        } else {
            h = v;
        }
    }

    #! Returns a new namespace prefix
    private:internal string getNsPrefix(string ns) {
        *string nsa = nsinfo.nsrmap{ns};
        if (!nsa) {
            int j = 0;
            do {
                nsa = "ns" + ++j + ":";
            } while (exists nsinfo.nsmap{nsa});

            nsinfo.nsmap{nsa} = ns;
            nsinfo.nsrmap{ns} = nsa;
        }

        return nsa;
    }

    private:internal string getKey(string key, *string nsa) {
        if (nsa && (nsinfo.nsmap{nsa} == DavNs)) {
            return DavNs + key[nsa.length()..];
        }

        return !nsa ? (DavNs + key) : key;
    }

    private:internal list<auto> fixDavPrefixes(string ns, list<auto> l, *string pfx) {
        return map fixDavPrefixes(ns, $1, pfx), l;
    }

    private:internal auto fixDavPrefixes(string ns, auto v, *string pfx) {
        return v;
    }
}
}

# unexperted namespace
namespace Priv {
# for namespace handling while parsing
hashdecl NsInfo {
    # map of namespace abbreviations to namespace URIs
    hash<string, string> nsmap = {
        DavNs: DavNs,
    };
    # reverse of nsmap
    hash<string, string> nsrmap = {
        DavNs: DavNs,
    };
}

thread_local hash<NsInfo> nsinfo;
}