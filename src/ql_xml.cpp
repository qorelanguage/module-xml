/*
  lib/ql_xml.cpp

  Qore XML functions

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

//! @file xml.q defines the functions exported by the module

#include "qore-xml-module.h"

#include "QoreXmlReader.h"
#include "ql_xml.h"

#include <libxml/xmlwriter.h>

#include <string.h>
#include <memory>

// list of libxml2 element type names
static const char *xml_element_type_names[] = {
   "XML_ELEMENT_NODE",
   "XML_ATTRIBUTE_NODE",
   "XML_TEXT_NODE",
   "XML_CDATA_SECTION_NODE",
   "XML_ENTITY_REF_NODE",
   "XML_ENTITY_NODE",
   "XML_PI_NODE",
   "XML_COMMENT_NODE",
   "XML_DOCUMENT_NODE",
   "XML_DOCUMENT_TYPE_NODE",
   "XML_DOCUMENT_FRAG_NODE",
   "XML_NOTATION_NODE",
   "XML_HTML_DOCUMENT_NODE",
   "XML_DTD_NODE",
   "XML_ELEMENT_DECL",
   "XML_ATTRIBUTE_DECL",
   "XML_ENTITY_DECL",
   "XML_NAMESPACE_DECL",
   "XML_XINCLUDE_START",
   "XML_XINCLUDE_END",
   "XML_DOCB_DOCUMENT_NODE"
};

#define XETN_SIZE (sizeof(xml_element_type_names) / sizeof(char *))

static const char *xml_node_type_names[] = {
    "XML_NODE_TYPE_NONE",
    "XML_NODE_TYPE_ELEMENT",
    "XML_NODE_TYPE_ATTRIBUTE",
    "XML_NODE_TYPE_TEXT",
    "XML_NODE_TYPE_CDATA",
    "XML_NODE_TYPE_ENTITY_REFERENCE",
    "XML_NODE_TYPE_ENTITY",
    "XML_NODE_TYPE_PROCESSING_INSTRUCTION",
    "XML_NODE_TYPE_COMMENT",
    "XML_NODE_TYPE_DOCUMENT",
    "XML_NODE_TYPE_DOCUMENT_TYPE",
    "XML_NODE_TYPE_DOCUMENT_FRAGMENT",
    "XML_NODE_TYPE_NOTATION",
    "XML_NODE_TYPE_WHITESPACE",
    "XML_NODE_TYPE_SIGNIFICANT_WHITESPACE",
    "XML_NODE_TYPE_END_ELEMENT",
    "XML_NODE_TYPE_END_ENTITY",
    "XML_NODE_TYPE_XML_DECLARATION",};

#define XNTN_SIZE (sizeof(xml_node_type_names) / sizeof(char *))

const char *get_xml_element_type_name(int t) {
   return (t > 0 && t <= (int)XETN_SIZE) ? xml_element_type_names[t - 1] : 0;
}

const char *get_xml_node_type_name(int t) {
   return (t > 0 && t <= (int)XNTN_SIZE) ? xml_node_type_names[t - 1] : 0;
}

namespace { // make classes local

class XmlRpcValue {
   private:
      AbstractQoreNode *val;
      AbstractQoreNode **vp;

   public:
      DLLLOCAL inline XmlRpcValue() : val(0), vp(0) {
      }

      DLLLOCAL inline ~XmlRpcValue() {
	 if (val) {
	    val->deref(0);
	    val = 0;
	 }
      }

      DLLLOCAL inline AbstractQoreNode *getValue() {
	 AbstractQoreNode *rv = val;
	 val = 0;
	 return rv;
      }

      DLLLOCAL inline void set(AbstractQoreNode *v) {
	 if (vp)
	    *vp = v;
	 else
	    val = v;
      }

      DLLLOCAL inline void setPtr(AbstractQoreNode **v) {
	 vp = v;
      }
};

class xml_node {
   public:
      AbstractQoreNode **node;
      xml_node *next;
      int depth;
      int vcount;
      int cdcount;

      DLLLOCAL xml_node(AbstractQoreNode **n, int d) 
	 : node(n), next(0), depth(d), vcount(0), cdcount(0) {
      }
};

class xml_stack {
   private:
      xml_node *tail;
      AbstractQoreNode *val;
      
   public:
      DLLLOCAL inline xml_stack() {
	 tail = 0;
	 val = 0;
	 push(&val, -1);
      }
      
      DLLLOCAL inline ~xml_stack() {
	 if (val)
	    val->deref(0);

	 while (tail)
	 {
	    //printd(5, "xml_stack::~xml_stack(): deleting=%p (%d), next=%p\n", tail, tail->depth, tail->next);
	    xml_node *n = tail->next;
	    delete tail;
	    tail = n;
	 }
      }
      
      DLLLOCAL inline void checkDepth(int depth) {
	 while (tail && depth && tail->depth >= depth) {
	    //printd(5, "xml_stack::checkDepth(%d): deleting=%p (%d), new tail=%p\n", depth, tail, tail->depth, tail->next);
	    xml_node *n = tail->next;
	    delete tail;
	    tail = n;
	 }
      }

      DLLLOCAL inline void push(AbstractQoreNode **node, int depth) {
	 xml_node *sn = new xml_node(node, depth);
	 sn->next = tail;
	 tail = sn;
      }
      DLLLOCAL inline AbstractQoreNode *getNode() {
	 return *tail->node;
      }
      DLLLOCAL inline void setNode(AbstractQoreNode *n) {
	 (*tail->node) = n;
      }
      DLLLOCAL inline AbstractQoreNode *getVal() {
	 AbstractQoreNode *rv = val;
	 val = 0;
	 return rv;
      }
      DLLLOCAL inline int getValueCount() const {
	 return tail->vcount;
      }
      DLLLOCAL inline void incValueCount() {
	 tail->vcount++;
      }
      DLLLOCAL inline int getCDataCount() const {
	 return tail->cdcount;
      }
      DLLLOCAL inline void incCDataCount() {
	 tail->cdcount++;
      }
};

} // anonymous namespace

#if 0
// does not work well, produces ugly, uninformative output
static void qore_xml_structured_error_func(ExceptionSink *xsink, xmlErrorPtr error) {
   QoreStringNode *desc = new QoreStringNode;

   if (error->line)
      desc->sprintf("line %d: ", error->line);

   if (error->int2)
      desc->sprintf("column %d: ", error->int2);

   desc->concat(error->message);
   desc->chomp();
   
   if (error->str1)
      desc->sprintf(", %s", error->str1);

   if (error->str2)
      desc->sprintf(", %s", error->str1);

   if (error->str3)
      desc->sprintf(", %s", error->str1);

   xsink->raiseException("PARSE-XML-EXCEPTION", desc);
}
#endif

#ifdef HAVE_XMLTEXTREADERRELAXNGSETSCHEMA
static void qore_xml_relaxng_error_func(ExceptionSink *xsink, const char *msg, ...) {
   if (*xsink)
      return;

   va_list args;
   QoreStringNode *desc = new QoreStringNode;

   while (true) {
      va_start(args, msg);
      int rc = desc->vsprintf(msg, args);
      va_end(args);
      if (!rc)
	 break;
   }
   desc->chomp();

   xsink->raiseException("XML-RELAXNG-PARSE-ERROR", desc);
}
#endif

#ifdef HAVE_XMLTEXTREADERSETSCHEMA
static void qore_xml_schema_error_func(ExceptionSink *xsink, const char *msg, ...) {
   if (*xsink)
      return;

   va_list args;
   QoreStringNode *desc = new QoreStringNode;

   while (true) {
      va_start(args, msg);
      int rc = desc->vsprintf(msg, args);
      va_end(args);
      if (!rc)
	 break;
   }
   desc->chomp();

   xsink->raiseException("XML-SCHEMA-PARSE-ERROR", desc);
}
#endif

#if defined(HAVE_XMLTEXTREADERRELAXNGSETSCHEMA) || defined(HAVE_XMLTEXTREADERSETSCHEMA)
static void qore_xml_schema_warning_func(ExceptionSink *xsink, const char *msg, ...) {
#ifdef DEBUG
   va_list args;
   QoreString buf;

   while (true) {
      va_start(args, msg);
      int rc = buf.vsprintf(msg, args);
      va_end(args);
      if (!rc)
	 break;
   }

   printf("%s", buf.getBuffer());
#endif
}
#endif

class QoreXmlRpcReader : public QoreXmlReader {
public:
   DLLLOCAL QoreXmlRpcReader(const QoreString *n_xml, int options, ExceptionSink *xsink) : QoreXmlReader(n_xml, options, xsink) {
   }

   DLLLOCAL int readXmlRpc(ExceptionSink *xsink) {
      return readSkipWhitespace(xsink) != 1;
   }

   DLLLOCAL int readXmlRpc(const char *info, ExceptionSink *xsink) {
      return readSkipWhitespace(info, xsink) != 1;
   }

   DLLLOCAL int readXmlRpcNode(ExceptionSink *xsink) {
      int nt = nodeTypeSkipWhitespace();
      if (nt == -1 && !*xsink)
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string");
      return nt;
   }

   DLLLOCAL int checkXmlRpcMemberName(const char *member, ExceptionSink *xsink) {
      const char *name = (const char *)xmlTextReaderConstName(reader);
      if (!name) {
	 xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", xml ? new QoreStringNode(*xml) : 0, "expecting element '%s', got NOTHING", member);
	 return -1;
      }
	 
      if (strcmp(name, member)) {
	 xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", xml ? new QoreStringNode(*xml) : 0, "expecting element '%s', got '%s'", member, name);
	 return -1;
      }
      return 0;
   }

   DLLLOCAL int getArray(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink);
   DLLLOCAL int getStruct(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink);
   DLLLOCAL int getString(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink);
   DLLLOCAL int getBoolean(XmlRpcValue *v, ExceptionSink *xsink);
   DLLLOCAL int getInt(XmlRpcValue *v, ExceptionSink *xsink);
   DLLLOCAL int getDouble(XmlRpcValue *v, ExceptionSink *xsink);
   DLLLOCAL int getDate(XmlRpcValue *v, ExceptionSink *xsink);
   DLLLOCAL int getBase64(XmlRpcValue *v, ExceptionSink *xsink);
   DLLLOCAL int getValueData(XmlRpcValue *v, const QoreEncoding *data_ccsid, bool read_next, ExceptionSink *xsink);
   DLLLOCAL int getParams(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink);
};

#ifdef HAVE_XMLTEXTREADERSETSCHEMA
QoreXmlSchemaContext::QoreXmlSchemaContext(const char *xsd, int size, ExceptionSink *xsink) : schema(0) {
   xmlSchemaParserCtxtPtr scp = xmlSchemaNewMemParserCtxt(xsd, size);
   if (!scp)
      return;
   
   //xmlSchemaSetParserStructuredErrors(scp, (xmlStructuredErrorFunc)qore_xml_structured_error_func, xsink);
   
   xmlSchemaSetParserErrors(scp, (xmlSchemaValidityErrorFunc)qore_xml_schema_error_func, 
			    (xmlSchemaValidityErrorFunc)qore_xml_schema_warning_func , xsink);
   schema = xmlSchemaParse(scp);
   xmlSchemaFreeParserCtxt(scp);
}
#endif

#ifdef HAVE_XMLTEXTREADERRELAXNGSETSCHEMA
QoreXmlRelaxNGContext::QoreXmlRelaxNGContext(const char *rng, int size, ExceptionSink *xsink) : schema(0) {
   xmlRelaxNGParserCtxtPtr rcp = xmlRelaxNGNewMemParserCtxt(rng, size);
   if (!rcp)
      return;
   
   xmlRelaxNGSetParserErrors(rcp, (xmlRelaxNGValidityErrorFunc)qore_xml_relaxng_error_func, 
			     (xmlRelaxNGValidityErrorFunc)qore_xml_schema_warning_func, xsink);
   
   schema = xmlRelaxNGParse(rcp);
   xmlRelaxNGFreeParserCtxt(rcp);
}
#endif

static int concatSimpleValue(QoreString &str, const AbstractQoreNode *n, ExceptionSink *xsink) {
   //printd(5, "concatSimpleValue() n=%p (%s) %s\n", n, n->getTypeName(), n->getType() == NT_STRING ? ((QoreStringNode *)n)->getBuffer() : "unknown");
   switch (n ? n->getType() : 0) {
      case NT_INT: {
	 const QoreBigIntNode *b = reinterpret_cast<const QoreBigIntNode *>(n);
	 str.sprintf("%lld", b->val);
	 return 0;
      }

      case NT_FLOAT: {
	 str.sprintf("%.9g", reinterpret_cast<const QoreFloatNode *>(n)->f);
	 return 0;
      }

      case NT_BOOLEAN: {
	 str.sprintf("%d", reinterpret_cast<const QoreBoolNode *>(n)->getValue());
	 return 0;
      }

      case NT_DATE: {
	 const DateTimeNode *date = reinterpret_cast<const DateTimeNode *>(n);
	 str.concat(date);
	 return 0;
      }
   }

   QoreStringValueHelper temp(n);
   str.concatAndHTMLEncode(*temp, xsink);
   return *xsink ? -1 : 0;   
}

static int concatSimpleCDataValue(QoreString &str, const AbstractQoreNode *n, ExceptionSink *xsink) {
   //printd(5, "concatSimpleValue() n=%p (%s) %s\n", n, n->getTypeName(), n->getType() == NT_STRING ? ((QoreStringNode *)n)->getBuffer() : "unknown");
   if (n && n->getType() == NT_STRING) {
      const QoreStringNode *qsn = reinterpret_cast<const QoreStringNode *>(n);
      if (strstr(qsn->getBuffer(), "]]>")) {
	 xsink->raiseException("MAKE-XML-ERROR", "CDATA text contains illegal ']]>' sequence");
	 return -1;
      }
      str.concat(qsn, xsink);
      return *xsink ? -1 : 0;
   }

   return concatSimpleValue(str, n, xsink);
}

static int makeXMLString(QoreString &str, const QoreHashNode &h, int indent, int format, ExceptionSink *xsink);

QoreStringNode *makeXMLString(const QoreEncoding *enc, const QoreHashNode &h, bool format, ExceptionSink *xsink) {
   SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?>", enc->getCode());
   if (format) str->concat('\n');
   return makeXMLString(*(*str), h, 0, format, xsink) ? 0 : str.release();
}

static void addXMLElement(const char *key, QoreString &str, const AbstractQoreNode *n, int indent, int format, ExceptionSink *xsink) {
   //QORE_TRACE("addXMLElement()");

   if (is_nothing(n)) {
      str.concat('<');
      str.concat(key);
      str.concat("/>");
      return;
   }

   qore_type_t ntype = n->getType();

   if (ntype == NT_LIST) {
      const QoreListNode *l = reinterpret_cast<const QoreListNode *>(n);
      // iterate through the list
      int ls = l->size();
      if (ls) {
	 for (int j = 0; j < ls; j++) {
	    const AbstractQoreNode *v = l->retrieve_entry(j);
	    // indent all but first entry if necessary
	    if (j && format) {
	       str.concat('\n');
	       str.addch(' ', indent);
	    }
	    
	    addXMLElement(key, str, v, indent, format, xsink);
	 }
      }
      else {    // close node
	 str.concat('<');
	 str.concat(key);
	 str.concat("/>");
      }
      return;
   }

   // open node
   str.concat('<');
   str.concat(key);

   if (ntype == NT_HASH) {
      const QoreHashNode *h = reinterpret_cast<const QoreHashNode *>(n);
      // inc = ignore node counter, see if special keys exists and increment counter even if they have no value
      qore_size_t inc = 0;
      int vn = 0;
      bool exists;
      const AbstractQoreNode *value = h->getKeyValueExistence("^value^", exists);
      if (!exists)
	 value = 0;
      else {
	 vn++;
	 if (is_nothing(value))
	    inc++;
	 // find all ^value*^ nodes
	 QoreString val;
	 while (true) {
	    val.sprintf("^value%d^", vn);
	    value = h->getKeyValueExistence(val.getBuffer(), exists);
	    if (!exists) {
	       value = 0;
	       break;
	    }
	    else if (is_nothing(value)) // if the node exists but there is no value, then skip
	       inc++;
	    vn++;
	 }
      }
      
      const AbstractQoreNode *attrib = h->getKeyValueExistence("^attributes^", exists);
      if (!exists)
	 attrib = 0;
      else
	 inc++;
      
      // add attributes for objects
      if (attrib && attrib->getType() == NT_HASH) {
	 const QoreHashNode *ah = reinterpret_cast<const QoreHashNode *>(attrib);
	 // add attributes to node
	 ConstHashIterator hi(ah);
	 while (hi.next()) {
	    const char *tkey = hi.getKey();
	    str.sprintf(" %s=\"", tkey);
	    const AbstractQoreNode *v = hi.getValue();
	    if (v) {
	       if (v->getType() == NT_STRING) 
		  str.concatAndHTMLEncode(reinterpret_cast<const QoreStringNode *>(v), xsink);
	       else { // convert to string and add
		  QoreStringValueHelper temp(v);
		  str.concat(*temp, xsink);
	       }
	    }
	    str.concat('\"');
	 }
      }
      
      //printd(5, "inc=%d vn=%d\n", inc, vn);
      
      // if there are no more elements, close node immediately
      if (h->size() == inc) {
	 str.concat("/>");
	 return;
      }
      
      // close node
      str.concat('>');

      if (!is_nothing(value) && h->size() == (inc + 1)) {
	 if (concatSimpleValue(str, value, xsink))
	    return;
      }
      else { // add additional elements and formatting only if the additional elements exist 
	 if (format && !vn)
	    str.concat('\n');
	 
	 makeXMLString(str, *h, indent + 2, !vn ? format : 0, xsink);
	 // indent closing entry
	 if (format && !vn) {
	    str.concat('\n');
	    str.addch(' ', indent);
	 }
      }
   }
   else {
      // close node
      str.concat('>');
      
      if (ntype == NT_OBJECT) {
	 const QoreObject *o = reinterpret_cast<const QoreObject *>(n);
	 // get snapshot of data
	 QoreHashNodeHolder h(o->copyData(xsink), xsink);
	 if (!*xsink) {
	    if (format)
	       str.concat('\n');
	    makeXMLString(str, *(*h), indent + 2, format, xsink);
	    // indent closing entry
	    if (format)
	       str.addch(' ', indent);
	 }
      }
      else 
	 concatSimpleValue(str, n, xsink);
   }
   
   // close node
   str.concat("</");
   str.concat(key);
   str.concat('>');
}

static int makeXMLString(QoreString &str, const QoreHashNode &h, int indent, int format, ExceptionSink *xsink) {
   QORE_TRACE("makeXMLString()");

   ConstHashIterator hi(h);
   bool done = false;
   while (hi.next()) {
      std::auto_ptr<QoreString> keyStr(hi.getKeyString());
      // convert string if needed
      if (keyStr->getEncoding() != str.getEncoding()) {
	 QoreString *ns = keyStr->convertEncoding(str.getEncoding(), xsink);
	 if (xsink->isEvent())
	    return -1;
	 keyStr.reset(ns);
      }

      const char *key = keyStr->getBuffer();
      if (!strcmp(key, "^attributes^"))
	 continue;

      if (!strncmp(key, "^value", 6)) {
	 if (concatSimpleValue(str, hi.getValue(), xsink))
	    return -1;
	 continue;
      }

      if (!strncmp(key, "^cdata", 5)) {
	 str.concat("<![CDATA[");
	 if (concatSimpleCDataValue(str, hi.getValue(), xsink))
	    return -1;
	 str.concat("]]>");
	 continue;
      }

      // make sure it's a valid XML tag element name
      if (!key || !isalpha(key[0])) {
	 xsink->raiseException("MAKE-XML-ERROR", "tag: \"%s\" is not a valid XML tag element name", key ? key : "");
	 return -1;
      }

      // process key name - remove ^# from end of key name if present
      qore_size_t l = keyStr->strlen() - 1;
      while (isdigit(key[l]))
	 l--;

      if (l != (keyStr->strlen() - 1) && key[l] == '^')
	 keyStr->terminate(l);

      // indent entry
      if (format) {
	 if (done)
	    str.concat('\n');
         str.addch(' ', indent);
      }
      //printd(5, "makeXMLString() level %d adding member %s\n", indent / 2, node->getBuffer());
      addXMLElement(key, str, hi.getValue(), indent, format, xsink);
      done = true;
   }

   return 0;
}

// returns top-level key name
static bool hash_ok(const QoreHashNode *h) {
   int count = 0;

   ConstHashIterator hi(h);

   while (hi.next()) {
      const char *k = hi.getKey();
      if (!k[0] || k[0] == '^')
	 continue;

      if (++count > 1)
	 break;
   }

   return count == 1;
}

static AbstractQoreNode *makeXMLStringIntern(const QoreStringNode *pstr, const QoreHashNode *pobj, const QoreEncoding *ccs, bool format, ExceptionSink *xsink) {
   SimpleRefHolder<QoreStringNode> str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?>", ccs->getCode());

   if (pstr) {
      TempEncodingHelper key(pstr, QCS_UTF8, xsink);
      if (!key)
	 return 0;
      addXMLElement(key->getBuffer(), *(*str), pobj, 0, 0, xsink);
   }
   else
      makeXMLString(*(*str), *pobj, 0, 0, xsink);

   //printd(5, "f_makeXMLString() returning %s\n", str->getBuffer());

   return str.release();
}

//! serializes a hash into an XML string without whitespace formatting but with an XML header
/** @param $key top-level key
    @param $h the rest of the data to serialize under the top-level key
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return an XML string corresponding to the input data, without whitespace formatting but with an XML header
    @throw MAKE-XML-ERROR An error occurred serializing the Qore data to an XML string
    @par Example:
    @code my string $xml = makeXMLString("key", $hash); @endcode
    @see @ref serialization
 */
