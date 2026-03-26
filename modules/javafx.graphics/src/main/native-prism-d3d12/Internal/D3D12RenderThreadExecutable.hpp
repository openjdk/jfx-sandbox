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

#include "D3D12IRenderTarget.hpp"
#include "D3D12PSOManager.hpp"
#include "D3D12ResourceManager.hpp"
#include "D3D12TextureBase.hpp"

#include <memory>


namespace D3D12 {
namespace Internal {

struct RenderingContextState
{
    PSOManager PSOManager;
    ResourceManager resourceManager;
    bool clearDelayed;
    D3D12_RECT clearRect;
    bool clearDepth;

    RenderingContextState(const NIPtr<NativeDevice>& nativeDevice)
        : PSOManager(nativeDevice)
        , resourceManager(nativeDevice)
        , clearDelayed(false)
        , clearRect()
        , clearDepth(false)
    {
    }
};

struct ClearRenderTargetArgs
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    Pixel_RGBA32_FLOAT rgba;
    D3D12_RECT clearRect;

    ClearRenderTargetArgs(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float r, float g, float b, float a, const D3D12_RECT& clearRect)
        : rtv(rtv)
        , rgba()
        , clearRect(clearRect)
    {
        rgba.r = r;
        rgba.g = g;
        rgba.b = b;
        rgba.a = a;
    }
};

struct ClearDepthStencilArgs
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    float depth;
    D3D12_RECT clearRect;

    ClearDepthStencilArgs(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth, const D3D12_RECT& clearRect)
        : dsv(dsv)
        , depth(depth)
        , clearRect(clearRect)
    {}
};

struct CopyTextureArgs
{
    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    uint32_t dstx;
    uint32_t dsty;
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12_BOX srcBox;
};

struct DrawArgs
{
    uint32_t elements;
    uint32_t vbOffset;
};

struct DispatchArgs
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

struct ResolveArgs
{
    ID3D12Resource* dst;
    ID3D12Resource* src;
    DXGI_FORMAT format;
};

struct ResolveRegionArgs
{
    ID3D12Resource* dst;
    uint32_t dstx;
    uint32_t dsty;
    ID3D12Resource* src;
    D3D12_RECT srcRect;
    DXGI_FORMAT format;
};

struct GraphicsShaders
{
    NIPtr<Shader> vertexShader;
    NIPtr<Shader> pixelShader;
};


class RenderThreadExecutable
{
public:
    virtual void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) = 0;
    virtual ~RenderThreadExecutable() {}
};

template <typename T>
class RenderThreadDataExecutable: public RenderThreadExecutable
{
protected:
    T mData;

public:
    RenderThreadDataExecutable()
        : mData()
    {}

    RenderThreadDataExecutable(const T& data)
        : mData(data)
    {}
};

using RenderThreadExecutablePtr = std::unique_ptr<RenderThreadExecutable>;


// Graphics render thread actions //

class ApplyDescriptorHeaps: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        ID3D12DescriptorHeap* heaps[] = {
            state.resourceManager.GetHeap().Get(),
            state.resourceManager.GetSamplerHeap().Get()
        };
        commandList->SetDescriptorHeaps(2, heaps);
    }
};

class ApplyIndexBuffer: public RenderThreadDataExecutable<D3D12_INDEX_BUFFER_VIEW>
{
public:
    ApplyIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->IASetIndexBuffer(&mData);
    }
};

class ApplyPipelineState: public RenderThreadDataExecutable<GraphicsPSOParameters>
{
public:
    ApplyPipelineState(const GraphicsPSOParameters& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        const D3D12PipelineStatePtr& pso = state.PSOManager.GetPSO(mData);
        commandList->SetPipelineState(pso.Get());
    }
};

class ApplyPrimitiveTopology: public RenderThreadDataExecutable<D3D12_PRIMITIVE_TOPOLOGY>
{
public:
    ApplyPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->IASetPrimitiveTopology(mData);
    }
};

