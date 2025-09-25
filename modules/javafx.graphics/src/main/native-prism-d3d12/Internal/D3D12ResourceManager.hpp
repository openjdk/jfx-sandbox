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

#include "../D3D12Common.hpp"
#include "../D3D12Constants.hpp"

#include "D3D12IWaitableOperation.hpp"
#include "D3D12RingBuffer.hpp"
#include "D3D12RingDescriptorHeap.hpp"
#include "D3D12Shader.hpp"
#include "D3D12TextureBase.hpp"

#include <unordered_map>


namespace D3D12 {
namespace Internal {

struct SamplerBindingIdentifier
{
    std::array<D3D12::Internal::SamplerDesc, Constants::MAX_TEXTURE_UNITS> descs;

    inline bool operator==(const SamplerBindingIdentifier& other) const
    {
        for (uint32_t i = 0; i < Constants::MAX_TEXTURE_UNITS; ++i)
        {
            if (descs[i] != other.descs[i]) return false;
        }
        return true;
    }
};

} // namespace Internal
} // namespace D3D12

template<>
struct std::hash<D3D12::Internal::SamplerBindingIdentifier>
{
    std::size_t operator()(const D3D12::Internal::SamplerBindingIdentifier& k) const
    {
        static_assert(
            (D3D12::Internal::SamplerDesc::TOTAL_BITS * D3D12::Constants::MAX_TEXTURE_UNITS) <= (sizeof(std::size_t) * 8),
            "Too many sampler settings used or too many texture units could potentially be used. "
            "Consider lowering those or rewriting the hashing functions"
        );

        std::size_t result = 0;
        for (uint32_t i = 0; i < D3D12::Constants::MAX_TEXTURE_UNITS; ++i)
        {
            result <<= D3D12::Internal::SamplerDesc::TOTAL_BITS;
            result |= std::hash<D3D12::Internal::SamplerDesc>()(k.descs[i]);
        }
        return result;
    }
};

namespace D3D12 {
namespace Internal {

class ResourceManager: public IWaitableOperation
{
    struct RuntimeParametersStash
    {
        NIPtr<Shader> vertexShader;
        NIPtr<Shader> pixelShader;
        TextureBank textures;
    } mRuntimeParametersStash;

    NIPtr<NativeDevice> mNativeDevice;
    NIPtr<Shader> mVertexShader;
    NIPtr<Shader> mPixelShader;
    TextureBank mTextures;
    RingDescriptorHeap mDescriptorHeap;
    RingDescriptorHeap mSamplerHeap;
    RingBuffer mConstantRingBuffer;
    SamplerBindingIdentifier mCurrentSamplerBinding;
    std::unordered_map<SamplerBindingIdentifier, Internal::DescriptorData> mLastSamplerDescriptors;
    uint32_t mSamplerRegionReserveProfilerID;

    // Compute Resources
    NIPtr<Shader> mComputeShader;

    void UpdateTextureDescriptorTable(const DescriptorData& dtable);
    bool PrepareConstants(const NIPtr<Shader>& shaderResourceData);
    bool PrepareTextureViews(const NIPtr<Shader>& shaderResourceData);
    bool PrepareSamplers(const NIPtr<Shader>& shaderResourceData);
    bool PrepareShaderResources(const NIPtr<Shader>& shader);

public:
    ResourceManager(const NIPtr<NativeDevice>& nativeDevice);
    ~ResourceManager();

    bool Init();
    void DeclareRingResources();
    void DeclareComputeRingResources();
    bool PrepareResources();
    bool PrepareComputeResources();
    void ApplyResources(const D3D12GraphicsCommandListPtr& commandList) const;
    void ApplyComputeResources(const D3D12GraphicsCommandListPtr& commandList) const;
    void ClearTextureUnit(uint32_t slot);
    void EnsureStates(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES state);
    void SetVertexShader(const NIPtr<Shader>& shader);
    void SetPixelShader(const NIPtr<Shader>& shader);
    void SetComputeShader(const NIPtr<Shader>& shader);
    void SetTexture(uint32_t slot, const NIPtr<TextureBase>& tex);

    void StashParameters();
    void RestoreStashedParameters();

    virtual void OnQueueSignal(uint64_t fenceValue) override;
    virtual void OnFenceSignaled(uint64_t fenceValue) override;

    inline const NIPtr<TextureBase>& GetTexture(uint32_t slot) const
    {
        return mTextures[slot];
    }

    inline const D3D12DescriptorHeapPtr& GetHeap() const
    {
        return mDescriptorHeap.GetHeap();
    }

    inline const D3D12DescriptorHeapPtr& GetSamplerHeap() const
    {
        return mSamplerHeap.GetHeap();
    }
};

} // namespace D3D12
} // namespace Internal
