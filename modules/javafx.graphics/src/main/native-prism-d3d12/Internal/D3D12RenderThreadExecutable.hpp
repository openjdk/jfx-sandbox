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

#include "../D3D12NativeSwapChain.hpp"

#include "D3D12IRenderTarget.hpp"
#include "D3D12ITrackedResource.hpp"
#include "D3D12LinearAllocator.hpp"
#include "D3D12RenderThreadState.hpp"
#include "D3D12Debug.hpp"

#include <memory>
#include <algorithm>
#include <list>


namespace D3D12 {
namespace Internal {

struct ClearRenderTargetArgs
{
    NIPtr<IRenderTarget> rt;
    Pixel_RGBA32_FLOAT rgba;
    D3D12_RECT clearRect;
    bool hasRect;

    ClearRenderTargetArgs(const NIPtr<IRenderTarget>& rt, float r, float g, float b, float a)
        : rt(rt), rgba(), clearRect(), hasRect(false)
    {
        rgba.r = r;
        rgba.g = g;
        rgba.b = b;
        rgba.a = a;
    }

    ClearRenderTargetArgs(const NIPtr<IRenderTarget>& rt, float r, float g, float b, float a, const D3D12_RECT& clearRect)
        : rt(rt), rgba(), clearRect(clearRect), hasRect(true)
    {
        rgba.r = r;
        rgba.g = g;
        rgba.b = b;
        rgba.a = a;
    }
};

struct ClearDepthStencilArgs
{
    NIPtr<ITrackedResource> dt;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
    float depth;
    D3D12_RECT clearRect;
    bool hasRect;

    ClearDepthStencilArgs(const NIPtr<ITrackedResource>& dt, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv, float depth)
        : dt(dt), dsv(dsv), depth(depth), clearRect(), hasRect(false)
    {}

    ClearDepthStencilArgs(const NIPtr<ITrackedResource>& dt, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv, float depth, const D3D12_RECT& clearRect)
        : dt(dt), dsv(dsv), depth(depth), clearRect(clearRect), hasRect(true)
    {}
};

struct CopyTextureArgs
{
    NIPtr<ITrackedResource> dstResource;
    NIPtr<ITrackedResource> srcResource;
    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    uint32_t dstx;
    uint32_t dsty;
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12_BOX srcBox;

    CopyTextureArgs(const NIPtr<ITrackedResource>& dstResource, const D3D12_TEXTURE_COPY_LOCATION& dstLoc, uint32_t dstx, uint32_t dsty, const NIPtr<ITrackedResource>& srcResource, const D3D12_TEXTURE_COPY_LOCATION& srcLoc, const D3D12_BOX& srcBox)
        : dstResource(dstResource), srcResource(srcResource), dstLoc(dstLoc), dstx(dstx), dsty(dsty), srcLoc(srcLoc), srcBox(srcBox)
    {}

    CopyTextureArgs(const NIPtr<ITrackedResource>& dstResource, const D3D12_TEXTURE_COPY_LOCATION& dstLoc, uint32_t dstx, uint32_t dsty, const NIPtr<ITrackedResource>& srcResource, const D3D12_TEXTURE_COPY_LOCATION& srcLoc)
        : dstResource(dstResource), dstLoc(dstLoc), dstx(dstx), dsty(dsty), srcResource(srcResource), srcLoc(srcLoc), srcBox()
    {}
};

struct CopyBufferRegionArgs
{
    D3D12ResourcePtr dst;
    uint64_t dstOffset;
    D3D12ResourcePtr src;
    uint64_t srcOffset;
    uint64_t size;

    CopyBufferRegionArgs(const D3D12ResourcePtr& dst, uint64_t dstOffset, const D3D12ResourcePtr& src, uint64_t srcOffset, uint64_t size)
        : dst(dst), dstOffset(dstOffset), src(src), srcOffset(srcOffset), size(size)
    {}
};

struct CopyResourceArgs
{
    D3D12ResourcePtr dst;
    D3D12ResourcePtr src;

    CopyResourceArgs(const D3D12ResourcePtr& dst, const D3D12ResourcePtr& src)
        : dst(dst), src(src)
    {}
};

struct DrawArgs
{
    uint32_t elements;
    uint32_t vbOffset;

