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

#include "D3D12ResourceManager.hpp"

#include "../D3D12NativeDevice.hpp"


namespace D3D12 {
namespace Internal {

ResourceManager::ResourceManager(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mPixelShader()
    , mTextures()
    , mSRVHeap(nativeDevice)
    , mSamplerHeap(nativeDevice)
    , mShaderHelpers()
{
    // TODO: D3D12: PERF fine-tune ring descriptor heap parameters
    if (!mSRVHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, 1024, 768))
    {
        D3D12NI_LOG_ERROR("Failed to initialize SRV Ring Descriptor Heap");
    }

    mSRVHeap.SetDebugName("SRV Descriptor Heap");

    // we always have the same amount of samplers as descriptors
    // so keep below heap the same size as mSRVHeap
    if (!mSamplerHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true, 1024, 768))
    {
        D3D12NI_LOG_ERROR("Failed to initialize Sampler Ring Descriptor Heap");
    }

    mSamplerHeap.SetDebugName("Sampler Heap");

    mShaderHelpers.constantAllocator = [this](size_t size, size_t alignment) -> RingBuffer::Region
        {
            return mNativeDevice->GetConstantRingBuffer()->Reserve(size, alignment);
        };
    mShaderHelpers.rvAllocator = [this](size_t count) -> DescriptorData
        {
            return mSRVHeap.Reserve(count);
        };
    mShaderHelpers.samplerAllocator = [this](size_t count) -> DescriptorData
        {
            return mSamplerHeap.Reserve(count);
        };
    mShaderHelpers.cbvCreator = [this](D3D12_GPU_VIRTUAL_ADDRESS cbufferPtr, UINT size, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbufferPtr;
            cbvDesc.SizeInBytes = size;
            mNativeDevice->GetDevice()->CreateConstantBufferView(&cbvDesc, destDescriptor);
        };
    mShaderHelpers.nullSRVCreator = [this](D3D12_SRV_DIMENSION dimension, D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            D3D12NI_ZERO_STRUCT(srvDesc);
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = dimension;
            mNativeDevice->GetDevice()->CreateShaderResourceView(nullptr, &srvDesc, descriptor);
        };
}

ResourceManager::~ResourceManager()
{
    for (uint32_t i = 0; i < Constants::MAX_TEXTURE_UNITS; ++i)
    {
        mTextures[i].reset();
    }

    mVertexShader.reset();
    mPixelShader.reset();

    D3D12NI_LOG_DEBUG("ResourceManager destroyed");
}

// Assumes it is called only if attached shader resources change or we switch to a new Command List
bool ResourceManager::PrepareResources()
{
    if (!mVertexShader->PrepareShaderResources(mShaderHelpers, mTextures)) return false;
    if (!mPixelShader->PrepareShaderResources(mShaderHelpers, mTextures)) return false;

    return true;
}

void ResourceManager::ApplyResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    mVertexShader->ApplyShaderResources(commandList);
    mPixelShader->ApplyShaderResources(commandList);
}

bool ResourceManager::PrepareComputeResources()
{
    return mComputeShader->PrepareShaderResources(mShaderHelpers, mTextures);
}

void ResourceManager::ApplyComputeResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    mComputeShader->ApplyShaderResources(commandList);
}

void ResourceManager::ClearTextureUnit(uint32_t slot)
{
    D3D12NI_ASSERT(slot < Constants::MAX_TEXTURE_UNITS, "Provided too high slot %u (max %u)", slot, Constants::MAX_TEXTURE_UNITS);

    mTextures[slot].reset();
}

void ResourceManager::EnsureStates(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES state)
{
    for (uint32_t i = 0; i < Constants::MAX_TEXTURE_UNITS; ++i)
    {
        if (mTextures[i])
        {
            mTextures[i]->EnsureState(commandList, state);
        }
    }
}

void ResourceManager::SetVertexShader(const NIPtr<Shader>& shader)
{
    mVertexShader = shader;
}

void ResourceManager::SetPixelShader(const NIPtr<Shader>& shader)
{
    mPixelShader = shader;
}

void ResourceManager::SetComputeShader(const NIPtr<Shader>& shader)
{
    mComputeShader = shader;
}

void ResourceManager::SetTexture(uint32_t slot, const NIPtr<NativeTexture>& tex)
{
    D3D12NI_ASSERT(slot < Constants::MAX_TEXTURE_UNITS, "Provided too high slot %u (max %u)", slot, Constants::MAX_TEXTURE_UNITS);

    mTextures[slot] = tex;
}

void ResourceManager::StashParameters()
{
    mRuntimeParametersStash.vertexShader = mVertexShader;
    mRuntimeParametersStash.pixelShader = mPixelShader;

    for (size_t i = 0; i < mTextures.size(); ++i)
    {
        mRuntimeParametersStash.textures[i] = mTextures[i];
    }
}

void ResourceManager::RestoreStashedParameters()
{
    mVertexShader = mRuntimeParametersStash.vertexShader;
    mPixelShader = mRuntimeParametersStash.pixelShader;

    for (size_t i = 0; i < mTextures.size(); ++i)
    {
        mTextures[i] = mRuntimeParametersStash.textures[i];
    }
}

} // namespace Internal
} // namespace D3D12
