/*
  Qore xml module

  Copyright (C) 2010 - 2021 Qore Technologies, s.r.o.

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

#include "qore-xml-module.h"

#include "QC_XmlRpcClient.h"
#include "QC_XmlNode.h"
#include "QC_XmlDoc.h"
#include "QC_XmlNode.h"
#include "QC_XmlReader.h"
#include "QC_SaxIterator.h"
#include "QC_AbstractXmlIoInputCallback.h"

#include "ql_xml.h"

#include <libxml/xmlversion.h>

#include <stdarg.h>

static QoreStringNode *xml_module_init();
static void xml_module_ns_init(QoreNamespace *rns, QoreNamespace *qns);
static void xml_module_delete();

DLLLOCAL void init_option_constants(QoreNamespace& ns);

// module declaration for Qore 0.9.5+
DLLEXPORT extern "C" void xml_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "xml";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "xml module";
    mod_info.author = "David Nichols";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = xml_module_init;
    mod_info.ns_init = xml_module_ns_init;
    mod_info.del = xml_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

thread_local AbstractXmlIoInputCallback* xml_io_callback = nullptr;

QoreNamespace XNS("Qore::Xml");

static void qoreXmlGenericErrorFunc(QoreString *err, const char *msg, ...) {
   va_list args;
   va_start(args, msg);
   err->clear();
   err->vsprintf(msg, args);
   va_end(args);
}

// ignore errors after initialization
static void qoreXmlIgnoreErrorFunc(QoreString *err, const char *msg, ...) {
}

// libxml2 I/O callback: can we provide the requested resource; 1 = yes, 0 = no
static int qoreXmlInputMatchCallback(const char* filename) {
    //printd(5, "qoreXmlInputMatchCallback() filename: %s xml_io_callback: %p\n", filename, xml_io_callback);
    return xml_io_callback ? xml_io_callback->match(filename) : 0;
}

// libxml2 I/O callback: open the requested resource; returns nullptr on error
static void* qoreXmlInputOpenCallback(const char* filename) {
    return xml_io_callback ? xml_io_callback->open(filename) : nullptr;
}

// libxml2 I/O callback: read the requested resource; returns the number of bytes read or -1 in case of error
static int qoreXmlInputReadCallback(void* context, char* buffer, int len) {
    return xml_io_callback ? xml_io_callback->read(context, buffer, len) : -1;
}

// libxml2 I/O callback: close the requested resource
static int qoreXmlInputCloseCallback(void* context) {
    return xml_io_callback ? xml_io_callback->close(context) : 0;
}

QoreStringNode* xml_module_init() {
    QoreString err;

    // set our generic error handler to catch initialization errors
    xmlSetGenericErrorFunc((void*)&err, (xmlGenericErrorFunc)qoreXmlGenericErrorFunc);

    // initialize libxml2 library
    LIBXML_TEST_VERSION

    if (err.strlen())
        return new QoreStringNode(err);

    {
        // register input callbacks
        int rc = xmlRegisterInputCallbacks(qoreXmlInputMatchCallback,
            (xmlInputOpenCallback)qoreXmlInputOpenCallback,
            (xmlInputReadCallback)qoreXmlInputReadCallback,
            (xmlInputCloseCallback)qoreXmlInputCloseCallback);
        if (rc == -1)
            return new QoreStringNodeMaker("error registering input callback; xmlRegisterInputCallbacks() returned %d; cannot initialize the libxml2 module", rc);
    }

    // ignore errors after initialization
    xmlSetGenericErrorFunc((void*)&err, (xmlGenericErrorFunc)qoreXmlIgnoreErrorFunc);

    XNS.addSystemClass(initXmlNodeClass(XNS));
    XNS.addSystemClass(initXmlDocClass(XNS));
    XNS.addSystemClass(initXmlReaderClass(XNS));
    XNS.addSystemClass(initSaxIteratorClass(XNS));
    XNS.addSystemClass(initFileSaxIteratorClass(XNS));
    XNS.addSystemClass(initInputStreamSaxIteratorClass(XNS));
    XNS.addSystemClass(initAbstractXmlIoInputCallbackClass(XNS));

    XNS.addSystemClass(initXmlRpcClientClass(XNS));

    init_xml_constants(XNS);

    // set up Option namespace for XML options
    QoreNamespace *option = new QoreNamespace("Option");

    init_option_constants(*option);

    XNS.addInitialNamespace(option);

    init_xml_functions(XNS);

    return 0;
}

void xml_module_ns_init(QoreNamespace *rns, QoreNamespace *qns) {
   qns->addNamespace(XNS.copy());
}

void xml_module_delete() {
   // cleanup libxml2 library
   xmlCleanupParser();
}
