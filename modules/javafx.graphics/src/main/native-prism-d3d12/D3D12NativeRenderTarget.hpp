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

#include "D3D12NativeSwapChain.hpp"
#include "D3D12NativeTexture.hpp"

#include "Internal/D3D12DescriptorData.hpp"
#include "Internal/D3D12IRenderTarget.hpp"


namespace D3D12 {

class NativeRenderTarget: public Internal::IRenderTarget
{
    NIPtr<NativeDevice> mNativeDevice;
    NIPtr<NativeTexture> mTexture;
    NIPtr<NativeTexture> mDepthTexture;
    Internal::DescriptorData mDescriptors;
    Internal::DescriptorData mDSVDescriptor;
    uint64_t mWidth;
    uint64_t mHeight;
    bool mDepthTestEnabled;

public:
    NativeRenderTarget(const NIPtr<NativeDevice>& nativeDevice);
    ~NativeRenderTarget();

    bool Init(const NIPtr<NativeTexture>& texture);
    bool EnsureHasDepthBuffer();
    bool Refresh();
    void SetDepthTestEnabled(bool enabled);

    inline const NIPtr<NativeTexture>& GetTexture() const
    {
        return mTexture;
    }


    // IRenderTarget overrides
    virtual void EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState) override;
    virtual void EnsureDepthState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState) override;

    inline const D3D12ResourcePtr& GetResource() const
    {
        return mTexture->GetResource();
    }

    inline const D3D12ResourcePtr& GetDepthResource() const
    {
        return mDepthTexture->GetResource();
    }

    inline DXGI_FORMAT GetFormat() const override
    {
        return mTexture->GetFormat();
    }

    inline uint64_t GetWidth() const override
    {
        return mWidth;
    }

    inline uint64_t GetHeight() const override
    {
        return mHeight;
    }

    inline bool IsDepthTestEnabled() const override
    {
        return mDepthTestEnabled;
    }

    inline bool HasDepthTexture() const override
    {
        return (mDepthTexture != nullptr);
    }

    inline const Internal::DescriptorData& GetRTVDescriptorData() const override
    {
        return mDescriptors;
    }

    inline const Internal::DescriptorData& GetDSVDescriptorData() const override
    {
        return mDSVDescriptor;
    }

    inline uint32_t GetMSAASamples() const override
    {
        return mTexture->GetMSAASamples();
    }
};

} // namespace D3D12