//# string makeXMLString(string $key, hash $h, *string $encoding) {}
static AbstractQoreNode *f_makeXMLString_str(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(pstr, const QoreStringNode, params, 0);
   HARD_QORE_PARAM(pobj, const QoreHashNode, params, 1);

   const QoreEncoding *ccsid = get_encoding_param(params, 2, QCS_UTF8);
   return makeXMLStringIntern(pstr, pobj, ccsid, false, xsink);
}

//! serializes a hash into an XML string without whitespace formatting but with an XML header
/** @param $h a hash of data to serialize: the hash must have one top-level key and no more or an exception will be raised
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return an XML string corresponding to the input data, without whitespace formatting but with an XML header
    @throw MAKE-XML-STRING-PARAMETER-EXCEPTION the hash passed not not have a single top-level key (either has no keys or more than one)
    @throw MAKE-XML-ERROR An error occurred serializing the Qore data to an XML string
    @par Example:
    @code my string $xml = makeXMLString($hash); @endcode
    @see @ref serialization
 */
//# string makeXMLString(hash $h, *string $encoding) {}
static AbstractQoreNode *f_makeXMLString(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(pobj, const QoreHashNode, params, 0);
   
   if (!hash_ok(pobj)) {
      xsink->raiseException("MAKE-XML-STRING-PARAMETER-EXCEPTION",
			    "this variant of makeXMLString() expects a hash with a single key for the top-level XML element name");
      return 0;
   }

   const QoreEncoding *ccsid = get_encoding_param(params, 1, QCS_UTF8);
   return makeXMLStringIntern(0, pobj, ccsid, false, xsink);
}

//! serializes a hash into an XML string with whitespace formatting and with an XML header
/** @param $key top-level key
    @param $h the rest of the data to serialize under the top-level key
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return an XML string corresponding to the input data, with whitespace formatting and with an XML header
    @throw MAKE-XML-ERROR An error occurred serializing the Qore data to an XML string
    @par Example:
    @code my string $xml = makeFormattedXMLString("key", $hash); @endcode
    @see @ref serialization
*/
//# string makeFormattedXMLString(string $key, hash $h, *string $encoding) {}
static AbstractQoreNode *f_makeFormattedXMLString_str(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(pstr, const QoreStringNode, params, 0);
   HARD_QORE_PARAM(pobj, const QoreHashNode, params, 1);
   const QoreEncoding *ccsid = get_encoding_param(params, 2, QCS_UTF8);

   return makeXMLStringIntern(pstr, pobj, ccsid, true, xsink);
}

//! serializes a hash into an XML string with whitespace formatting and with an XML header
/** @param $h a hash of data to serialize: the hash must have one top-level key and no more or an exception will be raised
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return an XML string corresponding to the input data, with whitespace formatting and with an XML header
    @throw MAKE-FORMATTED-XML-STRING-PARAMETER-EXCEPTION the hash passed not not have a single top-level key (either has no keys or more than one)
    @throw MAKE-XML-ERROR An error occurred serializing the Qore data to an XML string
    @par Example:
    @code my string $xml = makeFormattedXMLString($hash); @endcode
    @see @ref serialization
 */
//# string makeFormattedXMLString(hash $h, *string $encoding) {}
static AbstractQoreNode *f_makeFormattedXMLString(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(pobj, const QoreHashNode, params, 0);
   
   if (!hash_ok(pobj)) {
      xsink->raiseException("MAKE-FORMATTED-XML-STRING-PARAMETER-EXCEPTION",
			    "this variant of makeFormattedXMLString() expects a hash with a single key for the top-level XML element name");
      return 0;
   }

   const QoreEncoding *ccsid = get_encoding_param(params, 1, QCS_UTF8);
   return makeXMLStringIntern(0, pobj, ccsid, true, xsink);
}

//! serializes a hash into an XML string without whitespace formatting and without an XML header
/** @param $h a hash of data to serialize: the hash can have any number of keys
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return an XML string corresponding to the input data, without whitespace formatting and without an XML header
    @throw MAKE-XML-ERROR An error occurred serializing the Qore data to an XML string
    @par Example:
    @code my string $xml = makeXMLFragment($hash); @endcode
    @see @ref serialization
 */
//# string makeXMLFragment(hash $h, *string $encoding) {}
static AbstractQoreNode *f_makeXMLFragment(const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("f_makeXMLFragment()");

   HARD_QORE_PARAM(pobj, const QoreHashNode, params, 0);
   const QoreEncoding *ccsid = get_encoding_param(params, 1);

   SimpleRefHolder<QoreStringNode> str(new QoreStringNode(ccsid));
   if (makeXMLString(*(*str), *pobj, 0, 0, xsink))
      return 0;
   
   return str.release();
}

//! serializes a hash into an XML string with whitespace formatting but without an XML header
/** @param $h a hash of data to serialize: the hash can have any number of keys
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return an XML string corresponding to the input data, with whitespace formatting but without an XML header
    @throw MAKE-XML-ERROR An error occurred serializing the Qore data to an XML string
    @par Example:
    @code my string $xml = makeFormattedXMLFragment($hash); @endcode
    @see @ref serialization
 */
//# string makeFormattedXMLFragment(hash $h, *string $encoding) {}
static AbstractQoreNode *f_makeFormattedXMLFragment(const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("f_makeFormattedXMLFragment()");

   HARD_QORE_PARAM(pobj, const QoreHashNode, params, 0);
   const QoreEncoding *ccsid = get_encoding_param(params, 1);

   SimpleRefHolder<QoreStringNode> str(new QoreStringNode(ccsid));
   if (makeXMLString(*(*str), *pobj, 0, 1, xsink))
      return 0;
   
   return str.release();
}

