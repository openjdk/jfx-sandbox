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
#include "D3D12MipmapGenComputeShader.hpp"
#include "D3D12Profiler.hpp"
#include "D3D12RenderPayload.hpp"
#include "D3D12Utils.hpp"


namespace
{
    template <typename Executable, typename ...Args>
    D3D12::Internal::RenderThreadExecutablePtr CreateRTExec(Args&&... args)
    {
        return std::make_unique<Executable>(std::forward<Args>(args)...);
    }
}

namespace D3D12 {
namespace Internal {

void RenderingContext::SubmitRTPayload(RenderPayload::Type type)
{
    if (mRTPayload->HasWork())
    {
        // current payload has some work that was not reflected on a Command List yet
        // move current payload to the Render Thread for execution and create a fresh one for later
        mRTPayload->Finalize(type);
        mRenderThread.Execute(std::move(mRTPayload));
        mRTPayload = std::make_unique<RenderPayload>();
    }
}

void RenderingContext::RecordClear(float r, float g, float b, float a, bool clearDepth, const D3D12_RECT& clearRect)
{
    QueueTextureTransition(mRenderTarget.Get()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (mRenderTarget.Get()->HasDepthTexture())
        QueueTextureTransition(mRenderTarget.Get()->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    SubmitTextureTransitions();

    mRTPayload->AddStep(CreateRTExec<ClearRenderTargetAction>(mRenderTarget.Get()->GetRTVDescriptorData().CPU(0), r, g, b, a, clearRect));
    // NOTE: Here we check by NativeRenderTarget::HasDepthTexture() and not IsDepthTestEnabled()
    // Prism can sometimes set the RTT with depth test disabled, but then request its clear with
    // the depth texture (ex. hello.HelloViewOrder) and only afterwards re-set the RTT again enabling
    // depth testing. So we have to disregard the depth test flag, otherwise we would miss this DSV clear.
    if (clearDepth && mRenderTarget.Get()->HasDepthTexture())
    {
        mRTPayload->AddStep(CreateRTExec<ClearDepthStencilAction>(mRenderTarget.Get()->GetDSVDescriptorData().CPU(0), 1.0f, clearRect));
    }
}

void RenderingContext::EnsureBoundTextureStates(D3D12_RESOURCE_STATES state)
{
    QueueTextureTransition(mRenderTarget.Get()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (mRenderTarget.Get()->HasDepthTexture())
        QueueTextureTransition(mRenderTarget.Get()->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

    for (uint32_t i = 0; i < mTextures.Get().size(); ++i)
    {
        const NIPtr<TextureBase>& tex = mTextures.GetTexture(i);
        if (tex)
        {
            QueueTextureTransition(mTextures.GetTexture(i), state);
        }
    }

    SubmitTextureTransitions();
}

void RenderingContext::QueueTextureTransition(const NIPtr<Internal::TextureBase>& tex, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    if (tex->GetResourceState(subresource) == newState) return;

    D3D12_RESOURCE_BARRIER barrier;
    D3D12NI_ZERO_STRUCT(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = tex->GetResource().Get();
    barrier.Transition.StateBefore = tex->GetResourceState(subresource);
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = subresource;
    mBarrierQueue.emplace_back(barrier);

    tex->SetResourceState(newState, subresource);
}

void RenderingContext::SubmitTextureTransitions()
{
    if (mBarrierQueue.size() == 0) return;

    mRTPayload->AddStep(CreateRTExec<ResourceBarrierAction>(mBarrierQueue));
    mBarrierQueue.clear();
}

RenderingContext::RenderingContext(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mRenderThread(nativeDevice)
    , mRTPayload(std::make_unique<RenderPayload>())
    , mClearOptState()
    , mIndexBuffer()
    , mVertexBuffer()
    , mDescriptorHeap()
    , mPipelineState()
    , mGraphicsShaders()
    , mPrimitiveTopology()
    , mRenderTarget()
    , mScissor()
    , mDefaultScissor()
    , mTextures()
    , mViewport()
    , mComputePipelineState()
    , mComputeShader()
    , mComputeRootSignature()
    , mUsedRTs()
    , mBarrierQueue()
{
    D3D12NI_LOG_DEBUG("RenderingContext: D3D12 API opts are %s", Config::IsApiOptsEnabled() ? "enabled" : "disabled");

    mPrimitiveTopology.Set(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Some parameters/steps depend on PSO being set
    RenderingStep::StepDependency psoDep = [&pso = mPipelineState]() -> bool
    {
        return pso.IsSet();
    };
    mRootSignature.SetDependency(psoDep);
    mDescriptorHeap.SetDependency(psoDep);
    mGraphicsShaders.SetDependency(psoDep);
    mTextures.SetDependency(psoDep);

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
    mComputeShader.SetDependency(computePsoDep);

    mBarrierQueue.reserve(8);
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
    mRTPayload->AddStep(CreateRTExec<DrawAction>(elements, vbOffset));
    SubmitRTPayload(RenderPayload::Type::GRAPHICS);

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

    mRTPayload->AddStep(CreateRTExec<DispatchAction>(x, y, z));
    SubmitRTPayload(RenderPayload::Type::COMPUTE);
}

void RenderingContext::Resolve(const NIPtr<TextureBase>& dstTexture, const NIPtr<TextureBase>& srcTexture, DXGI_FORMAT resolveFormat)
{
    QueueTextureTransition(srcTexture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    QueueTextureTransition(dstTexture, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    SubmitTextureTransitions();

    mRTPayload->AddStep(CreateRTExec<ResolveAction>(dstTexture->GetResource().Get(), srcTexture->GetResource().Get(), resolveFormat));
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
    SubmitTextureTransitions();

    mRTPayload->AddStep(CreateRTExec<ResolveRegionAction>(dstTexture->GetResource().Get(), dstx, dsty, srcTexture->GetResource().Get(), srcRect, resolveFormat));
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
    SubmitTextureTransitions();

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(dstLoc, dstx, dsty, srcLoc, srcBox));
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
    SubmitTextureTransitions();

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(dstLoc, 0, 0, srcLoc, srcBox));
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
    SubmitTextureTransitions();

    mRTPayload->AddStep(CreateRTExec<CopyTextureAction>(dstLoc, dstx, dsty, srcLoc));
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
    SubmitTextureTransitions();

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
        SubmitTextureTransitions();

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
    if (!mTextures.GetTexture(unit)) return;

    mTextures.SetTexture(unit, nullptr);
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
    mGraphicsShaders.SetVertexShader(vertexShader);
    mVertexShaderConstants.Set(vertexShader);
}

void RenderingContext::SetPixelShader(const NIPtr<Shader>& pixelShader)
{
    if (mPipelineState.Get().pixelShader == pixelShader) return;

    mPipelineState.SetPixelShader(pixelShader);
    mGraphicsShaders.SetPixelShader(pixelShader);
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
    mRuntimeParametersStash.graphicsShaders.Set(mGraphicsShaders.Get());
    mRuntimeParametersStash.primitiveTopology.Set(mPrimitiveTopology.Get());
    mRuntimeParametersStash.renderTarget.Set(mRenderTarget.Get());
    mRuntimeParametersStash.rootSignature.Set(mRootSignature.Get());
    mRuntimeParametersStash.textures.Set(mTextures.Get());
}

void RenderingContext::RestoreStashedParameters()
{
    SetRenderTarget(mRuntimeParametersStash.renderTarget.Get());
    SetVertexShader(mRuntimeParametersStash.graphicsShaders.Get().vertexShader);
    SetPixelShader(mRuntimeParametersStash.graphicsShaders.Get().pixelShader);

    mPipelineState.Set(mRuntimeParametersStash.pipelineState.Get());
    mPrimitiveTopology.Set(mRuntimeParametersStash.primitiveTopology.Get());
    mRootSignature.Set(mRuntimeParametersStash.rootSignature.Get());
    mTextures.Set(mRuntimeParametersStash.textures.Get());
}

bool RenderingContext::Apply()
{
    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the compute PSO flag
    // other settings (RootSignatures, Descriptors etc) are all set separately
    mComputePipelineState.ClearApplied();

    // Add command list changes (if necessary) to current payload for the RenderThread
    // We start with resource changes - shaders and textures - and then a Resource update step.
    // These are a RenderingResource, so will be added to initial Resource steps in the Payload
    mGraphicsShaders.AddToPayload(mRTPayload);
    mTextures.AddToPayload(mRTPayload);
    mVertexShaderConstants.AddToPayload(mRTPayload);
    mPixelShaderConstants.AddToPayload(mRTPayload);

    // Now, update more straightforward on-Command-List parameters
    mRenderTarget.AddToPayload(mRTPayload);
    mPipelineState.AddToPayload(mRTPayload);
    mRootSignature.AddToPayload(mRTPayload);
    mDescriptorHeap.AddToPayload(mRTPayload);
    mViewport.AddToPayload(mRTPayload);
    mScissor.AddToPayload(mRTPayload);
    mDefaultScissor.AddToPayload(mRTPayload);
    mPrimitiveTopology.AddToPayload(mRTPayload);
    mVertexBuffer.AddToPayload(mRTPayload);
    mIndexBuffer.AddToPayload(mRTPayload);

    mRTPayload->AddStep(CreateRTExec<ApplyResources>());

    return true;
}

bool RenderingContext::ApplyCompute()
{
    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the graphics' PSO flag
    mPipelineState.ClearApplied();

    // resources
    mComputeShader.AddToPayload(mRTPayload);
    mTextures.AddToPayload(mRTPayload);
    mComputeShaderConstants.AddToPayload(mRTPayload);

    // command list recording steps
    mComputePipelineState.AddToPayload(mRTPayload);
    mComputeRootSignature.AddToPayload(mRTPayload);
    mDescriptorHeap.AddToPayload(mRTPayload);

    mRTPayload->AddStep(CreateRTExec<ApplyComputeResources>());

    return true;
}

void RenderingContext::ClearAppliedFlags()
{
    mIndexBuffer.ClearApplied();
    mVertexBuffer.ClearApplied();
    mDescriptorHeap.ClearApplied();
    mGraphicsShaders.ClearApplied();
    mPipelineState.ClearApplied();
    mPrimitiveTopology.ClearApplied();
    mRootSignature.ClearApplied();
    mRenderTarget.ClearApplied();
    mScissor.ClearApplied();
    mDefaultScissor.ClearApplied();
    mTextures.ClearApplied();
    mViewport.ClearApplied();

    mComputePipelineState.ClearApplied();
    mComputeShader.ClearApplied();
    mComputeRootSignature.ClearApplied();
}

void RenderingContext::SyncToRenderThread()
{
    SubmitRTPayload(RenderPayload::Type::OTHER);
    mRenderThread.WaitForCompletion();
}

void RenderingContext::FinishFrame()
{
    for (const auto& rt: mUsedRTs)
    {
        rt->ResetDirtyBBox();
    }

    mUsedRTs.clear();
}

} // namespace Internal
} // namespace D3D12