    DrawArgs(uint32_t elements, uint32_t vbOffset)
        : elements(elements), vbOffset(vbOffset)
    {}
};

struct DispatchArgs
{
    uint32_t x;
    uint32_t y;
    uint32_t z;

    DispatchArgs(uint32_t x, uint32_t y, uint32_t z)
        : x(x), y(y), z(z)
    {}
};

struct EnsureStatesArgs
{
    NIPtr<IRenderTarget> rt;
    D3D12_RESOURCE_STATES texState;

    EnsureStatesArgs(const NIPtr<IRenderTarget>& rt, D3D12_RESOURCE_STATES texState)
        : rt(rt)
        , texState(texState)
    {}
};

struct SwapChainBarrierArgs
{
    NIPtr<NativeSwapChain> swapChain;
    D3D12_RESOURCE_STATES newState;

    SwapChainBarrierArgs(const NIPtr<NativeSwapChain>& swapChain, D3D12_RESOURCE_STATES newState)
        : swapChain(swapChain)
        , newState(newState)
    {}
};

struct PrepareSwapChainArgs
{
    NIPtr<NativeSwapChain> swapChain;
    D3D12_RECT dirtyRegion;

    PrepareSwapChainArgs(const NIPtr<NativeSwapChain>& swapChain, const D3D12_RECT& dirtyRegion)
        : swapChain(swapChain)
        , dirtyRegion(dirtyRegion)
    {}
};

struct PresentArgs
{
    NIPtr<NativeSwapChain> swapChain;

    PresentArgs(const NIPtr<NativeSwapChain>& swapChain)
        : swapChain(swapChain)
    {}
};

struct ResolveArgs
{
    NIPtr<ITrackedResource> dst;
    NIPtr<ITrackedResource> src;
    DXGI_FORMAT format;

    ResolveArgs(const NIPtr<ITrackedResource>& dst, const NIPtr<ITrackedResource>& src, DXGI_FORMAT format)
        : dst(dst), src(src), format(format)
    {}
};

struct ResolveRegionArgs
{
    NIPtr<ITrackedResource> dst;
    uint32_t dstx;
    uint32_t dsty;
    NIPtr<ITrackedResource> src;
    D3D12_RECT srcRect;
    DXGI_FORMAT format;

    ResolveRegionArgs(const NIPtr<ITrackedResource>& dst, uint32_t dstx, uint32_t dsty, const NIPtr<ITrackedResource>& src, const D3D12_RECT& srcRect, DXGI_FORMAT format)
        : dst(dst), dstx(dstx), dsty(dsty), src(src), srcRect(srcRect), format(format)
    {}
};

struct GraphicsShaders
{
    NIPtr<Shader> vertexShader;
    NIPtr<Shader> pixelShader;
};

struct DescriptorHeaps
{
    D3D12DescriptorHeapPtr heap;
    D3D12DescriptorHeapPtr samplerHeap;
};

struct ResourceBarrierArgs
{
    NIPtr<ITrackedResource> resource;
    D3D12_RESOURCE_BARRIER barrier;
};


// Descriptor bindings-related bits are in D3D12Common.hpp to make them visible to Shaders


class RenderThreadExecutable
{
public:
    virtual void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) = 0;
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

    RenderThreadDataExecutable(T&& data)
        : mData(std::move(data))
    {}

    template <typename ...Args>
    RenderThreadDataExecutable(Args&&... args)
        : mData(std::forward<Args>(args)...)
    {
    }
};

using RenderThreadExecutableDeleter = LinearAllocatorDeleter<RenderThreadExecutable>;
using RenderThreadExecutablePtr = std::unique_ptr<RenderThreadExecutable, RenderThreadExecutableDeleter>;

template <typename Executable, typename ...Args>
RenderThreadExecutablePtr CreateRTExec(LinearAllocator& allocator, Args&&... args)
{
    return RenderThreadExecutablePtr(allocator.Construct<Executable>(std::forward<Args>(args)...), RenderThreadExecutableDeleter(&allocator));
}


// Graphics render thread actions //

class ApplyIndexBuffer: public RenderThreadDataExecutable<D3D12_INDEX_BUFFER_VIEW>
{
public:
    ApplyIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->indexBuffer.Set(mData);
    }
};

