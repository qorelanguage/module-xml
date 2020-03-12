# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

#! Main public WebDAV namespace
public namespace WebDAV {
#! The abstract WebDAV property handler class interface
public class AbstractWebdavPropertyHandler {
    #! Retrieves the value(s) of the given properties in the given namespace
    abstract auto get(string resource, string ns = DavNs, list<string> prop_names);

    #! Retrieves the value(s) of the given properties in the given namespace
    abstract auto get(string resource, string ns = DavNs, string prop_name);

    #! Returns all properties in all namespaces
    /** @retun a hash as NS -> property -> value
    */
    abstract *hash<string, hash<auto>> getAll(string resource);

    #! Returns all properties in the given namespaces
    /** @retun a hash as property -> value
    */
    abstract *hash<auto> getAllInNamespace(string resource, string ns);

    #! Sets a property value
    abstract set(string resource, string ns = DavNs, string prop_name, auto value);

    #! Deletes one or more properties
    abstract del(string resource, string ns = DavNs, list<string> prop_names);

    #! Deletes a property
    abstract del(string resource, string ns = DavNs, string prop_name);

    #! Deletes all properties for the given resource
    abstract delAll(string resource);

    #! Copies all properties to another target URL
    abstract cp(string src_url, string target_url);

    #! Moves properties from one target URL to another
    abstract move(string src_url, string target_url);
}
}

