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

public namespace WebDavHandler {

#! Proxy  WebDavHandler inteface class.
public class WebDavHandlerProxy inherits AbstractWebDavHandler {
    private {
        hash<string, AbstractWebDavHandler> m_ifaces;
    }

    #! Creates the object with an empty logger and an in-memory property handler
    /** @param interfaces the WebDAV handlers to proxy
    */
    constructor(hash<string, AbstractWebDavHandler> interfaces) {
        m_ifaces = interfaces;
    }

    #! Creates the object with the given argument and an in-memory property handler
    /** @param interfaces the WebDAV handlers to proxy
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(hash<string, AbstractWebDavHandler> interfaces, string virtual_relative_path_base)
            : AbstractWebDavHandler(NOTHING, virtual_relative_path_base) {
        m_ifaces = interfaces;
    }

    #! Creates the object with the given arguments
    /** @param interfaces the WebDAV handlers to proxy
        @param logger the logger
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(hash<string, AbstractWebDavHandler> interfaces, Logger logger,
            string virtual_relative_path_base = "/")
                : AbstractWebDavHandler(NOTHING, logger, NOTHING, virtual_relative_path_base) {
        m_ifaces = interfaces;
    }

    private *hash<auto> handleChildRaw(string method_name, Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
#printf("---- %s\n", method_name);
#printf("---- raw path %s\n", cx.raw_path);
        string path = getRelativePath(cx.raw_path);
#printf("----     path %s\n", path);

        cx.raw_path = "/" + path;

        list<string> parts = path.split("/");
#printf("---- %n\n", parts);

        if (parts.size() && m_ifaces.hasKey(parts[0])) {
#printf("---- %s\n", "Y");
            AbstractWebDavHandler handler = m_ifaces{parts[0]};
            return call_object_method(handler, method_name, sock, cx, hdr, body);
        }
#printf("---- %s\n", "N");
    }

    private hash<HttpResponseInfo> handleChild(string method_name, Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        *hash<auto> ret = handleChildRaw(method_name, sock, cx, hdr, body);
        return ret ? cast<hash<HttpResponseInfo>>(ret) : response(501, "Not Implemented");
    }

    /**
        GET

        Get a resource.
    */
    hash<HttpResponseInfo> handleGet(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleGet", sock, cx, hdr, body);
    }

    /**
        HEAD

        Same like GET but without sending a body.
    */
    hash<HttpResponseInfo> handleHead(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleHead", sock, cx, hdr, body);
    }

    /**
        POST

        Create new resource.
    */
    hash<HttpResponseInfo> handlePost(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handlePost", sock, cx, hdr, body);
    }

    /**
        PUT

        Modify resource.
    */
    hash<HttpResponseInfo> handlePut(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handlePut", sock, cx, hdr, body);
    }

    /**
        DELETE

        Delete a resource.
    */
    hash<HttpResponseInfo> handleDelete(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleDelete", sock, cx, hdr, body);
    }

    /**
        COPY

        Copy a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleCopy(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleCopy", sock, cx, hdr, body);
    }

    /**
        MOVE

        Move a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleMove(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleMove", sock, cx, hdr, body);
    }

    /**
        LOCK

        Put a lock on a resource. WebDavHandler supports both shared and exclusive locks.
    */
    hash<HttpResponseInfo> handleLock(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return response(501, "Not Implemented");
    }

    /**
        UNLOCK

        Remove a lock from a resource.
    */
    hash<HttpResponseInfo> handleUnlock(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleUnlock", sock, cx, hdr, body);
    }

    /**
        MKCOL

        Create collections (aka a directory).
    */
    hash<HttpResponseInfo> handleMkcol(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleMkcol", sock, cx, hdr, body);
    }

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.
    */
    hash<HttpResponseInfo> handlePropfind(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        # handle children first
        *hash<auto> ret = handleChildRaw("handlePropfind", sock, cx, hdr, body);
        if (ret) {
            return cast<hash<HttpResponseInfo>>(ret);
        }

        # then top level impl
        hash<auto> resp = {
            "multistatus" : {
                "^attributes^" : {"xmlns": "DAV:"},
                "response" : list(),
            },
        };

        HashIterator it(m_ifaces);
        while (it.next()) {
            hash<auto> props = {
                "displayname": it.getKey(),
                "size": 0,
            };
            string path = cx.raw_path + "/" + it.getKey();
            resp.multistatus.response += preparePropFindXmlResponse(path, path, True, props);
        }

        return <HttpResponseInfo>{
            "code": 207,
            "body": make_xml(resp),
        };
    }

    /**
        PROPPATCH

        Change and delete multiple properties on a resource in a single atomic act.
    */
    hash<HttpResponseInfo> handleProppatch(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleProppatch", sock, cx, hdr, body);
    }
} # class WebDavHandlerProxy
} # namespace WebDavHandler