class ApplyPipelineState: public RenderThreadDataExecutable<GraphicsPSOParameters>
{
public:
    ApplyPipelineState(const GraphicsPSOParameters& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->pipelineState.Set(rtState->PSOManager.GetPSO(mData));
    }
};

class ApplyPrimitiveTopology: public RenderThreadDataExecutable<D3D12_PRIMITIVE_TOPOLOGY>
{
public:
    ApplyPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->primitiveTopology.Set(mData);
    }
};

class ApplyRenderTarget: public RenderThreadDataExecutable<NIPtr<IRenderTarget>>
{
public:
    ApplyRenderTarget(const NIPtr<IRenderTarget>& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        if (!mData) return;

        const Internal::DescriptorData& rtData = mData->GetRTVDescriptorData();
        if (mData->IsDepthTestEnabled())
        {
            rtState->renderTarget.Set(RenderTargetCommandListData(
                rtData.count, rtData.cpu, mData->GetDSVDescriptorData().cpu
            ));
        }
        else
        {
            rtState->renderTarget.Set(RenderTargetCommandListData(
                rtData.count, rtData.cpu
            ));
        }
    }
};

class ApplyRootSignature: public RenderThreadDataExecutable<D3D12RootSignaturePtr>
{
public:
    ApplyRootSignature(const D3D12RootSignaturePtr& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->graphicsRootSignature.Set(mData);
    }
};

class ApplyScissor: public RenderThreadDataExecutable<D3D12_RECT>
{
public:
    ApplyScissor(const D3D12_RECT& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->scissor.Set(mData);
    }
};

class ApplyVertexBuffer: public RenderThreadDataExecutable<D3D12_VERTEX_BUFFER_VIEW>
{
public:
    ApplyVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->vertexBuffer.Set(mData);
    }
};

class ApplyViewport: public RenderThreadDataExecutable<D3D12_VIEWPORT>
{
public:
    ApplyViewport(const D3D12_VIEWPORT& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        rtState->viewport.Set(mData);
    }
};


// Graphics Pipeline Actions //

class ClearRenderTargetAction: public RenderThreadDataExecutable<ClearRenderTargetArgs>
{
public:
    ClearRenderTargetAction(const NIPtr<IRenderTarget>& rt, float r, float g, float b, float a)
        : RenderThreadDataExecutable(ClearRenderTargetArgs(rt, r, g, b, a))
    {}

    ClearRenderTargetAction(const NIPtr<IRenderTarget>& rt, float r, float g, float b, float a, const D3D12_RECT& clearRect)
        : RenderThreadDataExecutable(ClearRenderTargetArgs(rt, r, g, b, a, clearRect))
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->QueueTextureTransition(mData.rt->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        state->SubmitResourceTransitions(commandList);

        if (mData.hasRect)
        {
            commandList->ClearRenderTargetView(
                mData.rt->GetRTVDescriptorData().CPU(0),
                reinterpret_cast<const float*>(&mData.rgba),
                1, &mData.clearRect
            );
        }
        else
        {
            commandList->ClearRenderTargetView(
                mData.rt->GetRTVDescriptorData().CPU(0),
                reinterpret_cast<const float*>(&mData.rgba),
                0, nullptr
            );
        }
    }
};

class ClearDepthStencilAction: public RenderThreadDataExecutable<ClearDepthStencilArgs>
{
public:
    ClearDepthStencilAction(const NIPtr<ITrackedResource>& dt, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv, float depth)
        : RenderThreadDataExecutable(ClearDepthStencilArgs(dt, dsv, depth))
    {}

    ClearDepthStencilAction(const NIPtr<ITrackedResource>& dt, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv, float depth, const D3D12_RECT& clearRect)
        : RenderThreadDataExecutable(ClearDepthStencilArgs(dt, dsv, depth, clearRect))
    {}


    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->QueueTextureTransition(mData.dt, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        state->SubmitResourceTransitions(commandList);

        if (mData.hasRect)
        {
            commandList->ClearDepthStencilView(mData.dsv, D3D12_CLEAR_FLAG_DEPTH, mData.depth, 0, 1, &mData.clearRect);
        }
        else
        {
            commandList->ClearDepthStencilView(mData.dsv, D3D12_CLEAR_FLAG_DEPTH, mData.depth, 0, 0, nullptr);
        }
    }
};

