/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12Common.hpp"

#include <functional>


namespace D3D12 {
namespace Internal {

class Waitable
{
public:
    using WaitFinishedCallback = std::function<bool(uint64_t)>;

private:
    // TODO: D3D12: potential optimization - fetch the Event handle from a pool
    HANDLE mEventHandle;
    uint64_t mFenceValue;
    WaitFinishedCallback mWaitFinishedCallback;
    bool mWaitCompleted;

public:
    Waitable();
    Waitable(uint64_t fenceValue);
    Waitable(uint64_t fenceValue, const WaitFinishedCallback& waitCallback);
    ~Waitable();

    bool Wait();

    inline void SetFinishedCallback(const WaitFinishedCallback& waitCallback)
    {
        mWaitFinishedCallback = waitCallback;
    }

    inline const HANDLE& GetHandle() const
    {
        return mEventHandle;
    }
};

} // namespace Internal
} // namespace D3D12