static void addXMLRPCValue(QoreString *str, const AbstractQoreNode *n, int indent, const QoreEncoding *ccs, int format, ExceptionSink *xsink);

static inline void addXMLRPCValueInternHash(QoreString *str, const QoreHashNode *h, int indent, const QoreEncoding *ccs, int format, ExceptionSink *xsink) {
   str->concat("<struct>");
   if (format) str->concat('\n');
   ConstHashIterator hi(h);
   while (hi.next()) {
      std::auto_ptr<QoreString> member(hi.getKeyString());
      if (!member->strlen()) {
	 xsink->raiseException("XMLRPC-SERIALIZATION-ERROR", "empty member name in hash");
	 return;
      }
      // convert string if needed
      if (member->getEncoding() != ccs) {
	 QoreString *ns = member->convertEncoding(ccs, xsink);
	 if (xsink->isEvent()) {
	    return;	    
	 }
	 //printd(5, "addXMLRPCValueInternHashInternal() converted %s->%s, \"%s\"->\"%s\"\n", member->getEncoding()->getCode(), ccs->getCode(), member->getBuffer(), ns->getBuffer());
	 member.reset(ns);
      }
      //else printd(5, "addXMLRPCValueInternHashInternal() not converting %sx \"%s\"\n", member->getEncoding()->getCode(), member->getBuffer());
      // indent
      if (format)
         str->addch(' ', indent + 2);
      str->concat("<member>");
      if (format) {
	 str->concat('\n');
	 // indent
         str->addch(' ', indent + 4);
      }
      str->concat("<name>");
      str->concatAndHTMLEncode(member.get(), xsink);

      member.reset();

      str->concat("</name>");
      if (format) str->concat('\n');
      const AbstractQoreNode *val = hi.getValue();
      addXMLRPCValue(str, val, indent + 4, ccs, format, xsink);
      // indent
      if (format)
         str->addch(' ', indent + 2);
      str->concat("</member>");
      if (format) str->concat('\n');
   }
   // indent
   if (format)
      str->addch(' ', indent);
   str->concat("</struct>");
   //if (format) str->concat('\n');
}

static void addXMLRPCValueIntern(QoreString *str, const AbstractQoreNode *n, int indent, const QoreEncoding *ccs, int format, ExceptionSink *xsink) {
   assert(n);
   qore_type_t ntype = n->getType();

   if (ntype == NT_BOOLEAN)
      str->sprintf("<boolean>%d</boolean>", reinterpret_cast<const QoreBoolNode *>(n)->getValue());

   else if (ntype == NT_INT) {
      int64 val = reinterpret_cast<const QoreBigIntNode *>(n)->val;
      if (val >= -2147483647 && val <= 2147483647)
	 str->sprintf("<i4>%lld</i4>", val);
      else
	 str->sprintf("<string>%lld</string>", val);
   }

   else if (ntype == NT_STRING) {
      str->concat("<string>");
      str->concatAndHTMLEncode(reinterpret_cast<const QoreStringNode *>(n), xsink);
      str->concat("</string>");
   }

   else if (ntype == NT_FLOAT)
      str->sprintf("<double>%.20g</double>", reinterpret_cast<const QoreFloatNode *>(n)->f);
	
   else if (ntype == NT_DATE) {
      str->concat("<dateTime.iso8601>");
      str->concatISO8601DateTime(reinterpret_cast<const DateTimeNode *>(n));
      str->concat("</dateTime.iso8601>");
   }

   else if (ntype == NT_BINARY) {
      str->concat("<base64>");
      if (format) {
	 str->concat('\n');
	 // indent
         str->addch(' ', indent + 4);
      }
      str->concatBase64(reinterpret_cast<const BinaryNode *>(n));
      if (format) {
	 str->concat('\n');
	 // indent
         str->addch(' ', indent);
      }
      str->concat("</base64>");
   }

   else if (ntype == NT_HASH)
      addXMLRPCValueInternHash(str, reinterpret_cast<const QoreHashNode *>(n), indent + 2, ccs, format, xsink);

   else if (ntype == NT_LIST) {
      const QoreListNode *l = reinterpret_cast<const QoreListNode *>(n);
      str->concat("<array>");
      if (format) {
	 str->concat('\n');
	 // indent
         str->addch(' ', indent + 4);
      }
      if (l->size()) {
	 str->concat("<data>");
	 if (format) str->concat('\n');
	 for (unsigned i = 0; i < l->size(); i++)
	    addXMLRPCValue(str, l->retrieve_entry(i), indent + 6, ccs, format, xsink);
	 if (format)
            str->addch(' ', indent + 4);
	 str->concat("</data>");
      }
      else
	 str->concat("<data/>");
      if (format) {
	 str->concat('\n');
	 // indent
         str->addch(' ', indent + 2);
      }
      str->concat("</array>");
      //if (format) str->concat('\n');
   }
   else {
      xsink->raiseException("XMLRPC-SERIALIZATION-ERROR", "don't know how to serialize type '%s' to XML-RPC", get_type_name(n));
      return;
   }

   if (format) {
      str->concat('\n');
      // indent
      str->addch(' ' , indent);
   }
}

static void addXMLRPCValue(QoreString *str, const AbstractQoreNode *n, int indent, const QoreEncoding *ccs, int format, ExceptionSink *xsink) {
   QORE_TRACE("addXMLRPCValue()");

   // add value node
   // indent
   if (format)
      str->addch(' ', indent);
   
   if (!is_nothing(n) && !is_null(n)) {
      str->concat("<value>");
      if (format) {
	 str->concat('\n');
	 // indent
         str->addch(' ', indent + 2);
      }
      
      addXMLRPCValueIntern(str, n, indent, ccs, format, xsink);

      // close value node
      str->concat("</value>");
   }
   else
      str->concat("<value/>"); 
   if (format) str->concat('\n');

}

QoreStringNode *makeXMLRPCCallString(const QoreEncoding *ccs, int offset, const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(p0, const QoreStringNode, params, offset);

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?><methodCall><methodName>", ccs->getCode());
   str->concatAndHTMLEncode(p0, xsink);
   if (*xsink)
      return 0;

   str->concat("</methodName>");

   const AbstractQoreNode *p;

   // now process all params
   int ls = num_params(params);
   if (ls) {
      str->concat("<params>"); 

      for (int i = offset + 1; i < ls; i++) {
	 p = get_param(params, i);
	 str->concat("<param>");
	 addXMLRPCValue(*str, p, 0, ccs, 0, xsink);
	 if (*xsink)
	    return 0;
	 str->concat("</param>");
      }
      str->concat("</params>");
   }
   else
      str->concat("<params/>");
   str->concat("</methodCall>");

   return str.release();
}

