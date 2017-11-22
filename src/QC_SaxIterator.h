/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_SaxIterator.h

  Qore Programming Language

  Copyright (C) 2003 - 2017 Qore Technologies, s.r.o.

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

#ifndef _QORE_QC_SAXITERATOR_H

#define _QORE_QC_SAXITERATOR_H

#include "QC_XmlReader.h"
#include "qore/InputStream.h"

#include <string>

DLLEXPORT extern qore_classid_t CID_SAXITERATOR;
DLLLOCAL QoreClass* initSaxIteratorClass(QoreNamespace& ns);

DLLEXPORT extern qore_classid_t CID_FILESAXITERATOR;
DLLLOCAL QoreClass* initFileSaxIteratorClass(QoreNamespace& ns);

DLLEXPORT extern qore_classid_t CID_INPUTSTREAMSAXITERATOR;
DLLLOCAL QoreClass* initInputStreamSaxIteratorClass(QoreNamespace& ns);

DLLLOCAL extern QoreClass* QC_SAXITERATOR;

class QoreSaxIterator : public QoreXmlReaderData, public QoreAbstractIteratorBase {
protected:
   std::string element_name;
   int element_depth;
   bool val;

public:
   DLLLOCAL QoreSaxIterator(InputStream *is, const char* ename, const char* enc, const QoreHashNode* opts, ExceptionSink* xsink) : QoreXmlReaderData(is, enc, opts, xsink), element_name(ename), element_depth(-1), val(true) {
   }

   DLLLOCAL QoreSaxIterator(QoreStringNode* xml, const char* ename, const QoreHashNode* opts, ExceptionSink* xsink) : QoreXmlReaderData(xml, opts, xsink), element_name(ename), element_depth(-1), val(false) {
   }

   DLLLOCAL QoreSaxIterator(QoreXmlDocData* doc, const char* ename, ExceptionSink* xsink) : QoreXmlReaderData(doc, xsink), element_name(ename), element_depth(-1), val(false) {
   }

   DLLLOCAL QoreSaxIterator(ExceptionSink* xsink, const char* fn, const char* ename, const char* enc = nullptr, const QoreHashNode* opts = nullptr) : QoreXmlReaderData(fn, enc, opts, xsink), element_name(ename), element_depth(-1), val(false) {
   }

   DLLLOCAL QoreSaxIterator(const QoreSaxIterator& old, ExceptionSink* xsink) : QoreXmlReaderData(old, xsink), element_name(old.element_name), element_depth(-1), val(false) {
   }

   DLLLOCAL AbstractQoreNode* getReferencedValue(ExceptionSink* xsink) {
      SimpleRefHolder<QoreStringNode> holder(getOuterXml(xsink));
      if (!holder || *xsink)
         return nullptr;
      TempEncodingHelper str(*holder, QCS_UTF8, xsink);
      if (*xsink)
         return nullptr;
      str.makeTemp();

      QoreXmlReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
      if (!reader)
         return nullptr;

      ReferenceHolder<QoreHashNode> h(reader.parseXmlData(QCS_UTF8, XPF_NONE, xsink), xsink);
      if (*xsink)
         return nullptr;
      // issue #2487 element may be present with a prefix
      assert(h->size() == 1);
      AbstractQoreNode* n = h->getKeyValue(h->getFirstKey());
      return n ? n->refSelf() : nullptr;
   }

   DLLLOCAL bool next(ExceptionSink* xsink) {
      if (!val) {
         if (!isValid())
            reset(xsink);
      }

      while (true) {
         if (readSkipWhitespace(xsink) != 1) {
            val = false;
            break;
         }
         if (nodeType() == XML_READER_TYPE_ELEMENT) {
            if (element_depth >= 0 && element_depth != depth())
               continue;
            const char* n = localName();
            if (n && element_name == n) {
               if (element_depth == -1)
                  element_depth = depth();

               if (!val)
                  val = true;
               break;
            }
         }
      }

      return val;
   }

   DLLLOCAL bool valid() const {
      return val;
   }

   DLLLOCAL virtual const char* getName() const { return "SaxIterator"; }

   DLLLOCAL static const char* processOptionsGetEncoding(const QoreHashNode* opts, ExceptionSink* xsink) {
      const char* encoding = nullptr;
      if (opts) {
          ConstHashIterator i(opts);
          while (i.next()) {
              const char* key = i.getKey();
              if (!strcmp(key, "encoding")) {
                  const AbstractQoreNode* n = i.getValue();
                  if (get_node_type(n) != NT_STRING) {
                      xsink->raiseException("FILESAXITERATOR-OPTION-ERROR", "expecting type 'string' with option 'encoding'; got type '%s' instead", get_type_name(n));
                      return nullptr;
                  }
                  encoding = static_cast<const QoreStringNode*>(n)->c_str();
              }
          }
      }
      return encoding;
   }
};

#endif
