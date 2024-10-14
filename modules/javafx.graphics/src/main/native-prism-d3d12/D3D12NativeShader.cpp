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

bool NativeShader::ShouldCBufferHaveDescriptor(const JSLC::ShaderResourceCollection& resources, bool hasTextures) const
{
    uint32_t totalSlotsTaken = 16 + (hasTextures ? 1 : 0);

    for (auto& resource: resources)
    {
        if (resource.type == JSLC::ResourceBindingType::CONSTANT_32BIT ||
            resource.type == JSLC::ResourceBindingType::CONSTANT_64BIT ||
            resource.type == JSLC::ResourceBindingType::CONSTANT_96BIT ||
            resource.type == JSLC::ResourceBindingType::CONSTANT_128BIT)
        {
            totalSlotsTaken += GetTotalBindingSize(resource);
        }
    }

    return (totalSlotsTaken > MAX_AVAILABLE_SLOTS);
}

NativeShader::NativeShader(const NIPtr<NativeDevice>& nativeDevice)
    : Shader()
    , mNativeDevice(nativeDevice)
    , mRootSignature(nullptr)
    , mShaderResources()
    , mTextureDTableIndex(0)
    , mCBufferDescriptorIndex(0)
    , mTextureCount(0)
    , mLastAllocatedCBufferRegion()
    , mLastAllocatedDescriptorData()
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
    std::vector<D3D12_STATIC_SAMPLER_DESC> rsSamplers;
    D3D12_ROOT_PARAMETER rsParam;
    D3D12_STATIC_SAMPLER_DESC rsSampler;
    D3D12NI_ZERO_STRUCT(rsParam);
    D3D12NI_ZERO_STRUCT(rsSampler);

    // preinit sampler with common settings
    // TODO: D3D12: this hard-codes sampler settings, check if we shouldn't make some dynamically
    rsSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rsSampler.RegisterSpace = 0;
    rsSampler.ShaderRegister = 0;
    rsSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rsSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rsSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rsSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    rsSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    rsSampler.MinLOD = 0;
    rsSampler.MaxLOD = D3D12_FLOAT32_MAX;
    rsSampler.MipLODBias = 0;
    rsSampler.MaxAnisotropy = 1;
    rsSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    // PassThroughVS WorldViewProj param - for simplicity we'll use a RS Constant
    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rsParam.Constants.RegisterSpace = 0;
    rsParam.Constants.ShaderRegister = 0;
    rsParam.Constants.Num32BitValues = 16; // 4x4 float matrix
    rsParams.emplace_back(rsParam);

    // skipping some root parameters to ensure we have them available as tables:
    //   0 = MVP matrix root constant
    //   1 = DTable for textures (if needed, otherwise D3D12 rejects the RS)
    // above take at most 16 + 1 = 17 slots out of 64 available, however
    // we start counting from 16 and check if we add the DTable
    uint32_t usedSlots = 16;

    // counters to build mShaderResourceAssignments helper map
    uint32_t currentParamIndex = 1;
    uint32_t currentTextureParamDTableIndex = 0;
    // size of constant buffer storage used by the shader
    uint32_t constantDataTotalSize = 0;

    // DTable for purely texture data - that way we can easily map
    // classic texture "slot" provided by Prism to index in this table
    D3D12_DESCRIPTOR_RANGE textureDescriptorRange;
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

        usedSlots += DESCRIPTOR_TABLE_SLOT_SIZE;
        mTextureDTableIndex = currentParamIndex;
        currentParamIndex++;
    }

    bool cbufferViaDescriptor = ShouldCBufferHaveDescriptor(mShaderResources, mTextureDTableIndex > 0);

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
            // TODO: D3D12: Samplers are static for now
            rsSampler.ShaderRegister = binding.slot;
            rsSamplers.emplace_back(rsSampler);
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

            if (!cbufferViaDescriptor)
            {
                // enough space to put data directly in RS as a root constant
                rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                rsParam.Constants.Num32BitValues = bindingSlots;
                rsParam.Constants.RegisterSpace = 0;
                rsParam.Constants.ShaderRegister = binding.slot;
                rsParams.emplace_back(rsParam);

                AddShaderResource(
                    binding.name,
                    ResourceAssignment (
                        ResourceAssignmentType::ROOT_CONSTANT, currentParamIndex, 0, bindingSizeBytes, constantDataTotalSize
                    )
                );
                usedSlots += bindingSlots;
                ++currentParamIndex;
            }
            else
            {
                // emplace the assignment for future reference in SetConstants()
                AddShaderResource(
                    binding.name,
                    ResourceAssignment (
                        ResourceAssignmentType::DESCRIPTOR, currentParamIndex, 0, bindingSizeBytes, constantDataTotalSize
                    )
                );
            }

            constantDataTotalSize += paddedBindingSize;
            break;
        }
        default:
            continue;
        }
    }

    if (cbufferViaDescriptor)
    {
        // CBuffer data won't fit inside Root Signature as Root Constants,
        // we will have one Root Descriptor fitting all this data
        rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rsParam.Descriptor.RegisterSpace = 0;
        rsParam.Descriptor.ShaderRegister = 0;
        rsParams.emplace_back(rsParam);

        usedSlots += ROOT_DESCRIPTOR_SLOT_SIZE;
        mCBufferDescriptorIndex = currentParamIndex;
        ++currentParamIndex;
    }

    D3D12NI_LOG_DEBUG("Shader %s resource assignments (used %u slots):", mName.c_str(), usedSlots);
    for (const auto& r: mShaderResourceAssignments)
    {
        const ResourceAssignment& ra = r.second;
        D3D12NI_LOG_DEBUG("  - %s: rsIndex %d:%d type %s @ offset %d size %d", r.first.c_str(), ra.rootIndex, ra.index, ResourceAssignmentTypeToString(ra.type), ra.offsetInCBStorage, ra.sizeInCBStorage);
    }

    D3D12_ROOT_SIGNATURE_DESC rsDesc;
    D3D12NI_ZERO_STRUCT(rsDesc);
    rsDesc.pParameters = rsParams.data();
    rsDesc.NumParameters = static_cast<UINT>(rsParams.size());
    rsDesc.pStaticSamplers = rsSamplers.data();
    rsDesc.NumStaticSamplers = static_cast<UINT>(rsSamplers.size());
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
    mTextureCount = textureDescriptorRange.NumDescriptors;

    return true;
}

