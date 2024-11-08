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

#include "D3D12RenderingContext.hpp"

#include "../D3D12NativeDevice.hpp"

#include "D3D12Config.hpp"


namespace D3D12 {
namespace Internal {

RenderingContext::RenderingContext(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mState(nativeDevice)
    , mIndexBuffer()
    , mVertexBuffer()
    , mPipelineState()
    , mPrimitiveTopology()
    , mRenderTarget()
    , mScissor()
    , mDefaultScissor()
    , mResources()
    , mTransforms()
    , mViewport()
{
    D3D12NI_LOG_DEBUG("RenderingContext: D3D12 API opts are %s", Config::Instance().IsApiOptsEnabled() ? "enabled" : "disabled");

    mPrimitiveTopology.Set(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Some parameters/steps depend on PSO being set
    PipelineStateRenderingParameter& psoParam = mPipelineState;
    RenderingStep::StepDependency psoDep = [&psoParam](RenderingContextState& state) -> bool
    {
        return psoParam.IsSet();
    };
    mDescriptorHeap.SetDependency(psoDep);
    mTransforms.SetDependency(psoDep);
    mResources.SetDependency(psoDep);

    // Use the default scissor only if other custom scissor rect is not set
    // See SetRenderTarget() for more details
    ScissorRenderingParameter& scissorParam = mScissor;
    mDefaultScissor.SetDependency([&scissorParam](RenderingContextState& state) -> bool
    {
        return !scissorParam.IsSet();
    });
}

bool RenderingContext::Init()
{
    if (!mState.PSOManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize PSO Manager");
        return false;
    }

    return true;
}

void RenderingContext::Clear(float r, float g, float b, float a)
{
    if (!mRenderTarget.IsSet()) return;

    mRenderTarget.Apply(mNativeDevice->GetCurrentCommandList(), mState);

    DescriptorData rtData = mRenderTarget.Get()->GetDescriptorData();

    float rgba[4] = { r, g, b, a };
    mNativeDevice->GetCurrentCommandList()->ClearRenderTargetView(rtData.CPU(0), rgba, 1, &GetScissor().Get());
    // Depth Buffer is cleared when applying RenderTarget onto a Command List
}

void RenderingContext::ClearTextureUnit(uint32_t unit)
{
    if (!mState.resourceManager.GetTexture(unit)) return;

    mState.resourceManager.ClearTextureUnit(unit);
    mResources.ClearApplied();
}

void RenderingContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView)
{
    mIndexBuffer.Set(ibView);
}

void RenderingContext::SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbView)
{
    mVertexBuffer.Set(vbView);
}

void RenderingContext::SetRenderTarget(const NIPtr<NativeRenderTarget>& renderTarget)
{
    mRenderTarget.Set(renderTarget);

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
}

void RenderingContext::SetScissor(bool enabled, const D3D12_RECT& scissor)
{
    if (!enabled) mScissor.Unset();
    else mScissor.Set(scissor);
}

void RenderingContext::SetTexture(uint32_t unit, const NIPtr<NativeTexture>& texture)
{
    if (mState.resourceManager.GetTexture(unit) == texture) return;

    mState.resourceManager.SetTexture(unit, texture);
    mResources.ClearApplied();
}

void RenderingContext::SetCameraPos(const Coords_XYZW_FLOAT& pos)
{
    mTransforms.SetCameraPos(pos);
    mResources.ClearApplied();
}

void RenderingContext::SetViewProjTransform(const Matrix<float>& transform)
{
    mTransforms.SetViewProjTransform(transform);
    mResources.ClearApplied();
}

void RenderingContext::SetWorldTransform(const Matrix<float>& transform)
{
    mTransforms.SetWorldTransform(transform);
    mResources.ClearApplied();
}

// PSO-related setters

void RenderingContext::SetCompositeMode(CompositeMode mode)
{
    mPipelineState.SetCompositeMode(mode);
    mResources.ClearApplied();
}

void RenderingContext::SetCullMode(D3D12_CULL_MODE mode)
{
    mPipelineState.SetCullMode(mode);
}

void RenderingContext::SetFillMode(D3D12_FILL_MODE mode)
{
    mPipelineState.SetFillMode(mode);
}

void RenderingContext::SetVertexShader(const NIPtr<Shader>& vertexShader)
{
    if (mPipelineState.Get().vertexShader == vertexShader) return;

    mPipelineState.SetVertexShader(vertexShader);
    mState.resourceManager.SetVertexShader(vertexShader);

    mResources.ClearApplied();
}

void RenderingContext::SetPixelShader(const NIPtr<Shader>& pixelShader)
{
    if (mPipelineState.Get().pixelShader == pixelShader) return;

    mPipelineState.SetPixelShader(pixelShader);
    mState.resourceManager.SetPixelShader(pixelShader);

    mResources.ClearApplied();
}

void RenderingContext::Apply()
{
    // Prepare rendering steps. These should do all necessary allocations.
    mTransforms.Prepare(mState);
    mResources.Prepare(mState);

    // Apply changes on current Command List. Below calls must NOT do any operations
    // which might submit the Command List (ex. Ring Container allocations).
    const D3D12GraphicsCommandListPtr& commandList = mNativeDevice->GetCurrentCommandList();

    mRenderTarget.Apply(commandList, mState);
    mViewport.Apply(commandList, mState);

    mScissor.Apply(commandList, mState);
    mDefaultScissor.Apply(commandList, mState);

    mPipelineState.Apply(commandList, mState);
    mDescriptorHeap.Apply(commandList, mState);
    mTransforms.Apply(commandList, mState);
    mResources.Apply(commandList, mState);

    mPrimitiveTopology.Apply(commandList, mState);
    mVertexBuffer.Apply(commandList, mState);
    mIndexBuffer.Apply(commandList, mState);
}

void RenderingContext::EnsureBoundTextureStates(D3D12_RESOURCE_STATES state)
{
    mState.resourceManager.EnsureStates(mNativeDevice->GetCurrentCommandList(), state);
}

void RenderingContext::ClearAppliedFlags()
{
    mIndexBuffer.ClearApplied();
    mVertexBuffer.ClearApplied();

    mPipelineState.ClearApplied();
    mDescriptorHeap.ClearApplied();
    mPrimitiveTopology.ClearApplied();
    mRenderTarget.ClearApplied();
    mScissor.ClearApplied();
    mDefaultScissor.ClearApplied();
    mResources.ClearApplied();
    mTransforms.ClearApplied();
    mViewport.ClearApplied();
}

void RenderingContext::ClearResourcesApplied()
{
    mResources.ClearApplied();
}

} // namespace Internal
} // namespace D3D12
