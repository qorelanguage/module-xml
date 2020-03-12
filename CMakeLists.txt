cmake_minimum_required(VERSION 2.8.3)

project(qore-xml-module)

set (VERSION_MAJOR 1)
set (VERSION_MINOR 5)
set (VERSION_PATCH 0)

find_package(Qore 0.9 REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(OpenSSL REQUIRED)

list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBXML2_LIBRARIES})
list(APPEND CMAKE_REQUIRED_INCLUDES ${LIBXML2_INCLUDE_DIR})

include_directories( ${CMAKE_SOURCE_DIR}/src )
include_directories( ${LIBXML2_INCLUDE_DIR} )
include_directories( ${OPENSSL_INCLUDE_DIR} )
include_directories( ${QORE_INCLUDE_DIR} )

# Check for C++11.
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBXML_STATIC=1")
endif()

set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)

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
    src/QC_SaxIterator.qpp
    src/QC_FileSaxIterator.qpp
    src/QC_InputStreamSaxIterator.qpp
    src/QC_XmlDoc.qpp
    src/QC_XmlNode.qpp
    src/QC_XmlReader.qpp
    src/QC_XmlRpcClient.qpp
    src/ql_xml.qpp
    src/qc_option.qpp
    src/MakeXmlOpts.qpp
    src/QC_AbstractXmlIoInputCallback.qpp
)

set(CPP_SRC
    src/xml-module.cpp
    src/QoreXmlRpcReader.cpp
    src/QoreXmlReader.cpp
)

set(QMOD
    qlib/SalesforceSoapClient.qm
    qlib/SoapClient.qm
    qlib/SoapHandler.qm
    qlib/WSDL.qm
    qlib/XmlRpcHandler.qm
    qlib/XmlRpcConnection.qm
    qlib/SoapDataProvider
    qlib/WebDAV
)

set(SCRIPTS
    bin/soaputil
    bin/webdav-server
)

qore_wrap_qpp_value(QPP_SOURCES ${QPP_SRC})

SET (module_name "xml")

set(QORE_DOX_TMPL_SRC
  docs/mainpage.doxygen.tmpl
)

add_library(${module_name} SHARED ${QPP_SOURCES} ${CPP_SRC})

qore_binary_module(${module_name} "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" ${LIBXML2_LIBRARIES})
qore_user_modules("${QMOD}")
install(PROGRAMS ${SCRIPTS} DESTINATION bin)

qore_dist("${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

qore_config_info()

FIND_PACKAGE(Doxygen)
if (DOXYGEN_FOUND)
  qore_wrap_dox(QORE_DOX_SRC ${QORE_DOX_TMPL_SRC})
  add_custom_target(QORE_MOD_DOX_FILES DEPENDS ${QORE_DOX_SRC})
  add_dependencies(docs QORE_MOD_DOX_FILES)
endif()
