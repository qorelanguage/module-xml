# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SoapRequestDataProvider module definition

/** SoapRequestDataProvider.qc Copyright 2019 - 2020 Qore Technologies, s.r.o.

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

# minimum required Qore version
%requires qore >= 0.9.4
# assume local scope for variables, do not use "$" signs
%new-style
# require type definitions everywhere
%require-types
# strict argument handling
%strict-args
# enable all warnings
%enable-all-warnings

#! contains all public definitions in the SoapRequestDataProvider module
public namespace SoapDataProvider {
#! The Soap data provider class
public class SoapRequestDataProvider inherits SoapDataProviderBase {
    public {
        #! The SOAP client object for API calls
        *SoapClient soapclient;

        #! If the request supports a list of hashes in the response and therefore the record API
        *hash<string, AbstractDataField> record_type;
    }

    #! Creates the object from the arguments
    constructor(WebService webservice, *SoapClient soapclient, WSOperation op) : SoapDataProviderBase(webservice, op) {
        self.soapclient = soapclient;
        record_type = getRecordTypeIntern();
    }

    #! Returns the data provider name
    string getName() {
        return webservice.getName() + "/" + op.name + "/" + "request";
    }

    #! Returns data provider info
    hash<DataProviderInfo> getInfo() {
        hash<DataProviderInfo> rv({
            "name": getName(),
            "supports_read": True,
            "supports_request": True,
            "has_record": exists record_type,
        });
        rv += getStaticInfoImpl();
        return rv;
    }

    #! Returns the description of the record type, if any
    /** Returns the record type description for any successful response
    */
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return record_type;
    }

    #! Returns the description of a successful request message, if any
    private *AbstractDataProviderType getRequestTypeImpl() {
        return op.input.getDataProviderType();
    }

    #! Returns the description of a response message, if this object represents a response message
    /** If the object is a request message, then any successful response message should be returned here as well

        @return the response type for this response message

        @throw INVALID-OPERATION the data provider does not support the request API
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return op.output.getDataProviderType();
    }

    #! Returns a hash of error responses, if any
    private *hash<string, AbstractDataProviderType> getErrorResponseTypesImpl() {
        return cast<*hash<string, AbstractDataProviderType>>(map {
            $1.key: $1.value.getDataProviderType(),
        }, op.getFaultMessages().pairIterator());
    }

    #! Makes a request and returned the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRequestIntern(req, request_options);
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        This will execute the request and perform a default search on any record(s) returned
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> resp = doRequestIntern(search_options);
        # return an iterator for the result
        AbstractIterator i;
        switch (resp.response.typeCode()) {
            case NT_HASH:
                i = new SingleValueIterator(resp.response);
                break;
            case NT_LIST:
                i = new ListIterator(resp.response);
                break;
            default:
                throw "SOAP-SEARCH-ERROR", sprintf("the response has type %y, which does not support searching",
                    resp.response.type());
                break;
        }
        return new DefaultRecordIterator(i, where_cond);
    }

    #! Makes a REST request and returns the response
    private hash<auto> doRequestIntern(auto req, *hash<auto> options) {
        SoapClient soap = getSoapClient();
        # make the call
        return soap.callOperation(op.name, req, options);
    }

    #! Returns a REST client for HTTP operations
    private SoapClient getSoapClient() {
        if (!soapclient) {
            soapclient = new SoapClient({
                "wsdl": webservice,
            });
        }
        return soapclient;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = SoapDataProvider::ProviderInfo - "constructor_options";
        rv += {
            "supports_read": exists record_type,
            "supports_request": True,
            "has_record": exists record_type,
        };
        rv.request_options = rv.search_options = getApiOptions();
        return rv;
    }

    #! Returns API options for the current operation
    private *hash<string, hash<DataProviderOptionInfo>> getApiOptions() {
        return {
            "soap_header": <DataProviderOptionInfo>{
                "type": new HashDataType(),
                "desc": "a hash giving SOAP header information, if required by the message",
            },
            "http_header": <DataProviderOptionInfo>{
                "type": new HashDataType(),
                "desc": "a hash giving HTTP header information to include in the message (does not override "
                    "automatically-generated SOAP message headers)",
            },
            "xml_opts": <DataProviderOptionInfo>{
                "type": new QoreIntDataType(),
                "desc": "an integer XML generation option code",
            },
            "soapaction": <DataProviderOptionInfo>{
                "type": new QoreStringDataType(),
                "desc": "an optional string that will override the SOAPAction for the request; en empty string here "
                    "will prevent the SOAPAction from being sent",
            },
            "binding": <DataProviderOptionInfo>{
                "type": new QoreStringDataType(),
                "desc": " SOAP binding name, if not provided the first binding assigned to the operation will be "
                    "used",
            },
        };
    }

    #! Returns the description of the record type, if any
    private:internal *hash<string, AbstractDataField> getRecordTypeIntern() {
        AbstractDataProviderType type = op.output.getDataProviderType();
        if (type.getBaseTypeCode() == NT_LIST && (*hash<string, AbstractDataField> rv = type.getElementType().getFields())) {
            return rv;
        }
    }
}
}
