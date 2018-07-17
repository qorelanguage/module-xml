/* -*- indent-tabs-mode: nil -*- */
/*
    QoreXmlReader.cpp

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

#include "QoreXmlReader.h"
#include "QoreXmlRpcReader.h"

#include <memory>

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

void QoreXmlReader::processOpts(const QoreHashNode* opts, ExceptionSink* xsink) {
    assert(reader);
    if (!opts)
        return;

    ConstHashIterator i(opts);
    while (i.next()) {
        const char* key = i.getKey();
        if (!strcmp(key, "xsd")) {
            QoreValue n = i.get();
            if (n.getType() != NT_STRING) {
                xsink->raiseException("XMLREADER-XSD-ERROR", "expecting type 'string' with option 'xsd'; got type '%s' instead", n.getTypeName());
                return;
            }

            // set xml_input_io first
            XmlIoInputCallbackHelper xicbh(opts, xsink);
            if (*xsink)
                return;

#ifdef HAVE_XMLTEXTREADERSETSCHEMA
            const QoreStringNode* xsd = n.get<const QoreStringNode>();
            std::unique_ptr<QoreXmlSchemaContext> schema(new QoreXmlSchemaContext(*xsd, xsink));
            if (*xsink)
                return;

            int rc = setSchema(schema->getSchema());
            if (rc < 0) {
                xsink->raiseException("XSD-VALIDATION-ERROR", "XML schema could not be validated");
                return;
            }

            val = schema.release();
            //printd(5, "QoreXmlReader::processOpts() set schema %p\n", val);
            continue;
#else
            xsink->raiseException("MISSING-FEATURE-ERROR", "the libxml2 version used to compile the xml module did not support the xmlTextReaderSetSchema() function, XSD validation is not available; for maximum portability, use the constant Option::HAVE_PARSEXMLWITHSCHEMA to check if this function is implemented before using XSD validation functionality");
            return;
#endif
        }

        // ignore options already processed
        if (!strcmp(key, "encoding") || !strcmp(key, "xml_parse_options") || !strcmp(key, "xml_input_io"))
            continue;

        xsink->raiseException("XML-READER-ERROR", "unsupported option '%s'", key);
        return;
    }
}

QoreHashNode* QoreXmlReader::parseXmlData(const QoreEncoding* data_ccsid, int pflags, ExceptionSink* xsink) {
    if (read(xsink) != 1)
        return 0;

    QoreValue rv = getXmlData(xsink, data_ccsid, pflags, depth());

    if (!rv) {
        if (!*xsink)
            xsink->raiseExceptionArg("PARSE-XML-EXCEPTION", xml ? new QoreStringNode(*xml) : 0, "parse error parsing XML string");
        return 0;
    }
    assert(rv.getType() == NT_HASH);

    return rv.get<QoreHashNode>();
}

QoreValue QoreXmlReader::getXmlData(ExceptionSink* xsink, const QoreEncoding* data_ccsid, int pflags, int min_depth) {
    Qore::Xml::intern::xml_stack xstack;

    QORE_TRACE("getXMLData()");
    //printd(5, "QoreXmlReader::getXmlData() enc: %s flags: %d md: %d\n", data_ccsid->getCode(), pflags, min_depth);
    int rc = 1;

    while (rc == 1) {
        int nt = nodeTypeSkipWhitespace();
        // get node name
        const char* name = constName();
        if (!name)
            name = "--";
        else if (pflags & XPF_STRIP_NS_PREFIXES) {
            const char* p = strchr(name, ':');
            if (p)
                name = p + 1;
        }

        if (nt == -1) // ERROR
            break;

        if (nt == XML_READER_TYPE_ELEMENT) {
            int depth = QoreXmlReader::depth();
            xstack.checkDepth(depth);

            QoreValue n = xstack.getValue();
            // if there is no node pointer, then make a hash
            if (n.isNothing()) {
                QoreHashNode* h = new QoreHashNode;
                xstack.setNode(h);
                xstack.push(h->getKeyValueReference(name), depth);
            }
            else { // node ptr already exists
                QoreHashNode* h = n.getType() == NT_HASH ? n.get<QoreHashNode>() : nullptr;
                if (!h) {
                    h = new QoreHashNode;
                    xstack.setNode(h);
                    h->setKeyValue("^value^", n, xsink);
                    xstack.incValueCount();
                    xstack.push(h->getKeyValueReference(name), depth);
                }
                else {
                    // see if key already exists
                    QoreValue v;
                    bool exists;
                    v = h->getKeyValueExistence(name, exists);

                    if (!exists)
                        xstack.push(h->getKeyValueReference(name), depth);
                    else {
                        if (!(pflags & XPF_PRESERVE_ORDER)) {
                            QoreListNode* vl = v.getType() == NT_LIST ? v.get<QoreListNode>() : nullptr;
                            // if it's not a list, then make into a list with current value as first entry
                            if (!vl) {
                                QoreValue& vp = h->getKeyValueReference(name);
                                vl = new QoreListNode;
                                vl->push(v, xsink);
                                vp = vl;
                            }
                            xstack.push(vl->getEntryReference(vl->size()), depth);
                        }
                        else {
                            // see if last key was the same, if so make a list if it's not
                            const char* lk = h->getLastKey();
                            bool get_value = false;
                            if (keys_are_equal(name, lk, get_value)) {
                                // get actual key value if there was a suffix
                                if (get_value)
                                    v = h->getKeyValue(lk);

                                QoreListNode* vl = v.getType() == NT_LIST ? v.get<QoreListNode>() : nullptr;
                                // if it's not a list, then make into a list with current value as first entry
                                if (!vl) {
                                    QoreValue& vp = h->getKeyValueReference(lk);
                                    vl = new QoreListNode;
                                    vl->push(v, xsink);
                                    vp = vl;
                                }
                                xstack.push(vl->getEntryReference(vl->size()), depth);
                            }
                            else {
                                QoreString ns;
                                int c = 1;
                                while (true) {
                                    ns.sprintf("%s^%d", name, c);
                                    if (!h->existsKey(ns.c_str()))
                                        break;
                                    c++;
                                    ns.clear();
                                }
                                xstack.push(h->getKeyValueReference(ns.c_str()), depth);
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
                        return QoreValue();
                    h->setKeyValue(aname, value, xsink);
                }
                if (*xsink)
                    return QoreValue();

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
                    return QoreValue();

                QoreValue n = xstack.getValue();
                if (!n.isNothing()) {
                    QoreHashNode* h = n.getType() == NT_HASH ? n.get<QoreHashNode>() : nullptr;
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
                        h->setKeyValue("^value^", n, xsink);
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
                    return QoreValue();

                QoreValue n = xstack.getValue();
                if (n.getType() == NT_HASH) {
                    QoreHashNode* h = n.get<QoreHashNode>();
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
                    if (!n.isNothing()) {
                        h->setKeyValue("^value^", n, xsink);
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
                    return QoreValue();

                QoreValue n = xstack.getValue();
                if (n.getType() == NT_HASH) {
                    QoreHashNode* h = n.get<QoreHashNode>();
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
                    if (!n.isNothing()) {
                        h->setKeyValue("^value^", n, xsink);
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
    return rc ? QoreValue() : xstack.takeValue();
}
