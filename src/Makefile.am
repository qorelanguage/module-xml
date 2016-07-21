MNAME=xml

.qpp.cpp:
	qpp -V $<

GENERATED_SOURCES = QC_XmlDoc.cpp QC_XmlNode.cpp QC_XmlReader.cpp QC_XmlRpcClient.cpp QC_SaxIterator.cpp QC_FileSaxIterator.cpp QC_InputStreamSaxIterator.cpp ql_xml.cpp qc_option.cpp
CLEANFILES = $(GENERATED_SOURCES)

if COND_SINGLE_COMPILATION_UNIT
single-compilation-unit.cpp: $(GENERATED_SOURCES)
XML_SOURCES = single-compilation-unit.cpp
else
XML_SOURCES = xml-module.cpp QoreXmlReader.cpp QoreXmlRpcReader.cpp
nodist_xml_la_SOURCES = $(GENERATED_SOURCES)
endif

lib_LTLIBRARIES = xml.la
xml_la_SOURCES = $(XML_SOURCES)
xml_la_LDFLAGS = -module -avoid-version ${LIBXML2_LDFLAGS} ${MODULE_LDFLAGS} ${OPENSSL_LDFLAGS}

INCLUDES = -I$(top_srcdir)/include

AM_CPPFLAGS=$(QORE_CPPFLAGS) ${LIBXML2_CPPFLAGS} ${OPENSSL_CPPFLAGS}

if COND_PROFILE
AM_CXXFLAGS=-g -pg
else
if COND_DEBUG
AM_CXXFLAGS=-g
endif
endif

if COND_MINGW
RM_OTHER=$(DESTDIR)$(libdir)/$(MNAME).dll.a
endif

install-exec-hook:
	mv $(DESTDIR)$(libdir)/$(MNAME).$(MODULE_SUFFIX) $(DESTDIR)$(libdir)/$(MNAME)-api-$(MODULE_API).qmod
	$(RM) $(DESTDIR)$(libdir)/$(MNAME).la $(DESTDIR)$(libdir)/$(MNAME).a $(RM_OTHER)
