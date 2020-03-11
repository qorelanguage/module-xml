# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

#! Main public WebDAV namespace
public namespace WebDAV {
#! This class returns 501 Unimplemented responses to all WebDAV requests
public class DummyWebdavInterface inherits AbstractWebdavInterface {
    constructor(AbstractWebdavPropertyHandler property_handler, string relative_path_base = "/") : AbstractWebdavInterface(property_handler, relative_path_base) {
    }

    /**
        OPTIONS
    */
    hash<HttpResponseInfo> handleOptions(hash<auto> cx, hash<auto> hdr, *data body) {
        return new hash<HttpResponseInfo>({
            "code": 200,
            "hdr": {
                "DAV" : "1,2",
                "Depth" : "0",
                "Allow" : AbstractWebdavInterface::REQUEST_METHODS.keys().join(","),
            },
        });
    }

    /**
        GET

        Get a resource.
    */
    hash<HttpResponseInfo> handleGet(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        HEAD

        Same like GET but without sending a body.
    */
    hash<HttpResponseInfo> handleHead(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        POST

        Create new resource.
    */
    hash<HttpResponseInfo> handlePost(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        PUT

        Modify resource.
    */
    hash<HttpResponseInfo> handlePut(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        DELETE

        Delete a resource.
    */
    hash<HttpResponseInfo> handleDelete(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        COPY

        Copy a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleCopy(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        MOVE

        Move a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleMove(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        LOCK

        Put a lock on a resource. WebDAV supports both shared and exclusive locks.
    */
    hash<HttpResponseInfo> handleLock(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        UNLOCK

        Remove a lock from a resource.
    */
    hash<HttpResponseInfo> handleUnlock(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        MKCOL

        Create collections (aka a directory).
    */
    hash<HttpResponseInfo> handleMkcol(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.
    */
    hash<HttpResponseInfo> handlePropfind(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    private *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx, hash<auto> hdr, *hash<auto> request_xml, *hash<string, hash<auto>> update_hash, *hash<string, hash<string, bool>> remove_hash) {
        throw "WEBDAV-ERROR", "unimplemented", 501;
    }
}
}
