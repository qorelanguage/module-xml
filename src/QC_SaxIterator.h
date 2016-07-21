/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_SaxIterator.h

  Qore Programming Language

  Copyright (C) 2003 - 2016 David Nichols

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
DLLLOCAL QoreClass *initSaxIteratorClass(QoreNamespace& ns);

DLLEXPORT extern qore_classid_t CID_FILESAXITERATOR;
DLLLOCAL QoreClass *initFileSaxIteratorClass(QoreNamespace& ns);

DLLEXPORT extern qore_classid_t CID_INPUTSTREAMSAXITERATOR;
DLLLOCAL QoreClass *initInputStreamSaxIteratorClass(QoreNamespace& ns);

DLLLOCAL extern QoreClass* QC_SAXITERATOR;

class QoreSaxIterator : public QoreXmlReaderData, public QoreAbstractIteratorBase {
protected:
   std::string element_name;
   int element_depth;
   bool val;

public:
   DLLLOCAL QoreSaxIterator(InputStream *is, const char* ename, const char* enc, ExceptionSink* xsink) : QoreXmlReaderData(is, enc, xsink), element_name(ename), element_depth(-1), val(true) {
   }

   DLLLOCAL QoreSaxIterator(QoreStringNode* xml, const char* ename, ExceptionSink* xsink) : QoreXmlReaderData(xml, xsink), element_name(ename), element_depth(-1), val(false) {
   }

   DLLLOCAL QoreSaxIterator(QoreXmlDocData* doc, const char* ename, ExceptionSink* xsink) : QoreXmlReaderData(doc, xsink), element_name(ename), element_depth(-1), val(false) {
   }

   DLLLOCAL QoreSaxIterator(ExceptionSink* xsink, const char* fn, const char* ename, const char* enc = 0) : QoreXmlReaderData(fn, enc, xsink), element_name(ename), element_depth(-1), val(false) {
   }

   DLLLOCAL QoreSaxIterator(const QoreSaxIterator& old, ExceptionSink* xsink) : QoreXmlReaderData(old, xsink), element_name(old.element_name), element_depth(-1), val(false) {
   }

   DLLLOCAL AbstractQoreNode* getReferencedValue(ExceptionSink* xsink) {
      SimpleRefHolder<QoreStringNode> holder(getOuterXml());
      if (!holder)
         return 0;
      TempEncodingHelper str(*holder, QCS_UTF8, xsink);
      if (*xsink)
         return 0;
      str.makeTemp();

      QoreXmlReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
      if (!reader)
         return 0;

      ReferenceHolder<QoreHashNode> h(reader.parseXmlData(QCS_UTF8, XPF_NONE, xsink), xsink);
      AbstractQoreNode* n = h->getKeyValue(element_name.c_str());
      return n ? n->refSelf() : 0;
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
};

#endif
