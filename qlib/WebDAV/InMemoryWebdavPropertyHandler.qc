# -*- mode: qore; indent-tabs-mode: nil -*-

%new-style
%enable-all-warnings
%require-types
%strict-args

#! Main public WebDAV namespace
public namespace WebDAV {
#! An in-memory property handler that stores and retrieves all properties in memory
public class InMemoryWebdavPropertyHandler inherits AbstractWebdavPropertyHandler {
    private {
        #! Read-write lock for atomicity
        RWLock m_lock();

        #! Property cache; URL -> namespace -> property -> value
        hash<string, hash<string, hash<auto>>> m_cache = {};

        #! Debug flag
        bool debug;
    }

    constructor() {
    }

    #! Retrieves the value(s) of the given properties in the given namespace
    *hash<auto> get(string resource, string ns = DavNs, list<string> prop_names) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebdavPropertyHandler::get(%y, %y) %s: %y\n", prop_names, ns, resource, m_cache{resource}{prop_names});
        }
        return m_cache{resource}{ns}{prop_names};
    }

    #! Retrieves the value(s) of the given properties in the given namespace
    auto get(string resource, string ns = DavNs, string prop_name) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebdavPropertyHandler::get(%y, %y) %s: %y\n", prop_name, ns, resource, m_cache{resource}{prop_name});
        }
        return m_cache{resource}{ns}{prop_name};
    }

    #! Returns all properties in all namespaces
    /** @retun a hash as NS -> property -> value
    */
    *hash<string, hash<auto>> getAll(string resource) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebdavPropertyHandler::getAll(%y): %y\n", resource, m_cache{resource});
        }
        return m_cache{resource};
    }

    #! Returns all properties in the given namespaces
    /** @retun a hash as property -> value
    */
    *hash<auto> getAllInNamespace(string resource, string ns) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebdavPropertyHandler::getAll(%y, %y): %y\n", resource, ns, m_cache{resource});
        }
        return m_cache{resource}{ns};
    }

    #! Sets a property value
    set(string resource, string ns = DavNs, string prop_name, auto value) {
        AutoWriteLock al(m_lock);
        m_cache{resource}{ns}{prop_name} = value;
        if (debug) {
            printf("InMemoryWebdavPropertyHandler::set() CACHE %y\n", m_cache);
        }
    }

    #! Deletes one or more properties
    del(string resource, string ns = DavNs, list<string> prop_names) {
        AutoWriteLock al(m_lock);
        delete m_cache{resource}{ns}{prop_names};
    }

    #! Deletes a property
    del(string resource, string ns = DavNs, string prop_name) {
        AutoWriteLock al(m_lock);
        delete m_cache{resource}{ns}{prop_name};
    }

    #! Deletes all properties for the given resource
    delAll(string resource) {
        AutoWriteLock al(m_lock);
        delete m_cache{resource};
    }

    #! Copies all properties to another target URL
    cp(string src_url, string target_url) {
        AutoWriteLock al(m_lock);
        m_cache{target_url} = m_cache{src_url};
    }

    #! Moves properties from one target URL to another
    move(string src_url, string target_url) {
        AutoWriteLock al(m_lock);
        m_cache{target_url} = m_cache{src_url};
        delete m_cache{src_url};
    }
} # class InMemoryWebdavPropertyHandler
} # namespace WebDAV

