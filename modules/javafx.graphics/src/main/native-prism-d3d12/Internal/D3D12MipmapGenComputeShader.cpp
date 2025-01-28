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

#include "D3D12MipmapGenComputeShader.hpp"


namespace D3D12 {
namespace Internal {

MipmapGenComputeShader::MipmapGenComputeShader()
    : Shader()
    , mCBufferView()
    , mTextureDTable()
    , mUAVDTable()
{
}

bool MipmapGenComputeShader::Init(const std::string& name, ShaderPipelineMode mode, D3D12_SHADER_VISIBILITY visibility, void* code, size_t codeSize)
{
    if (mode != ShaderPipelineMode::COMPUTE && visibility != D3D12_SHADER_VISIBILITY_ALL)
    {
        D3D12NI_LOG_ERROR("Mode and/or visibility are incompatible");
        return false;
    }

    if (name != "MipmapGenCS")
    {
        D3D12NI_LOG_ERROR("This shader class should only be used to load and operate MipmapGenCS shader");
        return false;
    }

    if (!Shader::Init(name, mode, visibility, code, codeSize))
    {
        D3D12NI_LOG_ERROR("Failed to init base of NativeShader");
        return false;
    }

    mConstantBufferStorage.resize(sizeof(CBuffer));

    AddShaderResource("gData", ResourceAssignment(ResourceAssignmentType::DESCRIPTOR, 0, 0, sizeof(CBuffer), 0));

    return true;
}

void MipmapGenComputeShader::PrepareShaderResources(const DataAllocator& dataAllocator, const DescriptorAllocator& descriptorAllocator,
                                                    const CBVCreator& cbvCreator, const NativeTextureBank& textures)
{
    if (mConstantBufferStorage.size() != sizeof(CBuffer))
    {
        D3D12NI_LOG_ERROR("MipmapGenCS: Invalid Constant Buffer Storage");
        return;
    }

    if (!textures[0])
    {
        D3D12NI_LOG_ERROR("MipmapGenCS: Failed to prepare resources; a texture must be bound to slot 0");
        return;
    }

    const CBuffer* cb = reinterpret_cast<const CBuffer*>(mConstantBufferStorage.data());

    mTextureDTable = descriptorAllocator(1);
    if (!mTextureDTable)
    {
        D3D12NI_LOG_ERROR("MipmapGenCS: Failed to prepare resources; allocation of 1 SRV descriptor failed");
        return;
    }

    mUAVDTable = descriptorAllocator(cb->numLevels);
    if (!mTextureDTable)
    {
        D3D12NI_LOG_ERROR("MipmapGenCS: Failed to prepare resources; allocation of 4 UAV descriptors failed");
        return;
    }

    mCBufferView = dataAllocator(sizeof(CBuffer), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    if (!mCBufferView)
    {
        D3D12NI_LOG_ERROR("MipmapGenCS: Failed to prepare resources; allocation of CBV region failed");
        return;
    }

    memcpy(mCBufferView.cpu, mConstantBufferStorage.data(), sizeof(CBuffer));

    // write source mip level as SRV (our input)
    textures[0]->WriteSRVToDescriptor(mTextureDTable.CPU(0), 1, cb->sourceLevel);

    // write destination mip levels as UAVs (output)
    for (uint32_t i = 0; i < cb->numLevels; ++i)
    {
        textures[0]->WriteUAVToDescriptor(mUAVDTable.CPU(i), cb->sourceLevel + 1);
    }
}

void MipmapGenComputeShader::ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const
{
    commandList->SetComputeRootConstantBufferView(0, mCBufferView.gpu);
    commandList->SetComputeRootDescriptorTable(1, mUAVDTable.GPU(0));
    commandList->SetComputeRootDescriptorTable(2, mTextureDTable.GPU(0));
}

} // namespace Internal
} // namespace D3D12
