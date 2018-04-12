/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 QoreXmlRpcReader.h

 Qore Programming Language

 Copyright (C) 2003 - 2015 David Nichols

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

#ifndef _QORE_QOREXMLRPCREADER_H
#define _QORE_QOREXMLRPCREADER_H

#include "QoreXmlReader.h"

namespace Qore {
namespace Xml {
namespace intern { // make classes local
   class XmlRpcValue {
   private:
      QoreValue val;
      AbstractQoreNode** vp;

   public:
      DLLLOCAL XmlRpcValue() : vp(0) {
      }

      DLLLOCAL ~XmlRpcValue() {
         val.discard(0);
      }

      DLLLOCAL AbstractQoreNode* getValueNode() {
         return val.takeNode();
      }

      DLLLOCAL QoreValue getValue() {
         QoreValue rv;
         rv.swap(val);
         return rv;
      }

      DLLLOCAL void set(QoreValue v) {
         if (vp)
            *vp = v.takeNode();
         else
            discard(val.assignAndSanitize(v), 0);
      }

      DLLLOCAL void setPtr(AbstractQoreNode** v) {
         vp = v;
      }
   };

   class xml_node {
   public:
      QoreHashNode* h;
      const char* key;
      AbstractQoreNode** node;
      xml_node* next;
      int depth;
      int vcount;
      int cdcount;
      int commentcount;
      bool closed = false;

      DLLLOCAL xml_node(QoreHashNode* h, const char* key, AbstractQoreNode** n, int d)
         : h(h), key(key), node(n), next(0), depth(d), vcount(0), cdcount(0), commentcount(0) {
      }
   };

   class xml_stack {
   private:
      xml_node* tail = nullptr;
      AbstractQoreNode* val = nullptr;
      bool ignore_empty_elements = false;

   public:
      DLLLOCAL xml_stack(int pflags = 0) {
          if (pflags & XPF_IGNORE_EMPTY_ELEMENTS) {
              ignore_empty_elements = true;
          }
         push(0, 0, &val, -1);
      }

      DLLLOCAL ~xml_stack() {
         if (val)
            val->deref(0);

         while (tail) {
            //printd(5, "xml_stack::~xml_stack(): deleting: %p (%d), next: %p\n", tail, tail->depth, tail->next);
            xml_node* n = tail->next;
            delete tail;
            tail = n;
         }
      }

      DLLLOCAL void checkDepth(int depth, bool check_del = false) {
         while (tail && depth && tail->depth >= depth) {
            xml_node* n = tail->next;
            //printd(5, "xml_stack::checkDepth(%d): deleting: %p (%d), new tail: %p (key: %s node: %p depth: %d) cd: %d iee: %d\n", depth, tail, tail->depth, n, tail->key, tail->node ? *tail->node : nullptr, tail->depth, check_del, ignore_empty_elements);
            if (check_del) {
                delElement(tail);
            }
            delete tail;
            tail = n;
         }
      }

    DLLLOCAL void delElement(xml_node* n) {
        if (n->node && get_node_type(*n->node) == NT_NOTHING && ignore_empty_elements && !n->closed) {
            n->h->removeKey(n->key, nullptr);
            n->node = nullptr;
        }
    }

    DLLLOCAL void close(int depth) {
        xml_node* n = tail;
        while (n && n->depth > depth) {
            delElement(n);
            n = n->next;
        }
        assert(n);
        assert(n->depth == depth);

        //printd(5, "xml_stack::close(): key: %s node: %p depth: %d\n", n->key, *n->node, n->depth);
        assert(tail);
        if (!n->closed) {
            n->closed = true;
        }
    }

      DLLLOCAL void push(QoreHashNode* h, const char* key, AbstractQoreNode** node, int depth) {
         xml_node* sn = new xml_node(h, key, node, depth);
         sn->next = tail;
         tail = sn;
      }
      DLLLOCAL AbstractQoreNode* getNode() {
          return *tail->node;
      }
      DLLLOCAL void setNode(AbstractQoreNode* n) {
         (*tail->node) = n;
      }
      DLLLOCAL AbstractQoreNode* getVal() {
         AbstractQoreNode* rv = val;
         val = 0;
         return rv;
      }
      DLLLOCAL int getValueCount() const {
         return tail->vcount;
      }
      DLLLOCAL void incValueCount() {
         tail->vcount++;
      }
      DLLLOCAL int getCDataCount() const {
         return tail->cdcount;
      }
      DLLLOCAL void incCDataCount() {
         tail->cdcount++;
      }
      DLLLOCAL int getCommentCount() const {
         return tail->commentcount;
      }
      DLLLOCAL void incCommentCount() {
         tail->commentcount++;
      }
   };
}
}
}

class QoreXmlRpcReader : public QoreXmlReader {
public:
   DLLLOCAL QoreXmlRpcReader(const QoreString* n_xml, int options, ExceptionSink* xsink) : QoreXmlReader(n_xml, options, xsink) {
   }

   DLLLOCAL int readXmlRpc(ExceptionSink* xsink) {
      return readSkipWhitespace(xsink) != 1;
   }

   DLLLOCAL int readXmlRpc(const char* info, ExceptionSink* xsink) {
      return readSkipWhitespace(info, xsink) != 1;
   }

   DLLLOCAL int readXmlRpcNode(ExceptionSink* xsink) {
      int nt = nodeTypeSkipWhitespace();
      if (nt == -1 && !*xsink)
         xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string");
      return nt;
   }

   DLLLOCAL int checkXmlRpcMemberName(const char* member, ExceptionSink* xsink, bool close = false) {
      const char* name = (const char*)xmlTextReaderConstName(reader);
      if (!name) {
         xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", xml ? new QoreStringNode(*xml) : 0, "expecting %selement '%s', got NOTHING", close ? "closing " : "", member);
         return -1;
      }

      if (strcmp(name, member)) {
         xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", xml ? new QoreStringNode(*xml) : 0, "expecting %selement '%s', got '%s'", close ? "closing " : "", member, name);
         return -1;
      }
      return 0;
   }

   DLLLOCAL int getArray(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink);
   DLLLOCAL int getStruct(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink);
   DLLLOCAL int getString(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink);
   DLLLOCAL int getBoolean(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink);
   DLLLOCAL int getInt(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink);
   DLLLOCAL int getDouble(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink);
   DLLLOCAL int getDate(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink);
   DLLLOCAL int getBase64(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink);
   DLLLOCAL int getValueData(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, bool read_next, ExceptionSink* xsink);
   DLLLOCAL int getParams(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink);
};

#endif
