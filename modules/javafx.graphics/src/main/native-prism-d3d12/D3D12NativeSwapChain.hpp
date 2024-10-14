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
#include "D3D12Waitable.hpp"

#include <vector>
#include <deque>


namespace D3D12 {

class NativeSwapChain
{
    NIPtr<NativeDevice> mNativeDevice;
    DXGISwapChainPtr mSwapChain;
    std::vector<D3D12ResourcePtr> mBuffers;
    std::vector<D3D12_RESOURCE_STATES> mStates;
    std::deque<NIPtr<Internal::Waitable>> mPastFrameWaitables;
    UINT mBufferCount;
    UINT mCurrentBufferIdx;
    RECT mDirtyRegion;
    DXGI_FORMAT mFormat;
    bool mVSyncEnabled;
    UINT mSwapChainFlags;
    UINT mSwapInterval;
    UINT mPresentFlags;
    UINT mWidth;
    UINT mHeight;

    bool GetSwapChainBuffers(UINT count);

public:
    NativeSwapChain(const NIPtr<NativeDevice>& nativeDevice);
    ~NativeSwapChain();

    bool Init(const DXGIFactoryPtr& factory, HWND hwnd);

    void EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState);
    bool Prepare(LONG left, LONG top, LONG right, LONG bottom);
    bool Present();
    bool Resize(UINT width, UINT height);

    inline const D3D12ResourcePtr& GetBuffer(int index)
    {
        return mBuffers[index];
    }

    inline DXGI_FORMAT GetFormat() const
    {
        return mFormat;
    }

    inline UINT GetBufferCount() const
    {
        return static_cast<UINT>(mBufferCount);
    }

    inline UINT GetCurrentBufferIndex() const
    {
        return mCurrentBufferIdx;
    }

    inline const D3D12ResourcePtr& GetCurrentBuffer()
    {
        return GetBuffer(mCurrentBufferIdx);
    }

    inline UINT GetWidth() const
    {
        return mWidth;
    }

    inline UINT GetHeight() const
    {
        return mHeight;
    }
};

} // namespace D3D12
