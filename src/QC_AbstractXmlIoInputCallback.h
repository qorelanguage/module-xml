/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_AbstractXmlIoInputCallback.h

    Qore Programming Language

    Copyright (C) 2003 - 2022 Qore Technologies, s.r.o.

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

#ifndef _QORE_QC_ABSTRACTXMLIOINPUTCALLBACK_H

#define _QORE_QC_ABSTRACTXMLIOINPUTCALLBACK_H

#include "qore-xml-module.h"
#include "ql_xml.h"

#include <qore/InputStream.h>

DLLEXPORT extern qore_classid_t CID_ABSTRACTXMLIOINPUTCALLBACK;
DLLLOCAL QoreClass *initAbstractXmlIoInputCallbackClass(QoreNamespace& ns);

class AbstractXmlIoInputCallback : public AbstractPrivateData {
public:
    DLLLOCAL AbstractXmlIoInputCallback(QoreObject* self) : self(self) {
        // make a weak reference to the object
        self->tRef();
    }

    DLLLOCAL virtual ~AbstractXmlIoInputCallback() {
        assert(!input_stream);
        // remove the weak reference
        self->tDeref();
    }

    // libxml2 I/O callback: can we provide the requested resource; 1 = true, 0 = false
    DLLLOCAL int match(const char* filename) {
        assert(!input_stream);
        assert(xsink);

        // unhandled exceptions will appear on stdout
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(new QoreStringNode(filename), xsink);
        ValueHolder bufHolder(self->evalMethod("open", *args, xsink), xsink);
        //printd(5, "AbstractXmlIoInputCallback::match() '%s': %d\n", filename, (int)(bool)bufHolder);
        if (!bufHolder)
            return 0;
        input_stream = bufHolder.release().get<QoreObject>();
        return 1;
    }

    // libxml2 I/O callback: open the requested resource; returns nullptr on error
    DLLLOCAL void* open(const char* filename) {
        assert(input_stream);
        return input_stream;
    }

    // libxml2 I/O callback: read the requested resource; returns the number of bytes read or -1 in case of error
    DLLLOCAL int read(void* context, char* buffer, int len) {
        assert(input_stream);
        assert(context == input_stream);
        assert(len > 0);
        assert(buffer);
        assert(xsink);

        // unhandled exceptions will appear on stdout
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(len, xsink);
        ValueHolder bufHolder(input_stream->evalMethod("read", *args, xsink), xsink);
        //printd(5, "AbstractXmlIoInputCallback::read() %d: %d\n", len, (bool)bufHolder);
        if (!bufHolder)
            return -1;
        const BinaryNode* b = bufHolder->get<const BinaryNode>();
        assert(b->size() <= (size_t)len);
        memcpy(buffer, b->getPtr(), b->size());
        return (int)b->size();
    }

    // libxml2 I/O callback: close the requested resource
    DLLLOCAL int close(void* context) {
        assert(input_stream);
        assert(context == input_stream);
        assert(xsink);

        input_stream->deref(xsink);
        input_stream = nullptr;
        return 0;
    }

    // set exception context
    DLLLOCAL void setExceptionContext(ExceptionSink* xs) {
        assert(!xsink);
        xsink = xs;
    }

    // clear exception context
    DLLLOCAL void clearExceptionContext() {
        assert(xsink);
        xsink = nullptr;
    }

protected:
    QoreObject* self;
    QoreObject* input_stream = nullptr;
    // current exception context
    ExceptionSink* xsink = nullptr;
};

#endif
