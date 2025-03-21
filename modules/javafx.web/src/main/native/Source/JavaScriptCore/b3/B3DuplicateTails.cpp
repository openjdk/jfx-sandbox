/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "B3DuplicateTails.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3BreakCriticalEdges.h"
#include "B3FixSSA.h"
#include "B3InsertionSet.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3ValueInlines.h"
#include <wtf/IndexSet.h>

namespace JSC { namespace B3 {

namespace {

namespace B3DuplicateTailsInternal {
static constexpr bool verbose = false;
}

class DuplicateTails {
public:
    DuplicateTails(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
        , m_maxSize(Options::maxB3TailDupBlockSize())
        , m_maxSuccessors(Options::maxB3TailDupBlockSuccessors())
    {
    }

    void run()
    {
        // Breaking critical edges introduces blocks that jump to things. Those Jumps' successors
        // become candidates for tail duplication. Prior to critical edge breaking, some of those
        // Jumps would have been Branches, and so no tail duplication would have happened.
        breakCriticalEdges(m_proc);

        // Find blocks that would be candidates for tail duplication. They must be small enough
        // and they much not have too many successors.

        m_proc.resetValueOwners();

        IndexSet<BasicBlock*> candidates;

        for (BasicBlock* block : m_proc) {
            if (block->size() > m_maxSize)
                continue;
            if (block->numSuccessors() > m_maxSuccessors)
                continue;
            if (block->last()->type() != Void) // Demoting doesn't handle terminals with values.
                continue;

            bool canCopyBlock = true;
            for (Value* value : *block) {
                if (value->kind().isCloningForbidden()) {
                    canCopyBlock = false;
                    break;
                }
            }

            if (canCopyBlock)
                candidates.add(block);
        }

        // Collect the set of values that must be de-SSA'd.
        IndexSet<Value*> valuesToDemote;
        for (BasicBlock* block : m_proc) {
            for (Value* value : *block) {
                if (value->opcode() == Phi && candidates.contains(block))
                    valuesToDemote.add(value);
                for (Value* child : value->children()) {
                    if (child->owner != block && candidates.contains(child->owner))
                        valuesToDemote.add(child);
                }
            }
        }
        demoteValues(m_proc, valuesToDemote);
        if (B3DuplicateTailsInternal::verbose) {
            dataLog("Procedure after value demotion:\n");
            dataLog(m_proc);
        }

        for (BasicBlock* block : m_proc) {
            if (block->last()->opcode() != Jump)
                continue;

            BasicBlock* tail = block->successorBlock(0);
            if (!candidates.contains(tail))
                continue;

            // Don't tail duplicate a trivial self-loop, because the code below can't handle block and
            // tail being the same block.
            if (block == tail)
                continue;

            // We're about to change 'block'. Make sure that nobody duplicates block after this
            // point.
            candidates.remove(block);

            if (B3DuplicateTailsInternal::verbose)
                dataLog("Duplicating ", *tail, " into ", *block, "\n");

            block->removeLast(m_proc);

            HashMap<Value*, Value*> map;
            for (Value* value : *tail) {
                Value* clone = m_proc.clone(value);
                for (Value*& child : clone->children()) {
                    if (Value* replacement = map.get(child))
                        child = replacement;
                }
                if (value->type() != Void)
                    map.add(value, clone);
                block->append(clone);
            }
            block->successors() = tail->successors();
        }

        m_proc.resetReachability();
        m_proc.invalidateCFG();
    }

private:

    Procedure& m_proc;
    InsertionSet m_insertionSet;
    unsigned m_maxSize;
    unsigned m_maxSuccessors;
};

} // anonymous namespace

void duplicateTails(Procedure& proc)
{
    PhaseScope phaseScope(proc, "duplicateTails"_s);
    DuplicateTails duplicateTails(proc);
    duplicateTails.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

