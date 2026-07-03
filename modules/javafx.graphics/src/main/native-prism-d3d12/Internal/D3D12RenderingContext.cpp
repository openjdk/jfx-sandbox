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
    D3D12NI_ASSERT(mMainThreadTid == std::this_thread::get_id(), "CreateRTPayload() has to be called by the main thread");

    RenderPayloadPtr ret(std::move(mRTPayload));
    mPayloadAllocator.MoveToNewChunk();
    mRTPayload.reset(mPayloadAllocator.Construct<RenderPayload>());
    return ret;
}

void RenderingContext::SubmitRTPayload()
{
    D3D12NI_ASSERT(mMainThreadTid == std::this_thread::get_id(), "SubmitRTPayload() has to be called by the main thread");

    if (mRTPayload && mRTPayload->HasWork())
    {
        // current payload has some work that was not reflected on a Command List yet
        // move current payload to the Render Thread for execution and create a fresh one for later
        mRenderThread.Execute(ReplaceRTPayload());
    }
}

void RenderingContext::RecordClear(float r, float g, float b, float a, bool clearDepth, const D3D12_RECT& clearRect)
{
    mRTPayload->AddStep(CreateRTExec<ClearRenderTargetAction>(mPayloadAllocator, mRenderTarget.Get(), r, g, b, a, clearRect));
    // NOTE: Here we check by NativeRenderTarget::HasDepthTexture() and not IsDepthTestEnabled()
    // Prism can sometimes set the RTT with depth test disabled, but then request its clear with
    // the depth texture (ex. hello.HelloViewOrder) and only afterwards re-set the RTT again enabling
    // depth testing. So we have to disregard the depth test flag, otherwise we would miss this DSV clear.
    if (clearDepth && mRenderTarget.Get()->HasDepthTexture())
    {
        mRTPayload->AddStep(CreateRTExec<ClearDepthStencilAction>(mPayloadAllocator,
            mRenderTarget.Get()->GetDepthTexture(), mRenderTarget.Get()->GetDSVDescriptorData().CPU(0),  1.0f, clearRect
        ));
    }
}

void RenderingContext::EnsureBoundTextureStates(D3D12_RESOURCE_STATES state)
{
    mRTPayload->AddStep(CreateRTExec<EnsureStatesAction>(mPayloadAllocator, mRenderTarget.Get(), state));
}

void RenderingContext::ClearAppliedFlags()
{
    mPipelineState.ClearApplied();
    mPrimitiveTopology.ClearApplied();
    mRootSignature.ClearApplied();
    mRenderTarget.ClearApplied();
    mScissor.ClearApplied();
    mDefaultScissor.ClearApplied();
    mViewport.ClearApplied();

    mTextures.ClearApplied();
    mVertexShader.ClearApplied();
    mPixelShader.ClearApplied();
    mVertexShaderConstants.ClearApplied();
    mPixelShaderConstants.ClearApplied();

    mComputePipelineState.ClearApplied();
    mComputeRootSignature.ClearApplied();
    mComputeShader.ClearApplied();
    mComputeShaderConstants.ClearApplied();
}


