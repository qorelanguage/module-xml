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

#! Main public WebDavHandler namespace
public namespace WebDavHandler {
#! This class returns 501 Unimplemented responses to all WebDavHandler requests except \c OPTIONS
public class DummyWebDavHandler inherits AbstractWebDavHandler {
    #! Creates the object with an empty logger and an in-memory property handler
    constructor() {
    }

    #! Creates the object with the given argument and an in-memory property handler
    /** @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(string virtual_relative_path_base) : AbstractWebDavHandler(NOTHING, virtual_relative_path_base) {
    }

    #! Creates the object with the given arguments
    /** @param property_handler to handle properties
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(AbstractWebDavPropertyHandler property_handler, string virtual_relative_path_base = "/")
             : AbstractWebDavHandler(NOTHING, property_handler, virtual_relative_path_base) {
    }

    #! Creates the object with the given arguments
    /** @param logger the logger
        @param property_handler to handle properties
        @param virtual_relative_path_base the virtual base path for WebDavHandler requests; this path will be
        stripped from requests internally when serving WebDavHandler resources
    */
    constructor(Logger logger, AbstractWebDavPropertyHandler property_handler =
                new InMemoryWebDavPropertyHandler(), string virtual_relative_path_base = "/")
                : AbstractWebDavHandler(NOTHING, logger, property_handler, virtual_relative_path_base) {
    }

    /**
        GET

        Get a resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleGetImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        HEAD

        Same like GET but without sending a body.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleHeadImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        POST

        Create new resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handlePostImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        PUT

        Modify resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handlePutImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        DELETE

        Delete a resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleDeleteImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        COPY

        Copy a resource from one URI to another.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleCopyImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        MOVE

        Move a resource from one URI to another.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleMoveImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        LOCK

        Put a lock on a resource. WebDavHandler supports both shared and exclusive locks.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleLockImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        UNLOCK

        Remove a lock from a resource.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleUnlockImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        MKCOL

        Create collections (aka a directory).

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handleMkcolImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.

        @note This method always returns a <tt>501 Unimplemented</tt> response
    */
    private hash<HttpResponseInfo> handlePropfindImpl(Socket sock, hash<auto> cx, hash<auto> hdr, *data body) {
        return resp501();
    }

    #! Internal method for handling PROPPATCH requests
    /** @param href the href for the response
        @param cx the HTTP call context
        @param hdr as hash HTTP headers
        @param request_xml the parsed XML in the request
        @param actions a list of set and remove actions to execute on the given properties

        @return a status map of HTTP code -> namespace -> property -> True

        @throw WEBDAVHANDLER-ERROR an error occured

        @note This method always throws a \c WEBDAVHANDLER-ERROR exception to return a <tt>501 Unimplemented</tt>
        response
    */
    private *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx,
            hash<auto> hdr, *hash<auto> request_xml, *list<hash<PropPatchActionInfo>> actions) {
        throw "WEBDAVHANDLER-ERROR", "unimplemented", 501;
    }
}
}
