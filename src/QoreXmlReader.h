/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 QoreXmlReader.h

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

#ifndef _QORE_QOREXMLREADER_H
#define _QORE_QOREXMLREADER_H

#include <libxml/xmlreader.h>

#include "qore/InputStream.h"

#include "qore-xml-module.h"
#include "QoreXmlDoc.h"
#include "QC_AbstractXmlIoInputCallback.h"

#include <errno.h>

// FIXME: need to make error reporting consistent and set ExceptionSink for each call, not in constructor and then fix ql_xml.cc and adjust QC_XmlReader.cc

class XmlIoInputCallbackHelper {
public:
    DLLLOCAL XmlIoInputCallbackHelper(const QoreHashNode* opts, ExceptionSink* xs) : xsink(xs) {
        assert(!xml_io_callback);

        bool found = false;
        QoreValue v = opts->getValueKeyValueExistence("xml_input_io", found);
        if (found) {
            if (v.getType() != NT_OBJECT) {
                xsink->raiseException("XMLREADER-XSD-ERROR", "expecting type 'object' with option 'xml_input_io'; got type '%s' instead", v.getTypeName());
                return;
            }
            const QoreObject* obj = v.get<const QoreObject>();
            xml_io_callback = static_cast<AbstractXmlIoInputCallback*>(obj->getReferencedPrivateData(CID_ABSTRACTXMLIOINPUTCALLBACK, xsink));
            if (*xsink) {
                assert(!xml_io_callback);
                return;
            }
            if (!xml_io_callback) {
                assert(!*xsink);
                xsink->raiseException("XMLREADER-XSD-ERROR", "expecting an object of class 'AbstractXmlIoInputCallback' with option 'xml_input_io'; got class '%s' instead", obj->getClassName());
                return;
            }
            xml_io_callback->setExceptionContext(xsink);
        }
    }

    DLLLOCAL ~XmlIoInputCallbackHelper() {
        if (xml_io_callback) {
            xml_io_callback->clearExceptionContext();
            xml_io_callback->deref(xsink);
            xml_io_callback = nullptr;
        }
    }

private:
    ExceptionSink* xsink;
};

class QoreXmlReader {
protected:
   xmlTextReader* reader = nullptr;
   const QoreString* xml = nullptr;
   ExceptionSink* xs = nullptr;
   int fd = -1;
   ReferenceHolder<InputStream> inputStream;
   AbstractXmlValidator* val = nullptr;

   static void qore_xml_error_func(QoreXmlReader* xr, const char* msg, xmlParserSeverities severity, xmlTextReaderLocatorPtr locator) {
      if (severity == XML_PARSER_SEVERITY_VALIDITY_WARNING
          || severity == XML_PARSER_SEVERITY_WARNING) {
         printd(1, "XML parser warning: %s\n", msg);
         return;
      }
      if (!xr->xs)
         return;
      if (*(xr->xs))
         return;
      QoreStringNode* desc = new QoreStringNode(msg);
      desc->chomp();
      xr->xs->raiseException("PARSE-XML-EXCEPTION", desc);
   }

   static int streamReadCallback(void* context, char* buffer, int len) {
      QoreXmlReader *xmlReader = static_cast<QoreXmlReader*>(context);
      int64 i = xmlReader->inputStream->read(buffer, len, xmlReader->xs);
      if (*xmlReader->xs) {
         return -1;
      }
      return i;
   }

   static int streamCloseCallback(void* context) {
      return 0;
   }

   DLLLOCAL AbstractQoreNode* getXmlData(ExceptionSink* xsink, const QoreEncoding* data_ccsid, int pflags = XPF_NONE, int min_depth = -1);

   DLLLOCAL void init(const char* enc, int options, const QoreHashNode* opts, ExceptionSink* xsink) {
      assert(!xml);
      assert(!reader);
      xs = xsink;
      reader = xmlReaderForIO(streamReadCallback, streamCloseCallback, this, 0, enc, options);
      if (!reader) {
         xsink->raiseException("XML-READER-ERROR", "could not create XML reader");
         return;
      }

      xmlTextReaderSetErrorHandler(reader, (xmlTextReaderErrorFunc)qore_xml_error_func, this);

      if (opts)
         processOpts(opts, xsink);
      //printd(5, "QoreXmlReader::init() valid: %d\n", isValid());
   }

