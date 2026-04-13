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

#include "D3D12RenderThread.hpp"

#include "../D3D12NativeDevice.hpp"

#include "D3D12Config.hpp"
#include "D3D12Debug.hpp"
#include "D3D12Profiler.hpp"
#include "D3D12Logger.hpp"


namespace D3D12 {
namespace Internal {

RenderPayloadPtr& RenderThread::FetchPayload()
{
    std::unique_lock<std::mutex> lock(mPayloadQueueMutex);

    if (mPayloadQueue.size() > 0)
    {
        // we're here because there used to be an element on queue that we just finished
        // we should pop right now
        mPayloadQueue.pop();
    }

    while (mPayloadQueue.size() == 0)
    {
        mQueueEmptyCV.notify_one();

        mPayloadAvailableCV.wait(lock);
        if (mWorkerDone) return mNullPayload;
    }

    return mPayloadQueue.front();
}

void RenderThread::WorkerMain()
{
    while (!mWorkerDone)
    {
        RenderPayloadPtr& curPayload = FetchPayload();
        if (!curPayload) continue;

        // record what is needed on the Command List
        curPayload->ApplySteps(mState.commandListPool.CurrentCommandList(), mState);
    }
}

RenderThread::RenderThread(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mState(nativeDevice)
    , mNullPayload(nullptr, LinearAllocatorDeleter<RenderPayload>(nullptr))
    , mPayloadAvailableCV()
    , mPayloadQueueMutex()
    , mPayloadQueue()
    , mWorkerDone(false)
    , mWorkerThread(&RenderThread::WorkerMain, this)
{
    SetThreadDescription(mWorkerThread.native_handle(), L"JavaFX D3D12 RenderThread");
}

bool RenderThread::Init()
{
    if (!mState.PSOManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize PSO Manager");
        return false;
    }

    // NOTE: Command List Pool requires to have as many Command Allocators as SwapChain buffers + 1
    // This is the minimum we need (one per frame) and also ensures an extra one to pre-record more commands
    // if both SwapChain buffers are full.
    if (!mState.commandListPool.Init(D3D12_COMMAND_LIST_TYPE_DIRECT, 16, 4))
    {
        D3D12NI_LOG_ERROR("Failed to initialize Command List Pool");
        return false;
    }

    return true;
}

void RenderThread::Execute(RenderPayloadPtr&& payload)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::Execute() can only be called from main thread");

    std::unique_lock<std::mutex> lock(mPayloadQueueMutex);
    mPayloadQueue.emplace(std::move(payload));

    mPayloadAvailableCV.notify_one();
}

void RenderThread::WaitForCompletion()
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::WaitForCompletion() can only be called from main thread");

    std::unique_lock<std::mutex> lock(mPayloadQueueMutex);
    while (mPayloadQueue.size() != 0)
    {
        mQueueEmptyCV.wait(lock);
    }
}

D3D12GraphicsCommandListPtr RenderThread::FinalizeCommandList(LinearAllocator& allocator, RenderPayloadPtr&& payload)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::FinalizeCommandList() can only be called from main thread");

    // Command List has to be closed by the Render Thread, otherwise we risk thread safety
    // We'll do this by a custom Payload that will be added to the Queue and executed
    // First grab the reference to the current Command List - we can be sure this is what we return
    const D3D12GraphicsCommandListPtr& currentList = mState.commandListPool.CurrentCommandListRef();

    // Then submit the payload advancing the command list
    payload->AddStep(CreateRTExec<AdvanceCommandList>(allocator));
    Execute(std::move(payload));
    WaitForCompletion();

    // this list should now be closed and we can return it for execution on a Command Queue
    return currentList;
}

void RenderThread::Exit()
{
    mWorkerDone = true;
    mPayloadAvailableCV.notify_one();

    mWorkerThread.join();
}

} // namespace Internal
} // namespace D3D12
