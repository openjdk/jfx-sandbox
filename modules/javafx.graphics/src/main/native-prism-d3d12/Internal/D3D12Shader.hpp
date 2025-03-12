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

#pragma once

#include "../D3D12Common.hpp"

#include "D3D12RingBuffer.hpp"
#include "D3D12DescriptorData.hpp"

#include "../D3D12NativeTexture.hpp"

#include <map>
#include <string>
#include <functional>


namespace D3D12 {
namespace Internal {

/**
 * Common interface for all Shaders. Unifies some common functionalities
 * which will be used during rendering - see D3D12ResourceManager.hpp.
 *
 * Note that it is mostly only a container for Shader's bytecode. Shader
 * bytecode is compiled when a PSO is created.
 */
class Shader
{
protected:
    struct ResourceAssignment
    {
        ResourceAssignmentType type; // where our resource was assigned to
        uint32_t rootIndex; // at which root signature index is our resource
        uint32_t index; // at which index in dtable is our resource - only valid for DESCRIPTOR_TABLE types
        uint32_t sizeInCBStorage; // size in storage in bytes per element
        uint32_t offsetInCBStorage; // at which spot in mConstantBufferStorage our data should be kept

        ResourceAssignment(ResourceAssignmentType type, uint32_t rootIndex, uint32_t index, uint32_t sizeInCBStorage, uint32_t offsetInCBStorage)
            : type(type)
            , rootIndex(rootIndex)
            , index(index)
            , sizeInCBStorage(sizeInCBStorage)
            , offsetInCBStorage(offsetInCBStorage)
        {}
    };

    using ResourceAssignmentCollection = std::map<std::string, ResourceAssignment>;

    std::string mName;
    ShaderPipelineMode mMode;
    D3D12_SHADER_VISIBILITY mVisibility;
    D3D12_SHADER_BYTECODE mBytecode;
    std::vector<uint8_t> mBytecodeBuffer;
    std::vector<uint8_t> mConstantBufferStorage;
    ResourceAssignmentCollection mShaderResourceAssignments;

    void SetConstantBufferData(void* data, size_t size, size_t storageOffset);
    void AddShaderResource(const std::string& name, const ResourceAssignment& resource);

public:
    using ConstantAllocator = std::function<RingBuffer::Region(size_t size, size_t alignment)>;
    using ResourceViewAllocator = std::function<DescriptorData(size_t count)>;
    using SamplerAllocator = std::function<DescriptorData(size_t count)>;
    using CBVCreator = std::function<void(D3D12_GPU_VIRTUAL_ADDRESS cbufferPtr, UINT size, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)>;
    using NullResourceViewCreator = std::function<void(D3D12_SRV_DIMENSION dimension, D3D12_CPU_DESCRIPTOR_HANDLE descriptor)>;

    // TODO: D3D12: This probably can be done nicer, if InternalShader and this class can be made aware
    //              of NativeDevice... Explore that alternative
    struct ShaderResourceHelpers
    {
        ConstantAllocator constantAllocator; // allocates ring buffer space
        ResourceViewAllocator rvAllocator; // allocates resource views (SRV, UAV)
        SamplerAllocator samplerAllocator;
        CBVCreator cbvCreator;
        NullResourceViewCreator nullSRVCreator;
    };

    Shader();

    virtual bool Init(const std::string& name, ShaderPipelineMode mode, D3D12_SHADER_VISIBILITY visibility, void* code, size_t codeSize);
    bool SetConstants(const std::string& name, const void* data, size_t size);
    bool SetConstantsInArray(const std::string& name, uint32_t idx, const void* data, size_t size);

    virtual bool PrepareShaderResources(const ShaderResourceHelpers& helpers, const NativeTextureBank& textures) = 0;
    virtual void ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const = 0;

    inline const std::string& GetName() const
    {
        return mName;
    }

    inline ShaderPipelineMode GetMode() const
    {
        return mMode;
    }

    inline const D3D12_SHADER_BYTECODE& GetBytecode() const
    {
        return mBytecode;
    }
};

} // namespace Internal
} // namespace D3D12