   DLLLOCAL void init(const QoreString* n_xml, int options, const QoreHashNode* opts, ExceptionSink* xsink) {
      assert(!xml);
      assert(!reader);
      xml = n_xml;

      assert(xml->getEncoding() == QCS_UTF8);
      reader = xmlReaderForDoc((xmlChar*)xml->getBuffer(), 0, 0, options);
      if (!reader) {
         xsink->raiseException("XML-READER-ERROR", "could not create XML reader");
         return;
      }

      xmlTextReaderSetErrorHandler(reader, (xmlTextReaderErrorFunc)qore_xml_error_func, this);
      //printd(5, "QoreXmlReader::init() xml size: %d opts: %p reader: %p set error handler; options: %d\n", (int)xml->size(), opts, reader, options);

      if (opts)
         processOpts(opts, xsink);
   }

   DLLLOCAL void processOpts(const QoreHashNode* opts, ExceptionSink* xsink);

   DLLLOCAL void init(xmlDocPtr doc, ExceptionSink* xsink) {
      assert(!xml);
      assert(!reader);
      reader = xmlReaderWalker(doc);
      if (!reader) {
         xsink->raiseException("XML-READER-ERROR", "could not create XML reader");
         return;
      }
      // the following call causes a crash - I guess the document has already been parsed anyway
      //xmlTextReaderSetErrorHandler(reader, (xmlTextReaderErrorFunc)qore_xml_error_func, xsink);
   }

   DLLLOCAL void init(ExceptionSink* xsink, const char* fn, const char* encoding, int options, const QoreHashNode* opts = nullptr) {
      assert(!xml);
      assert(!reader);
      assert(fd == -1);
      fd = open(fn, O_RDONLY);
      if (fd < 0) {
         reader = 0;
         xsink->raiseErrnoException("XML-READER-ERROR", errno, "could not open '%s' for reading", fn);
         return;
      }

      reader = xmlReaderForFd(fd, 0, encoding, options);
      if (!reader) {
         close(fd);
         fd = -1;
         xsink->raiseException("XML-READER-ERROR", "could not create XML reader");
         return;
      }

      xmlTextReaderSetErrorHandler(reader, (xmlTextReaderErrorFunc)qore_xml_error_func, this);
      //printd(5, "QoreXmlReader::init() opts: %p reader: %p set error handler\n", opts, reader);

      if (opts)
         processOpts(opts, xsink);
   }

   DLLLOCAL int do_int_rv(int rc, ExceptionSink* xsink) {
      if (rc == -1 && !*xsink)
         xsink->raiseExceptionArg("PARSE-XML-EXCEPTION", xml ? new QoreStringNode(*xml) : 0, "error parsing XML string");
      return rc;
   }

   DLLLOCAL QoreXmlReader(ExceptionSink* xsink, InputStream *is, const char* enc, int options, const QoreHashNode* opts) : inputStream(is, xsink) {
      init(enc, options, opts, xsink);
   }

   DLLLOCAL QoreXmlReader(ExceptionSink* xsink, const QoreString* n_xml, int options, const QoreHashNode* opts = nullptr) : inputStream(xsink) {
      init(n_xml, options, opts, xsink);
   }

   DLLLOCAL QoreXmlReader(ExceptionSink* xsink, xmlDocPtr doc) : inputStream(xsink) {
      init(doc, xsink);
   }

   DLLLOCAL QoreXmlReader(ExceptionSink* xsink, const QoreString* n_xml, int options, xmlDocPtr doc, const char* fn, const char* enc) : inputStream(xsink) {
      if (fn)
         init(xsink, fn, enc, options);
      else
         init(xsink, n_xml, options, doc);
   }

   DLLLOCAL QoreXmlReader(ExceptionSink* xsink, const char* fn, const char* encoding, int options, const QoreHashNode* opts) : inputStream(xsink) {
      init(xsink, fn, encoding, options, opts);
   }