class CopyTextureAction: public RenderThreadDataExecutable<CopyTextureArgs>
{
    bool hasBox;

public:
    CopyTextureAction(const NIPtr<ITrackedResource>& dstResource, D3D12_TEXTURE_COPY_LOCATION dstLoc, uint32_t dstx, uint32_t dsty, const NIPtr<ITrackedResource>& srcResource, D3D12_TEXTURE_COPY_LOCATION srcLoc, D3D12_BOX srcBox)
        : RenderThreadDataExecutable(dstResource, dstLoc, dstx, dsty, srcResource, srcLoc, srcBox)
        , hasBox(true)
    {}

    CopyTextureAction(const NIPtr<ITrackedResource>& dstResource, D3D12_TEXTURE_COPY_LOCATION dstLoc, uint32_t dstx, uint32_t dsty, const NIPtr<ITrackedResource>& srcResource, D3D12_TEXTURE_COPY_LOCATION srcLoc)
        : RenderThreadDataExecutable(dstResource, dstLoc, dstx, dsty, srcResource, srcLoc)
        , hasBox(false)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        // resolve D3D12 resources if locations have them as null, this can matter for ex. SwapChain
        if (mData.dstLoc.pResource == nullptr)
        {
            mData.dstLoc.pResource = mData.dstResource->GetD3D12Resource().Get();
            state->QueueTextureTransition(mData.dstResource, D3D12_RESOURCE_STATE_COPY_DEST);
        }

        if (mData.srcLoc.pResource == nullptr)
        {
            mData.srcLoc.pResource = mData.srcResource->GetD3D12Resource().Get();
            state->QueueTextureTransition(mData.srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
        }

        state->SubmitResourceTransitions(commandList);
        commandList->CopyTextureRegion(&mData.dstLoc, mData.dstx, mData.dsty, 0, &mData.srcLoc, hasBox ? &mData.srcBox : nullptr);
    }
};

class CopyBufferRegionAction: public RenderThreadDataExecutable<CopyBufferRegionArgs>
{
public:
    CopyBufferRegionAction(const D3D12ResourcePtr& dst, uint64_t dstOffset, const D3D12ResourcePtr& src, uint64_t srcOffset, uint64_t size)
        : RenderThreadDataExecutable(dst, dstOffset, src, srcOffset, size)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        commandList->CopyBufferRegion(mData.dst.Get(), mData.dstOffset, mData.src.Get(), mData.srcOffset, mData.size);
    }
};

class CopyResourceAction: public RenderThreadDataExecutable<CopyResourceArgs>
{
public:
    CopyResourceAction(const D3D12ResourcePtr& dst, const D3D12ResourcePtr& src)
        : RenderThreadDataExecutable(dst, src)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        commandList->CopyResource(mData.dst.Get(), mData.src.Get());
    }
};

class EnsureStatesAction: public RenderThreadDataExecutable<EnsureStatesArgs>
{
public:
    EnsureStatesAction(const NIPtr<IRenderTarget>& rt, D3D12_RESOURCE_STATES texState)
        : RenderThreadDataExecutable(rt, texState)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->QueueTextureTransition(mData.rt->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (mData.rt->HasDepthTexture())
            state->QueueTextureTransition(mData.rt->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

        for (uint32_t i = 0; i < Constants::MAX_TEXTURE_UNITS; ++i)
        {
            const NIPtr<TextureBase>& tex = state->resourceManager.GetTexture(i);
            if (tex) state->QueueTextureTransition(tex, mData.texState);
        }

        state->SubmitResourceTransitions(commandList);
    }
};

class DrawAction: public RenderThreadDataExecutable<DrawArgs>
{
public:
    DrawAction(uint32_t elements, uint32_t vbOffset)
        : RenderThreadDataExecutable(elements, vbOffset)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        commandList->DrawIndexedInstanced(mData.elements, 1, 0, mData.vbOffset, 0);
    }
};