//! Serializes the argument into an XML string in XML-RPC call format without whitespace formatting
/** @param $method the method name for the XML-RPC call
    Additional arguments are serialized according to the default XML-RPC serialization rules
    @return an XML string in XML-RPC call format in the default encoding, without whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeXMLRPCCallString("omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCCallString(string $method, ...) {}
static AbstractQoreNode *f_makeXMLRPCCallString(const QoreListNode *params, ExceptionSink *xsink) {
   return makeXMLRPCCallString(QCS_DEFAULT, 0, params, xsink);
}

//! Serializes the argument into an XML string in XML-RPC call format without whitespace formatting with an explicit encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    @param $method the method name for the XML-RPC call
    Additional arguments are serialized according to the default XML-RPC serialization rules
    @return an XML string in XML-RPC call format in the encoding given by the first argument, without whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeXMLRPCCallStringWithEncoding("utf8", "omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCCallStringWithEncoding(string $encoding, string $method, ...) {}
static AbstractQoreNode *f_makeXMLRPCCallStringWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeXMLRPCCallString(get_hard_qore_encoding_param(params, 0), 1, params, xsink);
}

QoreStringNode *makeXMLRPCCallStringArgs(const QoreEncoding *ccs, int offset, const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("makeXMLRPCCallStringArgs()");

   HARD_QORE_PARAM(p0, const QoreStringNode, params, offset);

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?><methodCall><methodName>", ccs->getCode());
   str->concatAndHTMLEncode(p0, xsink);
   if (*xsink)
      return 0;

   str->concat("</methodName><params>"); 

   const AbstractQoreNode *p1;
   const QoreListNode *l;
   if ((p1 = get_param(params, offset + 1)) && (l = dynamic_cast<const QoreListNode *>(p1)) && l->size()) {

      // now process all params
      int ls = l->size();
      for (int i = 0; i < ls; i++) {
	 const AbstractQoreNode *p = l->retrieve_entry(i);
	 str->concat("<param>");
	 addXMLRPCValue(*str, p, 0, ccs, 0, xsink);
	 if (*xsink)
	    return 0;

	 str->concat("</param>");
      }
   }
   else if (p1 && p1->getType() != NT_LIST) {
      str->concat("<param>"); 
      addXMLRPCValue(*str, p1, 0, ccs, 0, xsink);
      if (*xsink)
	 return 0;

      str->concat("</param>");
   }

   str->concat("</params></methodCall>");
   return str.release();
}

//! Serializes the argument into an XML string in XML-RPC call format without whitespace formatting
/** @param $method the method name for the XML-RPC call
    @param $args a single argument or a list of arguments to the call
    @return an XML string in XML-RPC call format in the default encoding, without whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeXMLRPCCallStringArgs("omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCCallStringArgs(string $method, any $args) {}
static AbstractQoreNode *f_makeXMLRPCCallStringArgs(const QoreListNode *params, ExceptionSink *xsink) {
   return makeXMLRPCCallStringArgs(QCS_DEFAULT, 0, params, xsink);
}

//! Serializes the argument into an XML string in XML-RPC call format without whitespace formatting with an explicit encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    @param $method the method name for the XML-RPC call
    @param $args a single argument or a list of arguments to the call
    @return a string in XML-RPC call format in the encoding given by the first argument, without whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeXMLRPCCallStringArgsWithEncoding("utf8", "omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCCallStringArgsWithEncoding(string $encoding, string $method, any $args) {}
static AbstractQoreNode *f_makeXMLRPCCallStringArgsWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeXMLRPCCallStringArgs(get_hard_qore_encoding_param(params, 0), 0, params, xsink);
}

// returns true if the key names are equal, ignoring any possible "^" suffix in k2
static bool keys_are_equal(const char *k1, const char *k2, bool &get_value) {
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

AbstractQoreNode *QoreXmlReader::getXmlData(const QoreEncoding *data_ccsid, bool as_data, ExceptionSink *xsink) {
   xml_stack xstack;

   QORE_TRACE("getXMLData()");
   int rc = 1;

   while (rc == 1) {
      int nt = nodeTypeSkipWhitespace();
      // get node name
      const char *name = constName();
      if (!name)
	 name = "--";

      if (nt == -1) // ERROR
	 break;

      if (nt == XML_READER_TYPE_ELEMENT) {
	 int depth = QoreXmlReader::depth();
	 xstack.checkDepth(depth);

	 AbstractQoreNode *n = xstack.getNode();
	 // if there is no node pointer, then make a hash
	 if (!n) {
	    QoreHashNode *h = new QoreHashNode;
	    xstack.setNode(h);
	    xstack.push(h->getKeyValuePtr(name), depth);
	 }
	 else { // node ptr already exists
	    QoreHashNode *h = n->getType() == NT_HASH ? reinterpret_cast<QoreHashNode *>(n) : 0;
	    if (!h) {
	       h = new QoreHashNode;
	       xstack.setNode(h);
	       h->setKeyValue("^value^", n, 0);
	       xstack.incValueCount();
	       xstack.push(h->getKeyValuePtr(name), depth);
	    }
	    else {
	       // see if key already exists
	       AbstractQoreNode *v;
	       if (!(v = h->getKeyValue(name)))
		  xstack.push(h->getKeyValuePtr(name), depth);
	       else {
		  if (as_data) {
		     QoreListNode *vl = v->getType() == NT_LIST ? reinterpret_cast<QoreListNode *>(v) : 0;
		     // if it's not a list, then make into a list with current value as first entry
		     if (!vl) {
			AbstractQoreNode **vp = h->getKeyValuePtr(name);
			vl = new QoreListNode;
			vl->push(v);
			(*vp) = vl;
		     }
		     xstack.push(vl->get_entry_ptr(vl->size()), depth);
		  }
		  else {
		     // see if last key was the same, if so make a list if it's not
		     const char *lk = h->getLastKey();
		     bool get_value = false;
		     if (keys_are_equal(name, lk, get_value)) {
			// get actual key value if there was a suffix 
			if (get_value)
			   v = h->getKeyValue(lk);
			
			QoreListNode *vl = v->getType() == NT_LIST ? reinterpret_cast<QoreListNode *>(v) : 0;
			// if it's not a list, then make into a list with current value as first entry
			if (!vl) {
			   AbstractQoreNode **vp = h->getKeyValuePtr(lk);
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
			   AbstractQoreNode *et = h->getKeyValue(ns.getBuffer());
			   if (!et)
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
	    ReferenceHolder<QoreHashNode> h(new QoreHashNode(), xsink);
	    while (moveToNextAttribute(xsink) == 1) {
	       const char *aname = constName();
	       QoreStringNode *value = getValue(data_ccsid, xsink);
	       if (!value)
		  return 0;
	       h->setKeyValue(aname, value, xsink);
	    }
	    if (*xsink)
	       return 0;

	    // make new new a hash and assign "^attributes^" key
	    QoreHashNode *nv = new QoreHashNode;
	    nv->setKeyValue("^attributes^", h.release(), xsink);
	    xstack.setNode(nv);
	 }
	 //printd(5, "%s: type=%d, hasValue=%d, empty=%d, depth=%d\n", name, nt, xmlTextReaderHasValue(reader), xmlTextReaderIsEmptyElement(reader), depth);
      }
      else if (nt == XML_READER_TYPE_TEXT) {
	 int depth = QoreXmlReader::depth();
	 xstack.checkDepth(depth);

	 const char *str = constValue();
	 if (str) {
	    QoreStringNode *val = getValue(data_ccsid, xsink);
	    if (!val)
	       return 0;

	    AbstractQoreNode *n = xstack.getNode();
	    if (n) {
	       QoreHashNode *h = n->getType() == NT_HASH ? reinterpret_cast<QoreHashNode *>(n) : 0;
	       if (h) {
		  if (!xstack.getValueCount())
		     h->setKeyValue("^value^", val, xsink);
		  else {
		     QoreString kstr;
		     kstr.sprintf("^value%d^", xstack.getValueCount());
		     h->setKeyValue(kstr.getBuffer(), val, xsink);
		  }		  
	       }
	       else { // convert value to hash and save value node
		  h = new QoreHashNode;
		  xstack.setNode(h);
		  h->setKeyValue("^value^", n, 0);
		  xstack.incValueCount();

		  QoreString kstr;
		  kstr.sprintf("^value%d^", 1);
		  h->setKeyValue(kstr.getBuffer(), val, xsink);
	       }
	       xstack.incValueCount();
	    }
	    else
	       xstack.setNode(val);
	 }
      }
      else if (nt == XML_READER_TYPE_CDATA) {
	 int depth = QoreXmlReader::depth();
	 xstack.checkDepth(depth);

	 const char *str = constValue();
	 if (str) {
	    QoreStringNode *val = getValue(data_ccsid, xsink);
	    if (!val)
	       return 0;

	    AbstractQoreNode *n = xstack.getNode();
	    if (n && n->getType() == NT_HASH) {
	       QoreHashNode *h = reinterpret_cast<QoreHashNode *>(n);
	       if (!xstack.getCDataCount())
		  h->setKeyValue("^cdata^", val, xsink);
	       else {
		  QoreString kstr;
		  kstr.sprintf("^cdata%d^", xstack.getCDataCount());
		  h->setKeyValue(kstr.getBuffer(), val, xsink);
	       }		  
	    }
	    else { // convert value to hash and save value node
	       QoreHashNode *h = new QoreHashNode;
	       xstack.setNode(h);
	       if (n) {
		  h->setKeyValue("^value^", n, 0);
		  xstack.incValueCount();
	       }

	       h->setKeyValue("^cdata^", val, xsink);
	    }
	    xstack.incCDataCount();
	 }
      }
      rc = read();
   }
   return rc ? 0 : xstack.getVal();
}

int QoreXmlRpcReader::getStruct(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink) {
   int nt;

   QoreHashNode *h = new QoreHashNode();
   v->set(h);

   int member_depth = depth();
   while (true) {
      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
      
      if (nt == XML_READER_TYPE_END_ELEMENT)
	 break;
      
      if (nt != XML_READER_TYPE_ELEMENT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting 'member' element (got type %d)", nt);
	 return -1;
      }
      
      // check for 'member' element
      if (checkXmlRpcMemberName("member", xsink))
	 return -1;
      
      // get member name
      if (readXmlRpc(xsink))
	 return -1;
      
      if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting struct 'name'");
	 return -1;
      }
      
      // check for 'name' element
      if (checkXmlRpcMemberName("name", xsink))
	 return -1;
      
      if (readXmlRpc(xsink))
	 return -1;
      
      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
      
      if (nt != XML_READER_TYPE_TEXT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "empty member name in hash");
	 return -1;
      }
      
      const char *member_name = constValue();
      if (!member_name) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "empty member name in struct");
	 return -1;
      }
      
      QoreString member(member_name);
      //printd(5, "DEBUG: got member name '%s'\n", member_name);
      
      if (readXmlRpc(xsink))
	 return -1;
      
      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
      if (nt != XML_READER_TYPE_END_ELEMENT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting name close element");
	 return -1;
      }
      
      // get value
      if (readXmlRpc(xsink))
	 return -1;
      
      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
      if (nt != XML_READER_TYPE_ELEMENT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting struct 'value' for key '%s'", member.getBuffer());
	 return -1;
      }
      
      if (checkXmlRpcMemberName("value", xsink))
	 return -1;
      
      if (readXmlRpc(xsink))
	 break;
      
      v->setPtr(h->getKeyValuePtr(member.getBuffer()));
      
      // if if was not an empty value element
      if (member_depth < depth()) {
	 // check for close value tag
	 if ((nt = readXmlRpcNode(xsink)) == -1)
	    return -1;
	 if (nt != XML_READER_TYPE_END_ELEMENT) {
	    //printd(5, "struct member='%s', parsing value node\n", member.getBuffer());
	   
	    if (getValueData(v, data_ccsid, true, xsink))
	       return -1;

	    //printd(5, "struct member='%s', finished parsing value node\n", member.getBuffer());
	    	    
	    if ((nt = readXmlRpcNode(xsink)) == -1)
	       return -1;
	    if (nt != XML_READER_TYPE_END_ELEMENT) {
	       //printd(5, "EXCEPTION close /value: %d: %s\n", nt, (char *)constName());
	       xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting value close element");
	       return -1;
	    }
	    //printd(5, "close /value: %s\n", (char *)constName());
	 }
	 if (readXmlRpc(xsink))
	    return -1;
      }
      
      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
      if (nt != XML_READER_TYPE_END_ELEMENT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting member close element");
	 return -1;
      }
      //printd(5, "close /member: %s\n", (char *)constName());

      if (readXmlRpc(xsink))
	 return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getParams(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink) {
   int nt;
   int index = 0;

   QoreListNode *l = new QoreListNode();
   v->set(l);

   int array_depth = depth();

   while (true) {
      // expecting param open element
      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;

      //printd(5, "getParams() nt=%d name=%s\n", nt, constName());
      
      // if higher-level "params" element closed, then return
      if (nt == XML_READER_TYPE_END_ELEMENT)
	 return 0;
      
      if (nt != XML_READER_TYPE_ELEMENT) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting 'param' open element");
	 return -1;
      }
      
      if (checkXmlRpcMemberName("param", xsink))
	 return -1;
      
      v->setPtr(l->get_entry_ptr(index++));

      // get next value tag or param close tag
      if (readXmlRpc(xsink))
	 return -1;
      
      int value_depth = depth();
      // if param was not an empty node
      if (value_depth > array_depth) {
	 if ((nt = readXmlRpcNode(xsink)) == -1)
	    return -1;
	 
	 // if we got a "value" element
	 if (nt == XML_READER_TYPE_ELEMENT) {
	    if (checkXmlRpcMemberName("value", xsink))
	       return -1;
      	    
	    if (readXmlRpc(xsink))
	       return -1;

	    //printd(5, "just read <value>, now value_depth=%d, depth=%d\n", value_depth, depth());

	    // if this was <value/>, then skip
	    if (value_depth <= depth()) {
	       if ((nt = readXmlRpcNode(xsink)) == -1)
		  return -1;
	       
	       // if ! </value>
	       if (nt != XML_READER_TYPE_END_ELEMENT) {
		  if (getValueData(v, data_ccsid, true, xsink))
		     return -1;
		  
		  if ((nt = readXmlRpcNode(xsink)) == -1)
		     return -1;
		  
		  if (nt != XML_READER_TYPE_END_ELEMENT) {
		     xsink->raiseException("PARSE-XMLRPC-ERROR", "extra data in params, expecting value close tag");
		     return -1;
		  }
	       }
	       // get param close tag
	       if (readXmlRpc(xsink))
		  return -1;
	    }

	    if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
	       xsink->raiseException("PARSE-XMLRPC-ERROR", "extra data in params, expecting param close tag (got node type %s instead)", get_xml_node_type_name(nt));
	       return -1;
	    }	    
	 }
	 else if (nt != XML_READER_TYPE_END_ELEMENT) {
	    xsink->raiseException("PARSE-XMLRPC-ERROR", "extra data in params, expecting value element");
	    return -1;
	 }
	 // just read a param close tag, position reader at next element
	 if (readXmlRpc(xsink))
	    return -1;
      }
   }
   return 0;
}

int QoreXmlRpcReader::getString(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_END_ELEMENT) {
      // save an empty string
      v->set(null_string());
      return 0;
   }

   if (nt != XML_READER_TYPE_TEXT && nt != XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
      //printd(5, "getString() unexpected node type %d (expecting text %s)\n", nt, constName());
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in string");
      return -1;
   }

   QoreStringNode *qstr = getValue(data_ccsid, xsink);
   if (!qstr)
      return -1;

   //printd(5, "** got string '%s'\n", str);
   v->set(qstr);
   
   if (readXmlRpc(xsink))
      return -1;
   
   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;
   
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      printd(5, "getString() unexpected node type %d (expecting end element %s)\n", nt, constName());
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in string (%d)", nt);
      return -1;
   }

   return 0;
}

int QoreXmlRpcReader::getBoolean(XmlRpcValue *v, ExceptionSink *xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char *str = constValue();
      if (str) {
	 //printd(5, "** got boolean '%s'\n", str);
	 v->set(strtoll(str, 0, 10) ? boolean_true() : boolean_false());
      }

      if (readXmlRpc(xsink))
	 return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
   }
   else
      v->set(boolean_false());
   
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in boolean (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getInt(XmlRpcValue *v, ExceptionSink *xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char *str = constValue();
      if (str) {
	 //printd(5, "** got int '%s'\n", str);
	 // note that we can parse 64-bit integers here, which is not conformant to the standard
	 v->set(new QoreBigIntNode(strtoll(str, 0, 10)));
      }

      if (readXmlRpc(xsink))
	 return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
   }
   else
      v->set(new QoreBigIntNode());
   
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in int (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getDouble(XmlRpcValue *v, ExceptionSink *xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char *str = constValue();
      if (str) {
	 //printd(5, "** got float '%s'\n", str);
	 v->set(new QoreFloatNode(strtod(str, 0)));
      }

      // advance to next position
      if (readXmlRpc(xsink))
	 return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
   }
   else
      v->set(zero_float());
   
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in float (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getDate(XmlRpcValue *v, ExceptionSink *xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char *str = constValue();
      if (str)
	 v->set(new DateTimeNode(str));

      // advance to next position
      if (readXmlRpc(xsink))
	 return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
   }
   else
      v->set(zero_date());
   
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in float (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getBase64(XmlRpcValue *v, ExceptionSink *xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char *str = constValue();
      if (str) {
	 //printd(5, "** got base64 '%s'\n", str);
	 BinaryNode *b = parseBase64(str, strlen(str), xsink);
	 if (!b)
	    return -1;

	 v->set(b);
      }

      // advance to next position
      if (readXmlRpc(xsink))
	 return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
	 return -1;
   }
   else
      v->set(new BinaryNode());
   
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in base64 (%d)", nt);
      return -1;
   }
   return 0;
}

static int doEmptyValue(XmlRpcValue *v, const char *name, int depth, ExceptionSink *xsink) {
   if (!strcmp(name, "string"))
      v->set(null_string());
   else if (!strcmp(name, "i4") || !strcmp(name, "int") || !strcmp(name, "ex:i1") || !strcmp(name, "ex:i2") || !strcmp(name, "ex:i8"))
      v->set(zero());
   else if (!strcmp(name, "boolean"))
      v->set(get_bool_node(false));
   else if (!strcmp(name, "struct"))
      v->set(new QoreHashNode());
   else if (!strcmp(name, "array"))
      v->set(new QoreListNode());
   else if (!strcmp(name, "double") || !strcmp(name, "ex:float"))
      v->set(zero_float());
   else if (!strcmp(name, "dateTime.iso8601") || !strcmp(name, "ex:dateTime"))
      v->set(zero_date());
   else if (!strcmp(name, "base64"))
      v->set(new BinaryNode());
   else if (!strcmp(name, "ex:nil"))
      v->set(0);
   else {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "unknown XML-RPC type '%s' at level %d", name, depth);
      return -1;
   }
   return 0;
}

QoreHashNode *QoreXmlReader::parseXMLData(const QoreEncoding *data_ccsid, bool as_data, ExceptionSink *xsink) {
   if (read(xsink) != 1)
      return 0;

   AbstractQoreNode *rv = getXmlData(data_ccsid, as_data, xsink);

   if (!rv) {
      if (!*xsink)
	 xsink->raiseExceptionArg("PARSE-XML-EXCEPTION", xml ? new QoreStringNode(*xml) : 0, "parse error parsing XML string");
      return 0;
   }
   assert(rv->getType() == NT_HASH);

   return reinterpret_cast<QoreHashNode *>(rv);
}

int QoreXmlRpcReader::getArray(XmlRpcValue *v, const QoreEncoding *data_ccsid, ExceptionSink *xsink) {
   int nt;
   int index = 0;

   QoreListNode *l = new QoreListNode();
   v->set(l);

   int array_depth = depth();

   // expecting data open element
   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;
      
   // if higher-level element closed, then return
   if (nt == XML_READER_TYPE_END_ELEMENT)
      return 0;
      
   if (nt != XML_READER_TYPE_ELEMENT) {
      xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", new QoreStringNode(*xml), "error parsing XML string, expecting data open element");
      return -1;
   }
      
   if (checkXmlRpcMemberName("data", xsink))
      return -1;

   //printd(5, "getArray() level=%d before str=%s\n", depth(), (char *)constName());

   // get next value tag or data close tag
   if (readXmlRpc(xsink))
      return -1;

   int value_depth = depth();

   // if we just read an empty tag, then don't try to read to data close tag
   if (value_depth > array_depth) {
      while (true) {
	 if ((nt = readXmlRpcNode(xsink)) == -1)
	    return -1;
	 
	 if (nt == XML_READER_TYPE_END_ELEMENT)
	    break;
	 
	 // get "value" element
	 if (nt != XML_READER_TYPE_ELEMENT) {
	    xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", new QoreStringNode(*xml), "extra data in array, expecting value element");
	    return -1;
	 }
	 
	 if (checkXmlRpcMemberName("value", xsink))
	    return -1;
	 
	 v->setPtr(l->get_entry_ptr(index++));
	 
	 if (readXmlRpc(xsink))
	    return -1;

	 //printd(5, "DEBUG: vd=%d, d=%d\n", value_depth, depth());

	 // if this was <value/>, then skip
	 if (value_depth < depth()) {
	    if ((nt = readXmlRpcNode(xsink)) == -1)
	       return -1;

	    if (nt == XML_READER_TYPE_END_ELEMENT)
	       v->set(0);
	    else {
	       if (getValueData(v, data_ccsid, true, xsink))
		  return -1;

	       // check for </value> close tag
	       if ((nt = readXmlRpcNode(xsink)) == -1)
		  return -1;

	       if (nt != XML_READER_TYPE_END_ELEMENT) {
		  xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", new QoreStringNode(*xml), "extra data in array, expecting value close tag");
		  return -1;
	       }
	    }
	    // read </data> close tag element
	    if (readXmlRpc("expecting data close tag", xsink))
	       return -1;
	 }
      }
      // read </array> close tag element
      if (readXmlRpc("error reading array close tag", xsink))
	 return -1;
   }
   else if (value_depth == array_depth && readXmlRpc(xsink))
      return -1;

   //printd(5, "vd=%d ad=%d\n", value_depth, array_depth);
   
   // check for array close tag
   if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
      if (nt == XML_READER_TYPE_ELEMENT)
	 xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", new QoreStringNode(*xml), "expecting array close tag, got element '%s' instead", constName());
      else
	 xsink->raiseExceptionArg("PARSE-XMLRPC-ERROR", new QoreStringNode(*xml), "extra data in array, expecting array close tag, got node type %d", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getValueData(XmlRpcValue *v, const QoreEncoding *data_ccsid, bool read_next, ExceptionSink *xsink) {
   int nt = nodeTypeSkipWhitespace();
   if (nt == -1) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string");
      return -1;
   }

   if (nt == XML_READER_TYPE_ELEMENT) {
      int depth = QoreXmlReader::depth();
      
      // get xmlrpc type name
      const char *name = constName();
      if (!name) {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "expecting type name, got NOTHING at level %d", depth);
	 return -1;
      }

      //printd(5, "DEBUG: getValueData() parsing type '%s'\n", name);

      int rc = read();
      if (rc != 1) {
	 if (!read_next)
	    return doEmptyValue(v, name, depth, xsink);

	 xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string");
	 return -1;
      }

      // if this was an empty element, assign an empty value
      if (depth > QoreXmlReader::depth())
	 return doEmptyValue(v, name, depth, xsink);

      if (!strcmp(name, "string")) {
	 if (getString(v, data_ccsid, xsink))
	    return -1;
      }
      else if (!strcmp(name, "i4") || !strcmp(name, "int") || !strcmp(name, "ex:i1") || !strcmp(name, "ex:i2") || !strcmp(name, "ex:i8")) {
	 if (getInt(v, xsink))
	    return -1;
      }
      else if (!strcmp(name, "boolean")) {
	 if (getBoolean(v, xsink))
	    return -1;
      }
      else if (!strcmp(name, "struct")) {
	 if (getStruct(v, data_ccsid, xsink))
	    return -1;
      }
      else if (!strcmp(name, "array")) {
	 if (getArray(v, data_ccsid, xsink))
	    return -1;
      }
      else if (!strcmp(name, "double") || !strcmp(name, "ex:float")) {
	 if (getDouble(v, xsink))
	    return -1;
      }
      else if (!strcmp(name, "dateTime.iso8601") || !strcmp(name, "ex:dateTime")) {
	 if (getDate(v, xsink))
	    return -1;
      }
      else if (!strcmp(name, "base64")) {
	 if (getBase64(v, xsink))
	    return -1;
      }
      else {
	 xsink->raiseException("PARSE-XMLRPC-ERROR", "unknown XML-RPC type '%s' at level %d", name, depth);
	 return -1;
      }
      
      printd(5, "getValueData() finished parsing type '%s' element depth=%d\n", name, depth);
      if (xsink->isEvent())
	 return -1;
   }
   else if (nt == XML_READER_TYPE_TEXT) { // without type defaults to string
      QoreStringNode *qstr = getValue(data_ccsid, xsink);
      if (!qstr)
	 return -1;
      v->set(qstr);
   }

   return read_next ? readXmlRpc(xsink) : 0;
}

// NOTE: the libxml2 library requires all input to be in UTF-8 encoding
// syntax: parseXML(xml string [, output encoding])
static AbstractQoreNode *parseXMLIntern(bool as_data, const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(p0, const QoreStringNode, params, 0);

   const QoreEncoding *ccsid = get_encoding_param(params, 1);

   //printd(5, "parseXMLintern(%d, %s)\n", as_data, p0->getBuffer());

   // convert to UTF-8 
   TempEncodingHelper str(p0, QCS_UTF8, xsink);
   if (!str)
      return 0;

   QoreXmlReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
   if (!reader)
      return 0;

   return reader.parseXMLData(ccsid, as_data, xsink);
}

//! Parses an XML string and returns a Qore hash structure
/** If duplicate, out-of-order XML elements are found in the input string, they are deserialized to Qore hash elements with the same name as the XML element but including a caret \c '^' and a numeric prefix to maintain the same key order in the Qore hash as in the input XML string.
    
    This function should only be used when it is important to maintain the XML element order in the resulting Qore data structure (for example, when the data must be re-serialized to an XML string and the element order within a subelement must be maintained), for example, when parsing and reserializing an OSX property list in XML format.   In all other cases, parseXMLAsData() should be used instead.
    @param $xml the XML string to parse
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, all strings in the output hash will have the default encoding
    @return a Qore hash structure corresponding to the XML input string
    @throw PARSE-XML-EXCEPTION Error parsing the XML string
    @par Example:
    @code my hash $h = parseXML($xmlstr); @endcode
    @see parseXMLAsData()
    @see @ref serialization
*/
//# hash parseXML(string $xml, *string $encoding) {}
static AbstractQoreNode *f_parseXML(const QoreListNode *params, ExceptionSink *xsink) {
   return parseXMLIntern(false, params, xsink);
}

