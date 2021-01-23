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

%requires HttpServerUtil
%requires xml
%requires RestClient
%requires Util
%requires FsUtil
%try-module uuid
%define NO_UUID
%endtry

#! Main public WebDavHandler namespace
public namespace WebDavHandler {
#! Filesystem WebDavHandler inteface class
public class FsWebDavHandler inherits AbstractWebDavHandler {
    private {
        #! The root directory for serving files
        string basePath;
        bool debug;
    }

    #! Creates the object with an empty logger and an in-memory property handler
    /** @param path the path on the local filesystem for WebDAV file handline
        @param auth the authentication object to use to authenticate connections
    */
    constructor(string path, *HttpServer::AbstractAuthenticator auth) : AbstractWebDavHandler(auth) {
        basePath = normalize_dir(path);
    }

    #! Creates the object with the given argument and an in-memory property handler
    /** @param path the path on the local filesystem for WebDAV file handline
        @param auth the authentication object to use to authenticate connections
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(string path, *HttpServer::AbstractAuthenticator auth, string virtual_relative_path_base)
            : AbstractWebDavHandler(auth, virtual_relative_path_base) {
        basePath = normalize_dir(path);
    }

    #! Creates the object with the given arguments
    /** @param path the path on the local filesystem for WebDAV file handline
        @param auth the authentication object to use to authenticate connections
        @param property_handler to handle properties
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(string path, *HttpServer::AbstractAuthenticator auth, AbstractWebDavPropertyHandler property_handler,
            string virtual_relative_path_base = "/")
             : AbstractWebDavHandler(auth, property_handler, virtual_relative_path_base) {
        basePath = normalize_dir(path);
    }

    #! Creates the object with the given arguments
    /** @param path the path on the local filesystem for WebDAV file handline
        @param auth the authentication object to use to authenticate connections
        @param logger the logger
        @param property_handler to handle properties
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(string path, *HttpServer::AbstractAuthenticator auth, Logger logger,
            AbstractWebDavPropertyHandler property_handler = new InMemoryWebDavPropertyHandler(),
            string virtual_relative_path_base = "/") : AbstractWebDavHandler(auth, logger, property_handler,
                virtual_relative_path_base) {
        basePath = normalize_dir(path);
    }

    #! Converts a request path to a normalized real path on the filesystem in the root WebDavHandler directory
    /** @throw WEBDAVHANDLER-FORBIDDEN if the path is not valid
    */
    private string getPath(string raw_path) {
        string tmp = AbstractWebDavHandler::getRelativePath(raw_path);

        if (tmp.equalPartial("/")) {
            tmp = basePath + tmp;
        } else {
            tmp = basePath + DirSep + tmp;
        }

        return checkPathScope(tmp);
    }

    #! Ensures that the path is valid and returns a normalized path
    /** @throw WEBDAVHANDLER-FORBIDDEN if the path is not valid
    */
    private string checkPathScope(string tmp) {
        # it's strictly prohibitted to make file operations
        # out of the basePath prefix
        tmp = normalize_dir(tmp);

        if (!tmp.regex("^" + basePath)) {
            throw "WEBDAVHANDLER-FORBIDDEN", tmp;
        }

        return tmp;
    }

    #! Returns the content type for the file based on its extension
    private *string getContentType(string filePath) {
        return get_mime_type_from_ext(filePath, MimeTypeOctetStream);
    }

    #! Returns resources for use in a PROPFIND request based on a StatInfo hash
    private hash<auto> hstat2Resource(string path, string displayname, hash<StatInfo> h) {
        return {
            "DAV:displayname": displayname, #displayname.val() ? displayname : NOTHING,
            "DAV:size": h.size,
            "DAV:creationdate": h.ctime,
            "DAV:getlastmodified": h.mtime,
            "DAV:getcontentlength": h.type == "DIRECTORY" ? getContentType(path).size() : h.size,
            "DAV:getetag": getETag(path),
            "DAV:resourcetype": h.type == "DIRECTORY" ? {"DAV:collection": NOTHING} : "",
            "DAV:getcontenttype": h.type == "DIRECTORY" ? MimeTypeText : getContentType(path),
            "DAV:lockdiscovery": "None",
        };
    }

