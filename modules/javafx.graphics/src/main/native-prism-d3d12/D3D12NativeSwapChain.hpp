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

#pragma once

#include "D3D12Common.hpp"

#include "Internal/D3D12IRenderTarget.hpp"
#include "Internal/D3D12IWaitableOperation.hpp"

#include <vector>
#include <deque>


namespace D3D12 {

class NativeSwapChain: public Internal::IRenderTarget, Internal::IWaitableOperation
{
    NIPtr<NativeDevice> mNativeDevice;
    DXGISwapChainPtr mSwapChain;
    std::vector<D3D12ResourcePtr> mBuffers;
    std::vector<D3D12_RESOURCE_STATES> mStates;
    std::vector<Internal::DescriptorData> mRTVs;
    std::vector<uint64_t> mWaitFenceValues;
    UINT mSubmittedFrameCount;
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

    // for GetDepthResource()
    D3D12ResourcePtr mNullResource;

    bool GetSwapChainBuffers(UINT count);

public:
    NativeSwapChain(const NIPtr<NativeDevice>& nativeDevice);
    ~NativeSwapChain();

    bool Init(const DXGIFactoryPtr& factory, HWND hwnd);
    bool Prepare(LONG left, LONG top, LONG right, LONG bottom);
    bool Present();
    bool Resize(UINT width, UINT height);

    inline const D3D12ResourcePtr& GetBuffer(int index) const
    {
        return mBuffers[index];
    }

    inline UINT GetBufferCount() const
    {
        return static_cast<UINT>(mBufferCount);
    }

    inline UINT GetCurrentBufferIndex() const
    {
        return mCurrentBufferIdx;
    }

    inline const D3D12ResourcePtr& GetCurrentBuffer() const
    {
        return GetBuffer(mCurrentBufferIdx);
    }


    // IWaitableOperation overrides

    void OnQueueSignal(uint64_t fenceValue) override;
    void OnFenceSignaled(uint64_t fenceValue) override;


    // IRenderTarget overrides

    virtual void EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState) override;
    virtual void EnsureDepthState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState) override
    {
        // noop, SwapChain has no depth buffer
    }

    inline const D3D12ResourcePtr& GetResource() const override
    {
        return GetCurrentBuffer();
    }

    inline const D3D12ResourcePtr& GetDepthResource() const override
    {
        D3D12NI_ASSERT(false, "NativeSwapChain has no depth resource. This should not happen.");
        return mNullResource;
    }

    inline DXGI_FORMAT GetFormat() const
    {
        return mFormat;
    }

    inline uint64_t GetWidth() const override
    {
        return mWidth;
    }

    inline uint64_t GetHeight() const override
    {
        return mHeight;
    }

    inline bool HasDepthTexture() const
    {
        return false;
    }

    inline bool IsDepthTestEnabled() const
    {
        return false;
    }

    inline uint32_t GetMSAASamples() const
    {
        return 1;
    }

    inline const Internal::DescriptorData& GetRTVDescriptorData() const
    {
        return mRTVs[GetCurrentBufferIndex()];
    }

    inline const Internal::DescriptorData& GetDSVDescriptorData() const
    {
        return Internal::DescriptorData::NULL_DESCRIPTOR;
    }

};

} // namespace D3D12
