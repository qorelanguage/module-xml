# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SoapDataProvider module definition

/** SoapOperationDataProvider.qc Copyright 2019 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SoapDataProvider module
public namespace SoapDataProvider {
#! The Soap data provider class
public class SoapOperationDataProvider inherits SoapDataProviderBase {
    public {
        #! The SOAP client object for API calls
        *SoapClient soapclient;
    }

    #! Creates the object from the arguments
    constructor(WebService webservice, *SoapClient soapclient, WSOperation op) : SoapDataProviderBase(webservice, op) {
        self.soapclient = soapclient;
    }

    #! Returns the data provider name
    string getName() {
        return webservice.getName() + "/" + op.name;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return ("request", "response");
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case "request": return new SoapRequestDataProvider(webservice, soapclient, op);
            case "response": return new SoapResponseDataProvider(webservice, op);
        }
    }
}
}