    #! Returns a normalized path for a raw path
    private string getNormalizedPath(string raw_path) {
        raw_path = normalize_dir(getPath(raw_path));
        if (raw_path =~ /\/$/) {
            raw_path = raw_path.substr(0, -1);
        }
        return raw_path;
    }

    #! Returns responses for each relevant resource
    private:internal list<hash<auto>> preparePropFindResponse(string raw_path, string depth = "infinity", hash<StatInfo> h, bool all_props, *hash<string, hash<string, bool>> additional_props) {
        string local_path = getNormalizedPath(raw_path);
        if (debug) {
            printf("FsWebDavHandler::preparePropFindResponse() AAA %y -- %y\n", raw_path, basename(raw_path));
        }
        bool is_dir = (h.type == "DIRECTORY");

        list<hash<auto>> res = ();
        res += preparePropFindXmlResponse(local_path, raw_path, is_dir, hstat2Resource(local_path, basename(raw_path), h), all_props,
            additional_props);

        if (is_dir && depth != "0") {
            Dir d();
            d.chdir(local_path);
            foreach string name in (d.list()) {
                string local_name = local_path + DirSep + name;
                *hash<StatInfo> info = hstat(local_name);
                if (!info) {
                    continue;
                }
                if (depth == "infinity" && info.type == "DIRECTORY") {
                    res += preparePropFindResponse(local_name, depth, h, all_props, additional_props);
                } else {
                    raw_path =~ s/\/$//;
                    res += preparePropFindXmlResponse(local_name, raw_path + "/" + name, info.type == "DIRECTORY",
                        hstat2Resource(local_name, name, info), all_props, additional_props);
                }
            }
        }

        return res;
    }

    private:internal string getETag(string path) {
        hash<StatInfo> h = hstat(path);
        return sprintf("%d-%d-%s",
            h.dev, # inode
            h.size,
            format_date("YYYYMMDDTHHmmSS", h.mtime)
        );
    }

    private:internal hash<HttpResponseInfo> internalGet(hash<auto> cx, hash<auto> hdr, *data body, bool get = True) {
        # load file
        string path = getPath(cx.raw_path);
        *hash<StatInfo> h = hstat(path);
        if (!h) {
            return response(404, "%s: %s", path, strerror());
        }

        string ct;
        binary b;
        int size;

        switch (h.type) {
            case "DIRECTORY": {
                ct = MimeTypeText;
                string dirlist = getDirectoryList(path);
                if (get) {
                    b = dirlist.toBinary();
                }
                size = dirlist.size();
                break;
            }

            case "REGULAR":
            case "SYMBOLIC-LINK": {
                ct = getContentType(path);
                if (get) {
                    b = File::readBinaryFile(path);
                }
                size = h.size;
                break;
            }

            default:
                return response(404, "%s: not a regular file (%s)", path, h.type);
        }

        return <HttpResponseInfo>{
            "code": 200,
            "hdr": {
                "Content-Type": ct,
                "ETag": getETag(path),
                "DAV:getcontenttype": ct,
                "DAV:getcontentlength": size,
                "DAV:getlastmodified": h.mtime.format("YYYY-MM-DDTHHmmSS"),
                "DAV:creationdate": h.ctime.format("YYYY-MM-DDTHHmmSS"),
            },
            "body": b,
        };
    }

