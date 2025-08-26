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
#include "D3D12Constants.hpp"

#include "Internal/D3D12DescriptorData.hpp"
#include "Internal/D3D12SamplerDesc.hpp"

#include <array>


namespace D3D12 {

class NativeTexture
{
    static uint64_t textureCounter;
    static uint64_t depthTextureCounter;
    static uint64_t rttextureCounter;

    NIPtr<NativeDevice> mNativeDevice;
    D3D12ResourcePtr mTextureResource;
    D3D12_RESOURCE_DESC mResourceDesc;
    std::vector<D3D12_RESOURCE_STATES> mStates; // one state per subresource
    std::wstring mDebugName;
    UINT mMipLevels;
    Internal::SamplerDesc mSamplerDesc;
    Internal::DescriptorData mSRVDescriptor;

    bool InitInternal(const D3D12_RESOURCE_DESC& desc);

public:
    NativeTexture(const NIPtr<NativeDevice>& nativeDevice);
    ~NativeTexture();

    bool Init(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
              TextureUsage usage, TextureWrapMode wrapMode, int samples, bool useMipmap);
    UINT64 GetSize();
    bool Resize(UINT width, UINT height);
    void SetSamplerParameters(TextureWrapMode wrapMode, bool isLinear);

    void EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void WriteSRVToDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& descriptorCpu, UINT mipLevels = 0, UINT mostDetailedMip = 0);
    void WriteUAVToDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& descriptorCpu, UINT mipSlice);

    inline UINT64 GetWidth() const
    {
        return mResourceDesc.Width;
    }

    inline UINT64 GetHeight() const
    {
        return mResourceDesc.Height;
    }

    inline DXGI_FORMAT GetFormat() const
    {
        return mResourceDesc.Format;
    }

    inline const D3D12ResourcePtr& GetResource() const
    {
        return mTextureResource;
    }

    inline const std::wstring& GetDebugName() const
    {
        return mDebugName;
    }

    inline UINT GetMSAASamples() const
    {
        return mResourceDesc.SampleDesc.Count;
    }

    inline UINT GetMipLevels() const
    {
        return mMipLevels;
    }

    inline const Internal::SamplerDesc& GetSamplerDesc() const
    {
        return mSamplerDesc;
    }

    inline bool HasMipmaps() const
    {
        return (mMipLevels > 1);
    }
};

// Collection of Textures used by the backend during rendering
using NativeTextureBank = std::array<NIPtr<NativeTexture>, Constants::MAX_TEXTURE_UNITS>;

} // namespace D3D12