//! This is a variant that is basically a noop, included for backwards-compatibility for functions that ignored type errors in the calling parameters
/** @par Tags:
    @ref RUNTIME_NOOP
 */
//# nothing parseXML() {}

//! Parses an XML string as data (does not necessarily preserve key order) and returns a Qore hash structure
/** This function does not preserve hash order with out-of-order duplicate keys; all duplicate keys are collapsed to the same list.

    Note that data deserialized with this function may not be reserialized to an identical XML string to the input due to the fact that duplicate, out-of-order XML elements are collapsed into lists in the resulting Qore hash, thereby losing the order in the original XML string.

    For a similar function preserving the order of keys in the XML in the resulting Qore hash by generating Qore hash element names with numeric suffixes, see parseXML().
    @param $xml the XML string to parse
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, all strings in the output hash will have the default encoding
    @return a Qore hash structure corresponding to the XML input string
    @throw PARSE-XML-EXCEPTION Error parsing the XML string
    @par Example:
    @code my hash $h = parseXMLAsData($xmlstr); @endcode
    @see parseXML()
    @see @ref serialization
*/
//# hash parseXMLAsData(string $xml, *string $encoding) {}
static AbstractQoreNode *f_parseXMLAsData(const QoreListNode *params, ExceptionSink *xsink) {
   return parseXMLIntern(true, params, xsink);
}

//! Serializes the argument into an XML string in XML-RPC fault response format without whitespace formatting
/** @param $code the fault code for the response; will be converted to an integer
    @param $msg the fault message string; the encoding of this argument will define the output encoding of the fault string returned
    @return a string in XML-RPC fault format in the same encoding as given by the $msg argument, without whitespace formatting
    @par Example:
    @code my string $response = makeXMLRPCFaultResponseString(500, $errmsg); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCFaultResponseString(softint $code, string $msg) {}
static AbstractQoreNode *f_makeXMLRPCFaultResponseString(const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("f_makeXMLRPCFaultResponseString()");

   int code = (int)HARD_QORE_INT(params, 0);
   HARD_QORE_PARAM(p1, const QoreStringNode, params, 1);
   const QoreEncoding *ccsid = p1->getEncoding();

   // for speed, the XML is created directly here
   QoreStringNode *str = new QoreStringNode(ccsid);
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?><methodResponse><fault><value><struct><member><name>faultCode</name><value><int>%d</int></value></member><member><name>faultString</name><value><string>",
		ccsid->getCode(), code);
   str->concatAndHTMLEncode(p1->getBuffer());
   str->concat("</string></value></member></struct></value></fault></methodResponse>");

   return str;
}

//! Serializes the argument into an XML string in XML-RPC fault response format without whitespace formatting with an explicit output encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    @param $code the fault code for the response; will be converted to an integer
    @param $msg the fault message string
    @return a string in XML-RPC fault format in the encoding given by the first argument, without whitespace formatting
    @par Example:
    @code my string $response = makeXMLRPCFaultResponseStringWithEncoding("utf8", 500, $errmsg); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCFaultResponseStringWithEncoding(string $encoding, softint $code, string $msg) {}
static AbstractQoreNode *f_makeXMLRPCFaultResponseStringWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("f_makeXMLRPCFaultResponseStringWithEncoding()");

   const QoreEncoding *ccs = get_hard_qore_encoding_param(params, 0);
   int code = (int)HARD_QORE_INT(params, 1);
   HARD_QORE_PARAM(pstr, const QoreStringNode, params, 2);

   // for speed, the XML is created directly here
   QoreStringNodeHolder rv(new QoreStringNode(ccs));

   rv->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?><methodResponse><fault><value><struct><member><name>faultCode</name><value><int>%d</int></value></member><member><name>faultString</name><value><string>",
	       ccs->getCode(), code);
   rv->concatAndHTMLEncode(pstr, xsink);
   if (*xsink)
       return 0;

   rv->concat("</string></value></member></struct></value></fault></methodResponse>");

   return rv.release();
}

static AbstractQoreNode *makeFormattedXMLRPCFaultResponseString(bool with_enc, const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("makeFormattedXMLRPCFaultResponseString()");

   int offset = with_enc ? 1 : 0;
   const QoreEncoding *ccs = with_enc ? get_hard_qore_encoding_param(params, 0) : 0;

   int code = (int)HARD_QORE_INT(params, offset);

   HARD_QORE_PARAM(p1, const QoreStringNode, params, offset + 1);
   if (!ccs) ccs = p1->getEncoding();
   //printd(5, "ccsid=%016x (%s) (%s) code=%d\n", ccsid, ccsid->getCode(), ((QoreStringNode *)p1)->getBuffer(), code);

   // for speed, the XML is created directly here
   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?>\n<methodResponse>\n  <fault>\n    <value>\n      <struct>\n        <member>\n          <name>faultCode</name>\n          <value><int>%d</int></value>\n        </member>\n        <member>\n          <name>faultString</name>\n          <value><string>",
		ccs->getCode(), code);
   str->concatAndHTMLEncode(p1, xsink);
   if (*xsink)
       return 0;

   str->concat("</string></value>\n        </member>\n      </struct>\n    </value>\n  </fault>\n</methodResponse>");

   return str.release();
}

//! Serializes the argument into an XML string in XML-RPC fault response format with whitespace formatting
/** @param $code the fault code for the response; will be converted to an integer
    @param $msg the fault message string; the encoding of this argument will define the output encoding of the fault string returned
    @return a string in XML-RPC fault format in the same encoding as given by the $msg argument, with whitespace formatting
    @par Example:
    @code my string $response = makeFormattedXMLRPCFaultResponseString(500, $errmsg); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCFaultResponseString(softint $code, string $msg) {}
static AbstractQoreNode *f_makeFormattedXMLRPCFaultResponseString(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCFaultResponseString(false, params, xsink);
}

//! Serializes the argument into an XML string in XML-RPC fault response format with whitespace formatting with an explicit output encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    @param $code the fault code for the response; will be converted to an integer
    @param $msg the fault message string
    @return a string in XML-RPC fault format in the encoding given by the first argument, with whitespace formatting
    @par Example:
    @code my string $response = makeFormattedXMLRPCFaultResponseStringWithEncoding("utf8", 500, $errmsg); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCFaultResponseStringWithEncoding(string $encoding, softint $code, string $msg) {}
static AbstractQoreNode *f_makeFormattedXMLRPCFaultResponseStringWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCFaultResponseString(true, params, xsink);
}

// makeXMLRPCResponseString(params, ...)
static AbstractQoreNode *makeXMLRPCResponseString(bool with_enc, const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("makeXMLRPCResponseString()");

   unsigned offset = with_enc ? 1 : 0;
   const QoreEncoding *ccs = with_enc ? get_hard_qore_encoding_param(params, 0) : QCS_DEFAULT;

   if (num_params(params) == offset)
      return 0;

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?><methodResponse><params>", ccs->getCode());

   const AbstractQoreNode *p;

   // now loop through the params
   int ls = num_params(params);
   for (int i = offset; i < ls; i++) {
      p = get_param(params, i);
      str->concat("<param>");
      addXMLRPCValue(*str, p, 0, ccs, 0, xsink);
      if (*xsink)
	 return 0;

      str->concat("</param>");
   }

   str->concat("</params></methodResponse>");

   return str.release();
}

//! Serializes the arguments into an XML string formatted for an XML-RPC response without whitespace formatting
/** Any top-level arguments to the function will be serialized as the top-level params of the response message
    @return a string in XML-RPC response format; the encoding of the resulting string will always be the default encoding 
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $response = makeXMLRPCResponseString($answer); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCResponseString(...) {}
static AbstractQoreNode *f_makeXMLRPCResponseString(const QoreListNode *params, ExceptionSink *xsink) {
   return makeXMLRPCResponseString(false, params, xsink);
}

//! Serializes the arguments into an XML string formatted for an XML-RPC response without whitespace formatting and with an explicit output encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    Any top-level arguments after the first argument will be serialized as the top-level params of the response message
    @return a string in XML-RPC response format; the encoding will be that given by the first argument
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $response = makeXMLRPCResponseStringWithEncoding("utf8", $answer); @endcode
    @see @ref XMLRPC
*/
//# string makeXMLRPCResponseStringWithEncoding(string $encoding, ...) {}
static AbstractQoreNode *f_makeXMLRPCResponseStringWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeXMLRPCResponseString(true, params, xsink);
}