    private:internal *string getDirectoryList(string path) {
        Dir d();
        d.chdir(path);
        return (foldl $1 + "\n" + $2, d.list()) ?? "";
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
    private hash<HttpResponseInfo> handleGetImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        # True = GET, False = HEAD
        return internalGet(cx, hdr, body, True);
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
    */
    private hash<HttpResponseInfo> handleHeadImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        # True = GET, False = HEAD
        return internalGet(cx, hdr, body, False);
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
    */
    private hash<HttpResponseInfo> handlePostImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        string path = getPath(cx.raw_path);
        if (path =~ /\/$/) {
            return response(400, "root directory already exists");
            # handle dir creation
            # or error
        }

        *hash<StatInfo> h = hstat(path);

        string dirPath = dirname(path);
        Dir d();
        d.chdir(dirPath);
        if (!d.exists()) {
            return response(409, "Parent collection does not exist");
        }

        # save data
        try {
            File f();
            if (f.open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644, "UTF-8")) {
                return response(500, "Could not create new file");
            }
            f.write(body);
        } catch (hash<ExceptionInfo> ex) {
            return response(400, "Destination path is not a regular file");
        }

        hash<auto> hdr_resp = {
            "ETag": getETag(path),
        };
        # PUT of new resource gave should be 201 or 204, update is 200
        return response(201, hdr_resp);
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
    */
    private hash<HttpResponseInfo> handlePutImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return handlePost(cx, hdr, body);
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
    */
    private hash<HttpResponseInfo> handleDeleteImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        string path = getPath(cx.raw_path);

        *hash<StatInfo> h = hstat(path);
        if (!h) {
            return response(404, "%s: %s", path, strerror());
        }

        FsUtil::remove_path(path);
        deleteProperties(path);
        return response(204);
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
    */
    private hash<HttpResponseInfo> handleCopyImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return doCopyMoveIntern(cx, hdr, body, True);
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
    */
    private hash<HttpResponseInfo> handleMoveImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        # additional deletion of source
        string src_path = getPath(cx.raw_path);
        # copy to target first
        hash<HttpResponseInfo> ret = doCopyMoveIntern(cx, hdr, body, False);
        # then remove src
        if (ret.code >= 200 && ret.code < 300) {
            FsUtil::remove_path(src_path);
        }
        return ret;
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
    */
    private hash<HttpResponseInfo> handleLockImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        if (!body) {
            # RFC #4918: https://tools.ietf.org/html/rfc4918#page-62
            if (hdr."if") {
                return response(501);
            }
            return response(400, "missing message body in LOCK request");
        }

        # parse the mandatory XML body
        hash<auto> request_xml;
        try {
            request_xml = getXml(body);
        } catch (hash<ExceptionInfo> ex) {
            return response(400, sprintf("%s: %s", ex.err, ex.desc));
        }

        if (exists hdr.depth && hdr.depth != "0" && hdr.depth != "infinity") {
            string msg = sprintf("invalid Depth header value: %y for LOCK request; only Depth: \"0\" or "
                "\"infinity\" accepted", hdr.depth);
            return response(400, msg);
        }

        #printf("hdr: %y body: %y\n", hdr, request_xml);

        return response(501);
    }

    private string getLockToken() {
%ifndef NO_UUID
        return sprintf("urn:uuid:%s", UUID::get());
%else
        return sprintf("qoreopaquelocktoken:%s", get_random_string(64));
%endif
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
    */
    private hash<HttpResponseInfo> handleUnlockImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return response(501);
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
    */
    private hash<HttpResponseInfo> handleMkcolImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        # litmus: MKCOL with weird body must fail (RFC2518:8.3.1)
        if (exists body) {
            return response(415, "Unsupported Media Type");
        }

        # don't allow dirs creation out of given prefix
        string dir_path;
        try {
            dir_path = getPath(cx.raw_path);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "WEBDAVHANDLER-FORBIDDEN") {
                return response(403, "Forbidden");
            };
            rethrow;
        }

        if (dir_path =~ /\/$/) {
            dir_path = dir_path.substr(0, dir_path.size()-1);
        }

