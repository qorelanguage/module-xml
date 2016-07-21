cmake_minimum_required(VERSION 2.8.3)

project(qore-xml-module)

set (VERSION_MAJOR 1)
set (VERSION_MINOR 4)
set (VERSION_PATCH 0)


find_package(Qore REQUIRED)
find_package(LibXml2 REQUIRED)

list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBXML2_LIBRARIES})
list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBXML2_INCLUDE_DIR})

include_directories( ${CMAKE_SOURCE_DIR}/src )
include_directories( ${LIBXML2_INCLUDE_DIR} )

#include(CheckSymbolExists)
include(CheckFunctionExists)

macro(qore_check_symbol _symbol _define)
#    check_symbol_exists(${_symbol} ${_includes} ${_define})
    check_function_exists(${_symbol} ${_define})
    if (${_define})
#        message(STATUS "    adding define: -D${_define}")
        add_definitions(-D${_define})
    endif(${_define})
endmacro(qore_check_symbol)

qore_check_symbol(xmlTextReaderSetSchema HAVE_XMLTEXTREADERSETSCHEMA)
qore_check_symbol(xmlTextReaderRelaxNGSetSchema HAVE_XMLTEXTREADERRELAXNGSETSCHEMA)
qore_check_symbol(xmlChildElementCount HAVE_XMLCHILDELEMENTCOUNT)
qore_check_symbol(xmlFirstElementChild HAVE_XMLFIRSTELEMENTCHILD)
qore_check_symbol(xmlGetLastChild HAVE_XMLGETLASTCHILD)
qore_check_symbol(xmlLastElementChild HAVE_XMLLASTELEMENTCHILD)
qore_check_symbol(xmlNextElementSibling HAVE_XMLNEXTLEMENTSIBLING)
qore_check_symbol(xmlPreviousElementSibling HAVE_XMLPREVIOUSELEMENTSIBLING)
qore_check_symbol(xmlTextReaderIsNamespaceDecl HAVE_XMLTEXTREADERISNAMESPACEDECL)
qore_check_symbol(xmlTextReaderByteConsumed HAVE_XMLTEXTREADERBYTECONSUMED)
qore_check_symbol(xmlTextReaderGetParserColumnNumber HAVE_XMLTEXTREADERGETPARSERCOLUMNNUMBER)
qore_check_symbol(xmlTextReaderGetParserLineNumber HAVE_XMLTEXTREADERGETPARSERLINENUMBER)


set(QPP_SRC
    src/QC_FileSaxIterator.qpp
    src/QC_InputStreamSaxIterator.qpp
    src/QC_XmlDoc.qpp
    src/QC_XmlReader.qpp
    src/ql_xml.qpp
    src/QC_SaxIterator.qpp
    src/QC_XmlNode.qpp
    src/QC_XmlRpcClient.qpp
)

set(CPP_SRC
    src/xml-module.cpp
)

set(QMOD
    qlib/SoapClient.qm
    qlib/SoapHandler.qm
    qlib/WSDL.qm
    qlib/XmlRpcHandler.qm
)

qore_wrap_qpp_value(QPP_SOURCES ${QPP_SRC})

SET (module_name "xml")

add_library(${module_name} SHARED ${QPP_SOURCES} ${CPP_SRC})

qore_binary_module(${module_name} "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" ${LIBXML2_LIBRARIES})
qore_user_modules("${QMOD}")

qore_dist("${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

qore_config_info()
