/* -*- indent-tabs-mode: nil -*- */
/*
    QoreXmlRpcReader.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2018 Qore Technologies, s.r.o.

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

#include "QoreXmlRpcReader.h"

#include "ql_xml.h"

static int xmlrpc_do_empty_value(Qore::Xml::intern::XmlRpcValue *v, const char* name, int depth, ExceptionSink* xsink) {
   if (!strcmp(name, "string"))
      v->set(null_string());
   else if (!strcmp(name, "i4") || !strcmp(name, "int") || !strcmp(name, "ex:i1") || !strcmp(name, "ex:i2") || !strcmp(name, "ex:i8"))
      v->set(0ll);
   else if (!strcmp(name, "boolean"))
      v->set(false);
   else if (!strcmp(name, "struct"))
      v->set(new QoreHashNode);
   else if (!strcmp(name, "array"))
      v->set(new QoreListNode);
   else if (!strcmp(name, "double") || !strcmp(name, "ex:float"))
      v->set(0.0f);
   else if (!strcmp(name, "dateTime.iso8601") || !strcmp(name, "ex:dateTime"))
      v->set(zero_date());
   else if (!strcmp(name, "base64"))
      v->set(new BinaryNode);
   else if (!strcmp(name, "ex:nil"))
      v->set(reinterpret_cast<AbstractQoreNode*>(0));
   else {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "unknown XML-RPC type '%s' at level %d", name, depth);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getStruct(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink) {
   int nt;

   QoreHashNode* h = new QoreHashNode;
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

      const char* member_name = constValue();
      if (!member_name) {
         xsink->raiseException("PARSE-XMLRPC-ERROR", "empty member name in struct");
         return -1;
      }

      QoreString member(member_name);
      //printd(5, "QoreXmlRpcReader::getStruct() DEBUG: got member name '%s'\n", member_name);

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

      v->setReference(h->getKeyValueReference(member.getBuffer()));

      // if if was not an empty value element
      if (member_depth < depth()) {
         // check for close value tag
         if ((nt = readXmlRpcNode(xsink)) == -1)
            return -1;
         if (nt != XML_READER_TYPE_END_ELEMENT) {
            //printd(5, "QoreXmlRpcReader::getStruct() struct member='%s', parsing value node\n", member.getBuffer());

            if (getValueData(v, data_ccsid, true, xsink))
               return -1;

            //printd(5, "QoreXmlRpcReader::getStruct() struct member='%s', finished parsing value node\n", member.getBuffer());

            if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
               //printd(5, "EXCEPTION close /value: %d: %s\n", nt, (char*)constName());
               xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting value close element");
               return -1;
            }
            //printd(5, "QoreXmlRpcReader::getStruct() close /value: %s\n", (char*)constName());
         }
         if (readXmlRpc(xsink))
            return -1;
      }

      if ((nt = readXmlRpcNode(xsink)) == -1)
         return -1;
      if (nt != XML_READER_TYPE_END_ELEMENT) {
         //printd(5, "QoreXmlRpcReader::getStruct() error nt: %d\n", nt);
         xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting member close element");
         return -1;
      }
      //printd(5, "QoreXmlRpcReader::getStruct() close /member: %s\n", (char*)constName());

      if (readXmlRpc(xsink))
         return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getParams(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink) {
   int nt;
   int index = 0;

   QoreListNode* l = new QoreListNode;
   v->set(l);

   int array_depth = depth();

   while (true) {
      // expecting param open element
      if ((nt = readXmlRpcNode(xsink)) == -1)
         return -1;

      //printd(5, "getParams() nt: %d name: %s\n", nt, constName());

      // if higher-level "params" element closed, then return
      if (nt == XML_READER_TYPE_END_ELEMENT)
         return 0;

      if (nt != XML_READER_TYPE_ELEMENT) {
         xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string, expecting 'param' open element");
         return -1;
      }

      if (checkXmlRpcMemberName("param", xsink))
         return -1;

      v->setReference(l->getEntryReference(index++));

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

            //printd(5, "just read <value>, now value_depth: %d, depth: %d\n", value_depth, depth());

            // if this was <value/>, then skip
            if (value_depth <= depth()) {
               if ((nt = readXmlRpcNode(xsink)) == -1)
                  return -1;

               // if ! </value>
               if (nt != XML_READER_TYPE_END_ELEMENT) {
                  if (getValueData(v, data_ccsid, true, xsink))
                     return -1;

                  // position on </value> close tag
                  if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
                     xsink->raiseException("PARSE-XMLRPC-ERROR", "extra data in params, expecting value close tag");
                     return -1;
                  }

                  if (checkXmlRpcMemberName("value", xsink, true))
                     return -1;
               }

               // get param close tag
               if (readXmlRpc(xsink))
                  return -1;
            }

            if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
               xsink->raiseException("PARSE-XMLRPC-ERROR", "extra data in params, expecting param close tag (got node type %s instead)", get_xml_node_type_name(nt));
               return -1;
            }

            if (checkXmlRpcMemberName("param", xsink, true))
               return -1;
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

int QoreXmlRpcReader::getString(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink) {
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

   QoreStringNode* qstr = getValue(data_ccsid, xsink);
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

int QoreXmlRpcReader::getBoolean(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char* str = constValue();
      if (str) {
         //printd(5, "** got boolean '%s'\n", str);
         v->set(strtoll(str, 0, 10) ? true : false);
      }

      if (readXmlRpc(xsink))
         return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
         return -1;
   }
   else
      v->set(false);

   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in boolean (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getInt(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char* str = constValue();
      if (str) {
         //printd(5, "** got int '%s'\n", str);
         // note that we can parse 64-bit integers here, which is not conformant to the standard
         v->set(strtoll(str, 0, 10));
      }

      if (readXmlRpc(xsink))
         return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
         return -1;
   }
   else
      v->set(0ll);

   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in int (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getDouble(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char* str = constValue();
      if (str) {
         //printd(5, "** got float '%s'\n", str);
         v->set(strtod(str, 0));
      }

      // advance to next position
      if (readXmlRpc(xsink))
         return -1;

      if ((nt = readXmlRpcNode(xsink)) == -1)
         return -1;
   }
   else
      v->set(0.0f);

   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in float (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getDate(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char* str = constValue();
      //printd(5, "QoreXmlRpcReader::getDate() str: %p (%s)\n", str, str ? str : "(null)");
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
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in date (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getBase64(Qore::Xml::intern::XmlRpcValue *v, ExceptionSink* xsink) {
   int nt;

   if ((nt = readXmlRpcNode(xsink)) == -1)
      return -1;

   if (nt == XML_READER_TYPE_TEXT) {
      const char* str = constValue();
      if (str) {
         //printd(5, "** got base64 '%s'\n", str);
         BinaryNode* b = parseBase64(str, strlen(str), xsink);
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
      v->set(new BinaryNode);

   if (nt != XML_READER_TYPE_END_ELEMENT) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "extra information in base64 (%d)", nt);
      return -1;
   }
   return 0;
}

int QoreXmlRpcReader::getArray(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, ExceptionSink* xsink) {
   int nt;
   int index = 0;

   QoreListNode* l = new QoreListNode;
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

   //printd(5, "getArray() level: %d before str: %s\n", depth(), (char*)constName());

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

         v->setReference(l->getEntryReference(index++));

         if (readXmlRpc(xsink))
            return -1;

         //printd(5, "DEBUG: vd: %d, d: %d\n", value_depth, depth());

         // if this was <value/>, then skip
         if (value_depth < depth()) {
            if ((nt = readXmlRpcNode(xsink)) == -1)
               return -1;

            if (nt == XML_READER_TYPE_END_ELEMENT)
               v->set(reinterpret_cast<AbstractQoreNode*>(0));
            else {
               if (getValueData(v, data_ccsid, true, xsink))
                  return -1;

               //printd(5, "after getValueData() nt: %d name: %s\n", nt, constName());

               // check for </value> close tag
               if ((nt = nodeTypeSkipWhitespace()) != XML_READER_TYPE_END_ELEMENT) {
                  //printd(5, "nt: %d name: %s\n", nt, constName());
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

   //printd(5, "vd: %d ad: %d\n", value_depth, array_depth);

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

int QoreXmlRpcReader::getValueData(Qore::Xml::intern::XmlRpcValue *v, const QoreEncoding* data_ccsid, bool read_next, ExceptionSink* xsink) {
   int nt = nodeTypeSkipWhitespace();
   if (nt == -1) {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string");
      return -1;
   }

   //printd(5, "QoreXmlRpcReader::getValueData() DEBUG nt: %d read_next: %d\n", nt, read_next);

   if (nt == XML_READER_TYPE_ELEMENT) {
      int depth = QoreXmlReader::depth();

      // get xmlrpc type name
      const char* name = constName();
      if (!name) {
         xsink->raiseException("PARSE-XMLRPC-ERROR", "expecting type name, got NOTHING at level %d", depth);
         return -1;
      }

      //printd(5, "QoreXmlRpcReader::getValueData() DEBUG parsing type '%s'\n", name);

      int rc = readSkipWhitespace();
      if (rc != 1) {
         if (!read_next)
            return xmlrpc_do_empty_value(v, name, depth, xsink);

         xsink->raiseException("PARSE-XMLRPC-ERROR", "error parsing XML string");
         return -1;
      }

      //printd(5, "QoreXmlRpcReader::getValueData() old depth: %d new depth: %d element='%s' nt: %d\n", depth, QoreXmlReader::depth(), name, nodeType());

      // if this was an empty element, assign an empty value
      if (depth == QoreXmlReader::depth()) {
         int rc = xmlrpc_do_empty_value(v, name, depth, xsink);
         return !rc && read_next ? readXmlRpc(xsink) : rc;
      }

      if (depth > QoreXmlReader::depth())
         return xmlrpc_do_empty_value(v, name, depth, xsink);

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

      //printd(5, "getValueData() finished parsing type '%s' element depth: %d\n", name, depth);
      if (xsink->isEvent())
         return -1;
   }
   else if (nt == XML_READER_TYPE_TEXT) { // without type defaults to string
      QoreStringNode* qstr = getValue(data_ccsid, xsink);
      if (!qstr)
         return -1;
      v->set(qstr);
   }
   else {
      xsink->raiseException("PARSE-XMLRPC-ERROR", "unable to parse XML-RPC string; expecting element node, got type %d instead", nt);
      return -1;
   }

   return read_next ? readXmlRpc(xsink) : 0;
}