        # litmus: MKCOL on existing collection should return 405
        if (is_dir(dir_path)) {
            return response(405, "Collection Already Exists");
        } else if (!is_dir(normalize_dir(dir_path + "/.."))) {
            return response(409, "Previous directories in the path do not exist");
        }

        Dir d();
        d.chdir(dir_path);
        try {
            d.create(0755);
        } catch (hash<ExceptionInfo> ex) {
            return resp500("Could not create directory structure");
        }

        return response(201);
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
    */
    private hash<HttpResponseInfo> handlePropfindImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        #if (debug) {
            #printf("FsWebDavHandler::handlePropfindImpl() PPP: \n%N\n%N\n%N\n", cx, hdr, body);
        #}
        string path = getNormalizedPath(cx.raw_path);
        *hash<StatInfo> h = hstat(path);
        if (!h) {
            return response(404, "%s: %s", path, strerror());
        }

        # property map: ns -> property -> True
        hash<string, hash<string, bool>> additional_props;
        bool all_props = False;
        if (body) {
            # parse the mandatory XML body
            hash<auto> request_xml;
            try {
                request_xml = getXml(body);
            } catch (hash<ExceptionInfo> ex) {
                return response(400, sprintf("%s: %s", ex.err, ex.desc));
            }

            #printf("propfind: %N\n", request_xml);

            if (!request_xml.hasKey("DAV:propfind")) {
                return response(400, "XML body does not contain 'propfind' root tag");
            }

            if (request_xml."DAV:propfind"."DAV:prop") {
                foreach hash<auto> i in (request_xml."DAV:propfind"."DAV:prop".pairIterator()) {
                    string key = i.key;
                    key =~ s/[^:]+://g;
                    string ns = (i.key =~ x/^([a-z0-9_]+:)/i)[0] ?? DavNs;

                    *string full_ns = nsinfo.nsmap{ns};
                    if (!exists full_ns) {
                        return response(400, sprintf("unknown namespace prefix %y", ns));
                    }

                    map additional_props{full_ns}{key} = True, (i.value ?? True);
                }
            } else if (request_xml."DAV:propfind".hasKey("DAV:allprop")) {
                all_props = True;
            }
            if (debug) {
                printf("FsWebDavHandler::handlePropfindImpl() additional_props: %N\n", additional_props);
            }
        }

