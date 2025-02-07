/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12SamplerStorage.hpp"

#include "D3D12NativeDevice.hpp"

#include "../D3D12Constants.hpp"


namespace D3D12 {
namespace Internal {

D3D12_SAMPLER_DESC SamplerStorage::CreateDefaultSamplerDesc() const
{
    D3D12_SAMPLER_DESC desc;
    D3D12NI_ZERO_STRUCT(desc);
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    desc.MipLODBias = 0;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    return desc;
}

SamplerStorage::SamplerStorage(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mSamplerHeap(nativeDevice)
{
}

bool SamplerStorage::Init()
{
    if (!mSamplerHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, false))
    {
        D3D12NI_LOG_ERROR("Failed to initialize Sampler Heap");
        return false;
    }


    D3D12_SAMPLER_DESC clampZeroDesc = CreateDefaultSamplerDesc();

    mClampZeroSampler = mSamplerHeap.Allocate(1);
    if (!mClampZeroSampler)
    {
        D3D12NI_LOG_ERROR("Failed to allocate Clamp Zero Sampler");
        return false;
    }

    mNativeDevice->GetDevice()->CreateSampler(&clampZeroDesc, mClampZeroSampler.CPU(0));


    D3D12_SAMPLER_DESC clampEdgeDesc = CreateDefaultSamplerDesc();
    clampEdgeDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    clampEdgeDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    clampEdgeDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    mClampEdgeSampler = mSamplerHeap.Allocate(1);
    if (!mClampEdgeSampler)
    {
        D3D12NI_LOG_ERROR("Failed to allocate Clamp Edge Sampler");
        return false;
    }

    mNativeDevice->GetDevice()->CreateSampler(&clampEdgeDesc, mClampEdgeSampler.CPU(0));


    D3D12_SAMPLER_DESC repeatDesc = CreateDefaultSamplerDesc();
    repeatDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    repeatDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    repeatDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    mRepeatSampler = mSamplerHeap.Allocate(1);
    if (!mRepeatSampler)
    {
        D3D12NI_LOG_ERROR("Failed to allocate Repeat Sampler");
        return false;
    }

    mNativeDevice->GetDevice()->CreateSampler(&repeatDesc, mRepeatSampler.CPU(0));


    return true;
}

const DescriptorData& SamplerStorage::GetSampler(TextureWrapMode wrapMode) const
{
    switch (wrapMode)
    {
    case TextureWrapMode::CLAMP_NOT_NEEDED:
    case TextureWrapMode::CLAMP_TO_ZERO:
        return mClampZeroSampler;
    case TextureWrapMode::CLAMP_TO_EDGE:
        return mClampEdgeSampler;
    case TextureWrapMode::REPEAT:
        return mRepeatSampler;
    default:
        return mNullSampler;
    }
}

} // namespace Internal
} // namespace D3D12
