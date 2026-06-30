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

#include "D3D12RenderThreadState.hpp"


namespace D3D12 {
namespace Internal {

RenderThreadState::RenderThreadState(const NIPtr<NativeDevice>& nativeDevice, const CheckpointCallback& flushCallback, const CheckpointCallback& waitCallback, const CheckpointCallback& signalCallback)
    : PSOManager(nativeDevice)
    , commandListPool(nativeDevice, waitCallback)
    , resourceManager(nativeDevice, flushCallback, waitCallback)
    , vertexRingBuffer(nativeDevice, flushCallback, waitCallback)
    , barrierQueue()
    , barrierQueueSize(0)
    , heapsApplied(false) // LKTODO HACKY and needs to be solved better
    , flushCommandList(flushCallback)
    , waitForCheckpoint(waitCallback)
    , signal(signalCallback)
{
}

void RenderThreadState::QueueTextureTransition(const NIPtr<Internal::ITrackedResource>& resource, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    if (resource->GetD3D12ResourceState(subresource) == newState) return;

    QueueResourceTransition(resource->GetD3D12Resource(), resource->GetD3D12ResourceState(subresource), newState, subresource);
    resource->SetD3D12ResourceState(newState, subresource);
}

void RenderThreadState::QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    D3D12NI_ZERO_STRUCT(barrierQueue[barrierQueueSize]);
    barrierQueue[barrierQueueSize].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierQueue[barrierQueueSize].Transition.pResource = resource.Get();
    barrierQueue[barrierQueueSize].Transition.StateBefore = oldState;
    barrierQueue[barrierQueueSize].Transition.StateAfter = newState;
    barrierQueue[barrierQueueSize].Transition.Subresource = subresource;
    ++barrierQueueSize;

    if (barrierQueueSize == barrierQueue.size()) SubmitResourceTransitions(commandListPool.CurrentCommandListRef());
}

void RenderThreadState::SubmitResourceTransitions(const D3D12GraphicsCommandListPtr& commandList)
{
    if (barrierQueueSize == 0) return;

    commandList->ResourceBarrier(barrierQueueSize, barrierQueue.data());
    barrierQueueSize = 0;
}

void RenderThreadState::ApplySteps(const D3D12GraphicsCommandListPtr& commandList)
{
    indexBuffer.Apply(commandList);
    vertexBuffer.Apply(commandList);
    primitiveTopology.Apply(commandList);
    scissor.Apply(commandList);
    viewport.Apply(commandList);
    renderTarget.Apply(commandList);
    pipelineState.Apply(commandList);
    graphicsRootSignature.Apply(commandList);
}

void RenderThreadState::ClearAppliedSteps()
{
    indexBuffer.ClearApplied();
    vertexBuffer.ClearApplied();
    primitiveTopology.ClearApplied();
    scissor.ClearApplied();
    viewport.ClearApplied();
    renderTarget.ClearApplied();
    pipelineState.ClearApplied();
    graphicsRootSignature.ClearApplied();
}

} // namespace Internal
} // namespace D3D12