   DLLLOCAL void reset(ExceptionSink* xsink, const QoreString* n_xml, int options, xmlDocPtr doc) {
      reset();
      init(xsink, n_xml, options, doc);
   }

   DLLLOCAL void reset(ExceptionSink* xsink, const char* fn, const char* enc, int options) {
      reset();
      init(xsink, fn, enc, options);
   }

   DLLLOCAL void init(ExceptionSink* xsink, const QoreString* n_xml, int options, xmlDocPtr doc) {
      assert(!xs);

      if (n_xml) {
         assert(!doc);
         init(n_xml, options, nullptr, xsink);
      }
      else {
         assert(!n_xml);
         init(doc, xsink);
      }
   }

    DLLLOCAL void reset() {
        //printd(5, "QoreXmlReader::reset() reader: %p val: %p fd: %d\n", reader, val, fd);
        if (val) {
            delete val;
            val = nullptr;
        }
        if (reader) {
            xmlFreeTextReader(reader);
            reader = nullptr;
        }
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
        if (xml)
            xml = nullptr;
    }

public:
   DLLLOCAL QoreXmlReader(const QoreString* n_xml, int options, ExceptionSink* xsink) : xs(xsink), inputStream(xsink) {
      init(n_xml, options, nullptr, xsink);
   }

   DLLLOCAL QoreXmlReader(xmlDocPtr doc, ExceptionSink* xsink) : xs(xsink), inputStream(xsink) {
      init(doc, xsink);
   }

   DLLLOCAL ~QoreXmlReader() {
      reset();
   }

   DLLLOCAL operator bool() const {
      return reader != 0;
   }

   DLLLOCAL void setExceptionContext(ExceptionSink* xsink) {
      if (xs != xsink)
         xs = xsink;
      if (val)
         val->setExceptionContext(xsink);
   }

   // returns 1 = OK, 0 = no more nodes to read, -1 = error
   DLLLOCAL int read(ExceptionSink* xsink) {
      setExceptionContext(xsink);

      int rc = read();
      if (rc == -1) {
         if (!*xsink)
            xsink->raiseExceptionArg("PARSE-XML-EXCEPTION", xml ? new QoreStringNode(*xml) : 0, "cannot parse XML string");
      }
      return rc;
   }

   // returns 1 = OK, 0 = no more nodes to read, -1 = error
   DLLLOCAL int read(const char* info, ExceptionSink* xsink) {
      setExceptionContext(xsink);

      int rc = read();
      if (rc == -1) {
         if (!*xsink)
            xsink->raiseExceptionArg("PARSE-XML-EXCEPTION", xml ? new QoreStringNode(*xml) : 0, "cannot parse XML string: %s", info);
      }
      return rc;
   }

   // returns 1 = OK, 0 = no more nodes to read, -1 = error
   DLLLOCAL int read() {
      return xmlTextReaderRead(reader);
   }

