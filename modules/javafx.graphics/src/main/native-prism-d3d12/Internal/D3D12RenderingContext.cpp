/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12RenderingContext.hpp"

#include "../D3D12NativeDevice.hpp"

#include "D3D12Config.hpp"
#include "D3D12Debug.hpp"
#include "D3D12MipmapGenComputeShader.hpp"
#include "D3D12Profiler.hpp"
#include "D3D12RenderPayload.hpp"
#include "D3D12Utils.hpp"


namespace D3D12 {
namespace Internal {

RenderPayloadPtr RenderingContext::ReplaceRTPayload()
{
    D3D12NI_ASSERT(mLastFlushTid == std::this_thread::get_id(), "CreateRTPayload() has to be called by the main thread");

    RenderPayloadPtr ret(std::move(mRTPayload));
    mRTPayload.reset(mPayloadAllocator.Construct<RenderPayload>());
    return ret;
}

void RenderingContext::SubmitRTPayload()
{
    D3D12NI_ASSERT(mLastFlushTid == std::this_thread::get_id(), "SubmitRTPayload() has to be called by the main thread");

    if (mRTPayload && mRTPayload->HasWork())
    {
        // current payload has some work that was not reflected on a Command List yet
        // move current payload to the Render Thread for execution and create a fresh one for later
        mRenderThread.Execute(ReplaceRTPayload());
    }
}

void RenderingContext::RecordClear(float r, float g, float b, float a, bool clearDepth, const D3D12_RECT& clearRect)
{
    QueueTextureTransition(mRenderTarget.Get()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (mRenderTarget.Get()->HasDepthTexture())
        QueueTextureTransition(mRenderTarget.Get()->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    SubmitResourceTransitions();

    mRTPayload->AddStep(CreateRTExec<ClearRenderTargetAction>(mPayloadAllocator, mRenderTarget.Get()->GetRTVDescriptorData().CPU(0), r, g, b, a, clearRect));
    // NOTE: Here we check by NativeRenderTarget::HasDepthTexture() and not IsDepthTestEnabled()
    // Prism can sometimes set the RTT with depth test disabled, but then request its clear with
    // the depth texture (ex. hello.HelloViewOrder) and only afterwards re-set the RTT again enabling
    // depth testing. So we have to disregard the depth test flag, otherwise we would miss this DSV clear.
    if (clearDepth && mRenderTarget.Get()->HasDepthTexture())
    {
        mRTPayload->AddStep(CreateRTExec<ClearDepthStencilAction>(mPayloadAllocator, mRenderTarget.Get()->GetDSVDescriptorData().CPU(0), 1.0f, clearRect));
    }
}

void RenderingContext::EnsureBoundTextureStates(D3D12_RESOURCE_STATES state)
{
    QueueTextureTransition(mRenderTarget.Get()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (mRenderTarget.Get()->HasDepthTexture())
        QueueTextureTransition(mRenderTarget.Get()->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);


    for (uint32_t i = 0; i < Constants::MAX_TEXTURE_UNITS; ++i)
    {
        const NIPtr<TextureBase>& tex = mResourceManager.GetTexture(i);
        if (tex) QueueTextureTransition(tex, state);
    }

    SubmitResourceTransitions();
}

void RenderingContext::QueueTextureTransition(const NIPtr<Internal::TextureBase>& tex, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    if (tex->GetResourceState(subresource) == newState) return;

    QueueResourceTransition(tex->GetResource(), tex->GetResourceState(subresource), newState, subresource);
    tex->SetResourceState(newState, subresource);
}

void RenderingContext::QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    D3D12NI_ZERO_STRUCT(mBarrierQueue[mBarrierQueueSize]);
    mBarrierQueue[mBarrierQueueSize].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    mBarrierQueue[mBarrierQueueSize].Transition.pResource = resource.Get();
    mBarrierQueue[mBarrierQueueSize].Transition.StateBefore = oldState;
    mBarrierQueue[mBarrierQueueSize].Transition.StateAfter = newState;
    mBarrierQueue[mBarrierQueueSize].Transition.Subresource = subresource;
    ++mBarrierQueueSize;

    // TODO: D3D12: this 8 should be a constant somewhere
    if (mBarrierQueueSize == 8) SubmitResourceTransitions();
}

void RenderingContext::SubmitResourceTransitions()
{
    if (mBarrierQueueSize == 0) return;

    mRTPayload->AddStep(CreateRTExec<ResourceBarrierAction>(mPayloadAllocator, mBarrierQueue, mBarrierQueueSize));
    mBarrierQueueSize = 0;
}

RenderingContext::QuadVertices RenderingContext::AssembleVertexQuadForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst)
{
    uint32_t srcWidth = src.x1 - src.x0;
    uint32_t srcHeight = src.y1 - src.y0;
    uint32_t dstWidth = dst.x1 - dst.x0;
    uint32_t dstHeight = dst.y1 - dst.y0;

    // default viewport coordinates are from -1 to 1
    // so destination has to be doubled and shifted from 0-2 range to -1-1 range
    Coords_UV_FLOAT srcTexelSize{ 1.0f / srcWidth, 1.0f / srcHeight };
    Coords_UV_FLOAT dstTexelSize{ 2.0f / dstWidth, 2.0f / dstHeight };

    // dstBox V coord is inverted because of d3d12's coordinate system
    Coords_UV_FLOAT srcBoxTexelsMin{ src.x0 * srcTexelSize.u, src.y0 * srcTexelSize.v };
    Coords_UV_FLOAT srcBoxTexelsMax{ src.x1 * srcTexelSize.u, src.y1 * srcTexelSize.v };
    Coords_UV_FLOAT dstBoxTexelsMin{ (dst.x0 * dstTexelSize.u) - 1.0f, (dst.y1 * dstTexelSize.v) - 1.0f };
    Coords_UV_FLOAT dstBoxTexelsMax{ (dst.x1 * dstTexelSize.u) - 1.0f, (dst.y0 * dstTexelSize.v) - 1.0f };

    QuadVertices result;

    // Build a fullscreen quad used for slow blit path
    result[0].pos.x = dstBoxTexelsMin.u;
    result[0].pos.y = dstBoxTexelsMin.v;
    result[0].uv1.u = srcBoxTexelsMin.u;
    result[0].uv1.v = srcBoxTexelsMin.v;

    result[1].pos.x = dstBoxTexelsMin.u;
    result[1].pos.y = dstBoxTexelsMax.v;
    result[1].uv1.u = srcBoxTexelsMin.u;
    result[1].uv1.v = srcBoxTexelsMax.v;

    result[2].pos.x = dstBoxTexelsMax.u;
    result[2].pos.y = dstBoxTexelsMin.v;
    result[2].uv1.u = srcBoxTexelsMax.u;
    result[2].uv1.v = srcBoxTexelsMin.v;

    result[3].pos.x = dstBoxTexelsMax.u;
    result[3].pos.y = dstBoxTexelsMax.v;
    result[3].uv1.u = srcBoxTexelsMax.u;
    result[3].uv1.v = srcBoxTexelsMax.v;

    for (uint32_t i = 0; i < result.size(); ++i)
    {
        result[i].pos.z = 0.0f;
        result[i].color.r = 255;
        result[i].color.g = 255;
        result[i].color.b = 255;
        result[i].color.a = 255;
        result[i].uv2 = result[i].uv1;
    }

    return result;
}

// NOTE technically we don't query buffer ptr's size, but this function assumes we reserved enough
// space already.
BBox RenderingContext::AssembleVertexData(void* buffer, const Internal::MemoryView<float>& vertices,
                                          const Internal::MemoryView<signed char>& colors, UINT elementCount)
{
    Vertex_2D* bufVertices = reinterpret_cast<Vertex_2D*>(buffer);
    BBox bbox;

    size_t vertIdx = 0;
    size_t colorIdx = 0;
    for (UINT i = 0; i < elementCount; ++i)
    {
        D3D12NI_ASSERT(vertIdx < vertices.Size(), "Exceeded vertex array size");
        D3D12NI_ASSERT(colorIdx < colors.Size(), "Exceeded color array size");
        bufVertices[i].pos.x = vertices.Data()[vertIdx++];
        bufVertices[i].pos.y = vertices.Data()[vertIdx++];
        bufVertices[i].pos.z = vertices.Data()[vertIdx++];
        bufVertices[i].color.r = colors.Data()[colorIdx++];
        bufVertices[i].color.g = colors.Data()[colorIdx++];
        bufVertices[i].color.b = colors.Data()[colorIdx++];
        bufVertices[i].color.a = colors.Data()[colorIdx++];
        bufVertices[i].uv1.u = vertices.Data()[vertIdx++];
        bufVertices[i].uv1.v = vertices.Data()[vertIdx++];
        bufVertices[i].uv2.u = vertices.Data()[vertIdx++];
        bufVertices[i].uv2.v = vertices.Data()[vertIdx++];
    }

    if (elementCount == 4)
    {
        // only create a valid bbox when we render a quad
        // quad is the only way we can be sure bbox is valid
        // TODO: D3D12: maybe we should lift that limitation some day, would require reworking
        // bbox merging though and might be too heavy CPU wise
        for (UINT i = 0; i < elementCount; ++i)
        {
            bbox.Merge(bufVertices[i].pos.x, bufVertices[i].pos.y, bufVertices[i].pos.x, bufVertices[i].pos.y);
        }
    }

    return bbox;
}

RenderingContext::VertexSubregion RenderingContext::GetNewRegionForVertices(uint32_t vertexCount)
{
    if (vertexCount > (Constants::MAX_BATCH_VERTICES / 2))
    {
        // rendering more vertices might utilize the Ring Buffer better if we just reserve a separate space for them
        mVertexRingBuffer->DeclareRequired(vertexCount * 8 * sizeof(float));
        Internal::GPURingBuffer::GPURegion region = mVertexRingBuffer->ReserveCPU(vertexCount * 8 * sizeof(float));

        VertexSubregion separateRegion;
        separateRegion.subregion = region.cpuRegion;
        if (!separateRegion)
        {
            D3D12NI_LOG_ERROR("2D Vertex Ring Buffer allocation failed");
            return VertexSubregion();
        }

        separateRegion.view.BufferLocation = region.gpuRegion.gpu;
        separateRegion.view.SizeInBytes = static_cast<UINT>(region.gpuRegion.size);
        separateRegion.view.StrideInBytes = sizeof(Vertex_2D);

        return separateRegion;
    }

    if (!m2DVertexBatch.Valid() || vertexCount > m2DVertexBatch.Available())
    {
        // reserve space on Ring Buffer
        mVertexRingBuffer->DeclareRequired(Constants::MAX_BATCH_VERTICES * 8 * sizeof(float));

        Internal::GPURingBuffer::GPURegion newVertexRegion = mVertexRingBuffer->ReserveCPU(Constants::MAX_BATCH_VERTICES * 8 * sizeof(float));
        if (!newVertexRegion.cpuRegion)
        {
            D3D12NI_LOG_ERROR("2D Vertex Ring Buffer allocation failed");
            return VertexSubregion();
        }

        m2DVertexBatch.AssignNewRegion(newVertexRegion.cpuRegion, newVertexRegion.gpuRegion);
    }

    return m2DVertexBatch.Subregion(vertexCount);
}

void RenderingContext::ClearAppliedFlags()
{
    mIndexBuffer.ClearApplied();
    mVertexBuffer.ClearApplied();
    mDescriptorHeaps.ClearApplied();
    mDescriptors.ClearApplied();
    mPipelineState.ClearApplied();
    mPrimitiveTopology.ClearApplied();
    mRootSignature.ClearApplied();
    mRenderTarget.ClearApplied();
    mScissor.ClearApplied();
    mDefaultScissor.ClearApplied();
    mViewport.ClearApplied();

    mComputePipelineState.ClearApplied();
    mComputeRootSignature.ClearApplied();
}


RenderingContext::RenderingContext(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mResourceManager(nativeDevice)
    , mPayloadAllocator()
    , mCommandQueue()
    , mRenderThread(nativeDevice)
    , mRTPayload(nullptr, LinearAllocatorDeleter<RenderPayload>(&mPayloadAllocator))
    , mCheckpointQueue()
    , m2DVertexBatch()
    , mVertexRingBuffer()
    , mClearOptState()
    , mIndexBuffer()
    , mVertexBuffer()
    , mDescriptorHeaps()
    , mDescriptors()
    , mPipelineState()
    , mPrimitiveTopology()
    , mRenderTarget()
    , mScissor()
    , mDefaultScissor()
    , mViewport()
    , mComputePipelineState()
    , mComputeRootSignature()
    , mComputeDescriptors()
    , mUsedRTs()
    , mBarrierQueue()
    , mBarrierQueueSize(0)
{
    D3D12NI_LOG_DEBUG("RenderingContext: D3D12 API opts are %s", Config::IsApiOptsEnabled() ? "enabled" : "disabled");

    mPrimitiveTopology.Set(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Some parameters/steps depend on PSO being set
    RenderingStep::StepDependency psoDep = [&pso = mPipelineState]() -> bool
    {
        return pso.IsSet();
    };
    mRootSignature.SetDependency(psoDep);
    mDescriptorHeaps.SetDependency(psoDep);
    mDescriptors.SetDependency(psoDep);

    // Use the default scissor only if other custom scissor rect is not set
    // See SetRenderTarget() for more details
    mDefaultScissor.SetDependency([&scissor = mScissor]() -> bool
    {
        return !scissor.IsSet();
    });

    RenderingStep::StepDependency computePsoDep = [&computePso = mComputePipelineState]() -> bool
    {
        return computePso.IsSet();
    };
    mComputeRootSignature.SetDependency(computePsoDep);
    mComputeDescriptors.SetDependency(computePsoDep);

    mLastFlushTid = std::this_thread::get_id();

    ReplaceRTPayload();
}

bool RenderingContext::Init()
{
    D3D12_COMMAND_QUEUE_DESC cqDesc;
    D3D12NI_ZERO_STRUCT(cqDesc);
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    // TODO Command Queue should probably reside in Command List Pool
    //      Same with all Execute/Signal logic & checkpoints
    HRESULT hr = mNativeDevice->GetDevice()->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mCommandQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Direct Command Queue");

    hr = mCommandQueue->SetName(L"Main Command Queue");
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to name Direct Command Queue");

    hr = mNativeDevice->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create in-device Fence");

    if (!mResourceManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Resource Manager");
        return false;
    }

    mDescriptorHeaps.SetHeap(mResourceManager.GetHeap());
    mDescriptorHeaps.SetSamplerHeap(mResourceManager.GetSamplerHeap());

    if (!mRenderThread.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Render Thread");
        return false;
    }

    // TODO adjust thresholds if this idea works fine for memory optimization
    mVertexRingBuffer = std::make_shared<Internal::GPURingBuffer>(mNativeDevice);
    mVertexRingBuffer->SetDebugName("2D Vertex GPU Ring Buffer");
    if (!mVertexRingBuffer->Init(Internal::Config::MainRingBufferThreshold(), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT))
    {
        D3D12NI_LOG_ERROR("Failed to initialize 2D Vertex Ring Buffer");
        return false;
    }

    return true;
}

void RenderingContext::Release()
{
    mRenderThread.WaitForCompletion();
    mCheckpointQueue.WaitForNextCheckpoint(CheckpointType::ALL);
    mCheckpointQueue.PrintStats();

    mRenderThread.Exit();

    mNativeDevice.reset();
}

void RenderingContext::Clear(float r, float g, float b, float a, bool clearDepth)
{
    if (!mRenderTarget.IsSet()) return;

    // LKTODO I think Apply is not needed here...?
    //mRenderTarget.Apply(mNativeDevice->GetCurrentCommandList(), mState);
    DescriptorData rtData = mRenderTarget.Get()->GetRTVDescriptorData();

    // if the RTT was NOT fully used we don't have to clear the whole thing
    // determine how much of the space actually needs to be cleared (unless the request is for a smaller section)
    D3D12_RECT clearRect = GetScissor().Get();
    const BBox& rttDirtyBBox = mRenderTarget.Get()->GetDirtyBBox();

    if (Config::Instance().IsClearOptsEnabled() && mRenderTarget.Get()->BBoxEnabled() &&
        rttDirtyBBox.Valid() && rttDirtyBBox.Inside(clearRect))
    {
        // if RTT was dirited by less area than the clear rect demands it AND the clear area
        // contains our dirty bbox, we can safely shrink the clear rect to save some clear time
        clearRect.left = std::max(clearRect.left, static_cast<LONG>(std::round(rttDirtyBBox.min.x)));
        clearRect.top = std::max(clearRect.top, static_cast<LONG>(std::round(rttDirtyBBox.min.y)));
        clearRect.right = std::min(clearRect.right, static_cast<LONG>(std::round(rttDirtyBBox.max.x)));
        clearRect.bottom = std::min(clearRect.bottom, static_cast<LONG>(std::round(rttDirtyBBox.max.y)));
    }

    if (Config::Instance().IsClearOptsEnabled() &&
        r == 0.0f && g == 0.0f && b == 0.0f && a == 0.0f)
    {
        // clearing to all zeroes could be optimized out by directly overdrawing the RT
        // delay the clear until first Draw() call (or until RT switch) to see if it's actually possible
        mClearOptState.clearDelayed = true;
        mClearOptState.clearDepth = clearDepth;
        mClearOptState.clearRect = clearRect;
    }
    else
    {
        RecordClear(r, g, b, a, clearDepth, clearRect);
    }
}

// this is a pass-through to initialize the depth texture for an RTT
// we do this separately than the regular Clear() command when initializing depth textures
void RenderingContext::ClearDepth(const D3D12_CPU_DESCRIPTOR_HANDLE& dsv)
{
    mRTPayload->AddStep(CreateRTExec<ClearDepthStencilAction>(mPayloadAllocator, dsv, 1.0f));
}

void RenderingContext::Draw(uint32_t elements, uint32_t vbOffset)
{
    BBox invalidBox;
    Draw(elements, vbOffset, invalidBox);
}

void RenderingContext::Draw(uint32_t elements, uint32_t vbOffset, const BBox& dirtyBBox)
{
    bool clearDiscarded = false;
    CompositeMode currentCompositeMode = mPipelineState.Get().compositeMode;

    if (mClearOptState.clearDelayed)
    {
        // Check if we can discard this clear.
        // The clear can be discarded if we use composite mode SRC_OVER and
        // this draw call will overwrite the entire to-be-cleared area of the RTT.
        //
        // NOTE: compared to other parts related to Clear optimization here we're being
        // a bit more cautions with coordinates - min bbox gets ceil-ed while max bbox gets floor-ed.
        // There can be situations where despite coordinates crossing the 0.5 rounding "barrier" the
        // runtime won't actually render and overwrite pixels on the RTT (happens occasionally in CircleBlendAdd
        // renderperf test). This will create single-frame artifacts, because old RTT contents won't be
        // overwritten by the primitive we want to draw. To prevent those occasional artifacts we must push
        // a Clear() through here - under-estimating BBox coordinates makes it possible and ensures visual
        // correctness when using clear optimizations.
        if (currentCompositeMode == CompositeMode::SRC_OVER && dirtyBBox.Valid() &&
            std::ceil(dirtyBBox.min.x) <= mClearOptState.clearRect.left  && std::ceil(dirtyBBox.min.y) <= mClearOptState.clearRect.top &&
            std::floor(dirtyBBox.max.x) >= mClearOptState.clearRect.right && std::floor(dirtyBBox.max.y) >= mClearOptState.clearRect.bottom)
        {
            clearDiscarded = true;
            SetCompositeMode(CompositeMode::SRC);
        }
        else
        {
            RecordClear(0.0f, 0.0f, 0.0f, 0.0f, mClearOptState.clearDepth, mClearOptState.clearRect);
        }

        mClearOptState.clearDelayed = false;
    }

    // apply Context settings to the payload
    if (!Apply())
    {
        D3D12NI_LOG_ERROR("Failed to apply Rendering Context settings. Skipping draw call.");
        return;
    }

    // we separately ensure that textures bound to the Context are in correct state
    // there can be a situation where a Texture was bound to the Context and then updated
    // via updateTexture(). Its state will have to be re-set back to PIXEL_SHADER_RESOURCE
    // before the draw call.
    EnsureBoundTextureStates(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Tell RenderThread to submit a Draw command and record what we have
    if (mRTPayload->AddStep(CreateRTExec<DrawAction>(mPayloadAllocator, elements, vbOffset)))
    {
        SubmitRTPayload();
    }

    mRenderTarget.Get()->UpdateDirtyBBox(dirtyBBox);

    if (clearDiscarded)
    {
        // restore original composite mode
        SetCompositeMode(currentCompositeMode);
    }
}

void RenderingContext::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    if (!ApplyCompute())
    {
        D3D12NI_LOG_ERROR("Failed to apply Compute Rendering Context settings. Skipping dispatch call.");
        return;
    }

    if (mRTPayload->AddStep(CreateRTExec<DispatchAction>(mPayloadAllocator, x, y, z)))
    {
        SubmitRTPayload();
    }
}

void RenderingContext::Resolve(const NIPtr<TextureBase>& dstTexture, const NIPtr<TextureBase>& srcTexture, DXGI_FORMAT resolveFormat)
{
    QueueTextureTransition(srcTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    QueueTextureTransition(dstTexture, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    SubmitResourceTransitions();

    mRTPayload->AddStep(CreateRTExec<ResolveAction>(mPayloadAllocator, dstTexture->GetResource().Get(), srcTexture->GetResource().Get(), resolveFormat));
}

void RenderingContext::ResolveRegion(const NIPtr<TextureBase>& dstTexture, uint32_t dstx, uint32_t dsty,
                                     const NIPtr<TextureBase>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch,
                                     DXGI_FORMAT resolveFormat)
{
    D3D12_RECT srcRect;
    srcRect.left = srcx;
    srcRect.top = srcy;
    srcRect.right = srcx + srcw;
    srcRect.bottom = srcy + srch;

    QueueTextureTransition(srcTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    QueueTextureTransition(dstTexture, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    SubmitResourceTransitions();

    mRTPayload->AddStep(CreateRTExec<ResolveRegionAction>(mPayloadAllocator, dstTexture->GetResource().Get(), dstx, dsty, srcTexture->GetResource().Get(), srcRect, resolveFormat));
}

void RenderingContext::CopyTexture(const NIPtr<TextureBase>& dstTexture, uint32_t dstx, uint32_t dsty,
                                   const NIPtr<TextureBase>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch)
{
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.pResource = srcTexture->GetResource().Get();
    srcLoc.SubresourceIndex = 0;

    D3D12_BOX srcBox;
    D3D12NI_ZERO_STRUCT(srcBox);
    srcBox.left = srcx;
    srcBox.top = srcy;
    srcBox.right = srcx + srcw;
    srcBox.bottom = srcy + srch;
    srcBox.front = 0;
    srcBox.back = 1;

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    D3D12NI_ZERO_STRUCT(dstLoc);
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.pResource = dstTexture->GetResource().Get();
    dstLoc.SubresourceIndex = 0;

    QueueTextureTransition(srcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
    QueueTextureTransition(dstTexture, D3D12_RESOURCE_STATE_COPY_DEST);
    SubmitResourceTransitions();

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(mPayloadAllocator, dstLoc, dstx, dsty, srcLoc, srcBox));
}

void RenderingContext::CopyTexture(const Buffer& dstBuffer, uint32_t dstStride, const NIPtr<NativeTexture>& srcTexture,
                                   uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch)
{
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.pResource = srcTexture->GetResource().Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    D3D12_BOX srcBox;
    srcBox.left = srcx;
    srcBox.top = srcy;
    srcBox.right = srcx + srcw;
    srcBox.bottom = srcy + srch;
    srcBox.front = 0;
    srcBox.back = 1;

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    D3D12NI_ZERO_STRUCT(dstLoc);
    dstLoc.pResource = dstBuffer.GetResource().Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint.Footprint.Width = srcw;
    dstLoc.PlacedFootprint.Footprint.Height = srch;
    dstLoc.PlacedFootprint.Footprint.Depth = 1;
    dstLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(dstStride);
    dstLoc.PlacedFootprint.Footprint.Format = srcTexture->GetFormat();
    dstLoc.PlacedFootprint.Offset = 0;

    QueueTextureTransition(srcTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
    SubmitResourceTransitions();

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(mPayloadAllocator, dstLoc, 0, 0, srcLoc, srcBox));
}

void RenderingContext::CopyToTexture(const NIPtr<TextureBase>& dstTexture, uint32_t dstx, uint32_t dsty,
                                     ID3D12Resource* srcResource, uint32_t srcw, uint32_t srch, uint64_t srcOffset,
                                     uint32_t srcStride, DXGI_FORMAT format)
{
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.pResource = srcResource;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Footprint.Width = srcw;
    srcLoc.PlacedFootprint.Footprint.Height = srch;
    srcLoc.PlacedFootprint.Footprint.Depth = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = srcStride;
    srcLoc.PlacedFootprint.Footprint.Format = format;
    srcLoc.PlacedFootprint.Offset = srcOffset;

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    D3D12NI_ZERO_STRUCT(dstLoc);
    dstLoc.pResource = dstTexture->GetResource().Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    // Ensure we are in COPY_DEST state. Texture can be now bound to RenderingContext
    // and exist in a different state.
    QueueTextureTransition(dstTexture, D3D12_RESOURCE_STATE_COPY_DEST);
    SubmitResourceTransitions();

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(mPayloadAllocator, dstLoc, dstx, dsty, srcLoc));
}

void RenderingContext::CopyBufferRegion(const D3D12ResourcePtr& dst, uint64_t dstOffset, const D3D12ResourcePtr& src, uint64_t srcOffset, uint64_t size)
{
    mRTPayload->AddStep(CreateRTExec<CopyBufferRegionAction>(mPayloadAllocator, dst, dstOffset, src, srcOffset, size));
}

void RenderingContext::CopyResource(const D3D12ResourcePtr& dst, const D3D12ResourcePtr& src)
{
    mRTPayload->AddStep(CreateRTExec<CopyResourceAction>(mPayloadAllocator, dst, src));
}

bool RenderingContext::GenerateMipmaps(const NIPtr<NativeTexture>& texture)
{
    if (!texture->HasMipmaps()) return true;

    uint32_t mipLevels = texture->GetMipLevels();

    const NIPtr<Internal::Shader>& cs = mNativeDevice->GetInternalShader("MipmapGenCS");
    SetComputeShader(cs);
    SetTexture(0, texture);

    uint32_t srcWidth = static_cast<uint32_t>(texture->GetWidth());
    uint32_t srcHeight = static_cast<uint32_t>(texture->GetHeight());

    // transition entire texture with mips to UAV state
    QueueTextureTransition(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    SubmitResourceTransitions();

    // starting from 1, level 0 is our base mip level
    // also note, we're divinding by 2 a lot, so to make it faster we'll bit-shift instead
    MipmapGenComputeShader::CBuffer constants;
    uint32_t mipMapCount = mipLevels - 1; // mipLevels includes base level so we need to skip it
    for (uint32_t mipBase = 0; mipBase < mipMapCount; mipBase += constants.numLevels)
    {
        // dimensions of first mipmap
        uint32_t mip1Width = (srcWidth >> 1);
        uint32_t mip1Height = (srcHeight >> 1);

        // check how many times we can downsample at once
        // we guarantee at least one downsample (MipmapGenCS has the initial "heavy" path for that purpose)
        // but if both mip1Width and mip1Height have any zero-LSBs past that, we can do more downsamples within
        // one dispatch and save some CS invocations. Worst case scenario our texture dimensions have all LSB
        // bits set to 1, but it is an extremely rare occurence.
        uint32_t widthZeros = Internal::Utils::CountZeroBitsLSB(mip1Width, 3);
        uint32_t heightZeros = Internal::Utils::CountZeroBitsLSB(mip1Height, 3);
        uint32_t levels = 1 + std::min<uint32_t>(widthZeros, heightZeros);

        constants.sourceLevel = mipBase; // base level is one higher than our first mip
        constants.numLevels = std::min<uint32_t>(levels, mipMapCount - mipBase);
        constants.texelSizeMip1[0] = 1.0f / mip1Width;
        constants.texelSizeMip1[1] = 1.0f / mip1Height;
        cs->SetConstants("gData", &constants, sizeof(constants));

        // transition base level to non-PS-resource
        QueueTextureTransition(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                               Utils::CalcSubresource(mipBase, texture->GetMipLevels(), 0));

        // each thread group manages an 8x8 square, so we need to dispatch (width/8) X groups and (height/8) Y groups
        SubmitResourceTransitions();

        Dispatch(std::max<UINT>(srcWidth >> 3, 1), std::max<UINT>(srcHeight >> 3, 1), 1);

        // transition base level back to UAV
        // this should make all subresources have the same state again
        QueueTextureTransition(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                               Utils::CalcSubresource(mipBase, texture->GetMipLevels(), 0));

        srcWidth >>= constants.numLevels;
        srcHeight >>= constants.numLevels;
    }

    return true;
}

void RenderingContext::ClearTextureUnit(uint32_t unit)
{
    if (!mResourceManager.GetTexture(unit)) return;

    mResourceManager.SetTexture(unit, nullptr);
}

void RenderingContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView)
{
    if (mIndexBuffer.Get().BufferLocation == ibView.BufferLocation &&
        mIndexBuffer.Get().Format == ibView.Format &&
        mIndexBuffer.Get().SizeInBytes == ibView.SizeInBytes)
        return;

    mIndexBuffer.Set(ibView);
}

void RenderingContext::SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbView)
{
    if (mVertexBuffer.Get().BufferLocation == vbView.BufferLocation &&
        mVertexBuffer.Get().SizeInBytes == vbView.SizeInBytes &&
        mVertexBuffer.Get().StrideInBytes == vbView.StrideInBytes)
        return;

    mVertexBuffer.Set(vbView);
}

BBox RenderingContext::SetVertexBufferForQuads(const Internal::MemoryView<float>& vertices, const Internal::MemoryView<signed char>& colors,
                                               uint32_t vertexCount, uint32_t& retOffset)
{
    BBox box;

    VertexSubregion vertexRegion = GetNewRegionForVertices(vertexCount);
    if (!vertexRegion) box;

    box = AssembleVertexData(vertexRegion.subregion.cpu, vertices, colors, vertexCount);
    SetVertexBuffer(vertexRegion.view);

    retOffset = vertexRegion.startOffset;
    return box;
}

BBox RenderingContext::SetVertexBufferForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst, uint32_t& retOffset)
{
    BBox box;

    QuadVertices fsQuad = AssembleVertexQuadForBlit(src, dst);

    VertexSubregion vertexRegion = GetNewRegionForVertices(static_cast<uint32_t>(fsQuad.size()));
    if (!vertexRegion) return box;

    memcpy(vertexRegion.subregion.cpu, fsQuad.data(), fsQuad.size() * sizeof(Vertex_2D));
    SetVertexBuffer(vertexRegion.view);

    for (uint32_t i = 0; i < fsQuad.size(); ++i)
    {
        box.Merge(fsQuad[i].pos.x, fsQuad[i].pos.y, fsQuad[i].pos.x, fsQuad[i].pos.y);
    }

    return box;
}

void RenderingContext::SetRenderTarget(const NIPtr<IRenderTarget>& renderTarget)
{
    if (renderTarget == mRenderTarget.Get())
    {
        // faster path just to double-check if depth testing and MSAA should be enabled
        if (renderTarget)
        {
            mPipelineState.SetDepthTest(renderTarget->IsDepthTestEnabled());
            mPipelineState.SetMSAASamples(renderTarget->GetMSAASamples());

            // this is to ensure RTT set gets re-recorded on the command list
            // this can also include a Depth Texture in case this Set is called again with depthTest = true
            mRenderTarget.ClearApplied();
        }

        return;
    }

    if (mClearOptState.clearDelayed)
    {
        // there was a Clear() queued but we're changing the RT
        // we should submit the delayed Clear() call before we swap the RTs
        RecordClear(0.0f, 0.0f, 0.0f, 0.0f, mClearOptState.clearDepth, mClearOptState.clearRect);
        mClearOptState.clearDelayed = false;
    }

    mRenderTarget.Set(renderTarget);
    if (!renderTarget) return;

    // D3D9 behavior emulation - setting a new RenderTarget should disable
    // scissor testing and set the viewport to RT dimensions
    mScissor.Unset();

    D3D12_VIEWPORT viewport;
    D3D12NI_ZERO_STRUCT(viewport);
    viewport.Width = static_cast<float>(renderTarget->GetWidth());
    viewport.Height = static_cast<float>(renderTarget->GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    mViewport.Set(viewport);

    // for when we don't use custom scissor. D3D12 always has scissor testing enabled
    // so to "disable" it we need to set the scissor region to the whole viewport.
    D3D12_RECT defaultScissor;
    defaultScissor.left = 0;
    defaultScissor.top = 0;
    defaultScissor.right = static_cast<LONG>(renderTarget->GetWidth());
    defaultScissor.bottom = static_cast<LONG>(renderTarget->GetHeight());
    mDefaultScissor.Set(defaultScissor);

    mPipelineState.SetDepthTest(renderTarget->IsDepthTestEnabled());
    mPipelineState.SetMSAASamples(renderTarget->GetMSAASamples());

    mUsedRTs.insert(renderTarget);
}

void RenderingContext::SetScissor(bool enabled, const D3D12_RECT& scissor)
{
    if (!enabled)
    {
        // disabling scissor testing means we should unset the existing Scissor
        // and ensure next Apply() call will apply the default (full-viewport) scissor
        mScissor.Unset();
        mDefaultScissor.ClearApplied();
    }
    else
    {
        mScissor.Set(scissor);
    }
}

void RenderingContext::SetTexture(uint32_t unit, const NIPtr<TextureBase>& texture)
{
    mResourceManager.SetTexture(unit, texture);
}

// PSO-related setters

void RenderingContext::SetCompositeMode(CompositeMode mode)
{
    if (mode == mPipelineState.Get().compositeMode) return;

    mPipelineState.SetCompositeMode(mode);
}

void RenderingContext::SetCullMode(D3D12_CULL_MODE mode)
{
    if (mode == mPipelineState.Get().cullMode) return;

    mPipelineState.SetCullMode(mode);
}

void RenderingContext::SetFillMode(D3D12_FILL_MODE mode)
{
    if (mode == mPipelineState.Get().fillMode) return;

    mPipelineState.SetFillMode(mode);
}

void RenderingContext::SetVertexShader(const NIPtr<Shader>& vertexShader)
{
    if (mPipelineState.Get().vertexShader == vertexShader) return;

    mPipelineState.SetVertexShader(vertexShader);
    mResourceManager.SetVertexShader(vertexShader);
}

void RenderingContext::SetPixelShader(const NIPtr<Shader>& pixelShader)
{
    if (mPipelineState.Get().pixelShader == pixelShader) return;

    mPipelineState.SetPixelShader(pixelShader);
    mResourceManager.SetPixelShader(pixelShader);

    if (pixelShader)
    {
        mRootSignature.Set(mNativeDevice->GetRootSignatureManager()->GetGraphicsRootSignature());
    }
}

void RenderingContext::SetComputeShader(const NIPtr<Shader>& computeShader)
{
    if (mComputePipelineState.Get().shader == computeShader) return;

    mComputePipelineState.SetComputeShader(computeShader);
    mResourceManager.SetComputeShader(computeShader);
    mComputeRootSignature.Set(mNativeDevice->GetRootSignatureManager()->GetComputeRootSignature());
}

void RenderingContext::StashParamters()
{
    mRuntimeParametersStash.pipelineState.Set(mPipelineState.Get());
    mRuntimeParametersStash.primitiveTopology.Set(mPrimitiveTopology.Get());
    mRuntimeParametersStash.renderTarget.Set(mRenderTarget.Get());
    mRuntimeParametersStash.rootSignature.Set(mRootSignature.Get());
    mResourceManager.StashParameters();
}

void RenderingContext::RestoreStashedParameters()
{
    SetRenderTarget(mRuntimeParametersStash.renderTarget.Get());
    mPipelineState.Set(mRuntimeParametersStash.pipelineState.Get());
    mPrimitiveTopology.Set(mRuntimeParametersStash.primitiveTopology.Get());
    mRootSignature.Set(mRuntimeParametersStash.rootSignature.Get());
    mResourceManager.RestoreStashedParameters();
}

bool RenderingContext::Apply()
{
    mResourceManager.DeclareRingResources();
    if (!mResourceManager.PrepareResources()) return false;
    mDescriptors.MoveDescriptors(mResourceManager.CollectDescriptors());

    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the compute PSO flag
    // other settings (RootSignatures, Descriptors etc) are all set separately
    mComputePipelineState.ClearApplied();

    // Prepare a payload to record on the Command List
    mRenderTarget.AddToPayload(mPayloadAllocator, mRTPayload);
    mPipelineState.AddToPayload(mPayloadAllocator, mRTPayload);
    mRootSignature.AddToPayload(mPayloadAllocator, mRTPayload);
    mDescriptorHeaps.AddToPayload(mPayloadAllocator, mRTPayload);
    mDescriptors.AddToPayload(mPayloadAllocator, mRTPayload);
    mViewport.AddToPayload(mPayloadAllocator, mRTPayload);
    mScissor.AddToPayload(mPayloadAllocator, mRTPayload);
    mDefaultScissor.AddToPayload(mPayloadAllocator, mRTPayload);
    mPrimitiveTopology.AddToPayload(mPayloadAllocator, mRTPayload);
    mVertexBuffer.AddToPayload(mPayloadAllocator, mRTPayload);
    mIndexBuffer.AddToPayload(mPayloadAllocator, mRTPayload);

    return true;
}

bool RenderingContext::ApplyCompute()
{
    mResourceManager.DeclareComputeRingResources();
    if (!mResourceManager.PrepareComputeResources()) return false;
    mComputeDescriptors.MoveDescriptors(mResourceManager.CollectComputeDescriptors());

    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the graphics' PSO flag
    mPipelineState.ClearApplied();

    // command list recording steps
    mComputePipelineState.AddToPayload(mPayloadAllocator, mRTPayload);
    mComputeRootSignature.AddToPayload(mPayloadAllocator, mRTPayload);
    mDescriptorHeaps.AddToPayload(mPayloadAllocator, mRTPayload);
    mComputeDescriptors.AddToPayload(mPayloadAllocator, mRTPayload);

    return true;
}

// private
void RenderingContext::ExecuteCurrentCommandList()
{
    // FinalizeCommandList() will wait until the RenderThread is emptied
    D3D12GraphicsCommandListPtr cmdList = mRenderThread.FinalizeCommandList(mPayloadAllocator, ReplaceRTPayload());
    if (mVertexRingBuffer->HasUncommittedData())
    {
        mVertexRingBuffer->RecordTransferToGPU();
        D3D12GraphicsCommandListPtr copyVertexBufferList = mRenderThread.FinalizeCommandList(mPayloadAllocator, ReplaceRTPayload());

        // Copy vertex buffer list must happen before just-recorded list, this is
        // to ensure the copy will be executed first. This lets the driver merge the
        // lists and parallelize them better. Synchronization will be done via barriers.
        ID3D12CommandList* lists[2] = { copyVertexBufferList.Get(), cmdList.Get() };
        mCommandQueue->ExecuteCommandLists(2, lists);
    }
    else
    {
        // GPU vertex ring buffer was not used, we don't need to transfer any data
        // simply submit this Command List for execution and move on
        ID3D12CommandList* lists[1] = { cmdList.Get() };
        mCommandQueue->ExecuteCommandLists(1, lists);
    }
}

void RenderingContext::FlushCommandList(CheckpointType type)
{
    D3D12NI_ASSERT(mLastFlushTid == std::this_thread::get_id(), "FlushCommandLists() has to be called by the main thread");

    // submit existing RenderThread payload if there's any leftover commands there
    // and finish executing RenderThread payload
    ExecuteCurrentCommandList();
    mNativeDevice->Signal(type);

    ClearAppliedFlags();
    m2DVertexBatch.Invalidate();
}

bool RenderingContext::WaitForNextCheckpoint(CheckpointType type)
{
    return mCheckpointQueue.WaitForNextCheckpoint(type);
}

// Signal() is separate and not called everytime Execute() is called
// because we need to call it in SwapChain after we Present()
void RenderingContext::Signal(uint64_t fenceValue, CheckpointType type, Waitable&& waitable)
{
    HRESULT hr = mCommandQueue->Signal(mFence.Get(), fenceValue);
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to signal event on completion");

    hr = mFence->SetEventOnCompletion(fenceValue, waitable.GetHandle());
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to set Fence event on completion");

    mCheckpointQueue.AddCheckpoint(type, std::move(waitable));
}

void RenderingContext::FinishFrame()
{
    ExecuteCurrentCommandList();
    mRenderThread.AdvanceCommandAllocator();

    for (const auto& rt: mUsedRTs)
    {
        rt->ResetDirtyBBox();
    }

    mUsedRTs.clear();

    ClearAppliedFlags();
    m2DVertexBatch.Invalidate();

    mPayloadAllocator.MoveToNewChunk();
}

} // namespace Internal
} // namespace D3D12
