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

#include "../D3D12NativeTexture.hpp"

#include "D3D12Shader.hpp"
#include "D3D12RingBuffer.hpp"
#include "D3D12RingDescriptorHeap.hpp"
#include "D3D12IWaitableOperation.hpp"


namespace D3D12 {
namespace Internal {

class ResourceManager: public IWaitableOperation
{
    struct RuntimeParametersStash
    {
        NIPtr<Shader> vertexShader;
        NIPtr<Shader> pixelShader;
        NativeTextureBank textures;
    } mRuntimeParametersStash;

    NIPtr<NativeDevice> mNativeDevice;
    NIPtr<Shader> mVertexShader;
    NIPtr<Shader> mPixelShader;
    NativeTextureBank mTextures;
    RingDescriptorHeap mDescriptorHeap;
    RingDescriptorHeap mSamplerHeap;
    RingBuffer mConstantRingBuffer;
    Internal::DescriptorData mLastSamplerDescriptors;
    bool mSamplersDirty;

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
    bool PrepareResources();
    bool PrepareComputeResources();
    void ApplyResources(const D3D12GraphicsCommandListPtr& commandList) const;
    void ApplyComputeResources(const D3D12GraphicsCommandListPtr& commandList) const;
    void ClearTextureUnit(uint32_t slot);
    void EnsureStates(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES state);
    void SetVertexShader(const NIPtr<Shader>& shader);
    void SetPixelShader(const NIPtr<Shader>& shader);
    void SetComputeShader(const NIPtr<Shader>& shader);
    void SetTexture(uint32_t slot, const NIPtr<NativeTexture>& tex);

    void StashParameters();
    void RestoreStashedParameters();

    virtual void OnQueueSignal(uint64_t fenceValue) override;
    virtual void OnFenceSignaled(uint64_t fenceValue) override;

    inline const NIPtr<NativeTexture>& GetTexture(uint32_t slot) const
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
