/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ql_xml.h

    libxml2-based XML functions

    Qore Programming Language

    Copyright 2003 - 2022 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_QL_XML_H
#define _QORE_QL_XML_H

#include <libxml/xmlschemas.h>
#include <libxml/relaxng.h>

#include <qore/Qore.h>

class MakeXmlOpts;

DLLLOCAL void init_xml_constants(QoreNamespace& ns);

DLLLOCAL QoreStringNode* make_xml(ExceptionSink* xsink, const QoreHashNode &h, const MakeXmlOpts &opts);
DLLLOCAL QoreStringNode* make_xmlrpc_call(ExceptionSink* xsink, const QoreEncoding* ccs, int offset, const QoreListNode* args, int flags = 0);
DLLLOCAL QoreStringNode* make_xmlrpc_call_args(ExceptionSink* xsink, const QoreEncoding* ccs, int offset, const QoreListNode* args, int flags = 0);
// ccsid is the output encoding for strings
DLLLOCAL QoreHashNode* parse_xmlrpc_response(ExceptionSink* xsink, const QoreString* msg, const QoreEncoding* ccsid, int flags = 0);
DLLLOCAL void init_xml_functions(QoreNamespace& ns);

// returns the string corresponding to the element type
DLLLOCAL const char* get_xml_element_type_name(int t);

// returns the string corresponding to the node type
DLLLOCAL const char* get_xml_node_type_name(int t);

#ifdef HAVE_XMLTEXTREADERSETSCHEMA
class QoreXmlSchemaContext : public AbstractXmlValidator, Utf8StringHelper {
protected:
    xmlSchemaPtr schema = nullptr;
    xmlSchemaValidCtxtPtr ctx = nullptr;

public:
    DLLLOCAL QoreXmlSchemaContext(const QoreString& xsd, ExceptionSink* xsink);

    DLLLOCAL virtual ~QoreXmlSchemaContext() {
        if (ctx)
            xmlSchemaFreeValidCtxt(ctx);
        if (schema)
            xmlSchemaFree(schema);
    }

    DLLLOCAL operator bool() const {
        return schema != 0;
    }

    DLLLOCAL xmlSchemaPtr getSchema() {
        return schema;
    }

    DLLLOCAL xmlSchemaValidCtxtPtr getPtr();

    DLLLOCAL virtual int validateDoc(xmlDocPtr doc) {
        getPtr();
        return xmlSchemaValidateDoc(ctx, doc);
    }
};
#endif

#ifdef HAVE_XMLTEXTREADERRELAXNGSETSCHEMA
class QoreXmlRelaxNGContext {
   friend class QoreXmlRelaxNGValidContext;
protected:
   xmlRelaxNGPtr schema;

   DLLLOCAL xmlRelaxNGValidCtxtPtr getValidCtxtPtr() {
      return xmlRelaxNGNewValidCtxt(schema);
   }

public:
   DLLLOCAL QoreXmlRelaxNGContext(const char* rng, int size, ExceptionSink* xsink);
   DLLLOCAL ~QoreXmlRelaxNGContext() {
      if (schema)
         xmlRelaxNGFree(schema);
   }
   DLLLOCAL operator bool() const {
      return schema != 0;
   }
   DLLLOCAL xmlRelaxNGPtr getSchema() {
      return schema;
   }
};

class QoreXmlRelaxNGValidContext {
protected:
   xmlRelaxNGValidCtxtPtr ptr;

   DLLLOCAL QoreXmlRelaxNGValidContext(xmlRelaxNGValidCtxtPtr n_ptr) : ptr(n_ptr) {
   }

public:
   DLLLOCAL QoreXmlRelaxNGValidContext(QoreXmlRelaxNGContext& c) : ptr(c.getValidCtxtPtr()) {
      assert(ptr);
   }
   DLLLOCAL ~QoreXmlRelaxNGValidContext() {
      xmlRelaxNGFreeValidCtxt(ptr);
   }
   DLLLOCAL xmlRelaxNGValidCtxtPtr getPtr() {
      return ptr;
   }
   DLLLOCAL int validateDoc(xmlDocPtr doc) {
      return xmlRelaxNGValidateDoc(ptr, doc);
   }
};
#endif

#endif // _QORE_QL_XML_H
