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
#! The abstract WebDavHandler property handler class interface
public class AbstractWebDavPropertyHandler {
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