class PrepareSwapChainAction: public RenderThreadDataExecutable<PrepareSwapChainArgs>
{
public:
    PrepareSwapChainAction(const NIPtr<NativeSwapChain>& swapchain, const D3D12_RECT& dirtyRegion)
        : RenderThreadDataExecutable(swapchain, dirtyRegion)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        mData.swapChain->Prepare(mData.dirtyRegion);

        state->QueueTextureTransition(mData.swapChain->GetTexture(), D3D12_RESOURCE_STATE_PRESENT);
        state->SubmitResourceTransitions(commandList);
    }
};

class PresentAction: public RenderThreadDataExecutable<PresentArgs>
{
public:
    PresentAction(const NIPtr<NativeSwapChain>& swapchain)
        : RenderThreadDataExecutable(swapchain)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        mData.swapChain->Present(state);
    }
};

class ResolveAction: public RenderThreadDataExecutable<ResolveArgs>
{
public:
    ResolveAction(const NIPtr<ITrackedResource>& dst, const NIPtr<ITrackedResource>& src, DXGI_FORMAT format)
        : RenderThreadDataExecutable(dst, src, format)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->QueueTextureTransition(mData.src, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        state->QueueTextureTransition(mData.dst, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        state->SubmitResourceTransitions(commandList);

        commandList->ResolveSubresource(mData.dst->GetD3D12Resource().Get(), 0, mData.src->GetD3D12Resource().Get(), 0, mData.format);
    }
};

class ResolveRegionAction: public RenderThreadDataExecutable<ResolveRegionArgs>
{
public:
    ResolveRegionAction(const NIPtr<ITrackedResource>& dst, uint32_t dstx, uint32_t dsty, const NIPtr<ITrackedResource>& src, D3D12_RECT srcRect, DXGI_FORMAT format)
        : RenderThreadDataExecutable(dst, dstx, dsty, src, srcRect, format)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->QueueTextureTransition(mData.src, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        state->QueueTextureTransition(mData.dst, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        state->SubmitResourceTransitions(commandList);

        commandList->ResolveSubresourceRegion(mData.dst->GetD3D12Resource().Get(), 0, mData.dstx, mData.dsty,
                                              mData.src->GetD3D12Resource().Get(), 0, &mData.srcRect,
                                              mData.format, D3D12_RESOLVE_MODE_AVERAGE);
    }
};

// this is an external barrier coming by RenderingContext's demand
// some cases like Buffer resources don't internally track their state
// TODO: maybe they should?
class ResourceBarrierAction: public RenderThreadDataExecutable<ResourceBarrierArgs>
{
public:
    ResourceBarrierAction(const NIPtr<ITrackedResource>& resource, const D3D12_RESOURCE_BARRIER& barrier)
        : RenderThreadDataExecutable(resource, barrier)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>&) override final
    {
        bool resourceViaProvider = (mData.barrier.Transition.pResource == nullptr);

        if (resourceViaProvider)
        {
            D3D12_RESOURCE_STATES oldState = mData.resource->GetD3D12ResourceState(mData.barrier.Transition.Subresource);
            if (oldState == mData.barrier.Transition.StateAfter) return;

            mData.barrier.Transition.pResource = mData.resource->GetD3D12Resource().Get();
            mData.barrier.Transition.StateBefore = oldState;
        }

        commandList->ResourceBarrier(1, &mData.barrier);

        if (resourceViaProvider)
        {
            mData.resource->SetD3D12ResourceState(mData.barrier.Transition.StateAfter, mData.barrier.Transition.Subresource);
        }
    }
};


// Graphics resource-related actions

class SetTexturesAction: public RenderThreadDataExecutable<TextureBank>
{
public:
    SetTexturesAction(const TextureBank& bank)
        : RenderThreadDataExecutable(bank)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetTextures(mData);
    }
};

class SetVertexShaderAction: public RenderThreadDataExecutable<NIPtr<Shader>>
{
public:
    SetVertexShaderAction(const NIPtr<Shader>& shader)
        : RenderThreadDataExecutable(shader)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetVertexShader(mData);
    }
};

class SetPixelShaderAction: public RenderThreadDataExecutable<NIPtr<Shader>>
{
public:
    SetPixelShaderAction(const NIPtr<Shader>& shader)
        : RenderThreadDataExecutable(shader)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetPixelShader(mData);
    }
};

