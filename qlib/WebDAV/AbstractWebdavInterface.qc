# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

#! Main public WebDAV namespace
public namespace WebDAV {
#! Abstract WebDAV interface class
public class AbstractWebdavInterface {
    public {
        #! A hash of all supported WebDAV methods mapped to method names
        const REQUEST_METHODS = {
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
    }

    private {
        AbstractWebdavPropertyHandler m_property_handler;
        string m_relative_path_base;
    }

    constructor(AbstractWebdavPropertyHandler property_handler,
                string relative_path_base = "/") {
        m_property_handler = property_handler;
        m_relative_path_base = normalize_dir(relative_path_base);
    }

    list<string> getHttpMethods() {
        return AbstractWebdavInterface::REQUEST_METHODS.keys();
    }

    #! Returns a relative path from an absolute path
    string getRelativePath(string raw_path) {
        return regex_subst(raw_path, "^" + m_relative_path_base, "");
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

    #! Copies all properties from the given source URL to the target URL
    private copyProperties(string source_url, string target_url) {
        m_property_handler.cp(source_url, target_url);
    }

    #! Moves all properties from the given source URL to the target URL
    private moveProperties(string source_url, string target_url) {
        m_property_handler.move(source_url, target_url);
    }

    #! Deletes all properties from the given URL
    private deleteProperties(string source_url) {
        m_property_handler.delAll(source_url);
    }

    #! Returns a hash that can be serialized to an XML string for a PROPFIND request
    private hash<auto> preparePropFindXmlResponse(string local_path, string href, bool is_collection, hash<auto> base_props, bool all_props = False, *hash<string, hash<string, bool>> additional_props) {
        # this must be "hash"; cannot be "hash<auto>"
        hash res.href = href;

        # maps from status code -> namespace -> prop
        hash code_map;

        # handle additonal props from the Property Handler
        hash<auto> rv_props;
        if (all_props) {
            rv_props{DavNs}.resourcetype = is_collection ? {"collection": NOTHING} : NOTHING;
            rv_props{DavNs} = base_props;
            hash<string, hash<auto>> prop_val = m_property_handler.getAll(local_path);
            map rv_props{$1.key} += $1.value, prop_val.pairIterator();
        } else {
            if (*hash<auto> def_prop_map = remove additional_props{DavNs}) {
                if (remove def_prop_map.resourcetype) {
                    rv_props{DavNs}.resourcetype = is_collection ? {"collection": NOTHING} : NOTHING;
                }
                list<string> def_props = keys def_prop_map;
                rv_props{DavNs} += base_props{def_props} + m_property_handler.get(local_path, DavNs, def_props);
                # add 404 response for not found properties
                if (*list<string> missing = keys (def_prop_map - ((keys rv_props{DavNs}) ?? ""))) {
                    map code_map."404"{DavNs}{$1} = NOTHING, missing;
                }
            }
            foreach hash<auto> i in (additional_props.pairIterator()) {
                rv_props{i.key} = m_property_handler.get(local_path, i.key, keys i.value);
                # add 404 response for not found properties
                if (*list<string> missing = keys (i.value - ((keys rv_props{i.key}) ?? ""))) {
                    map code_map."404"{i.key}{$1} = NOTHING, missing;
                }
            }
        }

        res.propstat = ();

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

        #printf("CODE MAP: %N\n", code_map);

        int num = 1;
        hash<string, string> nsmap;
        hash<string, string> ps_attr;
        foreach hash<auto> i in (code_map.pairIterator()) {
            string msg = getStatusMessage(i.key);
            hash<auto> propstat;
            foreach hash<auto> pi in (i.value.pairIterator()) {
                if (pi.key != DavNs) {
                    *string pfx = nsmap{pi.key};
                    if (!pfx) {
                        pfx = nsmap{pi.key} = "ns" + ((num++).toString());
                        ps_attr += {
                            ("xmlns:" + pfx): pi.key,
                        };
                    }
                    # add ns prefix to prop name keys
                    code get_value = auto sub (auto val) {
                        if (val.typeCode() != NT_HASH) {
                            return val;
                        }
                        return map {(pfx + ":" + $1.key): get_value($1.value)}, val.pairIterator();
                    };
                    propstat.prop += get_value(pi.value);
                } else {
                    propstat.prop += pi.value;
                }
            }
            res.propstat += propstat + {
                "status": msg,
            };
        }
        if (ps_attr) {
            res."^attributes^" += ps_attr;
        }

        #printf("pRXH: rv_props: %N\n", rv_props);
        #printf("res: %N\n", res);

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

    /**
        PROPPATCH

        Create, change and delete one or more properties on a resource in a single atomic action
    */
    hash<HttpResponseInfo> handleProppatch(hash<auto> cx, hash<auto> hdr, *data body) {
        # parse the mandatory XML body
        hash<auto> request_xml;
        # update hash: NS -> key -> value
        hash<string, hash<auto>> update_hash;
        # remove hash: NS -> key -> True
        hash<string, hash<string, bool>> remove_hash;

        if (body) {
            try {
                request_xml = parse_xml(body, Qore::Xml::XPF_STRIP_NS_PREFIXES);
            } catch (hash<ExceptionInfo> ex) {
                return response(400, sprintf("%s: %s", ex.err, ex.desc));
            }

            if (!request_xml.hasKey("propertyupdate")) {
                return response(400, "XML body does not contain 'propertyupdate' root tag");
            }

            # process sets
            foreach hash<auto> prop in (request_xml.propertyupdate.set) {
                foreach hash<auto> i in (prop.prop.pairIterator()) {
                    auto value = i.value."^value^";
                    string ns = (remove i.value."^attributes^").xmlns ?? DavNs;
                    if (!exists value) {
                        value = i.value."^value^" ?? i.value;
                        if (value.typeCode() == NT_HASH && value.firstValue()."^attributes^") {
                            value{value.firstKey()} = value.firstValue() - "^attributes^";
                        }
                    }
                    update_hash{ns}{i.key} = value;
                }
            }

            # process removes
            foreach hash<auto> prop in (request_xml.propertyupdate."remove") {
                foreach hash<auto> i in (prop.prop.pairIterator()) {
                    string ns = (remove i.value."^attributes^").xmlns ?? DavNs;
                    remove_hash{ns}{i.key} = True;
                }
            }
        }

        # do the implementation-specific updates
        string href;
        *hash<string, hash<string, hash<string, bool>>> status_map;
        try {
            status_map = handleProppatchImpl(\href, cx, hdr, request_xml, update_hash, remove_hash);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "WEBDAV-ERROR") {
                return response(ex.arg ?? 500, ex.desc);
            }
            rethrow;
        }

        # create the response message
        hash response.propstat = ();
        int num = 1;
        hash<string, string> nsmap;
        hash<auto> ps_attr;
        foreach hash<auto> ci in (status_map.pairIterator()) {
            foreach hash<auto> nsi in (ci.value.pairIterator()) {
                hash<auto> propstat;
                if (nsi.key == DavNs) {
                    propstat.prop = keys nsi.value;
                } else {
                    *string pfx = nsmap{nsi.key};
                    if (!pfx) {
                        pfx = nsmap{nsi.key} = "ns" + ((num++).toString());
                        ps_attr += {
                            ("xmlns:" + pfx): nsi.key,
                        };
                    }
                    propstat.prop = map pfx + ":" + $1, keys nsi.value;
                }
                propstat.status = getStatusMessage(ci.key);
                response.propstat += propstat;
            }
        }
        if (ps_attr) {
            response."^attributes^" += ps_attr;
        }

        # prepare response
        hash resp.multistatus.response = {
            "href": href,
        };
        #prepareResourceHash(cx.raw_path, hdr.depth, h, False, additional_props);
        resp.multistatus."^attributes^" = {"xmlns": "DAV:"};
        resp.multistatus.response = response;

        return <HttpResponseInfo>{
            "code": 207,
            "body": make_xml(resp),
        };
    }

    /**
        OPTIONS

        Handles an OPTIONS request
    */
    abstract hash<HttpResponseInfo> handleOptions(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        GET

        Get a resource.
    */
    abstract hash<HttpResponseInfo> handleGet(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        HEAD

        Like GET but without sending a body.
    */
    abstract hash<HttpResponseInfo> handleHead(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        POST

        Create a new resource.
    */
    abstract hash<HttpResponseInfo> handlePost(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        PUT

        Modify a resource.
    */
    abstract hash<HttpResponseInfo> handlePut(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        DELETE

        Delete a resource.
    */
    abstract hash<HttpResponseInfo> handleDelete(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        COPY

        Copy a resource from one URI to another.
    */
    abstract hash<HttpResponseInfo> handleCopy(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        MOVE

        Move a resource from one URI to another.
    */
    abstract hash<HttpResponseInfo> handleMove(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        LOCK

        Put a lock on a resource. WebDAV supports both shared and exclusive locks.
    */
    abstract hash<HttpResponseInfo> handleLock(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        UNLOCK

        Remove a lock from a resource.
    */
    abstract hash<HttpResponseInfo> handleUnlock(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        MKCOL

        Create collections (aka a directory).
    */
    abstract hash<HttpResponseInfo> handleMkcol(hash<auto> cx, hash<auto> hdr, *data body);

    /**
        PROPFIND

        Retrieve properties, stored as XML, from a web resource. It is also
        overloaded to allow one to retrieve the collection structure (also
        known as directory hierarchy) of a remote system.
    */
    abstract hash<HttpResponseInfo> handlePropfind(hash<auto> cx, hash<auto> hdr, *data body);

    #! Internal method for handling PROPPATCH requests
    /** @param href the href for the response
        @param cx the HTTP call context
        @param hdr as hash HTTP headers
        @param request_xml the parsed XML in the request
        @param update_hash a hash of property update requests: NS -> key -> value
        @param remove_hash a hash of property remove requests: NS -> key -> True

        @return a status map of HTTP code -> namespace -> property -> True

        @throw WEBDAV-ERROR an error occured
    */
    private abstract *hash<string, hash<string, hash<string, bool>>> handleProppatchImpl(reference<string> href, hash<auto> cx, hash<auto> hdr, *hash<auto> request_xml, *hash<string, hash<auto>> update_hash, *hash<string, hash<string, bool>> remove_hash);
}
}

