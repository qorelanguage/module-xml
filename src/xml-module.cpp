/*
  Qore xml module

  Copyright (C) 2010 - 2014 Qore Technologies, sro

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

#include "ql_xml.h"

#include <libxml/xmlversion.h>

#include <stdarg.h>

static QoreStringNode *xml_module_init();
static void xml_module_ns_init(QoreNamespace *rns, QoreNamespace *qns);
static void xml_module_delete();

// qore module symbols
DLLEXPORT char qore_module_name[] = "xml";
DLLEXPORT char qore_module_version[] = PACKAGE_VERSION;
DLLEXPORT char qore_module_description[] = "xml module";
DLLEXPORT char qore_module_author[] = "David Nichols";
DLLEXPORT char qore_module_url[] = "http://qore.org";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = xml_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = xml_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = xml_module_delete;
DLLEXPORT qore_license_t qore_module_license = QL_MIT;
DLLEXPORT char qore_module_license_str[] = "MIT";

QoreNamespace XNS("Xml");

static void qoreXmlGenericErrorFunc(QoreString *err, const char *msg, ...) {
   va_list args;
   va_start(args, msg);
   err->clear();
   err->vsprintf(msg, args);
   va_end(args);
}

QoreStringNode *xml_module_init() {
   QoreString err;

   // set our generic error handler to catch initialization errors
   xmlSetGenericErrorFunc((void*)&err,  (xmlGenericErrorFunc)qoreXmlGenericErrorFunc);

   // initialize libxml2 library
   LIBXML_TEST_VERSION

   if (err.strlen())
      return new QoreStringNode(err);

   // reset the generic error handler back to the default
   initGenericErrorDefaultFunc(0);

   XNS.addSystemClass(initXmlNodeClass(XNS));
   XNS.addSystemClass(initXmlDocClass(XNS));
   XNS.addSystemClass(initXmlReaderClass(XNS));
   XNS.addSystemClass(initSaxIteratorClass(XNS));
   XNS.addSystemClass(initFileSaxIteratorClass(XNS));
   XNS.addSystemClass(initInputStreamSaxIteratorClass(XNS));

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
