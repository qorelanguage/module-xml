# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SoapDataProvider module definition

/** SoapDataProviderBase.qc Copyright 2019 Qore Technologies, s.r.o.

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
#! The Soap data provider base class
public class SoapDataProviderBase inherits AbstractDataProvider {
    private {
        #! The WebService object
        WebService webservice;

        #! The operation object
        WSOperation op;
    }

    #! Creates the object from the arguments
    constructor(WebService webservice, WSOperation op) {
        self.webservice = webservice;
        self.op = op;
    }

    #! Returns the type of the input message
    AbstractDataProviderType getArgumentType() {
        return op.input.getDataProviderType();
    }

    #! Returns True if the input message exists
    bool isArgumentRequired() {
        return exists op.input;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return SoapDataProvider::ProviderInfo - "constructor_options";
    }
}
}