void NativeShader::PrepareShaderResources(const DataAllocator& dataAllocator, const DescriptorAllocator& descriptorAllocator, const CBVCreator& cbvCreator)
{
    if (mTextureDTableIndex > 0)
    {
        mLastAllocatedDescriptorData = descriptorAllocator(mTextureCount);
    }

    if (IsCBufferDataViaDescriptor())
    {
        mLastAllocatedCBufferRegion = dataAllocator(mConstantBufferStorage.size(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

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
}

void NativeShader::ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    for (const auto& rIt: mShaderResourceAssignments)
    {
        const ResourceAssignment& r = rIt.second;

        switch (r.type)
        {
        case ResourceAssignmentType::ROOT_CONSTANT:
        {
            const uint8_t* srcPtr = mConstantBufferStorage.data() + r.offsetInCBStorage;
            commandList->SetGraphicsRoot32BitConstants(r.rootIndex, r.sizeInCBStorage / sizeof(DWORD), srcPtr, 0);
            break;
        }
        case ResourceAssignmentType::DESCRIPTOR:
        case ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS:
        case ResourceAssignmentType::DESCRIPTOR_TABLE_TEXTURES:
            // ignored - will be set later via its ways
            break;
        default:
            D3D12NI_LOG_ERROR("Unrecognized resource assignment type for resource %s", rIt.first.c_str());
        }
    }

    // texture descriptor table
    if (mTextureDTableIndex > 0)
    {
        commandList->SetGraphicsRootDescriptorTable(mTextureDTableIndex, mLastAllocatedDescriptorData.gpu);
    }

    if (IsCBufferDataViaDescriptor() && mLastAllocatedCBufferRegion)
    {
        commandList->SetGraphicsRootConstantBufferView(mCBufferDescriptorIndex, mLastAllocatedCBufferRegion.gpu);
    }
}

const Internal::DescriptorData& NativeShader::GetTextureDescriptorTable() const
{
    return mLastAllocatedDescriptorData;
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

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeShader_nSetConstantsF
    (JNIEnv* env, jobject obj, jlong ptr, jstring name, jobject floatBuf, jint offset, jint count)
{
    if (!ptr) return false;
    if (!name) return false;
    if (!floatBuf) return false;
    if (offset < 0) return false;
    if (count <= 0) return false;

    D3D12::Internal::JNIBuffer<jfloatArray> buffer(env, floatBuf, nullptr);
    D3D12::Internal::JNIString nameJStr(env, name);
    std::string nameStr(nameJStr);

    if (buffer.Data() == nullptr) return false;
    if (offset + count > buffer.Size()) return false;

    size_t sizeBytes = static_cast<size_t>(count) * sizeof(jfloat);
    size_t offsetBytes = static_cast<size_t>(offset) * sizeof(jfloat);

    const uint8_t* srcPtr = reinterpret_cast<const uint8_t*>(buffer.Data()) + offsetBytes;

    return D3D12::GetNIObject<D3D12::NativeShader>(ptr)->SetConstants(nameStr, srcPtr, sizeBytes);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeShader_nSetConstantsI
    (JNIEnv* env, jobject obj, jlong ptr, jstring name, jobject intBuf, jint offset, jint count)
{
    if (!ptr) return false;
    if (!name) return false;
    if (!intBuf) return false;
    if (offset < 0) return false;
    if (count <= 0) return false;

    D3D12::Internal::JNIBuffer<jintArray> buffer(env, intBuf, nullptr);
    D3D12::Internal::JNIString nameJStr(env, name);
    std::string nameStr(nameJStr);

    if (buffer.Data() == nullptr) return false;
    if (offset + count > buffer.Size()) return false;

    size_t sizeBytes = static_cast<size_t>(count) * sizeof(jint);
    size_t offsetBytes = static_cast<size_t>(offset) * sizeof(jint);

    const uint8_t* srcPtr = reinterpret_cast<const uint8_t*>(buffer.Data()) + offsetBytes;

    return D3D12::GetNIObject<D3D12::NativeShader>(ptr)->SetConstants(nameStr, srcPtr, sizeBytes);
}

#ifdef __cplusplus
}
#endif
