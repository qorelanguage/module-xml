/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 QC_XmlReader.h
 
 Qore Programming Language
 
 Copyright 2003 - 2010 David Nichols
 
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
DLLLOCAL QoreClass *initXmlReaderClass(QoreClass *XmlDoc);

class QoreXmlReaderData : public AbstractPrivateData, public QoreXmlReader {
private:
   QoreXmlDocData* doc;
   QoreStringNode* xmlstr;

   // not implemented
   DLLLOCAL QoreXmlReaderData(const QoreXmlReaderData &orig);

public:
   // n_xml must be in UTF8 encoding and must be referenced for the object
   DLLLOCAL QoreXmlReaderData(QoreStringNode* n_xml, ExceptionSink *xsink) : QoreXmlReader(xsink, n_xml, QORE_XML_PARSER_OPTIONS), doc(0), xmlstr(n_xml) {
   }

   DLLLOCAL QoreXmlReaderData(QoreXmlDocData *n_doc, ExceptionSink *xsink) : QoreXmlReader(xsink, n_doc->getDocPtr()), doc(n_doc), xmlstr(0) {
      doc->ref();
   }

   DLLLOCAL QoreXmlReaderData(const QoreXmlReaderData& old, ExceptionSink* xsink) : QoreXmlReader(xsink, old.xmlstr, QORE_XML_PARSER_OPTIONS, old.doc ? old.doc->getDocPtr() : 0), doc((QoreXmlDocData*)old.doc), xmlstr(old.xmlstr) {
      if (doc) {
         assert(!xmlstr);
         doc->ref();
      }
      else
         xmlstr->ref();
   }

   DLLLOCAL void reset(ExceptionSink* xsink) {
      QoreXmlReader::reset(xsink, xmlstr, QORE_XML_PARSER_OPTIONS, doc ? doc->getDocPtr() : 0);
   }

   DLLLOCAL QoreXmlReaderData *copy(ExceptionSink *xsink) {
      if (doc)
	 return new QoreXmlReaderData(doc, xsink);

      return new QoreXmlReaderData(xmlstr, xsink);
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
