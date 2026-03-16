/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
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

#include <memory>


namespace D3D12 {
namespace Internal {

class RenderThreadExecutable
{
public:
    virtual void Execute(const D3D12GraphicsCommandListPtr& commandList) = 0;
    virtual ~RenderThreadExecutable() {}
};

template <typename T>
class RenderThreadDataExecutable: public RenderThreadExecutable
{
protected:
    T mData;

public:
    RenderThreadDataExecutable(const T& data)
        : mData(data)
    {}
};

using RenderThreadExecutablePtr = std::unique_ptr<RenderThreadExecutable>;


// TODO TEMPORARY - used as a stub in non-ported parameters, should eventually be removed
class ApplyNoop: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr& commandList) override {}
};

template <typename T>
class ApplyNoopData: public RenderThreadDataExecutable<T>
{
public:
    ApplyNoopData(const T& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override {}
};

// Graphics render thread actions //

class ApplyIndexBuffer: public RenderThreadDataExecutable<D3D12_INDEX_BUFFER_VIEW>
{
public:
    ApplyIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override
    {
        commandList->IASetIndexBuffer(&mData);
    }
};

class ApplyVertexBuffer: public RenderThreadDataExecutable<D3D12_VERTEX_BUFFER_VIEW>
{
public:
    ApplyVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override
    {
        commandList->IASetVertexBuffers(0, 1, &mData);
    }
};

class ApplyPrimitiveTopology: public RenderThreadDataExecutable<D3D12_PRIMITIVE_TOPOLOGY>
{
public:
    ApplyPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override
    {
        commandList->IASetPrimitiveTopology(mData);
    }
};

class ApplyRootSignature: public RenderThreadDataExecutable<D3D12RootSignaturePtr>
{
public:
    ApplyRootSignature(const D3D12RootSignaturePtr& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override
    {
        commandList->SetGraphicsRootSignature(mData.Get());
    }
};

class ApplyScissor: public RenderThreadDataExecutable<D3D12_RECT>
{
public:
    ApplyScissor(const D3D12_RECT& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override
    {
        commandList->RSSetScissorRects(1, &mData);
    }
};

class ApplyViewport: public RenderThreadDataExecutable<D3D12_VIEWPORT>
{
public:
    ApplyViewport(const D3D12_VIEWPORT& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList) override
    {
        commandList->RSSetViewports(1, &mData);
    }
};

} // namespace Internal
} // namespace D3D12
