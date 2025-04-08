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

#include "../D3D12Constants.hpp"
#include "../D3D12NativeRenderTarget.hpp"
#include "../D3D12NativeShader.hpp"

#include "D3D12Config.hpp"
#include "D3D12PSOManager.hpp"
#include "D3D12ResourceManager.hpp"
#include "D3D12RingDescriptorHeap.hpp"

#include <functional>


namespace D3D12 {
namespace Internal {

struct RenderingContextState
{
    PSOManager PSOManager;
    ResourceManager resourceManager;

    RenderingContextState(const NIPtr<NativeDevice>& nativeDevice)
        : PSOManager(nativeDevice)
        , resourceManager(nativeDevice)
    {
    }
};

class RenderingStep
{
public:
    // Dependency callback. Should return true if step should be applied.
    // We assume the step should be unconditionally applied if there is no dependency set.
    using StepDependency = std::function<bool(RenderingContextState&)>;

private:
    bool mIsApplied;
    bool mOptimizeApply;
    StepDependency mDependency;

protected:
    virtual void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) = 0;
    virtual bool PrepareStep(RenderingContextState& state) { return true; }

public:
    RenderingStep()
        : mIsApplied(false)
        , mOptimizeApply(Config::Instance().IsApiOptsEnabled())
        , mDependency()
    {}

    virtual ~RenderingStep() {};

    // should be called right before any Draw calls
    virtual void Apply(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state)
    {
        if (mOptimizeApply && mIsApplied) return;
        if (mDependency && !mDependency(state)) return;

        ApplyOnCommandList(commandList, state);
        mIsApplied = true;
    }

    // Prepare should be called before Apply(), however ONLY inside RenderingContext.
    // Since Apply() calls will follow right after, we do not need to set the flags here.
    virtual bool Prepare(RenderingContextState& state)
    {
        if (mOptimizeApply && mIsApplied) return true;
        if (mDependency && !mDependency(state)) return true;

        return PrepareStep(state);
    }

    void ClearApplied()
    {
        mIsApplied = false;
    }

    void SetDependency(const StepDependency& dependency)
    {
        mDependency = dependency;
    }
};

template <typename T>
class RenderingParameter: public RenderingStep
{
    bool mIsSet;

protected:
    T mParameter;

    void FlagSet()
    {
        ClearApplied();
        mIsSet = true;
    }

public:
    RenderingParameter()
        : RenderingStep()
        , mParameter()
        , mIsSet(false)
    {}

    ~RenderingParameter() = default;

    void Apply(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        if (!mIsSet) return;

        RenderingStep::Apply(commandList, state);
    }

    bool Prepare(RenderingContextState& state) override final
    {
        if (!mIsSet) return true;

        return RenderingStep::Prepare(state);
    }

    void Set(T prop)
    {
        mParameter = prop;
        FlagSet();
    }

    void Unset()
    {
        mIsSet = false;
    }

    T& Get()
    {
        return mParameter;
    }

    bool IsSet() const
    {
        return mIsSet;
    }
};


// Graphics parameters //

class IndexBufferRenderingParameter: public RenderingParameter<D3D12_INDEX_BUFFER_VIEW>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->IASetIndexBuffer(&mParameter);
    }
};

class VertexBufferRenderingParameter: public RenderingParameter<D3D12_VERTEX_BUFFER_VIEW>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->IASetVertexBuffers(0, 1, &mParameter);
    }
};

class DescriptorHeapRenderingStep: public RenderingStep
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        ID3D12DescriptorHeap* heaps[] = {
            state.resourceManager.GetHeap().Get(),
            state.resourceManager.GetSamplerHeap().Get()
        };
        commandList->SetDescriptorHeaps(2, heaps);
    }
};

class RootSignatureRenderingParameter: public RenderingParameter<D3D12RootSignaturePtr>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->SetGraphicsRootSignature(mParameter.Get());
    }
};

