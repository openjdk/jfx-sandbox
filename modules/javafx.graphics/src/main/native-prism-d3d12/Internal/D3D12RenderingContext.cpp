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
#include "D3D12Profiler.hpp"


namespace D3D12 {
namespace Internal {

void RenderingContext::RecordClear(float r, float g, float b, float a, bool clearDepth, const D3D12_RECT& clearRect)
{
    mNativeDevice->QueueTextureTransition(mRenderTarget.Get()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (mRenderTarget.Get()->HasDepthTexture())
        mNativeDevice->QueueTextureTransition(mRenderTarget.Get()->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mNativeDevice->SubmitTextureTransitions();

    float rgba[4] = { r, g, b, a };
    mNativeDevice->GetCurrentCommandList()->ClearRenderTargetView(mRenderTarget.Get()->GetRTVDescriptorData().CPU(0), rgba, 1, &clearRect);
    // NOTE: Here we check by NativeRenderTarget::HasDepthTexture() and not IsDepthTestEnabled()
    // Prism can sometimes set the RTT with depth test disabled, but then request its clear with
    // the depth texture (ex. hello.HelloViewOrder) and only afterwards re-set the RTT again enabling
    // depth testing. So we have to disregard the depth test flag, otherwise we would miss this DSV clear.
    if (clearDepth && mRenderTarget.Get()->HasDepthTexture())
    {
        mNativeDevice->GetCurrentCommandList()->ClearDepthStencilView(mRenderTarget.Get()->GetDSVDescriptorData().CPU(0), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &clearRect);
    }

    Profiler::Instance().MarkEvent(mRecordClearProfilerID, Profiler::Event::Event);
}

RenderingContext::RenderingContext(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mState(nativeDevice)
    , mRecordClearProfilerID(std::numeric_limits<uint32_t>::max())
    , mIndexBuffer()
    , mVertexBuffer()
    , mPipelineState()
    , mPrimitiveTopology()
    , mRenderTarget()
    , mScissor()
    , mDefaultScissor()
    , mResources()
    , mViewport()
    , mComputePipelineState()
    , mComputeRootSignature()
    , mComputeResources()
    , mUsedRTs()
{
    D3D12NI_LOG_DEBUG("RenderingContext: D3D12 API opts are %s", Config::IsApiOptsEnabled() ? "enabled" : "disabled");

    mPrimitiveTopology.Set(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Some parameters/steps depend on PSO being set
    RenderingStep::StepDependency psoDep = [&pso = mPipelineState](RenderingContextState& state) -> bool
    {
        return pso.IsSet();
    };
    mRootSignature.SetDependency(psoDep);
    mDescriptorHeap.SetDependency(psoDep);
    mResources.SetDependency(psoDep);

    // Use the default scissor only if other custom scissor rect is not set
    // See SetRenderTarget() for more details
    mDefaultScissor.SetDependency([&scissor = mScissor](RenderingContextState& state) -> bool
    {
        return !scissor.IsSet();
    });

    RenderingStep::StepDependency computePsoDep = [&computePso = mComputePipelineState](RenderingContextState& state) -> bool
    {
        return computePso.IsSet();
    };
    mComputeRootSignature.SetDependency(computePsoDep);
    mComputeResources.SetDependency(computePsoDep);
}

bool RenderingContext::Init()
{
    if (!mState.PSOManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize PSO Manager");
        return false;
    }

    if (!mState.resourceManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Resource Manager");
        return false;
    }

    mRecordClearProfilerID = Profiler::Instance().RegisterSource("RenderingContext RecordClear");

    return true;
}

void RenderingContext::Clear(float r, float g, float b, float a, bool clearDepth)
{
    if (!mRenderTarget.IsSet()) return;

    mRenderTarget.Apply(mNativeDevice->GetCurrentCommandList(), mState);
    DescriptorData rtData = mRenderTarget.Get()->GetRTVDescriptorData();

    // if the RTT was NOT fully used we don't have to clear the whole thing
    // determine how much of the space actually needs to be cleared (unless the request is for a smaller section)
    D3D12_RECT clearRect = GetScissor().Get();
    const BBox& rttDirtyBBox = mRenderTarget.Get()->GetDirtyBBox();

    if (Config::Instance().IsClearOptsEnabled() && rttDirtyBBox.Valid() && rttDirtyBBox.Inside(clearRect))
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
        mState.clearDelayed = true;
        mState.clearDepth = clearDepth;
        mState.clearRect = clearRect;
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

    if (mState.clearDelayed)
    {
        // check if we can discard this clear
        // the clear can be discarded if we use composite mode SRC_OVER
        // and this draw call will overwrite the entire to-be-cleared area of the RTT
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
            std::ceil(dirtyBBox.min.x) <= mState.clearRect.left  && std::ceil(dirtyBBox.min.y) <= mState.clearRect.top &&
            std::floor(dirtyBBox.max.x) >= mState.clearRect.right && std::floor(dirtyBBox.max.y) >= mState.clearRect.bottom)
        {
            clearDiscarded = true;
            SetCompositeMode(CompositeMode::SRC);
        }
        else
        {
            RecordClear(0.0f, 0.0f, 0.0f, 0.0f, mState.clearDepth, mState.clearRect);
        }

        mState.clearDelayed = false;
    }

    // declare Ring Container spaces so that we can potentially flush the Command List early
    // and prevent mid-write flushes
    DeclareRingResources();

    // apply Context settings to the Command List
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

    mNativeDevice->GetCurrentCommandList()->DrawIndexedInstanced(elements, 1, 0, vbOffset, 0);

    if (dirtyBBox.Valid())
    {
        mRenderTarget.Get()->MergeDirtyBBox(dirtyBBox);
    }

    if (clearDiscarded)
    {
        // restore original composite mode
        SetCompositeMode(currentCompositeMode);
    }
}

void RenderingContext::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    DeclareComputeRingResources();

    if (!ApplyCompute())
    {
        D3D12NI_LOG_ERROR("Failed to apply Compute Rendering Context settings. Skipping dispatch call.");
        return;
    }

    mNativeDevice->GetCurrentCommandList()->Dispatch(x, y, z);
}

void RenderingContext::ClearTextureUnit(uint32_t unit)
{
    if (!mState.resourceManager.GetTexture(unit)) return;

    mState.resourceManager.ClearTextureUnit(unit);
    ClearResourcesApplied();
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

    if (mState.clearDelayed)
    {
        // there was a Clear() queued but we're changing the RT
        // we should submit the delayed Clear() call before we swap the RTs
        RecordClear(0.0f, 0.0f, 0.0f, 0.0f, mState.clearDepth, mState.clearRect);
        mState.clearDelayed = false;
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
    mState.resourceManager.SetTexture(unit, texture);
    ClearResourcesApplied();
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
    mState.resourceManager.SetVertexShader(vertexShader);

    ClearResourcesApplied();
}

void RenderingContext::SetPixelShader(const NIPtr<Shader>& pixelShader)
{
    if (mPipelineState.Get().pixelShader == pixelShader) return;

    mPipelineState.SetPixelShader(pixelShader);
    mState.resourceManager.SetPixelShader(pixelShader);

    if (pixelShader)
    {
        mRootSignature.Set(mNativeDevice->GetRootSignatureManager()->GetGraphicsRootSignature());
    }

    ClearResourcesApplied();
}

void RenderingContext::SetComputeShader(const NIPtr<Shader>& computeShader)
{
    if (mComputePipelineState.Get().shader == computeShader) return;

    mComputePipelineState.SetComputeShader(computeShader);
    mState.resourceManager.SetComputeShader(computeShader);

    mComputeRootSignature.Set(mNativeDevice->GetRootSignatureManager()->GetComputeRootSignature());
    ClearComputeResourcesApplied();
}

void RenderingContext::StashParamters()
{
    mRuntimeParametersStash.pipelineState.Set(mPipelineState.Get());
    mRuntimeParametersStash.primitiveTopology.Set(mPrimitiveTopology.Get());
    mRuntimeParametersStash.renderTarget.Set(mRenderTarget.Get());
    mRuntimeParametersStash.rootSignature.Set(mRootSignature.Get());
    mState.resourceManager.StashParameters();
}

void RenderingContext::RestoreStashedParameters()
{
    SetRenderTarget(mRuntimeParametersStash.renderTarget.Get());
    mPipelineState.Set(mRuntimeParametersStash.pipelineState.Get());
    mPrimitiveTopology.Set(mRuntimeParametersStash.primitiveTopology.Get());
    mRootSignature.Set(mRuntimeParametersStash.rootSignature.Get());

    mState.resourceManager.RestoreStashedParameters();
    ClearResourcesApplied();
}

void RenderingContext::DeclareRingResources()
{
    mState.resourceManager.DeclareRingResources();
}

void RenderingContext::DeclareComputeRingResources()
{
    mState.resourceManager.DeclareComputeRingResources();
}

bool RenderingContext::Apply()
{
    // Prepare rendering steps. These should do all necessary allocations.
    if (!mResources.Prepare(mState)) return false;

    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the compute PSO flag
    // other settings (RootSignatures, Descriptors etc) are all set separately
    mComputePipelineState.ClearApplied();

    // Apply changes on current Command List. Below calls must NOT do any operations
    // which might submit the Command List (ex. Ring Container allocations)

    const D3D12GraphicsCommandListPtr& commandList = mNativeDevice->GetCurrentCommandList();
    if (!commandList) return false;

    mRenderTarget.Apply(commandList, mState);
    mViewport.Apply(commandList, mState);

    mScissor.Apply(commandList, mState);
    mDefaultScissor.Apply(commandList, mState);

    mPipelineState.Apply(commandList, mState);
    mRootSignature.Apply(commandList, mState);
    mDescriptorHeap.Apply(commandList, mState);
    mResources.Apply(commandList, mState);

    mPrimitiveTopology.Apply(commandList, mState);
    mVertexBuffer.Apply(commandList, mState);
    mIndexBuffer.Apply(commandList, mState);

    return true;
}

bool RenderingContext::ApplyCompute()
{
    if (!mComputeResources.Prepare(mState)) return false;

    // there can only be one Pipeline State on a command list
    // To prevent using an incorrect Pipeline State after this Apply call we'll reset the graphics' PSO flag
    mPipelineState.ClearApplied();

    const D3D12GraphicsCommandListPtr& commandList = mNativeDevice->GetCurrentCommandList();
    if (!commandList) return false;

    mComputePipelineState.Apply(commandList, mState);
    mComputeRootSignature.Apply(commandList, mState);
    mDescriptorHeap.Apply(commandList, mState);
    mComputeResources.Apply(commandList, mState);

    return true;
}

void RenderingContext::EnsureBoundTextureStates(D3D12_RESOURCE_STATES state)
{
    mNativeDevice->QueueTextureTransition(mRenderTarget.Get()->GetTexture(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (mRenderTarget.Get()->HasDepthTexture())
        mNativeDevice->QueueTextureTransition(mRenderTarget.Get()->GetDepthTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

    mState.resourceManager.EnsureStates(mNativeDevice->GetCurrentCommandList(), state);
}

void RenderingContext::ClearAppliedFlags()
{
    mIndexBuffer.ClearApplied();
    mVertexBuffer.ClearApplied();

    mPipelineState.ClearApplied();
    mRootSignature.ClearApplied();
    mDescriptorHeap.ClearApplied();
    mPrimitiveTopology.ClearApplied();
    mRenderTarget.ClearApplied();
    mScissor.ClearApplied();
    mDefaultScissor.ClearApplied();
    mResources.ClearApplied();
    mViewport.ClearApplied();

    mComputePipelineState.ClearApplied();
    mComputeRootSignature.ClearApplied();
    mComputeResources.ClearApplied();
}

void RenderingContext::ClearResourcesApplied()
{
    // TODO: D3D12: This could be a bit more optimized - we could ex. provide flags
    // stating which parts of ResourceManager need refreshing.
    // We can imagine when rendering a lot of 2D data we would just keep using the same
    // Vertex Shader, so its resources (minus different constants) would stay intact,
    // while Pixel Shader resources would need a refresh.
    // Investigate if that would make sense in the long run.
    mResources.ClearApplied();
}

void RenderingContext::ClearComputeResourcesApplied()
{
    mComputeResources.ClearApplied();
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
