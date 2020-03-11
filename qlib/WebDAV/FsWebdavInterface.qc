# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

%requires HttpServerUtil
%requires xml
%requires RestClient
%requires Util
%requires FsUtil

#! Main public WebDAV namespace
public namespace WebDAV {
#! Filesystem WebDAV inteface class
public class FsWebdavInterface inherits AbstractWebdavInterface {
    private {
        string basePath;
        bool debug;
    }

    constructor(AbstractWebdavPropertyHandler property_handler, string path, string relative_path_base = "/")
        : AbstractWebdavInterface(property_handler, relative_path_base) {
        basePath = normalize_dir(path);
    }

    private string getPath(string raw_path) {
        string tmp = AbstractWebdavInterface::getRelativePath(raw_path);

        if (tmp.equalPartial("/"))
            tmp = basePath + tmp;
        else
            tmp = basePath + DirSep + tmp;

        return checkPathScope(tmp);
    }

    private string checkPathScope(string tmp) {
        # it's strictly prohibitted to make file operations
        # out of the basePath prefix
        tmp = normalize_dir(tmp);

        if (!tmp.regex("^" + basePath)) {
            throw "WEBDAV-FORBIDDEN", tmp;
        }

        return tmp;
    }

    private *string getContentType(string filePath) {
        return get_mime_type_from_ext(filePath, MimeTypeOctetStream);
        #return "application/octet-stream";
    }