        # prepare response
        hash<auto> resp;
        resp."DAV:multistatus"."DAV:response" = preparePropFindResponse(cx.raw_path, hdr.depth, h, all_props, additional_props);
        resp."DAV:multistatus"."^attributes^" = {"xmlns:DAV": "DAV:"};
        if (debug) {
            printf("FsWebDavHandler::handlePropfindImpl() PPP: \n%N\n%N\n%N\n", cx, hdr, resp);
            printf("PROPFIND RV: %y\nxml: %s\n", resp, make_xml(resp, XGF_ADD_FORMATTING));
        }
        return <HttpResponseInfo>{
            "code": 207,
            "body": make_xml(resp),
        };
    }

    #! Internal method for handling PROPPATCH requests
    /** Create, change and delete one or more properties on a resource in a single atomic action

        @param cx the HTTP call context
        @param hdr as hash HTTP headers
        @param request_xml the parsed XML in the request
        @param actions a list of set and remove actions to execute on the given properties

        @return a status map of HTTP code -> namespace -> property -> True

        @throw WEBDAVHANDLER-ERROR an error occured
    */
    private *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx, hash<auto> hdr, *hash<auto> request_xml, *list<hash<PropPatchActionInfo>> actions) {
        string path = getNormalizedPath(cx.raw_path);
        href = path;

        *hash<StatInfo> h = hstat(path);
        if (!h) {
            throw "WEBDAVHANDLER-ERROR", sprintf("%s: %s", path, strerror()), 404;
        }

        # property status map: code -> ns -> property -> True
        hash<string, hash<string, hash<string, bool>>> status_map;

        # verify and then execute property updates
        if (request_xml) {
            *hash<string, hash<string, hash<string, bool>>> verify_status_map = verifyPropertyUpdates(path, actions);
            if (verify_status_map) {
                status_map = verify_status_map;
            } else {
                foreach hash<PropPatchActionInfo> action in (actions) {
                    #printf("action: %y\n", action);
                    if (action.action == PPA_Set) {
                        property_handler.set(path, action.ns, action.prop, action.val);
                        status_map{200}{action.ns}{action.prop} = True;
                    } else {
                        property_handler.del(path, action.ns, action.prop);
                        status_map{200}{action.ns}{action.prop} = True;
                    }
                }
            }
        }
        return status_map;
    }

    #! Verifies that all property updates can be performed before executing them
    private *hash<string, hash<string, hash<string, bool>>> verifyPropertyUpdates(string path, *list<hash<PropPatchActionInfo>> actions) {
        hash<string, hash<string, hash<string, bool>>> status_map;

        # verify that all operations can succeed
        map status_map{409}{DavNs}{$1.prop} = True, actions, $1.ns == DavNs;

        # all updates fail if one fails
        if (!status_map) {
            return;
        }

        # set a 424 status for all other updates
        map status_map{424}{$1.ns}{$1.prop} = True, actions, !status_map{409}{$1.ns}{$1.prop};

        return status_map;
    }

    #! Process copy and moves (without deletion) internally including properties (properties are moved, files are copied)
    private:internal hash<HttpResponseInfo> doCopyMoveIntern(hash<auto> cx, hash<auto> hdr, *data body, bool copy_props) {
        try {
            internalCopyMove(cx, hdr, body, copy_props);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err =~ /^[0-9]{3}$/) {
                return response(ex.err.toInt(), ex.desc);
            } else if (ex.err == "WEBDAVHANDLER-FORBIDDEN") {
                return response(403, "Cannot copy out of current jail");
            }
            rethrow;
        }

        return response(hdr.overwrite == "F" ? 201 : 204);
    }

    #! Copy or move the file and any properties
    private:internal internalCopyMove(hash<auto> cx, hash<auto> hdr, *data body, bool copy_props) {
        string dest = hdr.destination;
        *hash<UrlInfo> url = parseURL(dest);
        string dest_path = getNormalizedPath(url.path);
        #string dest_path = checkPathScope(basePath + url.path); # TODO/FIXME: check path validity
        string src_path = getNormalizedPath(cx.raw_path);
        # TODO/FIXME: it's same code as in MKCOL -> new method?
        if (dest_path =~ /\/$/) {
            dest_path = dest_path.substr(0, dest_path.size()-1);
        }

        if (hdr.method == "MOVE" && hdr.hasKey("depth") && hdr.depth != "infinite") {
            throw "403", "Cannot MOVE with specified depth";
        }

        # copy to self -> 403
        if (src_path == dest_path) {
            throw "403", "Cannot copy to self";
        }

        # copy to root -> 403
        if (dest_path == basePath) {
            throw "403", "Cannot copy to root";
        }

        # check that destination directory exists. It should not be created
        string dest_dir = dirname(dest_path);
        if (!is_dir(dest_dir)) {
            throw "409", "Target collection does not exist";
        }

        # check Overwrite header
        if (hstat(dest_path)) {
            if (hdr.overwrite == "F") {
                throw "412", "Target does exist and it cannot be overwriten";
            } else if (hdr.overwrite != "F") {
                FsUtil::remove_path(dest_path);
            }
        }
        #if (hdr.depth == "0") {
        #    mkdir(dest_path);
        #}
        # TODO/FIXME: how to handle depth 1?
        #else if (hdr.depth == 1) {
        #}
        #else {
        FsUtil::copy_path(src_path, dest_path);
        #}

        if (copy_props) {
            copyProperties(src_path, dest_path);
        } else {
            moveProperties(src_path, dest_path);
        }
    }
} # class FsWebDavHandler
} # namespace WebDavHandler