//! Serializes the arguments into an XML string in XML-RPC value format without whitespace formatting and without an XML header
/** @param $value the value to serialize to XML-RPC format
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return if the $value argument is NOTHING, then NOTHING is returned, otherwise an XML string in XML-RPC value format without whitespace formatting and without an XML header is returned
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my *string $val = makeXMLRPCValueString($v); @endcode
    @see @ref XMLRPC
 */
//# *string makeXMLRPCValueString(any $value, *string $encoding) {}
static AbstractQoreNode *f_makeXMLRPCValueString(const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("f_makeXMLRPCValueString()");

   const AbstractQoreNode *p = get_param(params, 0);
   if (is_nothing(p))
       return 0;

   const QoreStringNode *estr = test_string_param(params, 1);
   const QoreEncoding *ccs = estr ? QEM.findCreate(estr) : QCS_DEFAULT;

   QoreStringNode *str = new QoreStringNode(ccs);
   addXMLRPCValueIntern(str, p, 0, ccs, 0, xsink);

   return str;
}

static AbstractQoreNode *makeFormattedXMLRPCCallStringArgs(bool with_enc, const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("makeFormattedXMLRPCCallStringArgs");

   int offset = with_enc ? 1 : 0;
   const QoreEncoding *ccs = with_enc ? get_hard_qore_encoding_param(params, 0) : QCS_DEFAULT;

   HARD_QORE_PARAM(p0, const QoreStringNode, params, offset);
   const AbstractQoreNode *p1;

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?>\n<methodCall>\n  <methodName>", ccs->getCode());
   str->concatAndHTMLEncode(p0, xsink);
   if (*xsink)
       return 0;

   str->concat("</methodName>\n  <params>\n");

   if ((p1 = get_param(params, offset + 1))) {
      const QoreListNode *l = dynamic_cast<const QoreListNode *>(p1);
      if (l) {
	 // now process all params
	 int ls = l->size();
	 for (int i = 0; i < ls; i++) {
	    const AbstractQoreNode *p = l->retrieve_entry(i);
	    str->concat("    <param>\n");
	    addXMLRPCValue(*str, p, 6, ccs, 1, xsink);
	    if (*xsink)
	       return 0;

	    str->concat("    </param>\n");
	 }
      }
      else {
	 str->concat("    <param>\n");
	 addXMLRPCValue(*str, p1, 6, ccs, 1, xsink);
	 if (*xsink)
	    return 0;

	 str->concat("    </param>\n");
      }
   }

   str->concat("  </params>\n</methodCall>");
   return str.release();
}

//! Serializes the argument into an XML string in XML-RPC call format with whitespace formatting
/** @param $method the method name for the XML-RPC call
    @param $args a single argument or a list of arguments to the call
    @return an XML string in XML-RPC call format in the default encoding, with whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeFormattedXMLRPCCallStringArgs("omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCCallStringArgs(string $method, any $args) {}
static AbstractQoreNode *f_makeFormattedXMLRPCCallStringArgs(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCCallStringArgs(false, params, xsink);
}

//! Serializes the argument into an XML string in XML-RPC call format with whitespace formatting
/** @param $encoding a string giving the output encoding for the resulting XML string
    @param $method the method name for the XML-RPC call
    @param $args a single argument or a list of arguments to the call
    @return a string in XML-RPC call format in the encoding given by the first argument, with whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeFormattedXMLRPCCallStringArgsWithEncoding("utf8", "omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCCallStringArgsWithEncoding(string $encoding, string $method, any $args) {}
static AbstractQoreNode *f_makeFormattedXMLRPCCallStringArgsWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCCallStringArgs(true, params, xsink);
}

static AbstractQoreNode *makeFormattedXMLRPCCallString(bool with_enc, const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("makeFormattedXMLRPCCallString");

   int offset = with_enc ? 1 : 0;
   const QoreEncoding *ccs = with_enc ? get_hard_qore_encoding_param(params, 0) : QCS_DEFAULT;

   HARD_QORE_PARAM(p0, const QoreStringNode, params, offset);

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?>\n<methodCall>\n  <methodName>", ccs->getCode());
   str->concatAndHTMLEncode(p0, xsink);
   if (*xsink)
       return 0;

   str->concat("</methodName>\n  <params>\n");

   // now loop through the params
   int ls = num_params(params);
   for (int i = offset + 1; i < ls; i++) {
      const AbstractQoreNode *p = get_param(params, offset + i);
      str->concat("    <param>\n");
      addXMLRPCValue(*str, p, 6, ccs, 1, xsink);
      if (*xsink)
	 return 0;

      str->concat("    </param>\n");
   }
   str->concat("  </params>\n</methodCall>");

   return str.release();
}

//! Serializes the argument into an XML string in XML-RPC call format with whitespace formatting
/** @param $method the method name for the XML-RPC call
    Additional arguments are serialized according to the default XML-RPC serialization rules
    @return an XML string in XML-RPC call format in the default encoding, with whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeFormattedXMLRPCCallString("omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCCallString(string $method, ...) {}
static AbstractQoreNode *f_makeFormattedXMLRPCCallString(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCCallString(false, params, xsink);
}

//! Serializes the argument into an XML string in XML-RPC call format with whitespace formatting with an explicit encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    @param $method the method name for the XML-RPC call
    Additional arguments are serialized according to the default XML-RPC serialization rules
    @return an XML string in XML-RPC call format in the encoding given by the first argument, with whitespace formatting
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $xmlcall = makeFormattedXMLRPCCallStringWithEncoding("utf8", "omq.system.start-workflow", $hash); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCCallStringWithEncoding(string $encoding, string $method, ...) {}
static AbstractQoreNode *f_makeFormattedXMLRPCCallStringWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCCallString(true, params, xsink);
}

// makeFormattedXMLRPCResponseString(params, ...)
static AbstractQoreNode *makeFormattedXMLRPCResponseString(bool with_enc, const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("makeFormattedXMLRPCResponseString");

   int offset = with_enc ? 1 : 0;
   const QoreEncoding *ccs = with_enc ? get_hard_qore_encoding_param(params, 0) : QCS_DEFAULT;

   int ls = num_params(params);
   if (ls == offset)
      return 0;

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   str->sprintf("<?xml version=\"1.0\" encoding=\"%s\"?>\n<methodResponse>\n  <params>\n", ccs->getCode());

   const AbstractQoreNode *p;

   // now loop through the params
   for (int i = offset; i < ls; i++) {
      p = get_param(params, i);
      str->concat("    <param>\n");
      addXMLRPCValue(*str, p, 6, ccs, 1, xsink);
      if (*xsink)
	 return 0;

      str->concat("    </param>\n");
   }

   str->concat("  </params>\n</methodResponse>");

   return str.release();
}

//! Serializes the arguments into an XML string formatted for an XML-RPC response with whitespace formatting
/** Any top-level arguments to the function will be serialized as the top-level params of the response message
    @return a string in XML-RPC response format with whitespace formatting; the encoding of the resulting string will always be the default encoding 
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $response = makeFormattedXMLRPCResponseString($answer); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCResponseString(...) {}
static AbstractQoreNode *f_makeFormattedXMLRPCResponseString(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCResponseString(false, params, xsink);
}

//! Serializes the arguments into an XML string formatted for an XML-RPC response with whitespace formatting and with an explicit output encoding
/** @param $encoding a string giving the output encoding for the resulting XML string
    Any top-level arguments after the first argument will be serialized as the top-level params of the response message
    @return a string in XML-RPC response format with whitespace formatting; the encoding will be that given by the first argument
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my string $response = makeFormattedXMLRPCResponseStringWithEncoding("utf8", $answer); @endcode
    @see @ref XMLRPC
*/
//# string makeFormattedXMLRPCResponseStringWithEncoding(string $encoding, ...) {}
static AbstractQoreNode *f_makeFormattedXMLRPCResponseStringWithEncoding(const QoreListNode *params, ExceptionSink *xsink) {
   return makeFormattedXMLRPCResponseString(true, params, xsink);
}

//! Serializes the arguments into an XML string in XML-RPC value format with whitespace formatting but without an XML header
/** @param $value the value to serialize to XML-RPC format
    @param $encoding an optional string giving the encoding for the output XML string; if this parameter is missing, the output string will have the default encoding
    @return if the $value argument is NOTHING, then NOTHING is returned, otherwise an XML string in XML-RPC value format with whitespace formatting but without an XML header is returned
    @throw XMLRPC-SERIALIZATION-ERROR empty member name in hash or cannot serialize type to XML-RPC (ex: object)
    @par Example:
    @code my *string $val = makeFormattedXMLRPCValueString($v); @endcode
    @see @ref XMLRPC
 */
//# *string makeFormattedXMLRPCValueString(any $value, *string $encoding) {}
static AbstractQoreNode *f_makeFormattedXMLRPCValueString(const QoreListNode *params, ExceptionSink *xsink) {
   QORE_TRACE("f_makeFormattedXMLRPCValueString()");

   const AbstractQoreNode *p;
   const QoreEncoding *ccs = QCS_DEFAULT;

   if (!(p = get_param(params, 0)))
      return 0;

   QoreStringNodeHolder str(new QoreStringNode(ccs));
   addXMLRPCValue(*str, p, 0, ccs, 1, xsink);
   if (*xsink)
      return 0;

   return str.release();
}

//! Deserializies an XML-RPC value string and returns a Qore data structure representing the information
/** @param $xml the XML string in XML-RPC value format to deserialize
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have the default encoding
    @return the Qore value corresponding to the XML-RPC value string
    @throw PARSE-XMLRPC-ERROR syntax error parsing XML-RPC string
    @par Example:
    @code my any $data = parseXMLRPCValue($xml); @endcode
    @see @ref XMLRPC
 */
//# any parseXMLRPCValue(string $xml, *string $encoding) {}
static AbstractQoreNode *f_parseXMLRPCValue(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(p0, const QoreStringNode, params, 0);

   const QoreEncoding *ccsid = get_encoding_param(params, 1);

   printd(5, "parseXMLRPCValue(%s)\n", p0->getBuffer());

   TempEncodingHelper str(p0, QCS_UTF8, xsink);
   if (!str)
      return 0;

   QoreXmlRpcReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
   if (!reader)
      return 0;

   if (reader.read(xsink) != 1)
      return 0;

   XmlRpcValue v;
   if (reader.getValueData(&v, ccsid, false, xsink))
      return 0;

   return v.getValue();
}

static inline AbstractQoreNode *qore_xml_exception(const char *ex, const char *info, ExceptionSink *xsink) {
   if (!*xsink)
      xsink->raiseException(ex, "error parsing XML string: %s", info);
   return 0;
}

static inline AbstractQoreNode *qore_xml_exception(const char *ex, ExceptionSink *xsink) {
   if (!*xsink)
      xsink->raiseException(ex, "error parsing XML string");
   return 0;
}

static inline QoreHashNode *qore_xml_hash_exception(const char *ex, const char *info, ExceptionSink *xsink, const QoreString *xml = 0) {
   if (!*xsink)
      xsink->raiseExceptionArg(ex, xml ? new QoreStringNode(*xml) : 0, "error parsing XML string: %s", info);
   return 0;
}

static inline QoreHashNode *qore_xml_hash_exception(const char *ex, ExceptionSink *xsink, const QoreString *xml = 0) {
   if (!*xsink)
      xsink->raiseExceptionArg(ex, xml ? new QoreStringNode(*xml) : 0, "error parsing XML string");
   return 0;
}

//! Deserializies an XML-RPC call string, returning a Qore data structure representing the call information
/** @param $xml the XML string in XML-RPC call format to deserialize
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have the default encoding
    @return a hash representing the XML-RPC call with the following keys:
    - \c methodName: the name of the method being called
    - \c params: the arguments to the method
    @throw PARSE-XMLRPC-CALL-ERROR missing 'methodCall' or 'methodName' element or other syntax error
    @throw PARSE-XMLRPC-ERROR syntax error parsing XML-RPC string
    @par Example:
    @code my hash $h = parseXMLRPCCall($xml); @endcode
    @see @ref XMLRPC    
 */
