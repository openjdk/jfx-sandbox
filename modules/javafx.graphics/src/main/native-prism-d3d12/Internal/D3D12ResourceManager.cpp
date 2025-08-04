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

#include "D3D12Utils.hpp"


namespace D3D12 {
namespace Internal {

bool ResourceManager::PrepareConstants(const NIPtr<Shader>& shader)
{
    // quietly exit early if constants do not need to be re-prepared
    if (!shader->AreConstantsDirty()) return true;

    const Shader::ResourceData& resourceData = shader->GetResourceData();
    Shader::DescriptorData& descriptors = shader->GetDescriptorData();

    if (resourceData.cbufferDirectSize > 0)
    {
        descriptors.ConstantDataDirectRegion = mConstantRingBuffer.Reserve(resourceData.cbufferDirectSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        if (!descriptors.ConstantDataDirectRegion)
        {
            D3D12NI_LOG_ERROR("Failed to reserve Constant Ring Buffer space for direct constant data");
            return false;
        }

        // We don't create a direct CBV here, instead it is created when calling ID3D12CommandList::SetGraphicsRootConstantBufferView()
        // so this is Shader's responsibility to be done in ApplyDescriptors(commandList)
    }

    if (resourceData.cbufferDTableCount > 0)
    {
        D3D12NI_ASSERT(resourceData.cbufferDTableSingleSize > 0, "Requested CBV DTable allocation, yet single size is zero");

        size_t singleCBufferSizeAligned = Utils::Align<size_t>(resourceData.cbufferDTableSingleSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        descriptors.ConstantDataDTableRegions = mConstantRingBuffer.Reserve(singleCBufferSizeAligned * resourceData.cbufferDTableCount, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        if (!descriptors.ConstantDataDTableRegions)
        {
            D3D12NI_LOG_ERROR("Failed to reserve Constant Ring Buffer space for descriptor table constant data of size %zd", singleCBufferSizeAligned * resourceData.cbufferDTableCount);
            return false;
        }

        descriptors.CBufferTableDescriptors = mDescriptorHeap.Reserve(resourceData.cbufferDTableCount);
        if (!descriptors.CBufferTableDescriptors)
        {
            D3D12NI_LOG_ERROR("Failed to reserve Ring Descriptor Heap space for CBV DTable of size %d", resourceData.cbufferDTableCount);
            return false;
        }

        // Descriptor Table must be populated with CBViews, we have their locations and sizes so we will do that right now
        // Shader will later populate the Ring Buffer regions with constant data, but the CBViews can be created now
        for (uint32_t i = 0; i < resourceData.cbufferDTableCount; ++i)
        {
            size_t offset = i * singleCBufferSizeAligned;

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            D3D12NI_ZERO_STRUCT(cbvDesc);
            cbvDesc.BufferLocation = descriptors.ConstantDataDTableRegions.gpu + offset;
            cbvDesc.SizeInBytes = static_cast<UINT>(singleCBufferSizeAligned); // this has to be size aligned to 256 bytes, not the actual (smaller) unaligned size
            mNativeDevice->GetDevice()->CreateConstantBufferView(&cbvDesc, descriptors.CBufferTableDescriptors.CPU(i));
        }
    }

    return true;
}

bool ResourceManager::PrepareTextureViews(const NIPtr<Shader>& shader)
{
    if (!mTexturesDirty) return true;

    const Shader::ResourceData& resourceData = shader->GetResourceData();
    Shader::DescriptorData& descriptors = shader->GetDescriptorData();

    if (resourceData.textureCount > 0)
    {
        descriptors.SRVDescriptors = mDescriptorHeap.Reserve(resourceData.textureCount);

        for (uint32_t i = 0; i < resourceData.textureCount; ++i)
        {
            // NULL textures will be auto-populated with null descriptors to prevent UB
            // We don't do other writes because of how Shaders might want to bind the textures
            // ex. MipmapGenComputeShader will bind subresources instead of Textures as a whole
            if (!mTextures[i])
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
                D3D12NI_ZERO_STRUCT(srvDesc);
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                mNativeDevice->GetDevice()->CreateShaderResourceView(nullptr, &srvDesc, descriptors.SRVDescriptors.CPU(i));
            }
        }
    }

    if (resourceData.uavCount > 0)
    {
        descriptors.UAVDescriptors = mDescriptorHeap.Reserve(resourceData.uavCount);
    }

    return true;
}

bool ResourceManager::PrepareSamplers(const NIPtr<Shader>& shader)
{
    const Shader::ResourceData& resourceData = shader->GetResourceData();
    Shader::DescriptorData& descriptors = shader->GetDescriptorData();

    // Reserve samplers and write them if needed
    if (resourceData.samplerCount > 0)
    {
        if (mSamplersDirty)
        {
            mLastSamplerDescriptors = mSamplerHeap.Reserve(resourceData.textureCount);

            for (uint32_t i = 0; i < resourceData.samplerCount; ++i)
            {
                mTextures[i]->WriteSamplerToDescriptor(mLastSamplerDescriptors.CPU(i));
            }

            mSamplersDirty = false;
        }

        descriptors.SamplerDescriptors = mLastSamplerDescriptors;
    }

    return true;
}

bool ResourceManager::PrepareShaderResources(const NIPtr<Shader>& shader)
{
    // We need to allocate space on respective heaps here:
    //  - Reserve RingBuffer space for CBVs, allocate and create CBV Descriptors for CBV DTable (if needed)
    //  - Allocate SRV Descriptors (don't create SRVs! those will be created by PrepareDescriptors() depending on how each shader uses them)
    //     - We need a way to inform the shaders to skip the SRV Descriptor write if there is no change in textures
    //  - Allocate Sampler descriptors, but only if they're needed
    //     - We need a way to inform the shaders to skip the Sampler Descriptor write
    // All these steps need to be done only if necessary:
    //  - Calling SetTexture() should dirty the SRV update and independently check if Samplers are dirty too
    //  - Setting Shader constants should dirty the Constant Data
    //     - Maybe we should do the constants set via ResourceManager? What about NativeShader API though...
    if (!PrepareConstants(shader)) return false;
    if (!PrepareTextureViews(shader)) return false;
    if (!PrepareSamplers(shader)) return false;

    // we provide mTextures to Shader mostly because of each shader accessing Texture (sub)resources differently
    // ex. MipmapGenComputeShader wants to populate Texture's subresources when generating mip levels instead of
    // simply viewing Textures as a whole resource, including all of its subresources.
    // As such, it makes more sense to let Shaders decide how to write Views onto Descriptors we just prepared for them.
    if (!shader->PrepareDescriptors(mTextures)) return false;

    return true;
}

ResourceManager::ResourceManager(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mVertexShader()
    , mPixelShader()
    , mTextures()
    , mDescriptorHeap(nativeDevice)
    , mSamplerHeap(nativeDevice)
    , mConstantRingBuffer(nativeDevice)
    , mTexturesDirty(true)
    , mSamplersDirty(true)
{
    mNativeDevice->RegisterWaitableOperation(this);
}

ResourceManager::~ResourceManager()
{
    mNativeDevice->UnregisterWaitableOperation(this);

    for (uint32_t i = 0; i < Constants::MAX_TEXTURE_UNITS; ++i)
    {
        mTextures[i].reset();
        mRuntimeParametersStash.textures[i].reset();
    }

    mVertexShader.reset();
    mPixelShader.reset();
    mRuntimeParametersStash.vertexShader.reset();
    mRuntimeParametersStash.pixelShader.reset();

    D3D12NI_LOG_DEBUG("ResourceManager destroyed");
}

bool ResourceManager::Init()
{
        // TODO: D3D12: PERF fine-tune ring descriptor heap parameters
    if (!mDescriptorHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, 12 * 1024, 9 * 1024))
    {
        D3D12NI_LOG_ERROR("Failed to initialize main Ring Descriptor Heap");
        return false;
    }

    // Maximum limit of Samplers is 2048
    //    https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
    //    https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels
    // TODO: This applies to Tier 2 hardware and above, Tier 1 limits Samplers to 16.
    //       We could possibly restrict that by raising Feature Level to 12 in NativeDevice;
    //       verify if this should be done after all
    if (!mSamplerHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true, 2048, 1536))
    {
        D3D12NI_LOG_ERROR("Failed to initialize Sampler Ring Descriptor Heap");
        return false;
    }

    if (!mConstantRingBuffer.Init(4 * 1024 * 1024, 3 * 1024 * 1024))
    {
        D3D12NI_LOG_ERROR("Failed to initialize constant data Ring Buffer");
        return false;
    }

    mDescriptorHeap.SetDebugName("CBV/SRV/UAV Descriptor Heap");
    mSamplerHeap.SetDebugName("Sampler Heap");
    mConstantRingBuffer.SetDebugName("Constant Ring Buffer");

    return true;
}

// Assumes it is called only if attached shader resources change or we switch to a new Command List
bool ResourceManager::PrepareResources()
{
    if (!PrepareShaderResources(mVertexShader)) return false;
    if (!PrepareShaderResources(mPixelShader)) return false;

    return true;
}

void ResourceManager::ApplyResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    mVertexShader->ApplyDescriptors(commandList);
    mPixelShader->ApplyDescriptors(commandList);
}

bool ResourceManager::PrepareComputeResources()
{
    return PrepareShaderResources(mComputeShader);
}

void ResourceManager::ApplyComputeResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    mComputeShader->ApplyDescriptors(commandList);
}

void ResourceManager::ClearTextureUnit(uint32_t slot)
{
    D3D12NI_ASSERT(slot < Constants::MAX_TEXTURE_UNITS, "Provided too high slot %u (max %u)", slot, Constants::MAX_TEXTURE_UNITS);

    mTextures[slot].reset();
    mTexturesDirty = true;
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
    if (shader == mVertexShader) return;

    mVertexShader = shader;
    mVertexShader->SetConstantsDirty(true);
}

void ResourceManager::SetPixelShader(const NIPtr<Shader>& shader)
{
    if (shader == mPixelShader) return;

    mPixelShader = shader;
    mPixelShader->SetConstantsDirty(true);
}

void ResourceManager::SetComputeShader(const NIPtr<Shader>& shader)
{
    if (shader == mComputeShader) return;

    mComputeShader = shader;
    mComputeShader->SetConstantsDirty(true);
}

void ResourceManager::SetTexture(uint32_t slot, const NIPtr<NativeTexture>& tex)
{
    D3D12NI_ASSERT(slot < Constants::MAX_TEXTURE_UNITS, "Provided too high slot %u (max %u)", slot, Constants::MAX_TEXTURE_UNITS);

    if (mTextures[slot] == tex) return;

    if (!mTextures[slot] || (mTextures[slot]->GetSamplerDesc() != tex->GetSamplerDesc()))
    {
        mSamplersDirty = true;
    }

    mTextures[slot] = tex;
    mTexturesDirty = true;
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
    SetVertexShader(mRuntimeParametersStash.vertexShader);
    SetPixelShader(mRuntimeParametersStash.pixelShader);

    for (size_t i = 0; i < mTextures.size(); ++i)
    {
        if (mRuntimeParametersStash.textures[i])
        {
            SetTexture(static_cast<uint32_t>(i), mRuntimeParametersStash.textures[i]);
        }
        else
        {
            ClearTextureUnit(static_cast<uint32_t>(i));
        }
    }
}

void ResourceManager::OnQueueSignal(uint64_t)
{
    // here we only have to mark the dirty flags so that we don't reuse descriptor data that's
    // already consumed and marked free by Ring Containers
    mTexturesDirty = true;
    mSamplersDirty = true;
}

void ResourceManager::OnFenceSignaled(uint64_t)
{
    // noop
}

} // namespace Internal
} // namespace D3D12
