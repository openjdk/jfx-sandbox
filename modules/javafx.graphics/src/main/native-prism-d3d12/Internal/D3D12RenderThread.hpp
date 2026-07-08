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

#include "../D3D12NativeTexture.hpp"

#include "D3D12CommandListPool.hpp"
#include "D3D12IRenderTarget.hpp"
#include "D3D12LinearAllocator.hpp"
#include "D3D12Matrix.hpp"
#include "D3D12RenderThreadContext.hpp"
#include "D3D12RenderPayload.hpp"
#include "D3D12PSOManager.hpp"

#include <unordered_set>
#include <forward_list>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>


namespace D3D12 {
namespace Internal {

// processes RenderPayload objects on separate thread
class RenderThread
{
    NIPtr<NativeDevice> mNativeDevice;
    D3D12CommandQueuePtr mCommandQueue;
    D3D12FencePtr mCommandQueueFence;
    uint64_t mFenceValue;
    std::vector<Internal::IWaitableOperation*> mWaitableOps;
    std::mutex mWaitableOpsMutex;
    CheckpointQueue mCheckpointQueue;
    RenderThreadContextPtr mContext;

    //NIPtr<Internal::RingBuffer> mVertexRingBuffer; // used for small texture uploads to prevent Committed Resource allocation
    //NIPtr<Internal::RingBuffer> mDataRingBuffer; // used for small texture uploads to prevent Committed Resource allocation

    RenderPayloadPtr mNullPayload; // used to leave FetchPayload() without returning a temporary ref

    std::condition_variable mPayloadAvailableCV;
    std::condition_variable mQueueEmptyCV;
    std::mutex mPayloadQueueMutex;
    std::queue<RenderPayloadPtr> mPayloadQueue;
    std::atomic<bool> mWorkerDone;
    std::thread mWorkerThread;

    RenderPayloadPtr& FetchPayload();
    void WorkerMain();
    void ExecuteCurrentCommandList();
    void AdvanceCommandAllocator();
    void FlushCommandListInternal(CheckpointType type);
    void WaitForCheckpointInternal(CheckpointType type);
    void Signal(CheckpointType type);

public:
    RenderThread(const NIPtr<NativeDevice>& nativeDevice);
    ~RenderThread() = default;

    bool Init();
    void RegisterWaitableOperation(Internal::IWaitableOperation* waitableOp);
    void UnregisterWaitableOperation(Internal::IWaitableOperation* waitableOp);
    const NIPtr<Waitable>& Execute(RenderPayloadPtr&& payload);
    void ScheduleCommandListSubmit(LinearAllocator& allocator, RenderPayloadPtr& payload);
    void ScheduleCommandAllocatorAdvance(LinearAllocator& allocator, RenderPayloadPtr& payload);
    void SchedulePresent(LinearAllocator& allocator, RenderPayloadPtr& payload, const PresentArgs& presentArgs, CheckpointType checkpointType);
    void ScheduleSignal(LinearAllocator& allocator, RenderPayloadPtr& payload, CheckpointType type);
    void ScheduleWaitForCheckpoint(LinearAllocator& allocator, RenderPayloadPtr& payload, CheckpointType type);
    void WaitUntilIdle();
    void Exit();

    inline const D3D12CommandQueuePtr& GetCommandQueue() const
    {
        // LKTODO check if we can avoid that
        return mCommandQueue;
    }
};

} // namespace Internal
} // namespace D3D12
