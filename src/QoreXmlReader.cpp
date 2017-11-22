/* -*- indent-tabs-mode: nil -*- */
/*
 QoreXmlReader.cpp

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

#include "QoreXmlReader.h"
#include "QoreXmlRpcReader.h"

static bool keys_are_equal(const char* k1, const char* k2, bool &get_value) {
   while (true) {
      if (!(*k1)) {
         if (!(*k2))
            return true;
         if ((*k2) == '^') {
            get_value = true;
            return true;
         }
         return false;
      }
      if ((*k1) != (*k2))
         break;
      k1++;
      k2++;
   }
   return false;
}

QoreHashNode* QoreXmlReader::parseXmlData(const QoreEncoding* data_ccsid, int pflags, ExceptionSink* xsink) {
   if (read(xsink) != 1)
      return 0;

   AbstractQoreNode* rv = getXmlData(xsink, data_ccsid, pflags, depth());

   if (!rv) {
      if (!*xsink)
         xsink->raiseExceptionArg("PARSE-XML-EXCEPTION", xml ? new QoreStringNode(*xml) : 0, "parse error parsing XML string");
      return 0;
   }
   assert(rv->getType() == NT_HASH);

   return reinterpret_cast<QoreHashNode*>(rv);
}

AbstractQoreNode* QoreXmlReader::getXmlData(ExceptionSink* xsink, const QoreEncoding* data_ccsid, int pflags, int min_depth) {
   Qore::Xml::intern::xml_stack xstack;

   QORE_TRACE("getXMLData()");
   int rc = 1;

   while (rc == 1) {
      int nt = nodeTypeSkipWhitespace();
      // get node name
      const char* name = constName();
      if (!name)
         name = "--";

      if (nt == -1) // ERROR
         break;

      if (nt == XML_READER_TYPE_ELEMENT) {
         int depth = QoreXmlReader::depth();
         xstack.checkDepth(depth);

         AbstractQoreNode* n = xstack.getNode();
         // if there is no node pointer, then make a hash
         if (!n) {
            QoreHashNode* h = new QoreHashNode;
            xstack.setNode(h);
            xstack.push(h->getKeyValuePtr(name), depth);
         }
         else { // node ptr already exists
            QoreHashNode* h = n->getType() == NT_HASH ? reinterpret_cast<QoreHashNode*>(n) : 0;
            if (!h) {
               h = new QoreHashNode;
               xstack.setNode(h);
               h->setKeyValue("^value^", n, 0);
               xstack.incValueCount();
               xstack.push(h->getKeyValuePtr(name), depth);
            }
            else {
               // see if key already exists
               AbstractQoreNode* v;
               bool exists;
               v = h->getKeyValueExistence(name, exists);

               if (!exists)
                  xstack.push(h->getKeyValuePtr(name), depth);
               else {
                  if (!(pflags & XPF_PRESERVE_ORDER)) {
                     QoreListNode* vl = get_node_type(v) == NT_LIST ? reinterpret_cast<QoreListNode*>(v) : 0;
                     // if it's not a list, then make into a list with current value as first entry
                     if (!vl) {
                        AbstractQoreNode** vp = h->getKeyValuePtr(name);
                        vl = new QoreListNode;
                        vl->push(v);
                        (*vp) = vl;
                     }
                     xstack.push(vl->get_entry_ptr(vl->size()), depth);
                  }
                  else {
                     // see if last key was the same, if so make a list if it's not
                     const char* lk = h->getLastKey();
                     bool get_value = false;
                     if (keys_are_equal(name, lk, get_value)) {
                        // get actual key value if there was a suffix
                        if (get_value)
                           v = h->getKeyValue(lk);

                        QoreListNode* vl = get_node_type(v) == NT_LIST ? reinterpret_cast<QoreListNode*>(v) : 0;
                        // if it's not a list, then make into a list with current value as first entry
                        if (!vl) {
                           AbstractQoreNode** vp = h->getKeyValuePtr(lk);
                           vl = new QoreListNode;
                           vl->push(v);
                           (*vp) = vl;
                        }
                        xstack.push(vl->get_entry_ptr(vl->size()), depth);
                     }
                     else {
                        QoreString ns;
                        int c = 1;
                        while (true) {
                           ns.sprintf("%s^%d", name, c);
                           if (!h->existsKey(ns.getBuffer()))
                              break;
                           c++;
                           ns.clear();
                        }
                        xstack.push(h->getKeyValuePtr(ns.getBuffer()), depth);
                     }
                  }
               }
            }
         }
         // add attributes to structure if possible
         if (hasAttributes()) {
            ReferenceHolder<QoreHashNode> h(new QoreHashNode, xsink);
            while (moveToNextAttribute(xsink) == 1) {
               const char* aname = constName();
               QoreStringNode* value = getValue(data_ccsid, xsink);
               if (!value)
                  return 0;
               h->setKeyValue(aname, value, xsink);
            }
            if (*xsink)
               return 0;

            // make new new a hash and assign "^attributes^" key
            QoreHashNode* nv = new QoreHashNode;
            nv->setKeyValue("^attributes^", h.release(), xsink);
            xstack.setNode(nv);
         }
         //printd(5, "%s: type: %d, hasValue: %d, empty: %d, depth: %d\n", name, nt, xmlTextReaderHasValue(reader), xmlTextReaderIsEmptyElement(reader), depth);
      }
      else if (nt == XML_READER_TYPE_TEXT) {
         int depth = QoreXmlReader::depth();
         xstack.checkDepth(depth);

         const char* str = constValue();
         if (str) {
            QoreStringNodeHolder val(getValue(data_ccsid, xsink));
            if (!val)
               return 0;

            AbstractQoreNode* n = xstack.getNode();
            if (n) {
               QoreHashNode* h = n->getType() == NT_HASH ? reinterpret_cast<QoreHashNode*>(n) : 0;
               if (h) {
                  if (!xstack.getValueCount())
                     h->setKeyValue("^value^", val.release(), xsink);
                  else {
                     QoreString kstr;
                     kstr.sprintf("^value%d^", xstack.getValueCount());
                     h->setKeyValue(kstr.getBuffer(), val.release(), xsink);
                  }
               }
               else { // convert value to hash and save value node
                  h = new QoreHashNode;
                  xstack.setNode(h);
                  h->setKeyValue("^value^", n, 0);
                  xstack.incValueCount();

                  QoreString kstr;
                  kstr.sprintf("^value%d^", 1);
                  h->setKeyValue(kstr.getBuffer(), val.release(), xsink);
               }
               xstack.incValueCount();
            }
            else
               xstack.setNode(val.release());
         }
      }
      else if (nt == XML_READER_TYPE_CDATA) {
         int depth = QoreXmlReader::depth();
         xstack.checkDepth(depth);

         const char* str = constValue();
         if (str) {
            QoreStringNode* val = getValue(data_ccsid, xsink);
            if (!val)
               return 0;

            AbstractQoreNode* n = xstack.getNode();
            if (n && n->getType() == NT_HASH) {
               QoreHashNode* h = reinterpret_cast<QoreHashNode*>(n);
               if (!xstack.getCDataCount())
                  h->setKeyValue("^cdata^", val, xsink);
               else {
                  QoreString kstr;
                  kstr.sprintf("^cdata%d^", xstack.getCDataCount());
                  h->setKeyValue(kstr.getBuffer(), val, xsink);
               }
            }
            else { // convert value to hash and save value node
               QoreHashNode* h = new QoreHashNode;
               xstack.setNode(h);
               if (n) {
                  h->setKeyValue("^value^", n, 0);
                  xstack.incValueCount();
               }

               h->setKeyValue("^cdata^", val, xsink);
            }
            xstack.incCDataCount();
         }
      } else if (nt == XML_READER_TYPE_COMMENT && (pflags & XPF_ADD_COMMENTS)) {
         int depth = QoreXmlReader::depth();
         xstack.checkDepth(depth);

         const char* str = constValue();
         if (str) {
            QoreStringNode* val = getValue(data_ccsid, xsink);
            if (!val)
               return 0;

            AbstractQoreNode* n = xstack.getNode();
            if (n && n->getType() == NT_HASH) {
               QoreHashNode* h = reinterpret_cast<QoreHashNode*>(n);
               if (!xstack.getCommentCount())
                  h->setKeyValue("^comment^", val, xsink);
               else {
                  QoreString kstr;
                  kstr.sprintf("^comment%d^", xstack.getCommentCount());
                  h->setKeyValue(kstr.getBuffer(), val, xsink);
               }
            }
            else { // convert value to hash and save value node
               QoreHashNode* h = new QoreHashNode;
               xstack.setNode(h);
               if (n) {
                  h->setKeyValue("^value^", n, 0);
                  xstack.incValueCount();
               }

               h->setKeyValue("^comment^", val, xsink);
            }
            xstack.incCommentCount();
         }
      }
      rc = read();

      if (min_depth > 0 && QoreXmlReader::depth() < min_depth) {
         rc = 0;
         break;
      }
   }
   return rc ? 0 : xstack.getVal();
}
