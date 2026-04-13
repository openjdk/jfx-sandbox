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
#include "D3D12LinearAllocator.hpp"
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
    virtual RenderThreadExecutablePtr CreateExecutable(LinearAllocator& allocator) const = 0;

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

    virtual void AddToPayload(LinearAllocator& allocator, const RenderPayloadPtr& payload)
    {
        if (CanBeSkipped()) return;

        payload->AddStep(CreateExecutable(allocator));
        mIsApplied = true;
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

    RenderThreadExecutablePtr CreateExecutable(LinearAllocator& allocator) const override final
    {
        return CreateRTExec<Executable>(allocator, mParameter);
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


// Graphics parameters //

class DescriptorHeapsRenderingParameter: public RenderingParameter<DescriptorHeaps, ApplyDescriptorHeaps>
{
public:
    void SetHeap(const D3D12DescriptorHeapPtr& heap)
    {
        mParameter.heap = heap;
        FlagSet();
    }

    void SetSamplerHeap(const D3D12DescriptorHeapPtr& heap)
    {
        mParameter.samplerHeap = heap;
        FlagSet();
    }
};

class DescriptorsRenderingParameter: public RenderingParameter<Descriptors, ApplyDescriptors>
{
public:
    void MoveDescriptors(Descriptors&& descriptors)
    {
        if (descriptors.CBVCount == 0 && descriptors.DTCount == 0) return;

        mParameter = std::move(descriptors);
        FlagSet();
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

class ComputeDescriptorsRenderingParameter: public RenderingParameter<Descriptors, ApplyComputeDescriptors>
{
public:
    void MoveDescriptors(Descriptors&& descriptors)
    {
        if (descriptors.CBVCount == 0 && descriptors.DTCount == 0) return;

        mParameter = std::move(descriptors);
        FlagSet();
    }
};

class ComputeRootSignatureRenderingParameter: public RenderingParameter<D3D12RootSignaturePtr, ApplyComputeRootSignature> {};

} // namespace Internal
} // namespace D3D12
