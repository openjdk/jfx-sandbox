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
    for (const InternalShaderResource::ResourceBinding& constantBuffer: shaderResources.constantBuffers)
    {
        if (constantBuffer.type == ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS)
        {
            mCBufferDTables.emplace_back(constantBuffer.rootIndex, constantBuffer.count);
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
    mTextureCount = shaderResources.textures.size();

    for (size_t i = 0; i < shaderResources.textures.size(); ++i)
    {
        const InternalShaderResource::ResourceBinding& texture = shaderResources.textures[i];

        AddShaderResource(
            texture.name,
            ResourceAssignment(
                texture.type, texture.rootIndex, static_cast<uint32_t>(i), 0, 0
            )
        );

        mTextureDTableRSIndex = texture.rootIndex;
    }

    // debug info

    D3D12NI_LOG_DEBUG("Internal Shader %s resource assignments:", mName.c_str());
    for (const auto& r: mShaderResourceAssignments)
    {
        const ResourceAssignment& ra = r.second;
        D3D12NI_LOG_DEBUG("  - %s: rsIndex %d:%d type %s @ offset %d size %d", r.first.c_str(), ra.rootIndex, ra.index, ResourceAssignmentTypeToString(ra.type), ra.offsetInCBStorage, ra.sizeInCBStorage);
    }

    return true;
}

void InternalShader::PrepareShaderResources(const DataAllocator& dataAllocator, const DescriptorAllocator& descriptorAllocator, const CBVCreator& cbvCreator)
{
    for (size_t i = 0; i < mCBufferDTables.size(); ++i)
    {
        mCBufferDTables[i].dtable = descriptorAllocator(mCBufferDTables[i].count);
    }

    for (size_t i = 0; i < mCBufferDescriptorRegions.size(); ++i)
    {
        CBufferRegion& cbr = mCBufferDescriptorRegions[i];
        cbr.region = dataAllocator(cbr.assignment.sizeInCBStorage, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        if (cbr.region)
        {
            const uint8_t* src = reinterpret_cast<uint8_t*>(mConstantBufferStorage.data()) + cbr.assignment.offsetInCBStorage;
            memcpy(cbr.region.cpu, src, cbr.assignment.sizeInCBStorage);
        }
        else
        {
            D3D12NI_LOG_ERROR("InternalShader %s: Failed to reserve space for constant buffer at RS index %u", mName.c_str(), cbr.assignment.rootIndex);
        }

        if (cbr.assignment.type == ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS)
        {
            // write a CBV onto our descriptors here
            size_t idx = 0;
            for (idx = 0; idx < mCBufferDTables.size(); ++idx)
            {
                if (mCBufferDTables[idx].rootIndex == cbr.assignment.rootIndex)
                {
                    cbvCreator(
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
            }
        }
    }

    if (mTextureCount > 0)
    {
        mTextureDTable = descriptorAllocator(mTextureCount);
        if (!mTextureDTable)
        {
            D3D12NI_LOG_ERROR("Internal shader %s: Failed to allocate DTable for %u textures", mName.c_str(), mTextureCount);
        }
    }
}

void InternalShader::ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    for (const auto& r: mCBufferDescriptorRegions)
    {
        const ResourceAssignment& ra = r.assignment;

        switch (ra.type)
        {
        case ResourceAssignmentType::ROOT_CONSTANT:
        {
            const uint8_t* srcPtr = mConstantBufferStorage.data() + ra.offsetInCBStorage;
            commandList->SetGraphicsRoot32BitConstants(ra.rootIndex, ra.sizeInCBStorage / sizeof(DWORD), srcPtr, 0);
            break;
        }
        case ResourceAssignmentType::DESCRIPTOR:
        {
            commandList->SetGraphicsRootConstantBufferView(ra.rootIndex, r.region.gpu);
            break;
        }
        case ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS:
        case ResourceAssignmentType::DESCRIPTOR_TABLE_TEXTURES:
            // ignored - will be set later via its ways
            break;
        default:
            D3D12NI_LOG_ERROR("Unrecognized resource assignment type for resource at RS index %u", ra.rootIndex);
        }
    }

    if (mTextureCount > 0)
    {
        commandList->SetGraphicsRootDescriptorTable(mTextureDTableRSIndex, mTextureDTable.gpu);
    }

    for (size_t i = 0; i < mCBufferDTables.size(); ++i)
    {
        commandList->SetGraphicsRootDescriptorTable(mCBufferDTables[i].rootIndex, mCBufferDTables[i].dtable.gpu);
    }
}

const DescriptorData& InternalShader::GetTextureDescriptorTable() const
{
    return mTextureDTable;
}

} // namespace Internal
} // namespace D3D12
