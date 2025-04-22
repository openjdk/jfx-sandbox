/*
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "../D3D12NativeTexture.hpp"

#include "D3D12IRenderTarget.hpp"
#include "D3D12Matrix.hpp"
#include "D3D12RenderingParameter.hpp"
#include "D3D12PSOManager.hpp"


namespace D3D12 {
namespace Internal {

/**
 * Stores information about current state of the Renderer and records parameter
 * setting commands on a Command List as needed.
 *
 * JFX assumes the "old API" behavior - API has a context and some things that were
 * set in the Context remain there. With D3D12, due to different API design this is
 * not the case:
 *   - There is no global Context which would be responsible for such behavior
 *   - Rendering parameters (ex. VBs, IBs, PSOs...) are set via a Command
 *     List and are assumed to be local only within that Command List
 *
 * That means, if we ever want to submit a Command List and start recording to a new
 * one, many objects related to current rendering state are lost and must be re-recorded.
 *
 * This class handles that problem while making sure we don't produce any redundant API
 * calls (aka. command records).
 */
class RenderingContext
{
    NIPtr<NativeDevice> mNativeDevice;
    RenderingContextState mState;

    // some parameters are set by the Java Runtime ex. transforms, composite mode, textures
    // whenever we need to execute some internal operation (ex. BlitTexture()) we have to
    // temporarily "stash" them to restore them later. This struct helps with that.
    // We probably won't need more than one "stash".
    struct RuntimeParametersStash
    {
        PipelineStateRenderingParameter pipelineState;
        PrimitiveTopologyRenderingParameter primitiveTopology;
        RenderTargetRenderingParameter renderTarget;
        RootSignatureRenderingParameter rootSignature;
    } mRuntimeParametersStash;

    // Graphics Pipeline
    IndexBufferRenderingParameter mIndexBuffer;
    VertexBufferRenderingParameter mVertexBuffer;
    DescriptorHeapRenderingStep mDescriptorHeap;
    PipelineStateRenderingParameter mPipelineState;
    RootSignatureRenderingParameter mRootSignature;
    PrimitiveTopologyRenderingParameter mPrimitiveTopology;
    RenderTargetRenderingParameter mRenderTarget;
    ScissorRenderingParameter mScissor; // used when explicitly set by updateClipRect()
    ScissorRenderingParameter mDefaultScissor; // used when scissor testing is disabled
    ResourceRenderingStep mResources;
    ViewportRenderingParameter mViewport;

    inline ScissorRenderingParameter& GetScissor()
    {
        if (mScissor.IsSet()) return mScissor;
        else return mDefaultScissor;
    }

    // Compute Pipeline
    ComputePipelineStateRenderingParameter mComputePipelineState;
    ComputeRootSignatureRenderingParameter mComputeRootSignature;
    ComputeResourceRenderingStep mComputeResources;

public:
    RenderingContext(const NIPtr<NativeDevice>& nativeDevice);
    ~RenderingContext() = default;

    bool Init();

    bool Apply();
    bool ApplyCompute();
    void EnsureBoundTextureStates(D3D12_RESOURCE_STATES state);
    void Clear(float r, float g, float b, float a, bool clearDepth);

    void ClearTextureUnit(uint32_t unit);
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView);
    void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbView);
    void SetRenderTarget(const NIPtr<IRenderTarget>& renderTarget);
    void SetScissor(bool enabled, const D3D12_RECT& scissor);
    void SetTexture(uint32_t unit, const NIPtr<NativeTexture>& texture);

    void SetCompositeMode(CompositeMode mode);
    void SetCullMode(D3D12_CULL_MODE mode);
    void SetFillMode(D3D12_FILL_MODE mode);
    void SetVertexShader(const NIPtr<Shader>& vertexShader);
    void SetPixelShader(const NIPtr<Shader>& pixelShader);
    void SetComputeShader(const NIPtr<Shader>& computeShader);

    void StashParamters();
    void RestoreStashedParameters();

    void ClearAppliedFlags();

    // below functions only clear ResourceManager applied flags
    // this is used ex. when JFX wants to use the same pipeline but changes a Shader constant
    void ClearResourcesApplied();
    void ClearComputeResourcesApplied();
};

} // namespace Internal
} // namespace D3D12