//# hash parseXMLRPCCall(string $xml, *string $encoding) {}
static AbstractQoreNode *f_parseXMLRPCCall(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(p0, const QoreStringNode, params, 0);

   const QoreEncoding *ccsid = get_encoding_param(params, 1);

   //printd(5, "parseXMLRPCCall() c=%d str=%s\n", p0->getBuffer()[0], p0->getBuffer());

   TempEncodingHelper str(p0, QCS_UTF8, xsink);
   if (!str)
      return 0;

   QoreXmlRpcReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
   if (!reader)
      return 0;

   if (reader.read(xsink) != 1)
      return 0;

   int nt;
   // get "methodCall" element
   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT)
      return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting 'methodCall' element", xsink);

   if (reader.checkXmlRpcMemberName("methodCall", xsink))
      return 0;

   // get "methodName" element
   if (reader.readXmlRpc("expecting methodName element", xsink))
      return 0;

   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT)
      return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting 'methodName' element", xsink);

   if (reader.checkXmlRpcMemberName("methodName", xsink))
      return 0;

   // get method name string
   if (reader.readXmlRpc("expecting method name", xsink))
      return 0;

   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_TEXT)
      return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting method name", xsink);

   const char *method_name = reader.constValue();
   if (!method_name)
      return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting method name", xsink);

   ReferenceHolder<QoreHashNode> h(new QoreHashNode(), xsink);
   h->setKeyValue("methodName", new QoreStringNode(method_name), 0);

   // get methodName close tag
   if (reader.readXmlRpc("expecting methodName close element", xsink))
      return 0;

   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT)
      return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting 'methodName' close element", xsink);

   // get "params" element
   if (reader.readXmlRpc("expecting params element", xsink))
      return 0;

   if ((nt = reader.readXmlRpcNode(xsink)) == -1)
      return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", xsink);

   // if the methodCall end element was not found
   if (nt != XML_READER_TYPE_END_ELEMENT) {
      if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT)
	 return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting 'params' element", xsink);

      if (reader.checkXmlRpcMemberName("params", xsink))
	 return 0;

      // get 'param' element or close params
      if (reader.readXmlRpc("expecting param element", xsink))
	 return 0;
      
      if ((nt = reader.readXmlRpcNode(xsink)) == -1)
	 return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", xsink);
      
      XmlRpcValue v;
      if (reader.depth()) {
	 if (nt != XML_READER_TYPE_END_ELEMENT) {
	    if (nt != XML_READER_TYPE_ELEMENT)
	       return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting 'params' element", xsink);
	    
	    if (reader.getParams(&v, ccsid, xsink))
	       return 0;
	 }

	 // get methodCall close tag
	 if (reader.readXmlRpc("expecting methodCall close tag", xsink))
	    return 0;
      }

      if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
	 return qore_xml_exception("PARSE-XMLRPC-CALL-ERROR", "expecting 'methodCall' close element", xsink);
      }

      h->setKeyValue("params", v.getValue(), xsink);
   }

   return h.release();
}

QoreHashNode *parseXMLRPCResponse(const QoreString *msg, const QoreEncoding *ccsid, ExceptionSink *xsink) {
   //printd(5, "parseXMLRPCResponse() %s\n", msg->getBuffer());

   TempEncodingHelper str(msg, QCS_UTF8, xsink);
   if (!str)
      return 0;

   QoreXmlRpcReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
   if (!reader)
      return 0;

   if (reader.read(xsink) != 1)
      return 0;

   int nt;
   // get "methodResponse" element
   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT)
       return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'methodResponse' element", xsink, *str);

   if (reader.checkXmlRpcMemberName("methodResponse", xsink))
      return 0;

   // check for params or fault element
   if (reader.readXmlRpc("expecting 'params' or 'fault' element", xsink))
      return 0;

   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT)
       return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'params' or 'fault' element", xsink, *str);

   const char *name = reader.constName();
   if (!name) {
      xsink->raiseExceptionArg("PARSE-XMLRPC-RESPONSE-ERROR", new QoreStringNode(*str), "missing 'params' or 'fault' element tag");
      return 0;
   }

   XmlRpcValue v;
   bool fault = false;
   if (!strcmp(name, "params")) {
      int depth = reader.depth();

      // get "params" element
      if (reader.readXmlRpc("expecting 'params' element", xsink))
	 return 0;

      int params_depth = reader.depth();

      // if params was not an empty element
      if (depth < params_depth) {
	 if ((nt = reader.readXmlRpcNode(xsink)) == -1)
	    return 0;

	 if (nt != XML_READER_TYPE_END_ELEMENT) {
	    if (nt != XML_READER_TYPE_ELEMENT)
	       return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'param' element", xsink, *str);
	       
	    if (reader.checkXmlRpcMemberName("param", xsink))
	       return 0;
	       
	    // get "value" element
	    if (reader.readXmlRpc("expecting 'value' element", xsink))
	       return 0;

	    // if param was not an empty element
	    depth = reader.depth();
	    if (params_depth < depth) {
	       if ((nt = reader.readXmlRpcNode(xsink)) == -1)
		  return 0;
	    
	       if (nt != XML_READER_TYPE_END_ELEMENT) {
		  if (nt != XML_READER_TYPE_ELEMENT)
		     return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'value' element", xsink, *str);
	       
		  if (reader.checkXmlRpcMemberName("value", xsink))
		     return 0;
	       		  
		  // position at next element
		  if (reader.readXmlRpc("expecting XML-RPC value element", xsink))
		     return 0;
		  
		  // if value was not an empty element
		  if (depth < reader.depth()) {
		     if (reader.getValueData(&v, ccsid, true, xsink))
			return 0;
		  }
		  if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT)
		     return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'param' end element", xsink, *str);
	       }

	       // get "params" end element
	       if (reader.readXmlRpc("expecting 'params' end element", xsink))
		  return 0;
	    }
	    if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT)
	       return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'params' end element", xsink, *str);
	 }
	 // get "methodResponse" end element
	 if (reader.readXmlRpc("expecting 'methodResponse' end element", xsink))
	    return 0;
      }
   }
   else if (!strcmp(name, "fault")) {
      fault = true;

      // get "value" element
      if (reader.readXmlRpc("expecting 'value' element", xsink))
	 return 0;
      
      if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_ELEMENT)
	 return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting fault 'value' element", xsink, *str);

      if (reader.checkXmlRpcMemberName("value", xsink))
	 return 0;

      // position at next element
      if (reader.readXmlRpc("expecting XML-RPC value element", xsink))
	 return 0;
      
      // get fault structure
      if (reader.getValueData(&v, ccsid, true, xsink))
	 return 0;

      if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT)
	 return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'fault' end element", xsink, *str);

      // get "methodResponse" end element
      if (reader.readXmlRpc("expecting 'methodResponse' end element", xsink))
	 return 0;
   }
   else {
      xsink->raiseException("PARSE-XMLRPC-RESPONSE-ERROR", "unexpected element '%s', expecting 'params' or 'fault'", name, *str);
      return 0;      
   }

   if ((nt = reader.nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT)
      return qore_xml_hash_exception("PARSE-XMLRPC-RESPONSE-ERROR", "expecting 'methodResponse' end element", xsink, *str);

   QoreHashNode *h = new QoreHashNode;
   if (fault)
      h->setKeyValue("fault", v.getValue(), 0);
   else
      h->setKeyValue("params", v.getValue(), 0);
   return h;
}

//! Deserializies an XML-RPC response string, returning a Qore data structure representing the response information
/** @param $xml the XML string in XML-RPC call format to deserialize
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have the default encoding
    @return a hash with one of the following keys:
    - \c fault: a hash describing a fault response
    - \c params: a hash describing a normal, non-fault response
    @throw PARSE-XMLRPC-RESPONSE-ERROR missing required element or other syntax error
    @throw PARSE-XMLRPC-ERROR syntax error parsing XML-RPC string
    @par Example:
    @code my hash $h = parseXMLRPCResponse($xml); @endcode
    @see @ref XMLRPC
 */
//# hash parseXMLRPCResponse(string $xml, *string $encoding) {}
static AbstractQoreNode *f_parseXMLRPCResponse(const QoreListNode *params, ExceptionSink *xsink) {
   HARD_QORE_PARAM(p0, const QoreStringNode, params, 0);

   const QoreEncoding *ccsid = get_encoding_param(params, 1);
   printd(5, "parseXMLRPCCall(%s)\n", p0->getBuffer());

   return parseXMLRPCResponse(p0, ccsid, xsink);
}

// NOTE: the libxml2 library requires all input to be in UTF-8 encoding
// syntax: parseXML(xml_string, xsd_string [, output encoding])
static AbstractQoreNode *parseXMLWithSchemaIntern(bool as_data, const QoreListNode *params, ExceptionSink *xsink) {
#ifdef HAVE_XMLTEXTREADERSETSCHEMA
   HARD_QORE_PARAM(p0, const QoreStringNode, params, 0);
   HARD_QORE_PARAM(p1, const QoreStringNode, params, 1);

   const QoreEncoding *ccsid = get_encoding_param(params, 2);

   printd(5, "parseXMLWithSchema() xml=%s\n xsd=%s\n", p0->getBuffer(), p1->getBuffer());

   // convert to UTF-8 
   TempEncodingHelper str(p0, QCS_UTF8, xsink);
   if (!str)
      return 0;

   TempEncodingHelper xsd(p1, QCS_UTF8, xsink);
   if (!xsd)
      return 0;

   QoreXmlSchemaContext schema(xsd->getBuffer(), xsd->strlen(), xsink);
   if (!schema) {
      if (!*xsink)
	 xsink->raiseException("XML-SCHEMA-ERROR", "XML schema passed as second argument to parseXMLWithSchema() could not be parsed");
      return 0;
   }

   QoreXmlReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
   if (!reader)
      return 0;

   int rc = reader.setSchema(schema.getSchema());
   if (rc < 0) {
      if (!*xsink)
	 xsink->raiseException("XSD-VALIDATION-ERROR", "XML schema passed as second argument to parseXMLWithSchema() could not be validated");      
      return 0;
   }

   return reader.parseXMLData(ccsid, as_data, xsink);
#else
   xsink->raiseException("MISSING-FEATURE-ERROR", "the libxml2 version used to compile the qore library did not support the xmlTextReaderSetSchema() function, therefore parseXMLWithSchema() and parseXMLAsDataWithSchema() are not available in Qore; for maximum portability, use the constant Option::HAVE_PARSEXMLWITHSCHEMA to check if this function is implemented before calling");
   return 0;
#endif
}

// NOTE: the libxml2 library requires all input to be in UTF-8 encoding
// syntax: parseXMLWithRelaxNGIntern(xml_string, xsd_string [, output encoding])
static AbstractQoreNode *parseXMLWithRelaxNGIntern(bool as_data, const QoreListNode *params, ExceptionSink *xsink) {
#ifdef HAVE_XMLTEXTREADERRELAXNGSETSCHEMA
   HARD_QORE_PARAM(p0, const QoreStringNode, params, 0);
   HARD_QORE_PARAM(p1, const QoreStringNode, params, 1);

   const QoreEncoding *ccsid = get_encoding_param(params, 2);

   printd(5, "parseXMLWithRelaxNG() xml=%s\n xsd=%s\n", p0->getBuffer(), p1->getBuffer());

   // convert to UTF-8 
   TempEncodingHelper str(p0, QCS_UTF8, xsink);
   if (!str)
      return 0;

   TempEncodingHelper rng(p1, QCS_UTF8, xsink);
   if (!rng)
      return 0;

   QoreXmlRelaxNGContext schema(rng->getBuffer(), rng->strlen(), xsink);
   if (!schema) {
      if (!*xsink)
	 xsink->raiseException("XML-RELAXNG-ERROR", "RelaxNG schema passed as second argument to parseXMLWithRelaxNG() could not be parsed");
      return 0;
   }

   QoreXmlReader reader(*str, QORE_XML_PARSER_OPTIONS, xsink);
   if (!reader)
      return 0;

   int rc = reader.setRelaxNG(schema.getSchema());
   if (rc < 0) {
      if (!*xsink)
	 xsink->raiseException("RELAXNG-VALIDATION-ERROR", "RelaxNG schema passed as second argument to parseXMLWithRelaxNG() could not be validated");      
      return 0;
   }

   return reader.parseXMLData(ccsid, as_data, xsink);
#else
   xsink->raiseException("MISSING-FEATURE-ERROR", "the libxml2 version used to compile the qore library did not support the xmlTextReaderSetRelaxNG() function, therefore parseXMLWithRelaxNG() and parseXMLAsDataWithRelaxNG() are not available in Qore; for maximum portability, use the constant Option::HAVE_PARSEXMLWITHRELAXNG to check if this function is implemented before calling");
   return 0;
#endif
}

//! Parses an XML string, validates the XML string against an XSD schema string, and returns a Qore hash structure
/** If any errors occur parsing the XSD string, parsing the XML string, or validating the XML against the XSD, exceptions are thrown. If no encoding string argument is passed, then all strings in the resulting hash will be in UTF-8 encoding regardless of the input encoding of the XML string.

    If duplicate, out-of-order XML elements are found in the input string, they are deserialized to Qore hash elements with the same name as the XML element but including a caret \c '^' and a numeric prefix to maintain the same key order in the Qore hash as in the input XML string.

    This function should only be used when it is important to maintain the XML element order in the resulting Qore data structure (for example, when the data must be re-serialized to an XML string and the element order within a subelement must be maintained), for example, when parsing and reserializing an OSX property list in XML format.  Otherwise parseXMLAsDataWithSchema() should be used instead.

    The availability of this function depends on the presence of libxml2's \c xmlTextReaderSetSchema() function when Qore was compiled; for maximum portability check the constant @ref optionconstants "HAVE_PARSEXMLWITHSCHEMA" before running this function.

    @param $xml the XML string to parse
    @param $xsd the XSD schema string to use to validate the XML string
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have UTF-8 encoding

    @return a Qore hash structure corresponding to the input

    @throw PARSE-XML-EXCEPTION error parsing the XML string
    @throw XML-SCHEMA-ERROR invalid XSD string
    @throw XSD-VALIDATION-ERROR the XML did not pass schema validation
    @throw MISSING-FEATURE-ERROR this exception is thrown when the function is not available; for maximum portability, check the constant @ref optionconstants "HAVE_PARSEXMLWITHSCHEMA" before calling this function

    @par Example:
    @code my hash $h = parseXMLWithSchema($xml, $xsd); @endcode

    @see parseXMLAsDataWithSchema(), parseXMLWithRelaxNG(), parseXMLAsDataWithRelaxNG()
*/
//# hash parseXMLWithSchema(string $xml, string $xsd, *string $encoding) {}
static AbstractQoreNode *f_parseXMLWithSchema(const QoreListNode *params, ExceptionSink *xsink) {
   return parseXMLWithSchemaIntern(false, params, xsink);
}