class SetVertexShaderConstantsAction: public RenderThreadDataExecutable<ResourceManager::ShaderConstants>
{
public:
    SetVertexShaderConstantsAction(ResourceManager::ShaderConstants&& constants)
        : RenderThreadDataExecutable(std::move(constants))
    {
    }

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetVertexShaderConstants(std::move(mData));
    }
};

class SetPixelShaderConstantsAction: public RenderThreadDataExecutable<ResourceManager::ShaderConstants>
{
public:
    SetPixelShaderConstantsAction(ResourceManager::ShaderConstants&& constants)
        : RenderThreadDataExecutable(std::move(constants))
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetPixelShaderConstants(std::move(mData));
    }
};

class PrepareResources: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.DeclareRingResources();
        if (!state->resourceManager.PrepareResources())
        {
            D3D12NI_LOG_ERROR("RenderThread: Failed to prepare resources for draw call");
        }
    }
};

class ApplyResources: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& rtState) override final
    {
        // LKTODO HACK
        if (!rtState->heapsApplied)
        {
            ID3D12DescriptorHeap* heaps[2] = {
                rtState->resourceManager.GetHeap().Get(),
                rtState->resourceManager.GetSamplerHeap().Get(),
            };
            commandList->SetDescriptorHeaps(2, heaps);

            rtState->heapsApplied = true;
        }

        rtState->ApplySteps(commandList);
        rtState->resourceManager.ApplyResources(commandList);
    }
};


// Compute

class ApplyComputePipelineState: public RenderThreadDataExecutable<ComputePSOParameters>
{
public:
    ApplyComputePipelineState(const ComputePSOParameters& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        const D3D12PipelineStatePtr& pso = state->PSOManager.GetPSO(mData);
        commandList->SetPipelineState(pso.Get());
    }
};

class ApplyComputeRootSignature: public RenderThreadDataExecutable<D3D12RootSignaturePtr>
{
public:
    ApplyComputeRootSignature(const D3D12RootSignaturePtr& data)
        : RenderThreadDataExecutable(data)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>&) override final
    {
        commandList->SetComputeRootSignature(mData.Get());
    }
};

class DispatchAction: public RenderThreadDataExecutable<DispatchArgs>
{
public:
    DispatchAction(uint32_t x, uint32_t y, uint32_t z)
        : RenderThreadDataExecutable(x, y, z)
    {}

    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>&) override final
    {
        commandList->Dispatch(mData.x, mData.y, mData.z);
    }
};


// Compute resource actions

class SetComputeShaderAction: public RenderThreadDataExecutable<NIPtr<Shader>>
{
public:
    SetComputeShaderAction(const NIPtr<Shader>& shader)
        : RenderThreadDataExecutable(shader)
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetComputeShader(mData);
    }
};

class SetComputeShaderConstantsAction: public RenderThreadDataExecutable<ResourceManager::ShaderConstants>
{
public:
    SetComputeShaderConstantsAction(ResourceManager::ShaderConstants&& constants)
        : RenderThreadDataExecutable(std::move(constants))
    {}

    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.SetComputeShaderConstants(std::move(mData));
    }
};

class PrepareComputeResources: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.DeclareComputeRingResources();
        if (!state->resourceManager.PrepareComputeResources())
        {
            D3D12NI_LOG_ERROR("RenderThread: Failed to prepare resources for dispatch call");
        }
    }
};

class ApplyComputeResources: public RenderThreadExecutable
{
public:
    void Execute(const D3D12GraphicsCommandListPtr& commandList, const std::unique_ptr<RenderThreadState>& state) override final
    {
        state->resourceManager.ApplyComputeResources(commandList);
    }
};


// Other/internal Executables //

class InternalRenderThreadRoutine: public RenderThreadDataExecutable<std::function<void()>>
{
    // Only RenderThread (and LinearAllocator) should be allowed to construct these objects
    friend class RenderThread;
    friend class LinearAllocator;
    using Callback = std::function<void()>;

    InternalRenderThreadRoutine(Callback&& c)
        : RenderThreadDataExecutable(std::move(c))
    {}

public:
    void Execute(const D3D12GraphicsCommandListPtr&, const std::unique_ptr<RenderThreadState>&) override final
    {
        mData();
    }
};

} // namespace Internal
} // namespace D3D12
