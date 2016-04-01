#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# require the xml module
%requires xml

# require all global variables to be declared with "our"
%require-our
# enable all warnings
%enable-all-warnings
# child programs do not inherit parent's restrictions
%no-child-restrictions
# require types to be declared
%require-types
# do not use $ for vars
%new-style

# test deprecated functions as well
%disable-warning deprecated

# make sure we have the right version of qore
%requires qore >= 0.8.12

%requires QUnit
%requires Util

%exec-class XmlTest

class XmlTest inherits QUnit::Test {
    public {
        const Xsd = '<?xml version="1.0" encoding="utf-8"?>
<xsd:schema targetNamespace="http://qoretechnologies.com/test/namespace" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <xsd:element name="TestElement">
    <xsd:complexType>
      <xsd:simpleContent>
        <xsd:extension base="xsd:string"/>
      </xsd:simpleContent>
    </xsd:complexType>
  </xsd:element>
</xsd:schema>
';

        const Hash = (
            "test": 1,
            "gee": "philly",
            "marguile": 1.0392,
            "list": (1, 2, 3, ("four": 4), 5),
            "hash": ("howdy": 123, "partner": 456),
            "list^1": "test",
            "bool": True,
            "time": now(),
            "bool^1": False,
            #"emptyhash": {},
            #"emptylist": (),
            "key": "this & that",
            );

        const Str = "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<file>
  <record id=\"1\"><name>test1</name></record>
  <record id=\"2\"><name>test2</name></record>
</file>
";

        const Rec = (
            "^attributes^": ("id": "1"),
            "name": "test1",
            );
    }

    constructor() : QUnit::Test("XmlTest", "1.0") {
        addTestCase("XmlTestCase", \xmlTestCase());
        addTestCase("DeprecatedXmlTestCase", \deprecatedXmlTestCase());
        addTestCase("FileSaxIteratorTestCase", \fileSaxIteratorTestCase());
        set_return_value(main());
    }

    xmlTestCase() {
        hash o = Hash;
        hash mo = ("o": o);
        string str = make_xml("o", o);
        assertEq(True, mo.o == parse_xml(str, XPF_PRESERVE_ORDER).o, "first parse_xml()");
        str = make_xml("o", o, XGF_ADD_FORMATTING);
        assertEq(True, mo == parse_xml(str, XPF_PRESERVE_ORDER), "second parse_xml()");

        list params = (1, True, "string", NOTHING, o + ("emptylist": (), "emptyhash": hash()));
        str = make_xmlrpc_call("test.method", params, XGF_ADD_FORMATTING);
        hash result = ( "methodName" : "test.method", "params" : params );
        assertEq(result, parse_xmlrpc_call(str), "make_xmlrpc_call() and parse_xmlrpc_call() 1");
        str = make_xmlrpc_call("test.method", params);
        assertEq(result, parse_xmlrpc_call(str), "make_xmlrpc_call() and parse_xmlrpc_call() 2");
        str = make_xmlrpc_call("test.method", (True, o));
        result = ( "methodName" : "test.method","params" : (True, o) );
        assertEq(result, parse_xmlrpc_call(str), "make_xmlrpc_call() and parse_xmlrpc_call() 3");
        str = make_xmlrpc_call("test.method", (True, o), XGF_ADD_FORMATTING);
        assertEq(result, parse_xmlrpc_call(str), "make_xmlrpc_call() and parse_xmlrpc_call() 4");

        str = make_xmlrpc_response(o);
        assertEq(("params": o), parse_xmlrpc_response(str), "make_xmlrpc_response() and parse_xmlrpc_response() 1");
        str = make_xmlrpc_response(o, XGF_ADD_FORMATTING);
        assertEq(("params": o), parse_xmlrpc_response(str), "make_xmlrpc_response() and parse_xmlrpc_response() 2");
        str = make_xmlrpc_fault(100, "error");
        hash fr = ( "fault" : ( "faultCode" : 100, "faultString" : "error" ) );
        assertEq(fr, parse_xmlrpc_response(str), "make_xmlrpc_fault() and parse_xmlrpc_response() 1");
        str = make_xmlrpc_fault(100, "error", XGF_ADD_FORMATTING);
        assertEq(fr, parse_xmlrpc_response(str), "make_xmlrpc_fault() and parse_xmlrpc_response() 2");
        o = ( "xml" : (o + ( "^cdata^" : "this string contains special characters &<> etc" )) );
        assertEq(True, o == parse_xml(make_xml(o), XPF_PRESERVE_ORDER), "xml serialization with cdata");

        if (Option::HAVE_PARSEXMLWITHSCHEMA) {
            o = ( "ns:TestElement" : ( "^attributes^" : ( "xmlns:ns" : "http://qoretechnologies.com/test/namespace" ), "^value^" : "testing" ) );

            assertEq(o, parse_xml_with_schema(make_xml(o), Xsd), "parse_xml_with_schema()");
        }

        str = make_xml(mo);
        XmlDoc xd(str);
        assertEq(True, xd.toQore() == mo, "XmlDoc::constructor(<string>), XmlDoc::toQore()");
        assertEq(True, parse_xml(xd.toString(), XPF_PRESERVE_ORDER) == mo, "XmlDoc::toString()");
        XmlNode n = xd.evalXPath("//list[2]")[0];
        assertEq("2", n.getContent(), "XmlDoc::evalXPath()");
        assertEq("XML_ELEMENT_NODE", n.getElementTypeName(), "XmlNode::getElementTypeName()");
        n = xd.getRootElement().firstElementChild();
        assertEq("test", n.getName(), "XmlDoc::geRootElement(), XmlNode::firstElementChild(), XmlNode::getName()");
        n = xd.getRootElement().lastElementChild();
        assertEq("key", n.getName(), "XmlNode::lastElementChild()");
        assertEq("bool", n.previousElementSibling().getName(), "XmlNode::previousElementSibling()");
        assertEq(14, xd.getRootElement().childElementCount(), "XmlNode::childElementCount()");

        xd = new XmlDoc(mo);
        assertEq(True, xd.toQore() == mo, "XmlDoc::constructor(<hash>), XmlDoc::toQore()");

        XmlReader xr = new XmlReader(xd);
        # move to first element
        xr.read();
        assertEq(Xml::XML_NODE_TYPE_ELEMENT, xr.nodeType(), "XmlReader::read(), XmlReader::Type()");
        assertEq(True, xr.toQore() == mo.o, "XmlReader::toQoreData()");
    }

