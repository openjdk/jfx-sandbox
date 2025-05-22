/*
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12Waitable.hpp"


namespace D3D12 {
namespace Internal {

Waitable::Waitable()
    : Waitable(0, WaitFinishedCallback())
{}

Waitable::Waitable(uint64_t fenceValue)
    : Waitable(fenceValue, WaitFinishedCallback())
{}

Waitable::Waitable(uint64_t fenceValue, const WaitFinishedCallback& waitCallback)
    : mEventHandle(NULL)
    , mFenceValue(fenceValue)
    , mWaitFinishedCallback(waitCallback)
    , mWaitCompleted(false)
{
    mEventHandle = CreateEventEx(NULL, NULL, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
}

Waitable::Waitable(Waitable&& other)
    : mEventHandle(std::move(other.mEventHandle))
    , mFenceValue(std::move(other.mFenceValue))
    , mWaitFinishedCallback(std::move(other.mWaitFinishedCallback))
    , mWaitCompleted(std::move(other.mWaitCompleted))
{
    other.mEventHandle = 0;
}

Waitable::~Waitable()
{
    if (mEventHandle)
    {
        CloseHandle(mEventHandle);
    }
}

bool Waitable::Wait()
{
    if (mWaitCompleted)
    {
        // we already waited, skip whatever happens next
        return true;
    }

    DWORD ret = WaitForSingleObject(mEventHandle, INFINITE);

    if (ret != WAIT_OBJECT_0)
    {
        D3D12NI_LOG_ERROR("Failed to wait for Event Handle: %x; last error: %x", ret, GetLastError());
        return false;
    }

    mWaitCompleted = true;

    if (mWaitFinishedCallback)
    {
        return mWaitFinishedCallback(mFenceValue);
    }

    return true;
}

} // namespace Internal
} // namespace D3D12
