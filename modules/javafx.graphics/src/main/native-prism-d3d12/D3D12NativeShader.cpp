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

#include "D3D12NativeShader.hpp"

#include "D3D12NativeDevice.hpp"

#include "Internal/D3D12Utils.hpp"
#include "Internal/JNIBuffer.hpp"
#include "Internal/JNIString.hpp"

#include "Decora_D3D12ShaderResourceDataHeader.hpp"
#include "Prism_D3D12ShaderResourceDataHeader.hpp"

#include <com_sun_prism_d3d12_ni_D3D12NativeShader.h>


namespace {

const char* ResourceTypeToString(D3D12::JSLC::ResourceBindingType type)
{
    switch (type)
    {
    case D3D12::JSLC::ResourceBindingType::CONSTANT_32BIT: return "CONSTANT_32BIT";
    case D3D12::JSLC::ResourceBindingType::CONSTANT_64BIT: return "CONSTANT_64BIT";
    case D3D12::JSLC::ResourceBindingType::CONSTANT_96BIT: return "CONSTANT_96BIT";
    case D3D12::JSLC::ResourceBindingType::CONSTANT_128BIT: return "CONSTANT_128BIT";
    case D3D12::JSLC::ResourceBindingType::TEXTURE: return "TEXTURE";
    case D3D12::JSLC::ResourceBindingType::SAMPLER: return "SAMPLER";
    default: return "UNKNOWN";
    }
}

} // namespace