class ApplyRenderTarget: public RenderThreadDataExecutable<NIPtr<IRenderTarget>>
{
public:
    ApplyRenderTarget(const NIPtr<IRenderTarget>& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        if (!mData) return;

        const Internal::DescriptorData& rtData = mData->GetRTVDescriptorData();
        commandList->OMSetRenderTargets(
            rtData.count, &rtData.cpu, true, mData->IsDepthTestEnabled() ? &mData->GetDSVDescriptorData().cpu : nullptr
        );
    }
};

class ApplyResources: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        state.resourceManager.ApplyResources(commandList);
    }
};

class ApplyRootSignature: public RenderThreadDataExecutable<D3D12RootSignaturePtr>
{
public:
    ApplyRootSignature(const D3D12RootSignaturePtr& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
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

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->RSSetScissorRects(1, &mData);
    }
};

class ApplyVertexBuffer: public RenderThreadDataExecutable<D3D12_VERTEX_BUFFER_VIEW>
{
public:
    ApplyVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->IASetVertexBuffers(0, 1, &mData);
    }
};

class ApplyViewport: public RenderThreadDataExecutable<D3D12_VIEWPORT>
{
public:
    ApplyViewport(const D3D12_VIEWPORT& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->RSSetViewports(1, &mData);
    }
};


// Graphics Pipeline Actions //

class ClearRenderTargetAction: public RenderThreadDataExecutable<ClearRenderTargetArgs>
{
public:
    ClearRenderTargetAction(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float r, float g, float b, float a, const D3D12_RECT& clearRect)
        : RenderThreadDataExecutable(ClearRenderTargetArgs(rtv, r, g, b, a, clearRect))
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->ClearRenderTargetView(
            mData.rtv,
            reinterpret_cast<const float*>(&mData.rgba),
            1, &mData.clearRect
        );
    }
};

class ClearDepthStencilAction: public RenderThreadDataExecutable<ClearDepthStencilArgs>
{
public:
    ClearDepthStencilAction(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth, const D3D12_RECT& clearRect)
        : RenderThreadDataExecutable(ClearDepthStencilArgs(dsv, depth, clearRect))
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->ClearDepthStencilView(mData.dsv, D3D12_CLEAR_FLAG_DEPTH, mData.depth, 0, 1, &mData.clearRect);
    }
};

class CopyTextureAction: public RenderThreadDataExecutable<CopyTextureArgs>
{
    bool hasBox;

public:
    CopyTextureAction(D3D12_TEXTURE_COPY_LOCATION dstLoc, uint32_t dstx, uint32_t dsty, D3D12_TEXTURE_COPY_LOCATION srcLoc, D3D12_BOX srcBox)
        : RenderThreadDataExecutable({dstLoc, dstx, dsty, srcLoc, srcBox})
        , hasBox(true)
    {}

    CopyTextureAction(D3D12_TEXTURE_COPY_LOCATION dstLoc, uint32_t dstx, uint32_t dsty, D3D12_TEXTURE_COPY_LOCATION srcLoc)
        : RenderThreadDataExecutable({dstLoc, dstx, dsty, srcLoc, {}})
        , hasBox(false)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->CopyTextureRegion(&mData.dstLoc, mData.dstx, mData.dsty, 0, &mData.srcLoc, hasBox ? &mData.srcBox : nullptr);
    }
};

class DrawAction: public RenderThreadDataExecutable<DrawArgs>
{
public:
    DrawAction(uint32_t elements, uint32_t vbOffset)
        : RenderThreadDataExecutable({elements, vbOffset})
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->DrawIndexedInstanced(mData.elements, 1, 0, mData.vbOffset, 0);
    }
};

class ResolveAction: public RenderThreadDataExecutable<ResolveArgs>
{
public:
    ResolveAction(ID3D12Resource* dst, ID3D12Resource* src, DXGI_FORMAT format)
        : RenderThreadDataExecutable({dst, src, format})
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->ResolveSubresource(mData.dst, 0, mData.src, 0, mData.format);
    }
};

