/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSAnchorValue.h"
#include "EventTarget.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class Element;

namespace Style {

class BuilderState;

enum class AnchorPositionResolutionStage : uint8_t {
    Initial,
    FinishedCollectingAnchorNames,
    FoundAnchors,
    Resolved,
};

struct AnchorPositionedState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HashMap<String, WeakRef<Element, WeakPtrImplWithEventTargetData>> anchorElements;
    HashSet<String> anchorNames;
    AnchorPositionResolutionStage stage;
};

using AnchorsForAnchorName = HashMap<String, Vector<WeakRef<Element, WeakPtrImplWithEventTargetData>>>;
using AnchorPositionedStates = WeakHashMap<Element, std::unique_ptr<AnchorPositionedState>, WeakPtrImplWithEventTargetData>;

class AnchorPositionEvaluator {
public:
    static Length resolveAnchorValue(const BuilderState&, const CSSAnchorValue&);
    static void findAnchorsForAnchorPositionedElement(Ref<const Element> anchorPositionedElement);
};

} // namespace Style

} // namespace WebCore
