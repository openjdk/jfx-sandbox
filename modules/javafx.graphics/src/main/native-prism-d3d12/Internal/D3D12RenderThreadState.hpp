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

#include "D3D12PSOManager.hpp"
#include "D3D12CheckpointQueue.hpp"
#include "D3D12CommandListPool.hpp"
#include "D3D12ResourceManager.hpp"


namespace D3D12 {
namespace Internal {

struct RenderThreadState
{
    PSOManager PSOManager;
    CommandListPool commandListPool; // TODO move to RenderThread internals
    ResourceManager resourceManager;
    Internal::RingBuffer vertexRingBuffer; // used for 2D Vertex data
    std::array<D3D12_RESOURCE_BARRIER, 8> barrierQueue;
    uint32_t barrierQueueSize;

    // RT callbacks to control command lists
    CheckpointCallback flushCommandList;
    CheckpointCallback waitForCheckpoint;
    CheckpointCallback signal;

    RenderThreadState(const NIPtr<NativeDevice>& nativeDevice, const CheckpointCallback& flushCallback, const CheckpointCallback& waitCallback, const CheckpointCallback& signalCallback)
        : PSOManager(nativeDevice)
        , commandListPool(nativeDevice, waitCallback)
        , resourceManager(nativeDevice, flushCallback, waitCallback)
        , vertexRingBuffer(nativeDevice, flushCallback, waitCallback)
        , barrierQueue()
        , barrierQueueSize(0)
        , flushCommandList(flushCallback)
        , waitForCheckpoint(waitCallback)
        , signal(signalCallback)
    {
    }

    void QueueTextureTransition(const NIPtr<Internal::ITrackedResource>& resource, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
    {
        if (resource->GetD3D12ResourceState(subresource) == newState) return;

        QueueResourceTransition(resource->GetD3D12Resource(), resource->GetD3D12ResourceState(subresource), newState, subresource);
        resource->SetD3D12ResourceState(newState, subresource);
    }

    void QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
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

    void SubmitResourceTransitions(const D3D12GraphicsCommandListPtr& commandList)
    {
        if (barrierQueueSize == 0) return;

        commandList->ResourceBarrier(barrierQueueSize, barrierQueue.data());
        barrierQueueSize = 0;
    }
};

} // namespace Internal
} // namespace D3D12