    deprecatedXmlTestCase() {
        hash o = Hash;
        hash mo = ("o": o);
        string str = makeXMLString("o", o);
        assertEq(True, mo.o == parseXML(str).o, "first parseXML()");
        str = makeFormattedXMLString("o", o);
        assertEq(True, mo == parseXML(str), "second parseXML()");
        list params = (1, True, "string", NOTHING, o + ("emptylist": (), "emptyhash": hash()));
        str = makeFormattedXMLRPCCallStringArgs("test.method", params);
        hash result = ( "methodName" : "test.method", "params" : params );
        assertEq(result, parseXMLRPCCall(str), "makeXMLRPCCallStringArgs() and parseXMLRPCCall()");
        str = makeFormattedXMLRPCCallStringArgs("test.method", params);
        assertEq(result, parseXMLRPCCall(str), "makeFormattedXMLRPCCallStringArgs() and parseXMLRPCCall()");
        str = makeXMLRPCCallString("test.method", True, o);
        result = ( "methodName" : "test.method","params" : (True, o) );
        assertEq(result, parseXMLRPCCall(str), "makeXMLRPCCallString() and parseXMLRPCCall()");
        str = makeFormattedXMLRPCCallString("test.method", True, o);
        assertEq(result, parseXMLRPCCall(str), "makeFormattedXMLRPCCallString() and parseXMLRPCCall()");

        str = makeXMLRPCResponseString(o);
        assertEq(("params": o), parseXMLRPCResponse(str), "first makeXMLRPCResponse() and parseXMLRPCResponse()");
        str = makeFormattedXMLRPCResponseString(o);
        assertEq(("params": o), parseXMLRPCResponse(str), "first makeFormattedXMLRPCResponse() and parseXMLRPCResponse()");
        str = makeXMLRPCFaultResponseString(100, "error");
        hash fr = ( "fault" : ( "faultCode" : 100, "faultString" : "error" ) );
        assertEq(fr, parseXMLRPCResponse(str), "second makeXMLRPCResponse() and parseXMLRPCResponse()");
        str = makeFormattedXMLRPCFaultResponseString(100, "error");
        assertEq(fr, parseXMLRPCResponse(str), "second makeXMLRPCResponse() and parseXMLRPCResponse()");
        o = ( "xml" : (o + ( "^cdata^" : "this string contains special characters &<> etc" )) );
        assertEq(True, o == parseXML(makeXMLString(o)), "xml serialization with cdata");

        if (Option::HAVE_PARSEXMLWITHSCHEMA) {
            o = ( "ns:TestElement" : ( "^attributes^" : ( "xmlns:ns" : "http://qoretechnologies.com/test/namespace" ), "^value^" : "testing" ) );

            assertEq(o, parseXMLWithSchema(makeXMLString(o), Xsd), "parseXMLWithSchema()");
        }
    }

    fileSaxIteratorTestCase() {
        string fn = sprintf("%s%s%s.xml", tmp_location(), DirSep, get_random_string());
        File f();
        f.open(fn, O_CREAT | O_WRONLY | O_TRUNC);
        f.write(Str);
        on_exit
            unlink(fn);
        FileSaxIterator i(fn, "record");
        assertEq(True, i.next());
        assertEq(Rec, i.getValue());
    }
}