    private hash<auto> hstat2Resource(string path, string displayname, hash<StatInfo> h) {
        return {
            "displayname": displayname,
            "size": h.size,
            "creationdate": h.ctime,
            "getlastmodified": h.mtime,
            "getcontentlength": h.type == "DIRECTORY" ? getContentType(path).size() : h.size,
            "getetag": getETag(path),
            "resourcetype": h.type == "DIRECTORY" ? "collection" : "",
            "getcontenttype": h.type == "DIRECTORY" ? MimeTypeText : getContentType(path),
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
    private list<hash<auto>> preparePropFindResponse(string raw_path, string depth = "infinity", hash<StatInfo> h, bool all_props, *hash<string, hash<string, bool>> additional_props) {
        string local_path = getNormalizedPath(raw_path);
        if (debug) {
            printf("FsWebdavInterface::preparePropFindResponse() AAA %n -- %n\n", raw_path, basename(raw_path));
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

    /**
        OPTIONS
    */
    hash<HttpResponseInfo> handleOptions(hash<auto> cx, hash<auto> hdr, *data body) {
        return <HttpResponseInfo>{
            "code" : 200,
            "hdr": {
                "DAV": "1,2",
                "Depth": "0",
                "Allow": AbstractWebdavInterface::REQUEST_METHODS.keys().join(","),
            },
        };
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

    /**
        GET

        Get a resource.
    */
    hash<HttpResponseInfo> handleGet(hash<auto> cx, hash<auto> hdr, *data body) {
        # True = GET, False = HEAD
        return internalGet(cx, hdr, body, True);
    }

    /**
        HEAD

        Same like GET but without sending a body.
    */
    hash<HttpResponseInfo> handleHead(hash<auto> cx, hash<auto> hdr, *data body) {
        # True = GET, False = HEAD
        return internalGet(cx, hdr, body, False);
    }

    /**
        POST

        Create new resource.
    */
    hash<HttpResponseInfo> handlePost(hash<auto> cx, hash<auto> hdr, *data body) {
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

    /**
        PUT

        Modify resource.
    */
    hash<HttpResponseInfo> handlePut(hash<auto> cx, hash<auto> hdr, *data body) {
        return handlePost(cx, hdr, body);
    }

    /**
        DELETE

        Delete a resource.
    */
    hash<HttpResponseInfo> handleDelete(hash<auto> cx, hash<auto> hdr, *data body) {
        string path = getPath(cx.raw_path);

        *hash<StatInfo> h = hstat(path);
        if (!h) {
            return response(404, "%s: %s", path, strerror());
        }

        FsUtil::remove_path(path);
        deleteProperties(path);
        return response(204);
    }

    private:internal internalCopy(hash<auto> cx, hash<auto> hdr, *data body, bool copy_props) {
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

    /**
        COPY

        Copy a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleCopy(hash<auto> cx, hash<auto> hdr, *data body) {
        return doCopyIntern(cx, hdr, body, True);
    }

    private hash<HttpResponseInfo> doCopyIntern(hash<auto> cx, hash<auto> hdr, *data body, bool copy_props) {
        try {
            internalCopy(cx, hdr, body, copy_props);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err =~ /^[0-9]{3}$/) {
                return response(ex.err.toInt(), ex.desc);
            } else if (ex.err == "WEBDAV-FORBIDDEN") {
                return response(403, "Cannot copy out of current jail");
            }
            rethrow;
        }

        return response(hdr.overwrite == "F" ? 201 : 204);
    }

    /**
        MOVE

        Move a resource from one URI to another.
    */
    hash<HttpResponseInfo> handleMove(hash<auto> cx, hash<auto> hdr, *data body) {
        # additional deletion of source
        string src_path = getPath(cx.raw_path);
        # copy to target first
        hash<HttpResponseInfo> ret = handleCopy(cx, hdr, body);
        # then remove src
        if (ret.code >= 200 && ret.code < 300) {
            FsUtil::remove_path(src_path);
        }
        return ret;
    }

    /**
        LOCK

        Put a lock on a resource. WebDAV supports both shared and exclusive locks.
    */
    hash<HttpResponseInfo> handleLock(hash<auto> cx, hash<auto> hdr, *data body) {
        return response(501);
    }

    /**
        UNLOCK

        Remove a lock from a resource.
    */
    hash<HttpResponseInfo> handleUnlock(hash<auto> cx, hash<auto> hdr, *data body) {
        return response(501);
    }

    /**
        MKCOL

        Create collections (aka a directory).
    */
    hash<HttpResponseInfo> handleMkcol(hash<auto> cx, hash<auto> hdr, *data body) {
        # litmus: MKCOL with weird body must fail (RFC2518:8.3.1)
        if (exists body) {
            return response(415, "Unsupported Media Type");
        }

        # don't allow dirs creation out of given prefix
        string dir_path;
        try {
            dir_path = getPath(cx.raw_path);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "WEBDAV-FORBIDDEN") {
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

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.
    */
    hash<HttpResponseInfo> handlePropfind(hash<auto> cx, hash<auto> hdr, *data body) {
        #if (debug) {
            #printf("FsWebdavInterface::handlePropfind() PPP: \n%N\n%N\n%N\n", cx, hdr, body);
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
                request_xml = parse_xml(body, Qore::Xml::XPF_STRIP_NS_PREFIXES);
            } catch (hash<ExceptionInfo> ex) {
                return response(400, sprintf("%s: %s", ex.err, ex.desc));
            }

            if (!request_xml.hasKey("propfind")) {
                return response(400, "XML body does not contain 'propfind' root tag");
            }

            if (request_xml.propfind.prop) {
                map additional_props{$1.value."^attributes^".xmlns ?? DavNs}{$1.key} = True, request_xml.propfind.prop.pairIterator();
            } else if (request_xml.propfind.hasKey("allprop")) {
                all_props = True;
            }
            #if (debug) {
                #printf("FsWebdavInterface::handlePropfind() Z %N\n", additional_props);
            #}
        }

        # TODO/FIXME: proper PROPFIND xml body handling!
        #   - propname

        # prepare response
        hash<auto> resp;
        resp.multistatus.response = preparePropFindResponse(cx.raw_path, hdr.depth, h, all_props, additional_props);
        resp.multistatus."^attributes^" = {"xmlns": "DAV:"};
        if (debug) {
            printf("FsWebdavInterface::handlePropfind() PPP: \n%N\n%N\n%N\n", cx, hdr, resp);
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
        @param update_hash a hash of property update requests: NS -> key -> value
        @param remove_hash a hash of property remove requests: NS -> key -> True

        @return a status map of HTTP code -> namespace -> property -> True

        @throw WEBDAV-ERROR an error occured
    */
    private *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx, hash<auto> hdr, *hash<auto> request_xml, *hash<string, hash<auto>> update_hash, *hash<string, hash<string, bool>> remove_hash) {
        string path = getNormalizedPath(cx.raw_path);
        href = path;

        *hash<StatInfo> h = hstat(path);
        if (!h) {
            throw "WEBDAV-ERROR", sprintf("%s: %s", path, strerror()), 404;
        }

        # property status map: code -> ns -> property -> True
        hash<string, hash<string, hash<string, bool>>> status_map;

        # verify and then execute property updates
        if (request_xml) {
            *hash<string, hash<string, hash<string, bool>>> verify_status_map = verifyPropertyUpdates(path, update_hash, remove_hash);
            if (verify_status_map) {
                status_map = verify_status_map;
            } else {
                foreach hash<auto> nsi in (update_hash.pairIterator()) {
                    foreach hash<auto> pi in (nsi.value.pairIterator()) {
                        #printf("PROPPATCH set %y (%y) = %y\n", pi.key, nsi.key, pi.value);
                        m_property_handler.set(path, nsi.key, pi.key, pi.value);
                        status_map{200}{nsi.key}{pi.key} = True;
                    }
                }

                foreach hash<auto> nsi in (remove_hash.pairIterator()) {
                    m_property_handler.del(path, nsi.key, keys nsi.value);
                    map status_map{200}{nsi.key}{$1} = True, keys nsi.value;
                }
            }
        }
        return status_map;
    }

    #! Verifies that all property updates can be performed before executing them
    private *hash<string, hash<string, hash<string, bool>>> verifyPropertyUpdates(string path, *hash<string, hash<auto>> update_hash, *hash<string, hash<string, bool>> remove_hash) {
        hash<string, hash<string, hash<string, bool>>> status_map;

        # verify that all operations can succeed
        # verify sets
        foreach hash<auto> nsi in (update_hash.pairIterator()) {
            if (nsi.key == DavNs) {
                map status_map{409}{DavNs}{$1} = True, keys nsi.value;
            }
        }

        # verify removes
        foreach hash<auto> nsi in (remove_hash.pairIterator()) {
            if (nsi.key == DavNs) {
                map status_map{409}{DavNs}{$1} = True, keys nsi.value;
            }
        }

        # all updates fail if one fails
        if (!status_map) {
            return;
        }

        foreach hash<auto> nsi in (update_hash.pairIterator()) {
            foreach hash<auto> pi in (nsi.value.pairIterator()) {
                if (!status_map{409}{nsi.key}{pi.key}) {
                    status_map{424}{nsi.key}{pi.key} = True;
                }
            }
        }

        foreach hash<auto> nsi in (remove_hash.pairIterator()) {
            foreach hash<auto> pi in (nsi.value.pairIterator()) {
                if (!status_map{409}{nsi.key}{pi.key}) {
                    status_map{424}{nsi.key}{pi.key} = True;
                }
            }
        }
        return status_map;
    }
} # class FsWebdavInterface
} # namespace WebDAV