RenderingContext::RenderingContext(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mPayloadAllocator()
    , mRenderThread(nativeDevice)
    , mRTPayload(nullptr, LinearAllocatorDeleter<RenderPayload>(&mPayloadAllocator))
    , mClearOptState()
    , mPipelineState()
    , mPrimitiveTopology()
    , mRenderTarget()
    , mScissor()
    , mDefaultScissor()
    , mViewport()
    , mTextures()
    , mVertexShader()
    , mPixelShader()
    , mVertexShaderConstants()
    , mPixelShaderConstants()
    , mComputePipelineState()
    , mComputeRootSignature()
    , mComputeShader()
    , mComputeShaderConstants()
    , mUsedRTs()
{
    D3D12NI_LOG_DEBUG("RenderingContext: D3D12 API opts are %s", Config::IsApiOptsEnabled() ? "enabled" : "disabled");

    mPrimitiveTopology.Set(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Some parameters/steps depend on PSO being set
    RenderingStep::StepDependency psoDep = [&pso = mPipelineState]() -> bool
    {
        return pso.IsSet();
    };
    mRootSignature.SetDependency(psoDep);

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

    mMainThreadTid = std::this_thread::get_id();

    mRTPayload.reset(mPayloadAllocator.Construct<RenderPayload>());
}

bool RenderingContext::Init()
{
    if (!mRenderThread.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Render Thread");
        return false;
    }

    return true;
}

void RenderingContext::Release()
{
    // empty current payload and make sure it's freed on the other side
    // this prevents the assertion in Free() when running in DebugNative
    mPayloadAllocator.MoveToNewChunk();
    mRenderThread.Execute(std::move(mRTPayload));
    mRTPayload.reset();

    mRenderThread.Exit(); // Exit() will also internally wait
    mNativeDevice.reset();
}

void RenderingContext::DisposePageable(const D3D12PageablePtr& pageable)
{
    if (mRTPayload)
    {
        mRTPayload->AddStep(CreateRTExec<DisposePageableAction>(mPayloadAllocator, pageable));
    }
}

void RenderingContext::Clear(float r, float g, float b, float a, bool clearDepth)
{
    if (!mRenderTarget.IsSet()) return;

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
void RenderingContext::ClearDepth(const NIPtr<NativeTexture>& depthTexture, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv)
{
    mRTPayload->AddStep(CreateRTExec<ClearDepthStencilAction>(mPayloadAllocator, depthTexture, dsv, 1.0f));
}

void RenderingContext::DrawQuads(const Internal::MemoryView<float>& vertices, const Internal::MemoryView<signed char>& colors, uint32_t vertexCount)
{
    if (!Apply())
    {
        D3D12NI_LOG_ERROR("Failed to apply Rendering Context settings. Skipping Draw Quads call.");
        return;
    }

    EnsureBoundTextureStates(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (mRTPayload->AddStep(CreateRTExec<DrawQuadsAction>(mPayloadAllocator, mPayloadAllocator, vertices, colors, vertexCount)))
    {
        SubmitRTPayload();
    }
}

void RenderingContext::DrawMeshView(const NIPtr<NativeMeshView>& meshView)
{
    if (!Apply())
    {
        D3D12NI_LOG_ERROR("Failed to apply Rendering Context settings. Skipping Draw Mesh View call.");
        return;
    }

    EnsureBoundTextureStates(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (mRTPayload->AddStep(CreateRTExec<DrawMeshViewAction>(mPayloadAllocator, meshView)))
    {
        SubmitRTPayload();
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

bool RenderingContext::PrepareSwapChain(const NIPtr<NativeSwapChain>& swapChain, const D3D12_RECT& dirtyRegion)
{
    mRTPayload->AddStep(CreateRTExec<PrepareSwapChainAction>(mPayloadAllocator, swapChain, dirtyRegion));

    return true;
}

/* LKTODO restore when clear opts are restored
void RenderingContext::ResolveRegion(const NIPtr<IRenderTarget>& dstRT, uint32_t dstx, uint32_t dsty,
                                     const NIPtr<TextureBase>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch,
                                     DXGI_FORMAT resolveFormat)
{
    ResolveRegion(dstRT->GetTexture(), dstx, dsty, srcTexture, srcx, srcy, srcw, srch, resolveFormat);
    dstRT->UpdateDirtyBBox(BBox(
        static_cast<float>(dstx),
        static_cast<float>(dsty),
        static_cast<float>(dstx + srcw),
        static_cast<float>(dsty + srch)
    ));

}
*/
bool RenderingContext::Present(const NIPtr<NativeSwapChain>& swapChain)
{
    swapChain->WaitForAvailableBuffer();

    mRTPayload->AddStep(CreateRTExec<PresentAction>(mPayloadAllocator, swapChain));
    SubmitRTPayload();

    return true;
}

void RenderingContext::Resolve(const NIPtr<ITrackedResource>& dstTexture, const NIPtr<ITrackedResource>& srcTexture, DXGI_FORMAT resolveFormat)
{
    mRTPayload->AddStep(CreateRTExec<ResolveAction>(mPayloadAllocator, dstTexture, srcTexture, resolveFormat));
}

void RenderingContext::ResolveRegion(const NIPtr<ITrackedResource>& dstTexture, uint32_t dstx, uint32_t dsty,
                                     const NIPtr<ITrackedResource>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch,
                                     DXGI_FORMAT resolveFormat)
{
    D3D12_RECT srcRect;
    srcRect.left = srcx;
    srcRect.top = srcy;
    srcRect.right = srcx + srcw;
    srcRect.bottom = srcy + srch;

    mRTPayload->AddStep(CreateRTExec<ResolveRegionAction>(mPayloadAllocator, dstTexture, dstx, dsty, srcTexture, srcRect, resolveFormat));
}

/* LKTODO restore when clear opts are restored
void RenderingContext::CopyTexture(const NIPtr<IRenderTarget>& dstRT, uint32_t dstx, uint32_t dsty,
                                   const NIPtr<TextureBase>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch)
{
    CopyTexture(dstRT->GetTexture(), dstx, dsty, srcTexture, srcx, srcy, srcw, srch);
    dstRT->UpdateDirtyBBox(BBox(
        static_cast<float>(dstx),
        static_cast<float>(dsty),
        static_cast<float>(dstx + srcw),
        static_cast<float>(dsty + srch)
    ));
}
*/
void RenderingContext::CopyTexture(const NIPtr<ITrackedResource>& dstTexture, uint32_t dstx, uint32_t dsty,
                                   const NIPtr<ITrackedResource>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch)
{
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.pResource = nullptr;
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
    dstLoc.pResource = nullptr;
    dstLoc.SubresourceIndex = 0;

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(mPayloadAllocator, dstTexture, dstLoc, dstx, dsty, srcTexture, srcLoc, srcBox));
}

void RenderingContext::CopyTextureToBuffer(const Buffer& dstBuffer, uint32_t dstStride, const NIPtr<NativeTexture>& srcTexture,
                                          uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch)
{
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.pResource = nullptr; // will be set by RenderThread with the ResourceProvider API
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

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(mPayloadAllocator, nullptr, dstLoc, 0, 0, srcTexture, srcLoc, srcBox));
}

void RenderingContext::CopyToTexture(const NIPtr<ITrackedResource>& dstTexture, uint32_t dstx, uint32_t dsty,
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
    dstLoc.pResource = nullptr; // set later by RenderThread
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(mPayloadAllocator, dstTexture, dstLoc, dstx, dsty, nullptr, srcLoc));
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
    TransitionTrackedResource(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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
        TransitionTrackedResource(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                  Utils::CalcSubresource(mipBase, texture->GetMipLevels(), 0));

        // each thread group manages an 8x8 square, so we need to dispatch (width/8) X groups and (height/8) Y groups
        Dispatch(std::max<UINT>(srcWidth >> 3, 1), std::max<UINT>(srcHeight >> 3, 1), 1);

        // transition base level back to UAV
        // this should make all subresources have the same state again
        TransitionTrackedResource(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                  Utils::CalcSubresource(mipBase, texture->GetMipLevels(), 0));

        srcWidth >>= constants.numLevels;
        srcHeight >>= constants.numLevels;
    }

    return true;
}

void RenderingContext::TransitionTrackedResource(const NIPtr<ITrackedResource>& resource, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    D3D12_RESOURCE_BARRIER barrier;
    D3D12NI_ZERO_STRUCT(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = nullptr;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = subresource;

    mRTPayload->AddStep(CreateRTExec<ResourceBarrierAction>(mPayloadAllocator, resource, barrier));
}

void RenderingContext::TransitionResource(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    if (oldState == newState) return;

    D3D12_RESOURCE_BARRIER barrier;
    D3D12NI_ZERO_STRUCT(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource.Get();
    barrier.Transition.StateBefore = oldState;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = subresource;

    mRTPayload->AddStep(CreateRTExec<ResourceBarrierAction>(mPayloadAllocator, nullptr, barrier));
}

void RenderingContext::ClearTextureUnit(uint32_t unit)
{
    if (!mTextures.GetTexture(unit)) return;

    mTextures.SetTexture(unit, nullptr);
}

/* LKTODO remove
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
}*/

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
    mTextures.SetTexture(unit, texture);
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
    mVertexShader.Set(vertexShader);
    mVertexShaderConstants.Set(vertexShader);
}

void RenderingContext::SetPixelShader(const NIPtr<Shader>& pixelShader)
{
    if (mPipelineState.Get().pixelShader == pixelShader) return;

    mPipelineState.SetPixelShader(pixelShader);
    mPixelShader.Set(pixelShader);
    mPixelShaderConstants.Set(pixelShader);

    if (pixelShader)
    {
        mRootSignature.Set(mNativeDevice->GetRootSignatureManager()->GetGraphicsRootSignature());
    }
}

void RenderingContext::SetComputeShader(const NIPtr<Shader>& computeShader)
{
    if (mComputePipelineState.Get().shader == computeShader) return;

    mComputePipelineState.SetComputeShader(computeShader);
    mComputeShader.Set(computeShader);
    mComputeShaderConstants.Set(computeShader);
    mComputeRootSignature.Set(mNativeDevice->GetRootSignatureManager()->GetComputeRootSignature());
}

void RenderingContext::StashParamters()
{
    mRuntimeParametersStash.pipelineState.Set(mPipelineState.Get());
    mRuntimeParametersStash.primitiveTopology.Set(mPrimitiveTopology.Get());
    mRuntimeParametersStash.renderTarget.Set(mRenderTarget.Get());
    mRuntimeParametersStash.rootSignature.Set(mRootSignature.Get());
    mRuntimeParametersStash.textures.Set(mTextures.Get());
    mRuntimeParametersStash.vertexShader.Set(mVertexShader.Get());
    mRuntimeParametersStash.pixelShader.Set(mPixelShader.Get());
}

void RenderingContext::RestoreStashedParameters()
{
    SetRenderTarget(mRuntimeParametersStash.renderTarget.Get());
    mPipelineState.Set(mRuntimeParametersStash.pipelineState.Get());
    mPrimitiveTopology.Set(mRuntimeParametersStash.primitiveTopology.Get());
    mRootSignature.Set(mRuntimeParametersStash.rootSignature.Get());
    mTextures.Set(mRuntimeParametersStash.textures.Get());
    SetVertexShader(mRuntimeParametersStash.vertexShader.Get());
    SetPixelShader(mRuntimeParametersStash.pixelShader.Get());
}

bool RenderingContext::Apply()
{
    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the compute PSO flag
    // other settings (RootSignatures, Descriptors etc) are all set separately
    mComputePipelineState.ClearApplied();

    // Prepare Resource Manager
    mTextures.AddToPayload(mPayloadAllocator, mRTPayload);
    mVertexShader.AddToPayload(mPayloadAllocator, mRTPayload);
    mPixelShader.AddToPayload(mPayloadAllocator, mRTPayload);
    mVertexShaderConstants.AddToPayload(mPayloadAllocator, mRTPayload);
    mPixelShaderConstants.AddToPayload(mPayloadAllocator, mRTPayload);

    mRenderTarget.AddToPayload(mPayloadAllocator, mRTPayload);
    mPipelineState.AddToPayload(mPayloadAllocator, mRTPayload);
    mRootSignature.AddToPayload(mPayloadAllocator, mRTPayload);
    mViewport.AddToPayload(mPayloadAllocator, mRTPayload);
    mScissor.AddToPayload(mPayloadAllocator, mRTPayload);
    mDefaultScissor.AddToPayload(mPayloadAllocator, mRTPayload);
    mPrimitiveTopology.AddToPayload(mPayloadAllocator, mRTPayload);

    return true;
}

bool RenderingContext::ApplyCompute()
{
    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the graphics' PSO flag
    mPipelineState.ClearApplied();

    // prepare compute resources
    mTextures.AddToPayload(mPayloadAllocator, mRTPayload);
    mComputeShader.AddToPayload(mPayloadAllocator, mRTPayload);
    mComputeShaderConstants.AddToPayload(mPayloadAllocator, mRTPayload);
    mComputePipelineState.AddToPayload(mPayloadAllocator, mRTPayload);
    mComputeRootSignature.AddToPayload(mPayloadAllocator, mRTPayload);

    return true;
}

void RenderingContext::FlushCommandList(CheckpointType type)
{
    D3D12NI_ASSERT(mMainThreadTid == std::this_thread::get_id(), "FlushCommandLists() has to be called by the main thread");

    // submit existing RenderThread payload if there's any leftover commands there
    // and finish executing RenderThread payload
    mRenderThread.ScheduleCommandListSubmit(mPayloadAllocator, mRTPayload);
    mRenderThread.ScheduleSignal(mPayloadAllocator, mRTPayload, type);
    mRenderThread.Execute(ReplaceRTPayload());

    ClearAppliedFlags();
}

bool RenderingContext::WaitForNextCheckpoint(CheckpointType type)
{
    return mRenderThread.WaitForCheckpoint(mPayloadAllocator, type);
}

void RenderingContext::Signal(CheckpointType type)
{
    mRenderThread.ScheduleSignal(mPayloadAllocator, mRTPayload, type);
    mRenderThread.Execute(ReplaceRTPayload());
}

// called after SwapChain::Prepare() and right before SwapChain::Present(). See D3D12SwapChain.java
// for reference.
void RenderingContext::FinishFrame()
{
    mRenderThread.ScheduleCommandListSubmit(mPayloadAllocator, mRTPayload);
    mRenderThread.ScheduleCommandAllocatorAdvance(mPayloadAllocator, mRTPayload);

    // skipping Signal() and Execute() here, will be done by Present()
    mPayloadAllocator.ResetChunks();

    for (const auto& rt: mUsedRTs)
    {
        rt->ResetDirtyBBox();
    }

    mUsedRTs.clear();

    ClearAppliedFlags();
}

} // namespace Internal
} // namespace D3D12