class ResolveRegionAction: public RenderThreadDataExecutable<ResolveRegionArgs>
{
public:
    ResolveRegionAction(ID3D12Resource* dst, uint32_t dstx, uint32_t dsty, ID3D12Resource* src, D3D12_RECT srcRect, DXGI_FORMAT format)
        : RenderThreadDataExecutable({dst, dstx, dsty, src, srcRect, format})
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->ResolveSubresourceRegion(mData.dst, 0, mData.dstx, mData.dsty,
                                              mData.src, 0, &mData.srcRect,
                                              mData.format, D3D12_RESOLVE_MODE_AVERAGE);
    }
};


class ResourceBarrierAction: public RenderThreadDataExecutable<std::vector<D3D12_RESOURCE_BARRIER>>
{
public:
    ResourceBarrierAction(const std::vector<D3D12_RESOURCE_BARRIER>& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->ResourceBarrier(static_cast<UINT>(mData.size()), mData.data());
    }
};

class SetGraphicsShaders: public RenderThreadDataExecutable<GraphicsShaders>
{
public:
    SetGraphicsShaders(const GraphicsShaders& data)
        : RenderThreadDataExecutable(data)
    {
    }

    void Execute(const D3D12GraphicsCommandListPtr&, RenderingContextState& state) override final
    {
        state.resourceManager.SetVertexShader(mData.vertexShader);
        state.resourceManager.SetPixelShader(mData.pixelShader);
    }
};

class SetVertexShaderConstants: public RenderThreadDataExecutable<Shader::ConstantBuffer>
{
public:
    SetVertexShaderConstants(const NIPtr<Shader>& vertexShader)
        : RenderThreadDataExecutable(vertexShader->GetConstantStorage())
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, RenderingContextState& state) override final
    {
        state.resourceManager.SetVertexShaderConstants(std::move(mData));
    }
};

class SetPixelShaderConstants: public RenderThreadDataExecutable<Shader::ConstantBuffer>
{
public:
    SetPixelShaderConstants(const NIPtr<Shader>& pixelShader)
        : RenderThreadDataExecutable(pixelShader->GetConstantStorage())
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, RenderingContextState& state) override final
    {
        state.resourceManager.SetPixelShaderConstants(std::move(mData));
    }
};

class SetTextures: public RenderThreadDataExecutable<TextureBank>
{
public:
    SetTextures(const TextureBank& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, RenderingContextState& state) override final
    {
        state.resourceManager.SetTextures(mData);
    }
};


// Compute

class ApplyComputePipelineState: public RenderThreadDataExecutable<ComputePSOParameters>
{
public:
    ApplyComputePipelineState(const ComputePSOParameters& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        const D3D12PipelineStatePtr& pso = state.PSOManager.GetPSO(mData);
        commandList->SetPipelineState(pso.Get());
    }
};

class ApplyComputeRootSignature: public RenderThreadDataExecutable<D3D12RootSignaturePtr>
{
public:
    ApplyComputeRootSignature(const D3D12RootSignaturePtr& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->SetComputeRootSignature(mData.Get());
    }
};

class DispatchAction: public RenderThreadDataExecutable<DispatchArgs>
{
public:
    DispatchAction(uint32_t x, uint32_t y, uint32_t z)
        : RenderThreadDataExecutable({x, y, z})
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState&) override final
    {
        commandList->Dispatch(mData.x, mData.y, mData.z);
    }
};

class SetComputeShader: public RenderThreadDataExecutable<NIPtr<Shader>>
{
public:
    SetComputeShader(const NIPtr<Shader>& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, RenderingContextState& state) override final
    {
        state.resourceManager.SetComputeShader(mData);
    }
};

class SetComputeShaderConstants: public RenderThreadDataExecutable<Shader::ConstantBuffer>
{
public:
    SetComputeShaderConstants(const NIPtr<Shader>& computeShader)
        : RenderThreadDataExecutable(computeShader->GetConstantStorage())
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, RenderingContextState& state) override final
    {
        state.resourceManager.SetComputeShaderConstants(std::move(mData));
    }
};

} // namespace Internal
} // namespace D3D12
