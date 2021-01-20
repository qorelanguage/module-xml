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
#! An in-memory property handler that stores and retrieves all properties in memory
public class InMemoryWebDavPropertyHandler inherits AbstractWebDavPropertyHandler {
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
            printf("InMemoryWebDavPropertyHandler::get(%y, %y) %s: %y\n", prop_names, ns, resource, m_cache{resource}{prop_names});
        }
        return m_cache{resource}{ns}{prop_names};
    }

    #! Retrieves the value(s) of the given properties in the given namespace
    auto get(string resource, string ns = DavNs, string prop_name) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebDavPropertyHandler::get(%y, %y) %s: %y\n", prop_name, ns, resource, m_cache{resource}{prop_name});
        }
        return m_cache{resource}{ns}{prop_name};
    }

    #! Returns all properties in all namespaces
    /** @retun a hash as NS -> property -> value
    */
    *hash<string, hash<auto>> getAll(string resource) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebDavPropertyHandler::getAll(%y): %y\n", resource, m_cache{resource});
        }
        return m_cache{resource};
    }

    #! Returns all properties in the given namespaces
    /** @retun a hash as property -> value
    */
    *hash<auto> getAllInNamespace(string resource, string ns) {
        AutoReadLock al(m_lock);
        if (debug) {
            printf("InMemoryWebDavPropertyHandler::getAll(%y, %y): %y\n", resource, ns, m_cache{resource});
        }
        return m_cache{resource}{ns};
    }

    #! Sets a property value
    set(string resource, string ns = DavNs, string prop_name, auto value) {
        AutoWriteLock al(m_lock);
        m_cache{resource}{ns}{prop_name} = value;
        if (debug) {
            printf("InMemoryWebDavPropertyHandler::set() CACHE %y\n", m_cache);
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
} # class InMemoryWebDavPropertyHandler
} # namespace WebDavHandler

