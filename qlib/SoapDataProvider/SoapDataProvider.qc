# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SoapDataProvider module definition

/** SoapDataProvider.qc Copyright 2019 - 2022 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the SoapDataProvider module
public namespace SoapDataProvider {
#! The application name for SOAP servers
public const AppName = "SOAP";

#! The Soap data provider class
public class SoapDataProvider inherits AbstractDataProvider {
    public {
        #! The SOAP schema
        WebService webservice;

        #! The client object for API calls
        *SoapClient soapclient;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SoapDataProvider",
            "supports_read": False,
            "supports_create": False,
            "supports_update": False,
            "supports_upsert": False,
            "supports_delete": False,
            "supports_native_search": False,
            "supports_bulk_create": False,
            "supports_bulk_upsert": False,
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "search_options": NOTHING,
            "create_options": NOTHING,
            "upsert_options": NOTHING,
            "transaction_management": False,
        };

        #! Constructor options
        const ConstructorOptions = {
            "schema": <DataProviderOptionInfo>{
                "display_name": "WSDL",
                "short_desc": "The location to the WSDL or the WSDL object itself",
                "type": (
                    AbstractDataProviderType::get(StringType, NOTHING, {
                        DTT_FromFile: True,
                        DTT_FromLocation: True,
                    }),
                    AbstractDataProviderType::get(new Type("WebService")),
                ),
                "desc": "The SOAP schema object (WebService) or source URL for the WSDL",
                "required": True,
            },
            "soapclient": <DataProviderOptionInfo>{
                "display_name": "SOAP Client",
                "short_desc": "The SOAP client object to use",
                "type": AbstractDataProviderType::get(new Type("SoapClient"), NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "The SoapClient object to use",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to the SOAP server",
                "type": AbstractDataProviderType::get(StringType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "the URL to the SOAP server; overrides any URL in the schema or in any SoapClient object "
                    "passed as an option",
            },
            "soapclient_options": <DataProviderOptionInfo>{
                "display_name": "SOAP Client Options",
                "short_desc": "Options to use when creating the SOAP client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the SoapClient constructor; only used if a SoapClient object is created for a "
                    "call",
            },
        };
    }

    private {
        #! hash of valid paths
        hash<auto> path_tree;
    }

    #! Creates the object from the arguments
    constructor(WebService webservice, *SoapClient soapclient) {
        self.webservice = webservice;
        if (soapclient) {
            self.soapclient = soapclient;
        }
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        switch (copts.schema.typeCode()) {
            case NT_STRING:
                webservice = WSDLLib::getWebServiceFromUrl(copts.schema);
                break;
            case NT_OBJECT:
                webservice = copts.schema;
                break;
        }

        if (copts.soapclient) {
            checkSoapClient(copts.soapclient);
            soapclient = copts.soapclient;
        }

        if (!soapclient) {
            if (copts.url || copts.soapclient_options) {
                hash<auto> opts += copts.soapclient_options + {
                    "wsdl": webservice,
                };
                if (copts.url) {
                    opts.url = copts.url;
                }
                soapclient = new SoapClient(opts);
            }
        } else if (copts.url) {
            soapclient.setURL(copts.url);
        }
    }

    #! Returns the data provider name
    string getName() {
        return webservice.getName();
    }

    #! Checks the SOAP client
    private checkSoapClient(SoapClient soap) {
        WebService webservice = soap.getWebService();
        if (webservice.getHash() != self.webservice.getHash()) {
            throw "CONSTRUCTOR-ERROR", sprintf("SoapClient provided in %s::constructor() is configured with a "
                "different WSDL than %y required by this data provider", self.className(),
                getName());
        }
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return webservice.getOperationNames();
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        try {
            WSOperation op = webservice.getOperation(name);
            return new SoapRequestDataProvider(webservice, soapclient, op);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "WSDL-OPERATION-ERROR") {
                rethrow;
            }
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
