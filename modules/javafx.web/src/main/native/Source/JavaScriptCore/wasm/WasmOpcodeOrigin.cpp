/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "WasmOpcodeOrigin.h"

#include <wtf/text/MakeString.h>

#if ENABLE(WEBASSEMBLY_OMGJIT)

namespace JSC { namespace Wasm {

void OpcodeOrigin::dump(PrintStream& out) const
{
    switch (opcode()) {
#if USE(JSVALUE64)
    case OpType::ExtGC:
        out.print("{opcode: ", makeString(gcOpcode()), ", location: ", RawHex(location()), "}");
        break;
    case OpType::Ext1:
        out.print("{opcode: ", makeString(ext1Opcode()), ", location: ", RawHex(location()), "}");
        break;
    case OpType::ExtSIMD:
        out.print("{opcode: ", makeString(simdOpcode()), ", location: ", RawHex(location()), "}");
        break;
    case OpType::ExtAtomic:
        out.print("{opcode: ", makeString(atomicOpcode()), ", location: ", RawHex(location()), "}");
        break;
#endif
    default:
    out.print("{opcode: ", makeString(opcode()), ", location: ", RawHex(location()), "}");
        break;
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
