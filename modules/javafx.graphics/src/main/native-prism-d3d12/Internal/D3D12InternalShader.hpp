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

#include "D3D12Shader.hpp"

#include <string>


namespace D3D12 {
namespace Internal {

class InternalShader: public Shader
{
    struct CBufferRegion
    {
        ResourceAssignment assignment;
        RingBuffer::Region region;

        CBufferRegion(ResourceAssignment a)
            : assignment(a)
        {}
    };

    struct CBufferDTable
    {
        uint32_t rootIndex;
        uint32_t count;
        DescriptorData dtable;

        CBufferDTable(uint32_t rootIndex, uint32_t count)
            : rootIndex(rootIndex)
            , count(count)
        {}
    };

    std::vector<CBufferRegion> mCBufferDescriptorRegions;
    std::vector<CBufferDTable> mCBufferDTables;
    size_t mTextureCount;
    size_t mSamplerCount;
    size_t mTotalRVDescriptorCount;
    uint32_t mTextureDTableRSIndex;
    DescriptorData mTextureDTable;
    uint32_t mSamplerDTableRSIndex;
    DescriptorData mSamplerDTable;

public:
    InternalShader();

    bool Init(const std::string& name, ShaderPipelineMode mode, D3D12_SHADER_VISIBILITY visibility, void* code, size_t codeSize) override;

    virtual bool PrepareShaderResources(const ShaderResourceHelpers& helpers, const NativeTextureBank& textures) override;
    virtual void ApplyShaderResources(const D3D12GraphicsCommandListPtr& commandList) const override;
};

} // namespace Internal
} // namespace D3D12
