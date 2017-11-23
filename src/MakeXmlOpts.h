/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
 MakeXmlOpts.h

 Qore Programming Language

 Copyright (C) 2017 Qore Technologies, s.r.o.

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

#ifndef MAKE_XML_OPTS_H
#define MAKE_XML_OPTS_H

#include <cassert>
#include <string>
#include <stdexcept>
#include "qore/QoreEncoding.h"
#include "qore/QoreHashNode.h"
#include "qore/QoreStringNode.h"
#include "qore/QoreValue.h"
#include "qore-xml-module.h"


/**
 * Wrapper for xml generating options.
 */
class MakeXmlOpts final {
public:
    /**
     * Exception that is thrown when the hash with options is not valid.
     */
    class InvalidHash final : public std::invalid_argument {
    public:
        explicit InvalidHash(const std::string &what_arg) :
            std::invalid_argument(what_arg)
        {}

        explicit InvalidHash(const char *what_arg) :
            std::invalid_argument(what_arg)
        {}

        InvalidHash(InvalidHash &&) = default;
    };

public:
    /**
     * Constructs the wrapper with default options.
     */
    explicit MakeXmlOpts();

    MakeXmlOpts(const MakeXmlOpts&) = default;
    MakeXmlOpts& operator=(const MakeXmlOpts&) = default;

    MakeXmlOpts(MakeXmlOpts &&) = default;
    MakeXmlOpts& operator=(MakeXmlOpts &&) = default;

public:
    /**
     * Creates the wrapper with options filled from flags and passed
     * encoding.
     * @param flags Formatting flags; see @ref xml_generation_constants for more
     *              information.
     * @param encoding An optional encoding for the output XML string
     * @returns MakeXmlOpts with options parsed from flags and encoding,
     *          not specified fields are defaulted.
     */
    static MakeXmlOpts createFromFlags(
            int flags, const QoreEncoding* encoding = nullptr);

    /**
     * Creates the wrapper with options filled from hash.
     * @param hash Hash with formatting options; see @ref xml_generation_opts
     *             for more information about possible values.
     * @returns MakeXmlOpts with options parsed from hash. Not specified
     *          options are defaulted. Unknown options are skipped.
     * @throws InvalidHash in case that known option is invalid (unknown options
     *         are skipped).
     */
    static MakeXmlOpts createFromHash(const QoreHashNode *hash);

private:
    /**
     * Parses the value from @p hash and stores it into the @p output variable.
     * @tparam T Type of the output variable
     * @param hash The hash from which the value should be parsed
     * @param key The key which value should be parsed
     * @param valueType What type shall be the parsed value
     * @param mandatory True, if the value shall be presented in the hash.
     * @throws InvalidHash in case that the value is mandatory but not presented
     *         in the hash or type of the value is not @p valueType.
     */
    template <typename T>
    static void parseValue(
            T &output, const QoreHashNode *hash,
            const std::string &key, qore_type_t valueType,
            bool mandatory = false);

public:
    /// xml document version as a string
    std::string m_docVersion;
    /// encoding used for the output xml
    const QoreEncoding *m_encoding;
    /// True, if the output xml shall be formatted with white spaces.
    bool m_formatWithWhitespaces;
    /// Use numeric character references instead of native characters for
    /// non-ascii character when generating XML
    bool m_useNumericRefs;
    /// format of dates when serializing into xml
    std::string m_dateFormat;
};

// ------------- impl --------------
template <typename T>
void MakeXmlOpts::parseValue(
        T &output, const QoreHashNode *hash,
        const std::string &key, qore_type_t valueType,
        bool mandatory) {
    assert(hash);
    bool exists = false;
    auto value = hash->getValueKeyValueExistence(key.c_str(), exists);
    if (!exists) {
        if (mandatory)
            throw InvalidHash(key);
        return;
    }
    if (value.getType() != valueType)
        throw InvalidHash(key);
    output = value.get<typename std::remove_pointer<T>::type>();
}

// std::string specialization
template <>
void MakeXmlOpts::parseValue<std::string>(
        std::string &output, const QoreHashNode *hash,
        const std::string &key, qore_type_t valueType,
        bool mandatory);


#endif // !MAKE_XML_OPTS_H