namespace D3D12 {

uint32_t NativeShader::GetTotalBindingSize(const JSLC::ResourceBinding& binding) const
{
    uint32_t sizePerSlot;
    switch (binding.type)
    {
    case JSLC::ResourceBindingType::CONSTANT_32BIT:
        sizePerSlot = 1;
        break;
    case JSLC::ResourceBindingType::CONSTANT_64BIT:
    case JSLC::ResourceBindingType::TEXTURE:
    case JSLC::ResourceBindingType::SAMPLER:
        sizePerSlot = 2;
        break;
    case JSLC::ResourceBindingType::CONSTANT_96BIT:
        sizePerSlot = 3;
        break;
    case JSLC::ResourceBindingType::CONSTANT_128BIT:
        sizePerSlot = 4;
        break;
    default:
        sizePerSlot = 0;
    }

    return sizePerSlot * binding.count;
}

// Checks if we have a Texture resource attached to the shader.
// If any TEXTURE binding exists, we will need a DTable for those.
bool NativeShader::RequiresTexturesDTable(const JSLC::ShaderResourceCollection& resources) const
{
    for (auto& resource: resources)
    {
        if (resource.type == JSLC::ResourceBindingType::TEXTURE)
        {
            return true;
        }
    }

    return false;
}

NativeShader::NativeShader(const NIPtr<NativeDevice>& nativeDevice)
    : Shader()
    , mNativeDevice(nativeDevice)
    , mRootSignature(nullptr)
    , mShaderResources()
    , mTextureDTableIndex(0)
    , mSamplerDTableIndex(0)
    , mCBufferDescriptorIndex(0)
    , mTextureCount(0)
    , mLastAllocatedCBufferRegion()
    , mLastAllocatedSRVDescriptors()
{
}

NativeShader::~NativeShader()
{
    if (mRootSignature)
    {
        mRootSignature.Reset();
    }

    mNativeDevice.reset();

    D3D12NI_LOG_DEBUG("NativeShader destroyed");
}

bool NativeShader::Init(const std::string& name, void* code, size_t size)
{
    // NativeShader-s are always 2D Pixel Shaders
    // All use common internal PassThroughVS vertex shader and are only used for UI rendering
    if (!Shader::Init(name, ShaderPipelineMode::UI_2D, D3D12_SHADER_VISIBILITY_PIXEL, code, size))
    {
        D3D12NI_LOG_ERROR("Failed to init base of NativeShader");
        return false;
    }

    auto resources = JSLC::DecoraShaders.find(mName);
    if (resources == JSLC::DecoraShaders.end())
    {
        resources = JSLC::PrismShaders.find(mName);
        if (resources == JSLC::PrismShaders.end())
        {
            D3D12NI_LOG_ERROR("Couldn't find %s shader resource data", mName.c_str());
            return false;
        }
    }

    mShaderResources = resources->second;
    if (!mShaderResources.empty())
    {
        D3D12NI_LOG_DEBUG("%s resources:", mName.c_str());

        for (auto& r: mShaderResources)
        {
            D3D12NI_LOG_DEBUG("  \\_ %s (%s, %d, %d)", r.name.c_str(), ResourceTypeToString(r.type), r.slot, r.count);
        }
    }
    else
    {
        D3D12NI_LOG_DEBUG("Shader %s has no resources attached", mName.c_str());
    }

    // build root signature for this shader
    std::vector<D3D12_ROOT_PARAMETER> rsParams;
    D3D12_ROOT_PARAMETER rsParam;
    D3D12NI_ZERO_STRUCT(rsParam);

    // PassThroughVS WorldViewProj param - for simplicity we'll use a RS Constant
    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rsParam.Descriptor.RegisterSpace = 0;
    rsParam.Descriptor.ShaderRegister = 0;
    rsParams.emplace_back(rsParam);

    // TODO: D3D12: All below is a very hacky way of tackling this. Instead of creating
    //              new RS per NativeShader we should use a common one. Rework this.

    // skipping some root parameters to ensure we have them available as tables:
    //   0 = MVP matrix root CBV descriptor
    //   1 = DTable for textures (if needed, otherwise D3D12 rejects the RS)
    // above take at most 2 + 1 = 3 slots out of 64 available, however
    // we start counting from 2 and check if we add the DTable
    uint32_t usedSlots = 2;

    // counters to build mShaderResourceAssignments helper map
    uint32_t currentParamIndex = 1;
    uint32_t currentTextureParamDTableIndex = 0;
    uint32_t currentSamplerParamDTableIndex = 0;
    // size of constant buffer storage used by the shader
    uint32_t constantDataTotalSize = 0;

    // DTable for purely texture data - that way we can easily map
    // classic texture "slot" provided by Prism to index in this table
    D3D12_DESCRIPTOR_RANGE textureDescriptorRange;
    D3D12_DESCRIPTOR_RANGE samplerDescriptorRange;
    if (RequiresTexturesDTable(mShaderResources))
    {
        D3D12NI_ZERO_STRUCT(textureDescriptorRange);
        textureDescriptorRange.BaseShaderRegister = 0;
        textureDescriptorRange.RegisterSpace = 0;
        textureDescriptorRange.NumDescriptors = 0;
        textureDescriptorRange.OffsetInDescriptorsFromTableStart = 0;
        textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

        rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsParam.DescriptorTable.NumDescriptorRanges = 1;
        rsParam.DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
        rsParams.emplace_back(rsParam);

        // JSLC should ensure that each texture has a matching sampler
        D3D12NI_ZERO_STRUCT(samplerDescriptorRange);
        samplerDescriptorRange.BaseShaderRegister = 0;
        samplerDescriptorRange.RegisterSpace = 0;
        samplerDescriptorRange.NumDescriptors = 0;
        samplerDescriptorRange.OffsetInDescriptorsFromTableStart = 0;
        samplerDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

        rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rsParam.DescriptorTable.NumDescriptorRanges = 1;
        rsParam.DescriptorTable.pDescriptorRanges = &samplerDescriptorRange;
        rsParams.emplace_back(rsParam);

        usedSlots += DESCRIPTOR_TABLE_SLOT_SIZE;
        mTextureDTableIndex = currentParamIndex;
        currentParamIndex++;
        mSamplerDTableIndex = currentParamIndex;
        currentParamIndex++;
    }

    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    for (auto& binding: mShaderResources)
    {
        switch (binding.type)
        {
        case JSLC::ResourceBindingType::TEXTURE:
        {
            // Texture SRVs always have to go to a descriptor heap
            ++textureDescriptorRange.NumDescriptors;

            AddShaderResource(
                binding.name,
                ResourceAssignment(
                    ResourceAssignmentType::DESCRIPTOR_TABLE_TEXTURES, mTextureDTableIndex, currentTextureParamDTableIndex, 0, 0
                )
            );

            ++currentTextureParamDTableIndex;
            break;
        }
        case JSLC::ResourceBindingType::SAMPLER:
        {
            // Samplers also always have to go to a descriptor heap
            ++samplerDescriptorRange.NumDescriptors;

            AddShaderResource(
                binding.name,
                ResourceAssignment(
                    ResourceAssignmentType::DESCRIPTOR_TABLE_SAMPLERS, mSamplerDTableIndex, currentSamplerParamDTableIndex, 0, 0
                )
            );

            ++currentSamplerParamDTableIndex;

            break;
        }
        case JSLC::ResourceBindingType::CONSTANT_32BIT:
        case JSLC::ResourceBindingType::CONSTANT_64BIT:
        case JSLC::ResourceBindingType::CONSTANT_96BIT:
        case JSLC::ResourceBindingType::CONSTANT_128BIT:
        {
            uint32_t bindingSlots = GetTotalBindingSize(binding);
            uint32_t bindingSizeBytes = bindingSlots * sizeof(DWORD);
            uint32_t paddedBindingSize = Internal::Utils::Align<uint32_t>(bindingSizeBytes, 16);

            // emplace the assignment for future reference in SetConstants()
            AddShaderResource(
                binding.name,
                ResourceAssignment (
                    ResourceAssignmentType::DESCRIPTOR, currentParamIndex, 0, bindingSizeBytes, constantDataTotalSize
                )
            );

            constantDataTotalSize += paddedBindingSize;
            break;
        }
        default:
            continue;
        }
    }

    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rsParam.Descriptor.RegisterSpace = 0;
    rsParam.Descriptor.ShaderRegister = 0;
    rsParams.emplace_back(rsParam);

    usedSlots += ROOT_DESCRIPTOR_SLOT_SIZE;
    mCBufferDescriptorIndex = currentParamIndex;
    ++currentParamIndex;

    D3D12NI_LOG_DEBUG("Shader %s resource assignments (used %u slots):", mName.c_str(), usedSlots);
    for (const auto& r: mShaderResourceAssignments)
    {
        const ResourceAssignment& ra = r.second;
        D3D12NI_LOG_DEBUG("  - %s: rsIndex %d:%d type %s @ offset %d size %d", r.first.c_str(), ra.rootIndex, ra.index, ResourceAssignmentTypeToString(ra.type), ra.offsetInCBStorage, ra.sizeInCBStorage);
    }

    // confidence check - we should have the same amount of samplers as textures
    if (currentTextureParamDTableIndex != currentSamplerParamDTableIndex)
    {
        D3D12NI_LOG_WARN("NativeShader %s: Not matching texture (%u) to sampler (%u) count", currentTextureParamDTableIndex, currentSamplerParamDTableIndex);
    }

    D3D12_ROOT_SIGNATURE_DESC rsDesc;
    D3D12NI_ZERO_STRUCT(rsDesc);
    rsDesc.pParameters = rsParams.data();
    rsDesc.NumParameters = static_cast<UINT>(rsParams.size());
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3DBlobPtr rsBlob;
    D3DBlobPtr rsErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rsBlob, &rsErrorBlob);
    if (FAILED(hr))
    {
        D3D12NI_LOG_ERROR("Failed to serialize Root Signature: %s", rsErrorBlob->GetBufferPointer());
        return false;
    }

