/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#pragma once

#include "../D3D12Common.hpp"

#include "../D3D12NativeTexture.hpp"

#include "D3D12LinearAllocator.hpp"
#include "D3D12RenderThreadExecutable.hpp"
#include "D3D12Waitable.hpp"

#include <array>


namespace D3D12 {
namespace Internal {

// collects steps that need to be processed by the Rendering Thread
class RenderPayload
{
private:
    static const uint32_t PAYLOAD_SIZE = 10240;
    static const uint32_t PAYLOAD_LIMIT = PAYLOAD_SIZE - 48;
    using StepList = std::array<RenderThreadExecutablePtr, PAYLOAD_SIZE>;

    NIPtr<Waitable> mWaitable;
    StepList mSteps;
    uint32_t mCurrentStep;

public:
    RenderPayload()
        : mWaitable(std::make_shared<Waitable>())
        , mSteps()
        , mCurrentStep(0)
    {
    }

    bool AddStep(RenderThreadExecutablePtr&& executable)
    {
        mSteps[mCurrentStep] = std::move(executable);
        mCurrentStep++;
        return (mCurrentStep > PAYLOAD_LIMIT);
    }

    bool ApplySteps(const RenderThreadContextPtr& context)
    {
        for (uint32_t i = 0; i < mCurrentStep; ++i)
        {
            mSteps[i]->Execute(context);
        }

        return mWaitable->Signal();
    }

    bool HasWork() const
    {
        return (mCurrentStep > 0);
    }

    inline const NIPtr<Waitable>& GetWaitable() const
    {
        return mWaitable;
    }
};

using RenderPayloadPtr = std::unique_ptr<RenderPayload, LinearAllocatorDeleter<RenderPayload>>;

} // namespace Internal
} // namespace D3D12
