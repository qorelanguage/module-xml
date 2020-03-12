# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

#! Main public WebDAV namespace
public namespace WebDAV {
#! This class returns 501 Unimplemented responses to all WebDAV requests except \c OPTIONS
public class DummyWebdavInterface inherits AbstractWebdavInterface {
    constructor(AbstractWebdavPropertyHandler property_handler, string relative_path_base = "/") : AbstractWebdavInterface(property_handler, relative_path_base) {
    }

    /**
        GET

        Get a resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleGetImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        HEAD

        Same like GET but without sending a body.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleHeadImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        POST

        Create new resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handlePostImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        PUT

        Modify resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handlePutImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        DELETE

        Delete a resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleDeleteImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        COPY

        Copy a resource from one URI to another.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleCopyImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        MOVE

        Move a resource from one URI to another.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleMoveImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        LOCK

        Put a lock on a resource. WebDAV supports both shared and exclusive locks.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleLockImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        UNLOCK

        Remove a lock from a resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleUnlockImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        MKCOL

        Create collections (aka a directory).

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleMkcolImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handlePropfindImpl(hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    #! Internal method for handling PROPPATCH requests
    /** @param href the href for the response
        @param cx the HTTP call context
        @param hdr as hash HTTP headers
        @param request_xml the parsed XML in the request
        @param actions a list of set and remove actions to execute on the given properties

        @return a status map of HTTP code -> namespace -> property -> True

        @throw WEBDAV-ERROR an error occured

        @note This method always throws a \c WEBDAV-ERROR exception to return a <tt>501 Unimplemented</tt> response
    */
    private *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx, hash<auto> hdr, *hash<auto> request_xml, *list<hash<PropPatchActionInfo>> actions) {
        throw "WEBDAV-ERROR", "unimplemented", 501;
    }
}
}