    hr = mNativeDevice->GetDevice()->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Root Signature");

    mConstantBufferStorage.resize(constantDataTotalSize);

    return true;
}

void NativeShader::PrepareShaderResources(const ShaderResourceHelpers& helpers, const NativeTextureBank& textures)
{
    if (mTextureDTableIndex > 0)
    {
        // counting backwards to cover all slots, even if there are NULL textures
        // hidden in-between (we don't know which texture slots Shaders need)
        uint32_t usedTextures = Constants::MAX_TEXTURE_UNITS;
        while (usedTextures > 0 && !textures[usedTextures - 1])
        {
            usedTextures--;
        }

        mTextureCount = usedTextures;
        mLastAllocatedSRVDescriptors = helpers.rvAllocator(usedTextures);
        mLastAllocatedSamplerDescriptors = helpers.samplerAllocator(usedTextures);

        // should not happen
        if ((!mLastAllocatedSRVDescriptors || !mLastAllocatedSamplerDescriptors) && usedTextures != 0)
        {
            D3D12NI_LOG_ERROR("Descriptor Tables are NULL, but we have textures we need to use");
            return;
        }

        for (uint32_t i = 0; i < usedTextures; ++i)
        {
            if (textures[i])
            {
                textures[i]->WriteSRVToDescriptor(mLastAllocatedSRVDescriptors.CPU(i));
                textures[i]->WriteSamplerToDescriptor(mLastAllocatedSamplerDescriptors.CPU(i));
            }
        }
    }

    mLastAllocatedCBufferRegion = helpers.constantAllocator(mConstantBufferStorage.size(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    if (mLastAllocatedCBufferRegion)
    {
        memcpy(mLastAllocatedCBufferRegion.cpu, mConstantBufferStorage.data(), mConstantBufferStorage.size());
    }
    else
    {
        // should not happen
        D3D12NI_LOG_ERROR("Failed to allocate cbuffer descriptor");
    }
}

void NativeShader::ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    // texture descriptor table
    if (mTextureDTableIndex > 0)
    {
        commandList->SetGraphicsRootDescriptorTable(mTextureDTableIndex, mLastAllocatedSRVDescriptors.gpu);
        commandList->SetGraphicsRootDescriptorTable(mSamplerDTableIndex, mLastAllocatedSamplerDescriptors.gpu);
    }

    if (mLastAllocatedCBufferRegion)
    {
        commandList->SetGraphicsRootConstantBufferView(mCBufferDescriptorIndex, mLastAllocatedCBufferRegion.gpu);
    }
}

} // namespace D3D12


#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeShader_nReleaseNativeObject
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

    D3D12::FreeNIObject<D3D12::NativeShader>(ptr);
}

#ifdef __cplusplus
}
#endif
