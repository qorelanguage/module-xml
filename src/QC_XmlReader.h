/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 QC_XmlReader.h

 Qore Programming Language

 Copyright (C) 2003 - 2014 David Nichols

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

#ifndef _QORE_QC_XMLREADER_H

#define _QORE_QC_XMLREADER_H

#include "QoreXmlReader.h"

#include "QC_XmlDoc.h"

DLLEXPORT extern qore_classid_t CID_XMLREADER;
DLLLOCAL QoreClass *initXmlReaderClass(QoreNamespace &XmlDoc);

class QoreXmlReaderData : public AbstractPrivateData, public QoreXmlReader {
private:
   QoreXmlDocData* doc;
   QoreStringNode* xmlstr;
   std::string fn;
   std::string enc;

   // not implemented
   DLLLOCAL QoreXmlReaderData(const QoreXmlReaderData &orig);

public:
   DLLLOCAL QoreXmlReaderData(InputStream* is, const char* n_enc, ExceptionSink* xsink) : QoreXmlReader(xsink, is, n_enc, QORE_XML_PARSER_OPTIONS), doc(0), xmlstr(0), enc(n_enc ? n_enc : "") {
   }

   // n_xml must be in UTF8 encoding and must be referenced for the object
   DLLLOCAL QoreXmlReaderData(QoreStringNode* n_xml, ExceptionSink* xsink) : QoreXmlReader(xsink, n_xml, QORE_XML_PARSER_OPTIONS), doc(0), xmlstr(n_xml) {
   }

   DLLLOCAL QoreXmlReaderData(QoreXmlDocData *n_doc, ExceptionSink* xsink) : QoreXmlReader(xsink, n_doc->getDocPtr()), doc(n_doc), xmlstr(0) {
      doc->ref();
   }

   DLLLOCAL QoreXmlReaderData(const char* n_fn, const char* n_enc, ExceptionSink* xsink) : QoreXmlReader(xsink, n_fn, n_enc, QORE_XML_PARSER_OPTIONS), doc(0), xmlstr(0), fn(n_fn), enc(n_enc ? n_enc : "") {
   }

   DLLLOCAL QoreXmlReaderData(const QoreXmlReaderData& old, ExceptionSink* xsink) : QoreXmlReader(xsink, old.xmlstr, QORE_XML_PARSER_OPTIONS, old.doc ? old.doc->getDocPtr() : 0, old.fn.empty() ? 0 : old.fn.c_str(), old.enc.empty() ? 0 : old.enc.c_str()), doc((QoreXmlDocData*)old.doc), xmlstr(old.xmlstr), fn(old.fn), enc(old.enc) {
      if (doc) {
         assert(!xmlstr);
         doc->ref();
      }
      else if (xmlstr) {
         assert(!doc);
         xmlstr->ref();
      }
   }

   DLLLOCAL void reset(ExceptionSink* xsink) {
      if (!fn.empty())
         QoreXmlReader::reset(xsink, fn.c_str(), enc.empty() ? 0 : enc.c_str(), QORE_XML_PARSER_OPTIONS);
      else if (xmlstr)
         QoreXmlReader::reset(xsink, xmlstr, QORE_XML_PARSER_OPTIONS, doc ? doc->getDocPtr() : 0);
      else
         xsink->raiseException("XMLREADER-RESET-ERROR", "Unsupported operation");
   }

   DLLLOCAL QoreXmlReaderData* copy(ExceptionSink* xsink) {
      return new QoreXmlReaderData(*this, xsink);
   }

   DLLLOCAL ~QoreXmlReaderData() {
      if (doc) {
	 assert(!xmlstr);
	 doc->deref();
      }
      else if (xmlstr)
	 xmlstr->deref();
   }
};

#endif