//! Parses an XML string as data (does not preserve hash order with out-of-order duplicate keys: collapses all to the same list), validates the XML string against an XSD schema string, and returns a Qore hash structure
/** If any errors occur parsing the XSD string, parsing the XML string, or validating the XML against the XSD, exceptions are thrown. If no encoding string argument is passed, then all strings in the resulting hash will be in UTF-8 encoding regardless of the input encoding of the XML string.

    Please note that data deserialized with this function may not be reserialized to an identical XML string to the input due to the fact that duplicate, out-of-order XML elements are collapsed into lists in the resulting Qore hash, thereby losing the order in the original XML string.

    For a similar function preserving the order of keys in the XML in the resulting Qore hash by generating Qore hash element names with numeric suffixes, see parseXMLWithSchema().

    If any errors occur parsing the XSD string, parsing the XML string, or validating the XML against the XSD, exceptions are thrown.  If no encoding string argument is passed, then all strings in the resulting hash will be in UTF-8 encoding regardless of the input encoding of the XML string.

    The availability of this function depends on the presence of libxml2's \c xmlTextReaderSetSchema() function when Qore was compiled; for maximum portability check the constant @ref optionconstants "HAVE_PARSEXMLWITHSCHEMA" before running this function.

    @param $xml the XML string to parse
    @param $xsd the XSD schema string to use to validate the XML string
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have UTF-8 encoding

    @return a Qore hash structure corresponding to the input

    @throw PARSE-XML-EXCEPTION error parsing the XML string
    @throw XML-SCHEMA-ERROR invalid XSD string
    @throw XSD-VALIDATION-ERROR the XML did not pass schema validation
    @throw MISSING-FEATURE-ERROR this exception is thrown when the function is not available; for maximum portability, check the constant @ref optionconstants "HAVE_PARSEXMLWITHSCHEMA" before calling this function

    @par Example:
    @code my hash $h = parseXMLAsDataWithSchema($xml, $xsd); @endcode

    @see parseXMLWithSchema(), parseXMLWithRelaxNG(), parseXMLAsDataWithRelaxNG()
*/
//# hash parseXMLAsDataWithSchema(string $xml, string $xsd, *string $encoding) {}
static AbstractQoreNode *f_parseXMLAsDataWithSchema(const QoreListNode *params, ExceptionSink *xsink) {
   return parseXMLWithSchemaIntern(true, params, xsink);
}

//! Parses an XML string, validates the XML string against a RelaxNG schema string, and returns a Qore hash structure
/** If any errors occur parsing the RelaxNG string, parsing the XML string, or validating the XML against the RelaxNG schema, exceptions are thrown. If no encoding string argument is passed, then all strings in the resulting hash will be in UTF-8 encoding regardless of the input encoding of the XML string.

    If duplicate, out-of-order XML elements are found in the input string, they are deserialized to Qore hash elements with the same name as the XML element but including a caret "^" and a numeric prefix to maintain the same key order in the Qore hash as in the input XML string.

    This function should only be used when it is important to maintain the XML element order in the resulting Qore data structure (for example, when the data must be re-serialized to an XML string and the element order within a subelement must be maintained), for example, when parsing and reserializing an OSX property list in XML format.  Otherwise parseXMLAsDataWithRelaxNG() should be used instead.

    The availability of this function depends on the presence of libxml2's \c xmlTextReaderRelaxNGSetSchema() function when Qore was compiled; for maximum portability check the constant @ref optionconstants "HAVE_PARSEXMLWITHRELAXNG" before running this function.

    @param $xml the XML string to parse
    @param $relaxng the RelaxNG schema string to use to validate the XML string
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have UTF-8 encoding

    @return a Qore hash structure corresponding to the input

    @throw PARSE-XML-EXCEPTION error parsing the XML string
    @throw XML-RELAXNG-ERROR invalid RelaxNG string
    @throw RELAXNG-VALIDATION-ERROR the XML did not pass RelaxNG schema validation
    @throw MISSING-FEATURE-ERROR this exception is thrown when the function is not available; for maximum portability, check the constant @ref optionconstants "HAVE_PARSEXMLWITHRELAXNG" before calling this function

    @par Example:
    @code my hash $h = parseXMLWithRelaxNG($xml, $relaxng); @endcode

    @see parseXMLWithSchema(), parseXMLAsDataWithSchema(), parseXMLAsDataWithRelaxNG()
*/
//# hash parseXMLWithRelaxNG(string $xml, string $relaxng, *string $encoding) {}
static AbstractQoreNode *f_parseXMLWithRelaxNG(const QoreListNode *params, ExceptionSink *xsink) {
   return parseXMLWithRelaxNGIntern(false, params, xsink);
}

//! Parses an XML string as data (does not preserve hash order with out-of-order duplicate keys: collapses all to the same list), validates the XML string against a RelaxNG schema string, and returns a Qore hash structure
/** If any errors occur parsing the RelaxNG schema string, parsing the XML string, or validating the XML against the XSD, exceptions are thrown. If no encoding string argument is passed, then all strings in the resulting hash will be in UTF-8 encoding regardless of the input encoding of the XML string.

    Please note that data deserialized with this function may not be reserialized to an identical XML string to the input due to the fact that duplicate, out-of-order XML elements are collapsed into lists in the resulting Qore hash, thereby losing the order in the original XML string.

    For a similar function preserving the order of keys in the XML in the resulting Qore hash by generating Qore hash element names with numeric suffixes, see parseXMLWithRelaxNG().

    The availability of this function depends on the presence of libxml2's \c xmlTextReaderRelaxNGSetSchema() function when Qore was compiled; for maximum portability check the constant @ref optionconstants "HAVE_PARSEXMLWITHRELAXNG" before running this function.

    @param $xml the XML string to parse
    @param $relaxng the RelaxNG schema string to use to validate the XML string
    @param $encoding an optional string giving the string encoding of any strings output; if this parameter is missing, the any strings output in the output hash will have UTF-8 encoding

    @return a Qore hash structure corresponding to the input

    @throw PARSE-XML-EXCEPTION error parsing the XML string
    @throw XML-RELAXNG-ERROR invalid RelaxNG string
    @throw RELAXNG-VALIDATION-ERROR the XML did not pass RelaxNG schema validation
    @throw MISSING-FEATURE-ERROR this exception is thrown when the function is not available; for maximum portability, check the constant @ref optionconstants "HAVE_PARSEXMLWITHRELAXNG" before calling this function

    @par Example:
    @code my hash $h = parseXMLAsDataWithRelaxNG($xml, $relaxng); @endcode

    @see parseXMLWithSchema(), parseXMLAsDataWithSchema(), parseXMLWithRelaxNG()
 */
//# hash parseXMLAsDataWithRelaxNG(string $xml, string $relaxng, *string $encoding) {}
static AbstractQoreNode *f_parseXMLAsDataWithRelaxNG(const QoreListNode *params, ExceptionSink *xsink) {
   return parseXMLWithRelaxNGIntern(true, params, xsink);
}

// this function does nothing - it's here for backwards-compatibility for functions
// that accept invalid arguments and return nothing
AbstractQoreNode *f_noop(const QoreListNode *args, ExceptionSink *xsink) {
   return 0;
}

void init_xml_functions() {
   builtinFunctions.add2("parseXML",                                           f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("parseXML",                                           f_parseXML, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXML",                                           f_parseXML, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLAsData",                                     f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("parseXMLAsData",                                     f_parseXMLAsData, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLAsData",                                     f_parseXMLAsData, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLWithSchema",                                 f_parseXMLWithSchema, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLWithSchema",                                 f_parseXMLWithSchema, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLWithRelaxNG",                                f_parseXMLWithRelaxNG, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLWithRelaxNG",                                f_parseXMLWithRelaxNG, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLAsDataWithSchema",                           f_parseXMLAsDataWithSchema, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLAsDataWithSchema",                           f_parseXMLAsDataWithSchema, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLAsDataWithRelaxNG",                          f_parseXMLAsDataWithRelaxNG, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLAsDataWithRelaxNG",                          f_parseXMLAsDataWithRelaxNG, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLRPCValue",                                   f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("parseXMLRPCValue",                                   f_parseXMLRPCValue, QC_RET_VALUE_ONLY, QDOM_DEFAULT, anyTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLRPCValue",                                   f_parseXMLRPCValue, QC_RET_VALUE_ONLY, QDOM_DEFAULT, anyTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);


   builtinFunctions.add2("parseXMLRPCCall",                                    f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("parseXMLRPCCall",                                    f_parseXMLRPCCall, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLRPCCall",                                    f_parseXMLRPCCall, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("parseXMLRPCResponse",                                f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("parseXMLRPCResponse",                                f_parseXMLRPCResponse, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("parseXMLRPCResponse",                                f_parseXMLRPCResponse, QC_RET_VALUE_ONLY, QDOM_DEFAULT, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeFormattedXMLString",                             f_makeFormattedXMLString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 1, hashTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLString",                             f_makeFormattedXMLString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, hashTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLString",                             f_makeFormattedXMLString_str, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLString",                             f_makeFormattedXMLString_str, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeFormattedXMLFragment",                           f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("makeFormattedXMLFragment",                           f_makeFormattedXMLFragment, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 1, hashTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLFragment",                           f_makeFormattedXMLFragment, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, hashTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeXMLString",                                      f_makeXMLString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 1, hashTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeXMLString",                                      f_makeXMLString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, hashTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeXMLString",                                      f_makeXMLString_str, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeXMLString",                                      f_makeXMLString_str, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, hashTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeXMLFragment",                                    f_noop, QC_RUNTIME_NOOP, QDOM_DEFAULT, nothingTypeInfo);
   builtinFunctions.add2("makeXMLFragment",                                    f_makeXMLFragment, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 1, hashTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeXMLFragment",                                    f_makeXMLFragment, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, hashTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeXMLRPCCallString",                               f_makeXMLRPCCallString, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeXMLRPCCallStringWithEncoding",                   f_makeXMLRPCCallStringWithEncoding, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   // makeXMLRPCCallStringArgs(string $method, any $args)
   builtinFunctions.add2("makeXMLRPCCallStringArgs",                           f_makeXMLRPCCallStringArgs, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG);
   // makeXMLRPCCallStringArgsWithEncoding(string $encoding, string $method, any $args)
   builtinFunctions.add2("makeXMLRPCCallStringArgsWithEncoding",               f_makeXMLRPCCallStringArgsWithEncoding, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG);

   // returns *string (nothing if no args passed)
   builtinFunctions.add2("makeXMLRPCResponseString",                           f_makeXMLRPCResponseString, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo);
   builtinFunctions.add2("makeXMLRPCResponseStringWithEncoding",               f_makeXMLRPCResponseStringWithEncoding, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeXMLRPCFaultResponseString",                      f_makeXMLRPCFaultResponseString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, softBigIntTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeXMLRPCFaultResponseStringWithEncoding",          f_makeXMLRPCFaultResponseStringWithEncoding, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, softBigIntTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   // *string makeXMLRPCValueString(any $any)  
   builtinFunctions.add2("makeXMLRPCValueString",                              f_makeXMLRPCValueString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo, 1, anyTypeInfo, QORE_PARAM_NO_ARG);
   // *string makeXMLRPCValueString(any $any, string $encoding)  
   builtinFunctions.add2("makeXMLRPCValueString",                              f_makeXMLRPCValueString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo, 2, anyTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeFormattedXMLRPCCallString",                      f_makeFormattedXMLRPCCallString, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLRPCCallStringWithEncoding",          f_makeFormattedXMLRPCCallStringWithEncoding, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeFormattedXMLRPCCallStringArgs",                  f_makeFormattedXMLRPCCallStringArgs, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLRPCCallStringArgsWithEncoding",      f_makeFormattedXMLRPCCallStringArgsWithEncoding, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeFormattedXMLRPCResponseString",                  f_makeFormattedXMLRPCResponseString, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY);
   builtinFunctions.add2("makeFormattedXMLRPCResponseStringWithEncoding",      f_makeFormattedXMLRPCResponseStringWithEncoding, QC_USES_EXTRA_ARGS | QC_RET_VALUE_ONLY, QDOM_DEFAULT, 0, 1, stringTypeInfo, QORE_PARAM_NO_ARG);

   builtinFunctions.add2("makeFormattedXMLRPCFaultResponseString",             f_makeFormattedXMLRPCFaultResponseString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 2, softBigIntTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
   builtinFunctions.add2("makeFormattedXMLRPCFaultResponseStringWithEncoding", f_makeFormattedXMLRPCFaultResponseStringWithEncoding, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, softBigIntTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);

   // *string makeFormattedXMLRPCValueString(any $any)  
   builtinFunctions.add2("makeFormattedXMLRPCValueString",                     f_makeFormattedXMLRPCValueString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo, 1, anyTypeInfo, QORE_PARAM_NO_ARG);

   // *string makeFormattedXMLRPCValueString(any $any, string $encoding)  
   builtinFunctions.add2("makeFormattedXMLRPCValueString",                     f_makeFormattedXMLRPCValueString, QC_RET_VALUE_ONLY, QDOM_DEFAULT, stringOrNothingTypeInfo, 2, anyTypeInfo, QORE_PARAM_NO_ARG, stringTypeInfo, QORE_PARAM_NO_ARG);
}
