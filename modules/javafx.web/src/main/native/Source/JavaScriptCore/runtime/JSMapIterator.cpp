/*
 * Copyright (C) 2013-2024 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSMapIterator.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSMap.h"

namespace JSC {

const ClassInfo JSMapIterator::s_info = { "Map Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSMapIterator) };

JSMapIterator* JSMapIterator::createWithInitialValues(VM& vm, Structure* structure)
{
    JSMapIterator* iterator = new (NotNull, allocateCell<JSMapIterator>(vm)) JSMapIterator(vm, structure);
    iterator->finishCreation(vm);
    return iterator;
}

void JSMapIterator::finishCreation(JSGlobalObject* globalObject, JSMap* iteratedObject, IterationKind kind)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Base::finishCreation(vm);
    setEntry(vm, 0);
    setIteratedObject(vm, iteratedObject);

    iteratedObject->materializeIfNeeded(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    setStorage(vm, iteratedObject->m_storage.get());

    internalField(Field::Kind).set(vm, this, jsNumber(static_cast<int32_t>(kind)));
}

void JSMapIterator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSMapIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSMapIterator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSMapIterator);


JSC_DEFINE_HOST_FUNCTION(mapIteratorPrivateFuncMapIteratorNext, (JSGlobalObject * globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());

    VM& vm = globalObject->vm();
    JSCell* cell = callFrame->uncheckedArgument(0).asCell();
    if (cell == vm.orderedHashTableSentinel())
        return JSValue::encode(cell);
    return JSValue::encode(jsCast<JSMapIterator*>(cell)->next(vm));
}

JSC_DEFINE_HOST_FUNCTION(mapIteratorPrivateFuncMapIteratorKey, (JSGlobalObject * globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());

    VM& vm = globalObject->vm();
    JSCell* cell = callFrame->uncheckedArgument(0).asCell();
    if (cell == vm.orderedHashTableSentinel())
        return JSValue::encode(cell);
    return JSValue::encode(jsCast<JSMapIterator*>(cell)->nextKey(vm));
}

JSC_DEFINE_HOST_FUNCTION(mapIteratorPrivateFuncMapIteratorValue, (JSGlobalObject * globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());

    VM& vm = globalObject->vm();
    JSCell* cell = callFrame->uncheckedArgument(0).asCell();
    if (cell == vm.orderedHashTableSentinel())
        return JSValue::encode(cell);
    return JSValue::encode(jsCast<JSMapIterator*>(cell)->nextValue(vm));
}

}
