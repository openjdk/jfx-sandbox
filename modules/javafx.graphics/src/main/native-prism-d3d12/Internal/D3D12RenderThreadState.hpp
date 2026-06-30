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

#include "D3D12RenderingStep.hpp"

#include "D3D12PSOManager.hpp"
#include "D3D12CheckpointQueue.hpp"
#include "D3D12CommandListPool.hpp"
#include "D3D12ResourceManager.hpp"


namespace D3D12 {
namespace Internal {

template <typename T>
class CommandListDataStep: public RenderingDataStep<T>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const = 0;

public:
    void Apply(const D3D12GraphicsCommandListPtr& commandList)
    {
        if (this->CanBeSkipped()) return;

        this->ApplyImpl(commandList);
        this->mIsApplied = true;
    }
};

class IndexBufferCommandListStep: public CommandListDataStep<D3D12_INDEX_BUFFER_VIEW>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->IASetIndexBuffer(&this->mParameter);
    }
};

class VertexBufferCommandListStep: public CommandListDataStep<D3D12_VERTEX_BUFFER_VIEW>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->IASetVertexBuffers(0, 1, &this->mParameter);
    }
};

class PrimitiveTopologyCommandListStep: public CommandListDataStep<D3D12_PRIMITIVE_TOPOLOGY>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->IASetPrimitiveTopology(this->mParameter);
    }
};

class ScissorCommandListStep: public CommandListDataStep<D3D12_RECT>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->RSSetScissorRects(1, &this->mParameter);
    }
};

class ViewportCommandListStep: public CommandListDataStep<D3D12_VIEWPORT>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->RSSetViewports(1, &this->mParameter);
    }
};

class PipelineStateCommandListStep: public CommandListDataStep<D3D12PipelineStatePtr>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->SetPipelineState(mParameter.Get());
    }
};

class GraphicsRootSignatureCommandListStep: public CommandListDataStep<D3D12RootSignaturePtr>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->SetGraphicsRootSignature(mParameter.Get());
    }
};

struct RenderTargetCommandListData
{
    UINT count;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;

    RenderTargetCommandListData()
        : RenderTargetCommandListData(0, D3D12_CPU_DESCRIPTOR_HANDLE{0}, D3D12_CPU_DESCRIPTOR_HANDLE{0})
    {}

    RenderTargetCommandListData(UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE& rtv)
        : RenderTargetCommandListData(count, rtv, D3D12_CPU_DESCRIPTOR_HANDLE{0})
    {}

    RenderTargetCommandListData(UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE& rtv, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv)
        : count(count), rtv(rtv), dsv(dsv)
    {}
};

class RenderTargetCommandListStep: public CommandListDataStep<RenderTargetCommandListData>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->OMSetRenderTargets(
            mParameter.count, &mParameter.rtv, true, mParameter.dsv.ptr > 0 ? &mParameter.dsv : nullptr
        );
    }
};

class RenderThreadState
{
public:
    PSOManager PSOManager;
    CommandListPool commandListPool; // TODO move to RenderThread internals
    ResourceManager resourceManager;
    Internal::RingBuffer vertexRingBuffer; // used for 2D Vertex data
    std::array<D3D12_RESOURCE_BARRIER, 8> barrierQueue;
    uint32_t barrierQueueSize;
    bool heapsApplied;

    // RT callbacks to control command lists
    CheckpointCallback flushCommandList;
    CheckpointCallback waitForCheckpoint;
    CheckpointCallback signal;

    // RenderThread-side render parameters. This mostly clones what RenderingContext has.
    // RenderingContext updates these via RTExecutables and those are reflected on CommandLists.
    // The reason for duplicating these is to both reduce Command List records, as well as to
    // restore the Command List to its "known state" when internally RenderThread flushes
    // the Command List
    /*
    PipelineStateRenderingParameter mPipelineState;
    RenderTargetRenderingParameter mRenderTarget;
    */
    IndexBufferCommandListStep indexBuffer;
    VertexBufferCommandListStep vertexBuffer;
    PrimitiveTopologyCommandListStep primitiveTopology;
    ScissorCommandListStep scissor;
    ViewportCommandListStep viewport;
    PipelineStateCommandListStep pipelineState;
    RenderTargetCommandListStep renderTarget;
    GraphicsRootSignatureCommandListStep graphicsRootSignature;

    RenderThreadState(const NIPtr<NativeDevice>& nativeDevice, const CheckpointCallback& flushCallback, const CheckpointCallback& waitCallback, const CheckpointCallback& signalCallback);

    void QueueTextureTransition(const NIPtr<Internal::ITrackedResource>& resource, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void SubmitResourceTransitions(const D3D12GraphicsCommandListPtr& commandList);

    void ApplySteps(const D3D12GraphicsCommandListPtr& commandList);
    void ClearAppliedSteps();
};

} // namespace Internal
} // namespace D3D12