class PipelineStateRenderingParameter: public RenderingParameter<GraphicsPSOParameters>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        const D3D12PipelineStatePtr& pso = state.PSOManager.GetPSO(mParameter);
        commandList->SetPipelineState(pso.Get());
    }

public:
    void SetVertexShader(const NIPtr<Shader>& vertexShader)
    {
        if (mParameter.vertexShader == vertexShader) return;

        mParameter.vertexShader = vertexShader;
        FlagSet();
    }

    void SetPixelShader(const NIPtr<Shader>& pixelShader)
    {
        if (mParameter.pixelShader == pixelShader) return;

        mParameter.pixelShader = pixelShader;
        FlagSet();
    }

    void SetCompositeMode(CompositeMode mode)
    {
        if (mParameter.compositeMode == mode) return;

        mParameter.compositeMode = mode;
        FlagSet();
    }

    void SetCullMode(D3D12_CULL_MODE mode)
    {
        if (mode == mParameter.cullMode) return;

        mParameter.cullMode = mode;
        FlagSet();
    }

    void SetFillMode(D3D12_FILL_MODE mode)
    {
        if (mode == mParameter.fillMode) return;

        mParameter.fillMode = mode;
        FlagSet();
    }

    void SetDepthTest(bool enabled)
    {
        if (mParameter.enableDepthTest == enabled) return;

        mParameter.enableDepthTest = enabled;
        FlagSet();
    }

    void SetMSAASamples(UINT msaaSamples)
    {
        if (mParameter.msaaSamples == msaaSamples) return;

        mParameter.msaaSamples = msaaSamples;
        FlagSet();
    }
};

class PrimitiveTopologyRenderingParameter: public RenderingParameter<D3D12_PRIMITIVE_TOPOLOGY>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->IASetPrimitiveTopology(mParameter);
    }
};

class RenderTargetRenderingParameter: public RenderingParameter<NIPtr<NativeRenderTarget>>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        if (!mParameter) return;

        mParameter->EnsureState(commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (mParameter->IsDepthTestEnabled())
        {
            mParameter->EnsureDepthState(commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            commandList->ClearDepthStencilView(mParameter->GetDSVDescriptor().cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }

        const Internal::DescriptorData& rtData = mParameter->GetDescriptorData();
        commandList->OMSetRenderTargets(
            rtData.count, &rtData.cpu, true, mParameter->IsDepthTestEnabled() ? &mParameter->GetDSVDescriptor().cpu : nullptr
        );
    }
};

class ScissorRenderingParameter: public RenderingParameter<D3D12_RECT>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->RSSetScissorRects(1, &mParameter);
    }
};

class ResourceRenderingStep: public RenderingStep
{
    bool PrepareStep(RenderingContextState& state) override final
    {
        return state.resourceManager.PrepareResources();
    }

    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        state.resourceManager.ApplyResources(commandList);
    }
};

class ViewportRenderingParameter: public RenderingParameter<D3D12_VIEWPORT>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->RSSetViewports(1, &mParameter);
    }
};


// Compute parameters //

class ComputePipelineStateRenderingParameter: public RenderingParameter<ComputePSOParameters>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state)
    {
        commandList->SetPipelineState(state.PSOManager.GetPSO(mParameter).Get());
    }

public:
    void SetComputeShader(const NIPtr<Shader>& shader)
    {
        mParameter.shader = shader;
        FlagSet();
    }
};

class ComputeRootSignatureRenderingParameter: public RenderingParameter<D3D12RootSignaturePtr>
{
    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override
    {
        commandList->SetComputeRootSignature(mParameter.Get());
    }
};

class ComputeResourceRenderingStep: public RenderingStep
{
    bool PrepareStep(RenderingContextState& state) override final
    {
        return state.resourceManager.PrepareComputeResources();
    }

    void ApplyOnCommandList(const D3D12GraphicsCommandListPtr& commandList, RenderingContextState& state) override final
    {
        state.resourceManager.ApplyComputeResources(commandList);
    }
};

} // namespace Internal
} // namespace D3D12
