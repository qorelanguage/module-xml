moddir = $(userlibdir)

AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

noinst_HEADERS = src/QC_XmlDoc.h \
	src/QC_XmlNode.h \
	src/QC_XmlReader.h \
	src/QC_XmlRpcClient.h \
	src/QC_SaxIterator.h \
	src/QoreXPath.h \
	src/QoreXmlDoc.h \
	src/QoreXmlReader.h \
	src/QoreXmlRpcReader.h \
	src/ql_xml.h \
	src/qore-xml-module.h \
	src/MakeXmlOpts.h \
    src/QC_AbstractXmlIoInputCallback.h

USER_MODULES = qlib/XmlRpcHandler.qm qlib/WSDL.qm qlib/SoapClient.qm qlib/SoapHandler.qm qlib/XmlRpcConnection.qm qlib/SalesforceSoapClient.qm

USER_SPLIT_MODULES = qlib/SoapDataProvider qlib/WebDavHandler

SoapDataProvider_moddir= $(moddir)/SoapDataProvider
dist_SoapDataProvider_mod_DATA = $(wildcard qlib/SoapDataProvider/*.q[cm])

WebDavHandler_moddir= $(moddir)/WebDavHandler
dist_WebDavHandler_mod_DATA = $(wildcard qlib/WebDavHandler/*.q[cm])

EXTRA_DIST = COPYING.LGPL COPYING.MIT AUTHORS README \
	RELEASE-NOTES \
	src/QC_XmlDoc.qpp \
	src/QC_XmlNode.qpp \
	src/QC_XmlReader.qpp \
	src/QC_XmlRpcClient.qpp \
	src/QC_SaxIterator.qpp \
	src/QC_FileSaxIterator.qpp \
	src/QC_InputStreamSaxIterator.qpp \
	src/ql_xml.qpp \
	src/qc_option.qpp \
	src/MakeXmlOpts.qpp \
    src/QC_AbstractXmlIoInputCallback.qpp \
	test/xml.qtest \
	test/soap.qtest \
	test/test.wsdl \
	examples/xml-rpc-client.q \
	examples/XmlRpcServerValidation.q \
	$(USER_MODULES) \
	qore-xml-module.spec

ACLOCAL_AMFLAGS=-I m4

dist_mod_DATA = $(USER_MODULES)
dist_bin_SCRIPTS = bin/soaputil bin/webdav-server

if COND_DOXYGEN
USER_MODS = $(basename $(notdir $(USER_MODULES)))
USER_SPLIT_MODS = $(basename $(notdir $(USER_SPLIT_MODULES)))
DOXYGEN_OUTPUT = docs/xml/html $(addprefix docs/,$(addsuffix /html,$(USER_MODS) $(USER_SPLIT_MODS)))
DOXYFILES = docs/doxyfile docs/mainpage.doxygen
DOXYFILES_SRC = docs/doxyfile.tmpl docs/qlib/Doxyfile.tmpl docs/footer_template.html
TMP_MOD_TARG = docs/qlib/XmlRpcHandler.qm.dox.h docs/qlib/Doxyfile.XmlRpcHandler \
	docs/qlib/SoapClient.qm.dox.h docs/qlib/Doxyfile.SoapClient \
	docs/qlib/SoapHandler.qm.dox.h docs/qlib/Doxyfile.SoapHandler \
	docs/qlib/WSDL.qm.dox.h docs/qlib/Doxyfile.WSDL \
	docs/qlib/SalesforceSoapClient.qm.dox.h docs/qlib/Doxyfile.SalesforceSoapClient \
	docs/qlib/XmlRpcConnection.qm.dox.h
QLIB_TMP_DOXYFILES = $(TMP_MOD_TARG) $(addsuffix .tag,$(USER_MODS) $(USER_SPLIT_MODS))
QDX = qdx

DOXYFILE_SPLIT_TARG = $(addprefix docs/qlib/Doxyfile.,$(USER_SPLIT_MODS))
$(DOXYFILE_SPLIT_TARG): docs/qlib/Doxyfile.%: docs/qlib/Doxyfile.tmp $(QDX_DEP)
	$(QDX) -M=qlib/$*:qlib/$*.qm -txml.tag=../../xml/html -tWSDL.tag=../../WSDL/html $< $@

TMP_MOD_SPLIT_TARG = $(addprefix docs/qlib/,$(addsuffix .qm,$(USER_SPLIT_MODS)))
$(TMP_MOD_SPLIT_TARG): docs/qlib/%.qm: qlib/% $(QDX_DEP)
	$(QDX) $< $@

TMP_MOD_HTML_TARG = $(addsuffix /html,$(addprefix docs/,$(USER_SPLIT_MODS)))
$(TMP_MOD_HTML_TARG): docs/%/html: docs/qlib/%.qm docs/qlib/Doxyfile.%
	$(MKDIR_P) $@
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.$*
	$(QDX) --post $@ $@/search

DX_CLEANFILES = ${DOXYFILES} $(QLIB_TMP_DOXYFILES) $(DOXYFILE_SPLIT_TARG) docs/qlib/Doxyfile.tmp

docs/qlib/Doxyfile.tmp: docs/qlib/Doxyfile.tmpl
	sed s:__USERMODDIR__:${usermoddir}: $< > $@

docs/qlib/Doxyfile.SoapClient: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/SoapClient.qm:qlib/SoapClient.qm.dox.h -txml.tag=../../xml/html -tWSDL.tag=../../WSDL/html $< $@

docs/qlib/SoapClient.qm.dox.h: qlib/SoapClient.qm
	$(QDX) $< $@

docs/SoapClient/html: docs/qlib/SoapClient.qm.dox.h docs/qlib/Doxyfile.SoapClient
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.SoapClient
	$(QDX) --post docs/SoapClient/html docs/SoapClient/html/search

docs/qlib/Doxyfile.SalesforceSoapClient: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/SalesforceSoapClient.qm:qlib/SalesforceSoapClient.qm.dox.h -txml.tag=../../xml/html -tWSDL.tag=../../WSDL/html -tSoapClient.tag=../../SoapClient/html $< $@

docs/qlib/SalesforceSoapClient.qm.dox.h: qlib/SalesforceSoapClient.qm
	$(QDX) $< $@

docs/SalesforceSoapClient/html: docs/qlib/SalesforceSoapClient.qm.dox.h docs/qlib/Doxyfile.SalesforceSoapClient
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.SalesforceSoapClient
	$(QDX) --post docs/SalesforceSoapClient/html docs/SalesforceSoapClient/html/search

docs/qlib/Doxyfile.SoapHandler: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/SoapHandler.qm:qlib/SoapHandler.qm.dox.h -txml.tag=../../xml/html $< $@

docs/qlib/SoapHandler.qm.dox.h: qlib/SoapHandler.qm
	$(QDX) $< $@

docs/SoapHandler/html: docs/qlib/SoapHandler.qm.dox.h docs/qlib/Doxyfile.SoapHandler
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.SoapHandler
	$(QDX) --post docs/SoapHandler/html docs/SoapHandler/html/search

docs/qlib/Doxyfile.WSDL: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/WSDL.qm:qlib/WSDL.qm.dox.h -txml.tag=../../xml/html $< $@

docs/qlib/WSDL.qm.dox.h: qlib/WSDL.qm
	$(QDX) $< $@

docs/WSDL/html: docs/qlib/WSDL.qm.dox.h docs/qlib/Doxyfile.WSDL
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.WSDL
	$(QDX) --post docs/WSDL/html docs/WSDL/html/search

docs/qlib/Doxyfile.XmlRpcHandler: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/XmlRpcHandler.qm:qlib/XmlRpcHandler.qm.dox.h -txml.tag=../../xml/html $< $@

docs/qlib/XmlRpcHandler.qm.dox.h: qlib/XmlRpcHandler.qm
	$(QDX) $< $@

docs/XmlRpcHandler/html: docs/qlib/XmlRpcHandler.qm.dox.h docs/qlib/Doxyfile.XmlRpcHandler
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.XmlRpcHandler
	$(QDX) --post docs/XmlRpcHandler/html docs/XmlRpcHandler/html/search

docs/qlib/Doxyfile.XmlRpcConnection: docs/qlib/Doxyfile.tmp
	$(QDX) -M=qlib/XmlRpcConnection.qm:qlib/XmlRpcConnection.qm.dox.h -txml.tag=../../xml/html $< $@

docs/qlib/XmlRpcConnection.qm.dox.h: qlib/XmlRpcConnection.qm
	$(QDX) $< $@

docs/XmlRpcConnection/html: docs/qlib/XmlRpcConnection.qm.dox.h docs/qlib/Doxyfile.XmlRpcConnection
	cd docs; $(DOXYGEN_CMD) qlib/Doxyfile.XmlRpcConnection
	$(QDX) --post docs/XmlRpcConnection/html docs/XmlRpcConnection/html/search

docs/xml/html: $(DOXYFILES)
	cd docs; $(DOXYGEN_CMD) doxyfile;
	$(QDX) --post docs/xml/html docs/xml/search

docs/mainpage.doxygen: docs/mainpage.doxygen.tmpl
	$(QDX) -d $< $@

docs/doxyfile: docs/doxyfile.tmpl
	sed -e s/__VERSION__/${VERSION}/ -e s:__USERMODDIR__:${usermoddir}: $< > $@

doxygen-doc: $(DOXYGEN_OUTPUT)

EXTRA_DIST += $(DOXYGEN_OUTPUT)

html-local: $(DOXYGEN_OUTPUT)

install-html-local:
	for dox in xml $(USER_MODS) $(USER_SPLIT_MODS); do $(MKDIR_P) $(DESTDIR)$(datadir)/$(PACKAGE)/docs/$$dox/html/search; done
	for dir in $(DOXYGEN_OUTPUT); do \
	   for f in $$dir/*; do if [ -f $$f ]; then $(INSTALL_DATA) $$f $(DESTDIR)$(datadir)/$(PACKAGE)/$$dir; fi; done; \
	   for f in $$dir/search/*; do $(INSTALL_DATA) $$f $(DESTDIR)$(datadir)/$(PACKAGE)/$$dir/search; done; \
	done

clean-local:
	$(RM) -rf $(DOXYGEN_OUTPUT)

uninstall-local:
	$(RM) -rf $(DESTDIR)$(datadir)/$(PACKAGE)/docs

else
doxygen-doc:
	echo no doxygen support
endif

mostlyclean-local:
	rm -rf ${DOXYGEN_OUTPUT} ${DX_CLEANFILES}

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