   // returns 1 = OK, 0 = no more nodes to read, -1 = error
   DLLLOCAL int readSkipWhitespace() {
      int rc;
      while (true) {
         rc = read();
         if (rc != 1)
            break;
         int nt = xmlTextReaderNodeType(reader);
         if (nt != XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
            break;
      }
      return rc;
   }

   // returns 1 = OK, 0 = no more nodes to read, -1 = error
   DLLLOCAL int readSkipWhitespace(ExceptionSink* xsink) {
      int rc;
      while (true) {
         rc = read(xsink);
         if (rc != 1)
            break;
         int nt = xmlTextReaderNodeType(reader);
         if (nt != XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
            break;
      }
      return rc;
   }

   // returns 1 = OK, 0 = no more nodes to read, -1 = error
   DLLLOCAL int readSkipWhitespace(const char* info, ExceptionSink* xsink) {
      int rc;
      while (true) {
         rc = read(info, xsink);
         if (rc != 1)
            break;
         int nt = xmlTextReaderNodeType(reader);
         if (nt != XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
            break;
      }
      return rc;
   }

   DLLLOCAL int nodeType() {
      return xmlTextReaderNodeType(reader);
   }

   // gets the node type but skips whitespace
   DLLLOCAL int nodeTypeSkipWhitespace() {
      int nt;
      while (true) {
         nt = xmlTextReaderNodeType(reader);
         if (nt != XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
            break;

         // get next element
         if (read() != 1)
            return -1;
      }
      return nt;
   }

   DLLLOCAL int depth() {
      return xmlTextReaderDepth(reader);
   }

   DLLLOCAL const char* constName() {
      return (const char*)xmlTextReaderConstName(reader);
   }

   DLLLOCAL const char* constValue() {
      return (const char*)xmlTextReaderConstValue(reader);
   }

   DLLLOCAL bool hasAttributes() {
      return xmlTextReaderHasAttributes(reader) == 1;
   }

   DLLLOCAL bool hasValue() {
      return xmlTextReaderHasValue(reader) == 1;
   }

   DLLLOCAL bool isDefault() {
      return xmlTextReaderIsDefault(reader) == 1;
   }

   DLLLOCAL bool isEmptyElement() {
      return xmlTextReaderIsEmptyElement(reader) == 1;
   }

   DLLLOCAL bool isNamespaceDecl() {
#ifdef HAVE_XMLTEXTREADERISNAMESPACEDECL
      return xmlTextReaderIsNamespaceDecl(reader) == 1;
#else
      xmlNodePtr node = xmlTextReaderCurrentNode(reader);
      if (!node)
         return false;

      return node->type == XML_NAMESPACE_DECL ? true : false;
#endif
   }

   DLLLOCAL bool isValid() const {
      return xmlTextReaderIsValid(reader) == 1;
   }

   DLLLOCAL bool isError() {
      return xmlTextReaderIsValid(reader) < 0;
   }

   DLLLOCAL int moveToNextAttribute(ExceptionSink* xsink) {
      return do_int_rv(xmlTextReaderMoveToNextAttribute(reader), xsink);
   }

   DLLLOCAL QoreStringNode* getValue(const QoreEncoding* id, ExceptionSink* xsink) {
      if (id == QCS_UTF8)
         return new QoreStringNode(constValue(), QCS_UTF8);

      return QoreStringNode::createAndConvertEncoding(constValue(), QCS_UTF8, id, xsink);
   }

#ifdef HAVE_XMLTEXTREADERSETSCHEMA
   DLLLOCAL int setSchema(xmlSchemaPtr schema) {
      //printd(5, "QoreXmlReader::setSchema() reader: %p schema: %p\n", reader, schema);
      return xmlTextReaderSetSchema(reader, schema);
   }
#endif

#ifdef HAVE_XMLTEXTREADERRELAXNGSETSCHEMA
   DLLLOCAL int setRelaxNG(xmlRelaxNGPtr schema) {
      return xmlTextReaderRelaxNGSetSchema(reader, schema);
   }
#endif

   DLLLOCAL int attributeCount() {
      return xmlTextReaderAttributeCount(reader);
   }

   DLLLOCAL const char* baseUri() {
      return (const char*)xmlTextReaderConstBaseUri(reader);
   }

#ifdef HAVE_XMLTEXTREADERBYTECONSUMED
   DLLLOCAL int64 bytesConsumed() {
      return xmlTextReaderByteConsumed(reader);
   }
#endif

   DLLLOCAL const char* encoding() {
      return (const char*)xmlTextReaderConstEncoding(reader);
   }

   DLLLOCAL const char* localName() {
      return (const char*)xmlTextReaderConstLocalName(reader);
   }

   DLLLOCAL const char* namespaceUri() {
      return (const char*)xmlTextReaderConstNamespaceUri(reader);
   }

   DLLLOCAL const char* prefix() {
      return (const char*)xmlTextReaderConstPrefix(reader);
   }

   DLLLOCAL const char* xmlLang() {
      return (const char*)xmlTextReaderConstXmlLang(reader);
   }

   DLLLOCAL const char* xmlVersion() {
      return (const char*)xmlTextReaderConstXmlVersion(reader);
   }

   DLLLOCAL QoreStringNode* getAttribute(const char* attr) {
      return doString(xmlTextReaderGetAttribute(reader, (xmlChar*)attr));
   }

   DLLLOCAL QoreStringNode* getAttributeOffset(int offset) {
      return doString(xmlTextReaderGetAttributeNo(reader, offset));
   }

   DLLLOCAL QoreStringNode* getAttributeNs(const char* lname, const char* ns) {
      return doString(xmlTextReaderGetAttributeNs(reader, (const xmlChar*)lname, (const xmlChar*)ns));
   }

#ifdef HAVE_XMLTEXTREADERGETPARSERCOLUMNNUMBER
   DLLLOCAL int getParserColumnNumber() {
      return xmlTextReaderGetParserColumnNumber(reader);
   }
#endif

#ifdef HAVE_XMLTEXTREADERGETPARSERLINENUMBER
   DLLLOCAL int getParserLineNumber() {
      return xmlTextReaderGetParserLineNumber(reader);
   }
#endif

   DLLLOCAL QoreStringNode* lookupNamespace(const char* prefix) {
      return doString(xmlTextReaderLookupNamespace(reader, (xmlChar*)prefix));
   }

   DLLLOCAL int moveToAttribute(const char* attr, ExceptionSink* xsink) {
      return do_int_rv(xmlTextReaderMoveToAttribute(reader, (xmlChar*)attr), xsink);
   }

   DLLLOCAL int moveToAttributeOffset(int offset, ExceptionSink* xsink) {
      return do_int_rv(xmlTextReaderMoveToAttributeNo(reader, offset), xsink);
   }

   DLLLOCAL int moveToAttributeNs(const char* lname, const char* ns, ExceptionSink* xsink) {
      return do_int_rv(xmlTextReaderMoveToAttributeNs(reader, (const xmlChar*)lname, (const xmlChar*)ns), xsink);
   }

   DLLLOCAL int moveToElement(ExceptionSink* xsink) {
      return do_int_rv(xmlTextReaderMoveToElement(reader), xsink);
   }

   DLLLOCAL int moveToFirstAttribute(ExceptionSink* xsink) {
      return do_int_rv(xmlTextReaderMoveToFirstAttribute(reader), xsink);
   }

   DLLLOCAL int next(ExceptionSink* xsink) {
      setExceptionContext(xsink);
      int rc = xmlTextReaderNext(reader);
      if (rc == -1 && !*xsink)
         xsink->raiseException("PARSE-XML-EXCEPTION", "error parsing XML string");
      return rc;
   }

/*
   // only implemented for readers build on a document
   DLLLOCAL int nextSibling() {
      return xmlTextReaderNextSibling(reader);
   }
*/

   DLLLOCAL QoreStringNode* getInnerXml(ExceptionSink* xsink) {
      setExceptionContext(xsink);
      return doString(xmlTextReaderReadInnerXml(reader));
   }

   DLLLOCAL QoreStringNode* getOuterXml(ExceptionSink* xsink) {
      setExceptionContext(xsink);
      return doString(xmlTextReaderReadOuterXml(reader));
   }

#ifdef HAVE_XMLTEXTREADERRELAXNGSETSCHEMA
   DLLLOCAL void relaxNGValidate(const char* rng, ExceptionSink* xsink) {
      if (xmlTextReaderRelaxNGValidate(reader, rng))
         xsink->raiseException("XMLREADER-RELAXNG-ERROR", "an error occurred setting the RelaxNG schema for validation; this function must be called before the first call to XmlReader::read()");
   }
#endif

#ifdef HAVE_XMLTEXTREADERSETSCHEMA
   DLLLOCAL void schemaValidate(const char* xsd, ExceptionSink* xsink) {
      if (xmlTextReaderSchemaValidate(reader, xsd))
         xsink->raiseException("XMLREADER-XSD-ERROR", "an error occurred setting the W3C XSD schema for validation; this function must be called before the first call to XmlReader::read()");
   }
#endif

   DLLLOCAL QoreHashNode* parseXmlData(const QoreEncoding* data_ccsid, int pflags, ExceptionSink* xsink);
};

#endif
