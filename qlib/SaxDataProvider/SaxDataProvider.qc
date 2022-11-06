# -*- mode: qore; indent-tabs-mode: nil -*-
# Qore SaxDataProvider class definition

/*  SaxDataProvider.qc Copyright 2012 - 2022 Qore Technologies, s.r.o.

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

#! The main namespace for the SaxDataProvider module
public namespace SaxDataProvider {
#! Provides a data provider for reading XML files
/** Supports generic record search operators and options

    @note If the SAX iterator does not return a hash for each iteration, then this data provider will return A
    hash with the deserialized XML data returned in the \c data key
*/
public class SaxDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SaxDataProvider",
            "supports_read": True,
            "has_record": True,
            "supports_search_expressions": True,
            "constructor_options": ConstructorOptions,
            "search_options": GenericRecordSearchOptions,
            "expressions": AbstractDataProvider::GenericExpressions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "location": <DataProviderOptionInfo>{
                "type": AbstractDataProviderType::get(StringType),
                "desc": "the location of the XML data; mutually exclusive with \"stream\"; resolved with a call to "
                    "FileLocationHandler::getBinaryStreamFromLocation()",
            },
            "stream": <DataProviderOptionInfo>{
                "type": AbstractDataProviderType::get(new Type("InputStream")),
                "desc": "an input stream for XML data; mutually exclusive with \"location\"",
            },
            "element_name": <DataProviderOptionInfo>{
                "type": AbstractDataProviderTypeMap."string",
                "desc": "the name of the element to iterate",
                "required": True,
            },
            "encoding": <DataProviderOptionInfo>{
                "type": AbstractDataProviderTypeMap."string",
                "desc": "the character encoding of the data at `location` or in `stream`",
            },
            "xsd": <DataProviderOptionInfo>{
                "type": AbstractDataProviderTypeMap."string",
                "desc": "an XSD string to validate the input XML",
            },
        };

        #! XML iteration option list
        const CsvIterationOptionList = keys (ConstructorOptions - ("path", "stream"));
    }

    private {
        #! the iterator object
        SaxIteratorWrapper i;

        #! the source location
        string location;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.location) {
            if (copts.stream) {
                error("CONSTRUCTOR-ERROR", "cannot provide both \"stream\" and \"location\" options; use either one "
                    "or the other");
            }
            i = new SaxIteratorWrapper(
                    new InputStreamSaxIterator(FileLocationHandler::getBinaryStreamFromLocation(copts.location),
                    copts.element_name, copts.encoding)
                );
            location = copts.location;
            return;
        }
        if (copts.stream) {
            i = new SaxIteratorWrapper(new InputStreamSaxIterator(copts.stream, copts.element_name, copts.encoding));
            return;
        }
        # create a dummy interator with no input
        i = new SaxIteratorWrapper(new InputStreamSaxIterator(new StringInputStream(""), copts.element_name));
    }

    #! Returns the object name
    string getName() {
        return location ?? sprintf("stream %s", self.uniqueHash());
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        return new DefaultRecordIterator(i, where_cond, search_options, getRecordType());
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<DataProviderExpression> where_cond,
            *hash<auto> search_options) {
        return new DefaultRecordIterator(i, where_cond, search_options, getRecordType());
    }

    #! Returns the description of the record type, if any
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}

# private namespace
namespace Priv {
class SaxIteratorWrapper inherits AbstractIterator {
    private {
        SaxIterator i;
    }

    constructor(SaxIterator i) {
        self.i = i;
    }

    bool next() {
        return i.next();
    }

    hash<auto> getValue() {
        auto rv = i.getValue();
        return rv.typeCode() == NT_HASH
            ? rv
            : {"data": rv};
    }

    bool valid() {
        return i.valid();
    }
}
}
