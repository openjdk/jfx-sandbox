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

#pragma once

#include "D3D12Common.hpp"
#include "D3D12Constants.hpp"

#include "D3D12SamplerDesc.hpp"

#include <array>


namespace D3D12 {
namespace Internal {

/**
 *
 */
class TextureBase
{
protected:
    D3D12ResourcePtr mResource;
    std::vector<D3D12_RESOURCE_STATES> mStates;
    SamplerDesc mSamplerDesc;
    std::string mDebugName;

public:
    TextureBase()
        : mResource()
        , mStates()
        , mSamplerDesc()
        , mDebugName()
    {}

    void Init(const D3D12ResourcePtr& resource, uint32_t subresourceCount, D3D12_RESOURCE_STATES initialState)
    {
        mResource = resource;
        mStates.resize(subresourceCount);
        SetResourceState(initialState);
    }

    virtual void WriteSRVToDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& descriptorCpu, UINT mipLevels = 0, UINT mostDetailedMip = 0) {}
    virtual void WriteUAVToDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& descriptorCpu, UINT mipSlice) {}

    inline const D3D12ResourcePtr& GetResource() const
    {
        return mResource;
    }

    inline D3D12_RESOURCE_STATES GetResourceState(uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) return mStates[0];
        else return mStates[subresource];
    }

    inline void SetResourceState(D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        {
            for (uint32_t i = 0; i < mStates.size(); ++i)
            {
                mStates[i] = newState;
            }
        }
        else
        {
            mStates[subresource] = newState;
        }
    }

    inline const SamplerDesc& GetSamplerDesc() const
    {
        return mSamplerDesc;
    }

    inline const std::string& GetName() const
    {
        return mDebugName;
    }
};

// Collection of Textures used by the backend during rendering
using TextureBank = std::array<NIPtr<TextureBase>, Constants::MAX_TEXTURE_UNITS>;

} // namespace Internal
} // namespace D3D12
