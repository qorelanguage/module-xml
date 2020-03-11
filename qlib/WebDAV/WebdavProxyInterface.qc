# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

public namespace WebDAV {

#! Proxy  WebDAV inteface class.
public class WebdavProxyInterface inherits AbstractWebdavInterface {
    private {
        hash<string, AbstractWebdavInterface> m_ifaces;
    }

    constructor(hash<string, AbstractWebdavInterface> interfaces, string relative_path_base = "/")
        : AbstractWebdavInterface(relative_path_base) {
        m_ifaces = interfaces;
    }

    /**
        OPTIONS
    */
    hash<HttpResponseInfo> handleOptions(hash<auto> cx, hash<auto> hdr, *data body) {
        return new hash<HttpResponseInfo>({
            "code": 200,
            "hdr": {
                "DAV": "1,2",
                "Depth": "0",
                "Allow": AbstractWebdavInterface::REQUEST_METHODS.keys().join(","),
            },
        });
    }

    private *hash<auto> handleChildRaw(string method_name, hash<auto> cx, hash<auto> hdr, *data body) {
printf("---- %s\n", method_name);
printf("---- raw path %s\n", cx.raw_path);
        string path = getRelativePath(cx.raw_path);
printf("----     path %s\n", path);

        cx.raw_path = "/" + path;

        list<string> parts = path.split("/");
printf("---- %n\n", parts);

        if (parts.size() && m_ifaces.hasKey(parts[0])) {
printf("---- %s\n", "Y");
            AbstractWebdavInterface iface = m_ifaces{parts[0]};
            return call_object_method(iface, method_name, cx, hdr, body);
        }
printf("---- %s\n", "N");
    }

    private hash<HttpResponseInfo> handleChild(string method_name, hash<auto> cx, hash<auto> hdr, *data body) {
        *hash<auto> ret = handleChildRaw(method_name, cx, hdr, body);
        return ret ? cast<hash<HttpResponseInfo>>(ret) : response(501, "Not Implemented");
    }

    /**
        GET

        Get a resource.
    */
    hash<HttpResponseInfo> handleGet(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleGet", cx, hdr, body);
    }

    /**
        HEAD

        Same like GET but without sending a body.
    */
    hash<HttpResponseInfo> handleHead(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleHead", cx, hdr, body);
    }

    /**
        POST

        Create new resource.
    */
    hash<HttpResponseInfo> handlePost(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handlePost", cx, hdr, body);
    }

    /**
        PUT

        Modify resource.
    */
    hash<HttpResponseInfo> handlePut(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handlePut", cx, hdr, body);
    }

    /**
        DELETE

        Delete a resource.
    */
    hash<HttpResponseInfo> handleDelete(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleDelete", cx, hdr, body);
    }

    /**
        COPY

        Copy a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleCopy(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleCopy", cx, hdr, body);
    }

    /**
        MOVE

        Move a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleMove(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleMove", cx, hdr, body);
    }

    /**
        LOCK

        Put a lock on a resource. WebDAV supports both shared and exclusive locks.
    */
    hash<HttpResponseInfo> handleLock(hash<auto> cx, hash<auto> hdr, *data body) {
        return response(501, "Not Implemented");
    }

    /**
        UNLOCK

        Remove a lock from a resource.
    */
    hash<HttpResponseInfo> handleUnlock(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleUnlock", cx, hdr, body);
    }

    /**
        MKCOL

        Create collections (aka a directory).
    */
    hash<HttpResponseInfo> handleMkcol(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleMkcol", cx, hdr, body);
    }

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.
    */
    hash<HttpResponseInfo> handlePropfind(hash<auto> cx, hash<auto> hdr, *data body) {
        # handle children first
        *hash<auto> ret = handleChildRaw("handlePropfind", cx, hdr, body);
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
    hash<HttpResponseInfo> handleProppatch(hash<auto> cx, hash<auto> hdr, *data body) {
        return handleChild("handleProppatch", cx, hdr, body);
    }
} # class WebdavProxyInterface
} # namespace WebDAV
