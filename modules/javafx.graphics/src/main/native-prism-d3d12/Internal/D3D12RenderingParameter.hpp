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

#pragma once

#include "../D3D12Common.hpp"

#include "../D3D12Constants.hpp"
#include "../D3D12NativeShader.hpp"

#include "D3D12CommandListPool.hpp"
#include "D3D12Config.hpp"
#include "D3D12IRenderTarget.hpp"
#include "D3D12RingDescriptorHeap.hpp"
#include "D3D12RenderThreadExecutable.hpp"
#include "D3D12RenderPayload.hpp"

#include <functional>


namespace D3D12 {
namespace Internal {


class RenderingStep
{
public:
    // Dependency callback. Should return true if step should be applied.
    // We assume the step should be unconditionally applied if there is no dependency set.
    using StepDependency = std::function<bool()>;

private:
    bool mIsApplied;
    const bool mOptimizeApply;
    StepDependency mDependency;

protected:
    virtual RenderThreadExecutablePtr CreateExecutable() const = 0;

    virtual bool CanBeSkipped() const
    {
        return (mOptimizeApply && mIsApplied) || (mDependency && !mDependency());
    }

public:
    RenderingStep()
        : mIsApplied(false)
        , mOptimizeApply(Config::IsApiOptsEnabled())
        , mDependency()
    {}

    virtual ~RenderingStep() {}

    virtual void AddToPayload(const std::unique_ptr<RenderPayload>& payload)
    {
        if (CanBeSkipped()) return;

        payload->AddStep(CreateExecutable());
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

template <typename T, typename Executable>
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

    RenderThreadExecutablePtr CreateExecutable() const override final
    {
        return std::make_unique<Executable>(mParameter);
    }

    bool CanBeSkipped() const override final
    {
        return (!mIsSet || RenderingStep::CanBeSkipped());
    }

public:
    RenderingParameter()
        : RenderingStep()
        , mParameter()
        , mIsSet(false)
    {}

    ~RenderingParameter() = default;

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

template <typename T, typename Executable>
class RenderingResource: public RenderingParameter<T, Executable>
{
public:
    RenderingResource()
        : RenderingParameter()
    {}

    virtual void AddToPayload(const RenderPayloadPtr& payload) override
    {
        if (CanBeSkipped()) return;

        payload->AddResourceStep(CreateExecutable());
    }
};


// generic Shader Constants resource
// requires slightly different path checking if shader's constants are dirtied

template <typename Executable>
class ShaderConstantsResource: public RenderingParameter<NIPtr<Shader>, Executable>
{
public:
    ShaderConstantsResource()
        : RenderingParameter()
    {}

    virtual void AddToPayload(const RenderPayloadPtr& payload) override
    {
        if (CanBeSkipped() && !mParameter->AreConstantsDirty()) return;

        payload->AddResourceStep(CreateExecutable());
        mParameter->SetConstantsDirty(false);
    }
};


// Graphics parameters //

class DescriptorHeapRenderingStep: public RenderingStep
{
    RenderThreadExecutablePtr CreateExecutable() const override
    {
        return std::make_unique<ApplyDescriptorHeaps>();
    }
};

class PipelineStateRenderingParameter: public RenderingParameter<GraphicsPSOParameters, ApplyPipelineState>
{
public:
    void SetVertexShader(const NIPtr<Shader>& vertexShader)
    {
        mParameter.vertexShader = vertexShader;
        FlagSet();
    }

    void SetPixelShader(const NIPtr<Shader>& pixelShader)
    {
        mParameter.pixelShader = pixelShader;
        FlagSet();
    }

    void SetCompositeMode(CompositeMode mode)
    {
        mParameter.compositeMode = mode;
        FlagSet();
    }

    void SetCullMode(D3D12_CULL_MODE mode)
    {
        mParameter.cullMode = mode;
        FlagSet();
    }

    void SetFillMode(D3D12_FILL_MODE mode)
    {
        mParameter.fillMode = mode;
        FlagSet();
    }

    // below Sets do the set-redundancy-check because they're used in multiple places
    // when setting a new RenderTarget in RenderingContext; this is to sometimes prevent
    // PipelineState change when RTT changes but its parameters (depth/MSAA) remain the
    // same as the old RTT.
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

class IndexBufferRenderingParameter: public RenderingParameter<D3D12_INDEX_BUFFER_VIEW, ApplyIndexBuffer> {};
class PrimitiveTopologyRenderingParameter: public RenderingParameter<D3D12_PRIMITIVE_TOPOLOGY, ApplyPrimitiveTopology> {};
class RenderTargetRenderingParameter: public RenderingParameter<NIPtr<IRenderTarget>, ApplyRenderTarget> {};
class RootSignatureRenderingParameter: public RenderingParameter<D3D12RootSignaturePtr, ApplyRootSignature> {};
class ScissorRenderingParameter: public RenderingParameter<D3D12_RECT, ApplyScissor> {};
class VertexBufferRenderingParameter: public RenderingParameter<D3D12_VERTEX_BUFFER_VIEW, ApplyVertexBuffer> {};
class ViewportRenderingParameter: public RenderingParameter<D3D12_VIEWPORT, ApplyViewport> {};

// Graphics resources //

class GraphicsShadersRenderingParameter: public RenderingResource<GraphicsShaders, SetGraphicsShaders>
{
public:
    void SetVertexShader(const NIPtr<Shader>& vertexShader)
    {
        mParameter.vertexShader = vertexShader;
        FlagSet();
    }

    void SetPixelShader(const NIPtr<Shader>& pixelShader)
    {
        mParameter.pixelShader = pixelShader;
        FlagSet();
    }
};

class TextureRenderingParameter: public RenderingResource<TextureBank, SetTextures>
{
public:
    void SetTexture(uint32_t unit, const NIPtr<TextureBase>& texture)
    {
        mParameter[unit] = texture;
        FlagSet();
    }

    const NIPtr<TextureBase>& GetTexture(uint32_t unit) const
    {
        return mParameter[unit];
    }
};

class VertexShaderConstantsRenderingStep: public ShaderConstantsResource<SetVertexShaderConstants> {};
class PixelShaderConstantsRenderingStep: public ShaderConstantsResource<SetPixelShaderConstants> {};


// Compute parameters //

class ComputePipelineStateRenderingParameter: public RenderingParameter<ComputePSOParameters, ApplyComputePipelineState>
{
public:
    void SetComputeShader(const NIPtr<Shader>& shader)
    {
        mParameter.shader = shader;
        FlagSet();
    }
};

class ComputeRootSignatureRenderingParameter: public RenderingParameter<D3D12RootSignaturePtr, ApplyComputeRootSignature> {};

// Compute resources //

class ComputeShaderRenderingParameter: public RenderingResource<NIPtr<Shader>, SetComputeShader> {};
class ComputeShaderConstantsRenderingStep: public ShaderConstantsResource<SetComputeShaderConstants> {};

} // namespace Internal
} // namespace D3D12
