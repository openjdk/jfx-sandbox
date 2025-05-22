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

#include "D3D12InternalShader.hpp"

#include "Internal_D3D12ShaderResourceDataHeader.hpp"


namespace D3D12 {
namespace Internal {

InternalShader::InternalShader()
    : Shader()
    , mCBufferDescriptorRegions()
    , mTextureCount(0)
    , mTextureDTableRSIndex(0)
    , mTextureDTable()
{
}

bool InternalShader::Init(const std::string& name, ShaderPipelineMode mode, D3D12_SHADER_VISIBILITY visibility, void* code, size_t codeSize)
{
    // try an exact name search first
    auto resources = InternalShaderResource::InternalShaders.find(name);
    if (resources == InternalShaderResource::InternalShaders.end())
    {
        // It is possible we try to load a variant of an internal shader
        // Assume Shader variants are denoted by an _ sign
        // variants SHOULD have the same resource declarations though, so filter out "_" and suffix
        size_t underscore = name.find_last_of('_');
        std::string basename;
        if (underscore == std::string::npos)
        {
            D3D12NI_LOG_ERROR("Cannot locate resources for internal shader %s", name.c_str());
            return false;
        }

        basename = name.substr(0, underscore);
        resources = InternalShaderResource::InternalShaders.find(basename);
        if (resources == InternalShaderResource::InternalShaders.end())
        {
            D3D12NI_LOG_ERROR("Cannot locate resources for internal shader %s", name.c_str());
            return false;
        }
    }

    if (!Shader::Init(name, mode, visibility, code, codeSize))
    {
        D3D12NI_LOG_ERROR("Failed to init base of NativeShader");
        return false;
    }

    InternalShaderResource::ShaderResources shaderResources = resources->second;

    // constant buffer preparation
    uint32_t constantBufferTotalSize = 0;
    mTotalRVDescriptorCount = 0;
    for (const InternalShaderResource::ResourceBinding& constantBuffer: shaderResources.constantBuffers)
    {
        if (constantBuffer.type == ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS)
        {
            mCBufferDTables.emplace_back(constantBuffer.rootIndex, constantBuffer.count);
            mTotalRVDescriptorCount += constantBuffer.count;
        }

        for (uint32_t i = 0; i < constantBuffer.count; ++i)
        {
            ResourceAssignment assignment(constantBuffer.type, constantBuffer.rootIndex, i, constantBuffer.size, constantBufferTotalSize);

            std::string name = constantBuffer.name;
            if (constantBuffer.type == ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS)
            {
                name += '[';
                name += std::to_string(i);
                name += ']';
            }

            AddShaderResource(name, assignment);
            mCBufferDescriptorRegions.emplace_back(assignment);

            constantBufferTotalSize += constantBuffer.size;
        }
    }

    mConstantBufferStorage.resize(constantBufferTotalSize);

    // count how many DTable slots we need for textures (if any)
    mTextureCount = 0;
    for (size_t i = 0; i < shaderResources.textures.size(); ++i)
    {
        const InternalShaderResource::ResourceBinding& texture = shaderResources.textures[i];

        AddShaderResource(
            texture.name,
            ResourceAssignment(
                texture.type, texture.rootIndex, static_cast<uint32_t>(i), 0, 0
            )
        );

        mTextureCount++;
    }

    mTotalRVDescriptorCount += mTextureCount;

    // and similarly, for samplers
    mSamplerCount = 0;
    for (size_t i = 0; i < shaderResources.samplers.size(); ++i)
    {
        const InternalShaderResource::ResourceBinding& sampler = shaderResources.samplers[i];

        AddShaderResource(
            sampler.name,
            ResourceAssignment(
                sampler.type, sampler.rootIndex, static_cast<uint32_t>(i), 0, 0
            )
        );

        mSamplerCount++;
    }

    mTextureDTableRSIndex = ShaderSlots::PHONG_PS_TEXTURE_DTABLE;
    mSamplerDTableRSIndex = ShaderSlots::PHONG_PS_SAMPLER_DTABLE;

    // debug info

    D3D12NI_LOG_DEBUG("Internal Shader %s resource assignments (needs %d descriptors total):", mName.c_str(), mTotalRVDescriptorCount);
    for (const auto& r: mShaderResourceAssignments)
    {
        const ResourceAssignment& ra = r.second;
        D3D12NI_LOG_DEBUG("  - %s: rsIndex %d:%d type %s @ offset %d size %d", r.first.c_str(), ra.rootIndex, ra.index, ResourceAssignmentTypeToString(ra.type), ra.offsetInCBStorage, ra.sizeInCBStorage);
    }

    return true;
}

bool InternalShader::PrepareShaderResources(const ShaderResourceHelpers& helpers, const NativeTextureBank& textures)
{
    // To prevent following situations in the middle of rendering:
    //   - alloc some CBVs/SRVs
    //   - Signal (no Draw had a chance to consume the Descriptors yet!)
    //   - alloc more CBVs/SRVs
    // we allocate once in full and then split allocated space
    DescriptorData allDescriptors;
    UINT usedDescriptors = 0;
    if (mTotalRVDescriptorCount > 0)
    {
        allDescriptors = helpers.rvAllocator(mTotalRVDescriptorCount);
        if (!allDescriptors)
        {
            D3D12NI_LOG_ERROR("InternalShader %s: Failed to allocate space for %d descriptors", mName.c_str(), mTotalRVDescriptorCount);
            return false;
        }
    }

    for (size_t i = 0; i < mCBufferDTables.size(); ++i)
    {
        mCBufferDTables[i].dtable = allDescriptors.Range(usedDescriptors, mCBufferDTables[i].count);
        usedDescriptors += mCBufferDTables[i].count;
    }

    for (size_t i = 0; i < mCBufferDescriptorRegions.size(); ++i)
    {
        CBufferRegion& cbr = mCBufferDescriptorRegions[i];
        cbr.region = helpers.constantAllocator(cbr.assignment.sizeInCBStorage, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        if (cbr.region)
        {
            const uint8_t* src = reinterpret_cast<uint8_t*>(mConstantBufferStorage.data()) + cbr.assignment.offsetInCBStorage;
            memcpy(cbr.region.cpu, src, cbr.assignment.sizeInCBStorage);
        }
        else
        {
            D3D12NI_LOG_ERROR("InternalShader %s: Failed to reserve space for constant buffer at RS index %u", mName.c_str(), cbr.assignment.rootIndex);
            return false;
        }

        if (cbr.assignment.type == ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS)
        {
            // write a CBV onto our descriptors here
            size_t idx = 0;
            for (idx = 0; idx < mCBufferDTables.size(); ++idx)
            {
                if (mCBufferDTables[idx].rootIndex == cbr.assignment.rootIndex)
                {
                    helpers.cbvCreator(
                        cbr.region.gpu,
                        static_cast<UINT>(cbr.region.size),
                        mCBufferDTables[idx].dtable.CPU(cbr.assignment.index)
                    );
                    break;
                }
            }

            if (idx == mCBufferDTables.size())
            {
                D3D12NI_LOG_ERROR("InternalShader %s: Couldn't find descriptor table entry for Constant Buffer at RS index %u:%u", mName.c_str(), mCBufferDescriptorRegions[i].assignment.rootIndex, mCBufferDescriptorRegions[i].assignment.index);
                return false;
            }
        }
    }

    if (mTextureCount > 0)
    {
        mTextureDTable = allDescriptors.Range(usedDescriptors, static_cast<UINT>(mTextureCount));

        mSamplerDTable = helpers.samplerAllocator(mSamplerCount);
        if (!mSamplerDTable)
        {
            D3D12NI_LOG_ERROR("Internal shader %s: Failed to allocate DTable for %u samplers", mName.c_str(), mSamplerCount);
            return false;
        }

        uint32_t usedTextures = Constants::MAX_TEXTURE_UNITS;
        while (usedTextures > 0 && !textures[usedTextures - 1])
        {
            usedTextures--;
        }

        // should not happen
        if ((!mTextureDTable || !mSamplerDTable) && usedTextures != 0)
        {
            D3D12NI_LOG_ERROR("Internal shader %s: Descriptor Tables are NULL, but we have textures we need to use", mName.c_str());
            return false;
        }

        for (uint32_t i = 0; i < usedTextures; ++i)
        {
            if (textures[i])
            {
                textures[i]->WriteSRVToDescriptor(mTextureDTable.CPU(i));
                textures[i]->WriteSamplerToDescriptor(mSamplerDTable.CPU(i));
            }
            else
            {
                helpers.nullSRVCreator(D3D12_SRV_DIMENSION_TEXTURE2D, mTextureDTable.CPU(i));
            }
        }
    }

    return true;
}

void InternalShader::ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    for (const auto& r: mCBufferDescriptorRegions)
    {
        const ResourceAssignment& ra = r.assignment;

        switch (ra.type)
        {
        case ResourceAssignmentType::DESCRIPTOR:
        {
            commandList->SetGraphicsRootConstantBufferView(ra.rootIndex, r.region.gpu);
            break;
        }
        case ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS:
        case ResourceAssignmentType::DESCRIPTOR_TABLE_TEXTURES:
        case ResourceAssignmentType::DESCRIPTOR_TABLE_SAMPLERS:
            // ignored - will be set later
            break;
        default:
            D3D12NI_LOG_ERROR("Internal shader %s: Unrecognized resource assignment type for resource at RS index %u", mName.c_str(), ra.rootIndex);
        }
    }

    // LKDEBUG - SOMETIMES THERE IS PROBLEM HERE BUT I DON'T KNOW WHAT YET
    // [D3D12-ERROR] <Internal\D3D12Debug.cpp:66> D3D12 EXECUTION Error: GPU-BASED VALIDATION: Draw, Descriptor type doesn't match shader register type: Descriptor Heap Index To DescriptorTableStart: [506], Descriptor Heap Index FromTableStart: [0], Descriptor Type in Heap: D3D12_DESCRIPTOR_RANGE_TYPE_CBV, Register Type: D3D12_DESCRIPTOR_RANGE_TYPE_SRV, Index of Descriptor Range: 0, Shader Stage: PIXEL, Root Parameter Index: [4], Draw Index: [0], Shader Code: C:\Users\Lukasz\dev\code\jfx-sandbox\rt\modules\javafx.graphics\src\main\native-prism-d3d12\hlsl6\Mtl1PS.hlsl(89,20-20), Asm Instruction Range: [0xe-0xffffffff], Asm Operand Index: [0], Command List: 0x000001FC5A84DE90:'Direct Command List #7', Command List Type: D3D12_COMMAND_LIST_TYPE_DIRECT, SRV/UAV/CBV Descriptor Heap: 0x000001FC088B5210:'SRV Descriptor Heap', Sampler Descriptor Heap: 0x000001FC088B49A0:'Sampler Heap', Pipeline State: 0x000001FC55C6C520:'GPSO-Mtl1VS-Mtl1PS_s1n-SRC_OVER-4xMSAA-Depth',

    if (mTextureCount > 0 && mTextureDTable && mSamplerDTable)
    {
        commandList->SetGraphicsRootDescriptorTable(mTextureDTableRSIndex, mTextureDTable.gpu);
        commandList->SetGraphicsRootDescriptorTable(mSamplerDTableRSIndex, mSamplerDTable.gpu);
    }

    for (size_t i = 0; i < mCBufferDTables.size(); ++i)
    {
        commandList->SetGraphicsRootDescriptorTable(mCBufferDTables[i].rootIndex, mCBufferDTables[i].dtable.gpu);
    }
}

} // namespace Internal
} // namespace D3D12
