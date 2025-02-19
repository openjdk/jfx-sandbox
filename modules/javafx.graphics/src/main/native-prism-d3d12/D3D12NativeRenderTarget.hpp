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

#include "D3D12NativeSwapChain.hpp"
#include "D3D12NativeTexture.hpp"

#include "Internal/D3D12DescriptorData.hpp"


namespace D3D12 {

class NativeRenderTarget
{
    NIPtr<NativeDevice> mNativeDevice;
    NIPtr<NativeTexture> mTexture;
    NIPtr<NativeTexture> mDepthTexture;
    Internal::DescriptorData mDescriptors;
    Internal::DescriptorData mDSVDescriptor;
    UINT64 mWidth;
    UINT64 mHeight;
    bool mDepthTestEnabled;

public:
    NativeRenderTarget(const NIPtr<NativeDevice>& nativeDevice);
    ~NativeRenderTarget();

    bool Init(const NIPtr<NativeTexture>& texture);
    bool EnsureHasDepthBuffer();
    bool Refresh();
    void EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState);
    void EnsureDepthState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState);
    void SetDepthTestEnabled(bool enabled);

    inline UINT64 GetWidth() const
    {
        return mWidth;
    }

    inline UINT64 GetHeight() const
    {
        return mHeight;
    }

    inline bool IsDepthTestEnabled() const
    {
        return mDepthTestEnabled;
    }

    inline Internal::DescriptorData GetDescriptorData() const
    {
        return mDescriptors;
    }

    inline Internal::DescriptorData GetDSVDescriptor() const
    {
        return mDSVDescriptor;
    }

    inline const NIPtr<NativeTexture>& GetTexture() const
    {
        return mTexture;
    }

    inline const D3D12ResourcePtr& GetResource() const
    {
        return mTexture->GetResource();
    }

    inline const D3D12ResourcePtr& GetDepthResource() const
    {
        return mDepthTexture->GetResource();
    }

    inline UINT GetMSAASamples() const
    {
        return mTexture->GetMSAASamples();
    }
};

} // namespace D3D12
