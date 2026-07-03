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
        if (!curPayload->ApplySteps(mContext))
        {
            D3D12NI_LOG_ERROR("RenderThread: Failed to apply current payload's steps. This should not happen. Pausing execution.");
            mWorkerDone = true;
        }
    }

    mCheckpointQueue.PrintStats();
}

void RenderThread::ExecuteCurrentCommandList()
{
    D3D12NI_ASSERT(std::this_thread::get_id() == mWorkerThread.get_id(), "This routine must be called from Render Thread");

    D3D12GraphicsCommandListPtr cmdList = mContext->AdvanceCommandList();
    if (!cmdList)
    {
        D3D12NI_LOG_ERROR("RenderThread: Received empty Command List, aborting execution.");
        return;
    }

    ID3D12CommandList* lists[1] = { cmdList.Get() };
    mCommandQueue->ExecuteCommandLists(1, lists);

    mContext->ClearAppliedSteps();
    mContext->Invalidate2DVertexBatch();
}

void RenderThread::AdvanceCommandAllocator()
{
    // TODO this should be a separate "FinishFrame" command from D3D12RenderThreadExecutable.hpp
    mContext->AdvanceCommandAllocator();
}

void RenderThread::Signal(CheckpointType type)
{
    mFenceValue++;
    if (mFenceValue == 0) mFenceValue++;

    // mark this point in time in places that need it
    {
        std::unique_lock<std::mutex> lock(mWaitableOpsMutex);

        for (Internal::IWaitableOperation* op: mWaitableOps)
        {
            op->OnQueueSignal(type, mFenceValue);
        }
    }

    Waitable waitable(mFenceValue, [this, type](uint64_t fenceValue) -> bool
    {
        std::unique_lock<std::mutex> lock(mWaitableOpsMutex);

        for (Internal::IWaitableOperation* op: mWaitableOps)
        {
            op->OnFenceSignaled(type, fenceValue);
        }

        return true;
    });

    HRESULT hr = mCommandQueue->Signal(mCommandQueueFence.Get(), mFenceValue);
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to signal event on completion");

    hr = mCommandQueueFence->SetEventOnCompletion(mFenceValue, waitable.GetHandle());
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to set Fence event on completion");

    mCheckpointQueue.AddCheckpoint(type, std::move(waitable));
}

void RenderThread::FlushCommandListInternal(CheckpointType type)
{
    D3D12NI_ASSERT(std::this_thread::get_id() == mWorkerThread.get_id(), "This routine must be called from Render Thread");

    ExecuteCurrentCommandList();
    Signal(type);
}

void RenderThread::WaitForCheckpointInternal(CheckpointType type)
{
    D3D12NI_ASSERT(std::this_thread::get_id() == mWorkerThread.get_id(), "This routine must be called from Render Thread");
    mCheckpointQueue.WaitForNextCheckpoint(type);
}


RenderThread::RenderThread(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mCommandQueue()
    , mCommandQueueFence()
    , mFenceValue(0)
    , mWaitableOps()
    , mCheckpointQueue()
    , mContext()
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
    mContext = std::make_unique<RenderThreadContext>(mNativeDevice,
        std::bind(&RenderThread::FlushCommandListInternal, this, std::placeholders::_1),
        std::bind(&RenderThread::WaitForCheckpointInternal, this, std::placeholders::_1),
        std::bind(&RenderThread::Signal, this, std::placeholders::_1)
    );
    if (!mContext)
    {
        D3D12NI_LOG_ERROR("Failed to create RenderThreadState");
        return false;
    }

    if (!mContext->Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize RenderThread's State object");
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC cqDesc;
    D3D12NI_ZERO_STRUCT(cqDesc);
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    // TODO Command Queue should probably reside in Command List Pool
    //      Same with all Execute/Signal logic & checkpoints
    HRESULT hr = mNativeDevice->GetDevice()->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mCommandQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Direct Command Queue");

    hr = mCommandQueue->SetName(L"Main Command Queue");
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to name Direct Command Queue");

    hr = mNativeDevice->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mCommandQueueFence));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create in-device Fence");

    return true;
}

void RenderThread::RegisterWaitableOperation(Internal::IWaitableOperation* waitableOp)
{
    std::unique_lock<std::mutex> lock(mWaitableOpsMutex);

    mWaitableOps.emplace_back(waitableOp);
}

void RenderThread::UnregisterWaitableOperation(Internal::IWaitableOperation* waitableOp)
{
    std::unique_lock<std::mutex> lock(mWaitableOpsMutex);

    for (size_t i = 0; i < mWaitableOps.size(); ++i)
    {
        if (mWaitableOps[i] == waitableOp)
        {
            if (i != mWaitableOps.size() - 1)
            {
                mWaitableOps[i] = mWaitableOps[mWaitableOps.size() - 1];
            }

            mWaitableOps.pop_back();
        }
    }
}

NIPtr<Waitable> RenderThread::Execute(RenderPayloadPtr&& payload)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::Execute() can only be called from main thread");

    const NIPtr<Waitable>& waitable = payload->GetWaitable();

    std::unique_lock<std::mutex> lock(mPayloadQueueMutex);
    mPayloadQueue.emplace(std::move(payload));

    mPayloadAvailableCV.notify_one();
    return waitable;
}

void RenderThread::ScheduleCommandListSubmit(LinearAllocator& allocator, RenderPayloadPtr& payload)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::ScheduleCommandListSubmit() can only be called from main thread");

    payload->AddStep(CreateRTExec<InternalRenderThreadRoutine>(allocator, std::bind(&RenderThread::ExecuteCurrentCommandList, this)));
}

void RenderThread::ScheduleCommandAllocatorAdvance(LinearAllocator& allocator, RenderPayloadPtr& payload)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::ScheduleCommandAllocatorAdvance() can only be called from main thread");

    payload->AddStep(CreateRTExec<InternalRenderThreadRoutine>(allocator, std::bind(&RenderThread::AdvanceCommandAllocator, this)));
}

void RenderThread::ScheduleSignal(LinearAllocator& allocator, RenderPayloadPtr& payload, CheckpointType type)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::ScheduleSignal() can only be called from main thread");

    payload->AddStep(CreateRTExec<InternalRenderThreadRoutine>(allocator, std::bind(&RenderThread::Signal, this, type)));
}

bool RenderThread::WaitForCheckpoint(LinearAllocator& allocator, CheckpointType type)
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::WaitForCheckpoint() can only be called from main thread");

    // TODO: We allocate a whole Payload for just one step here. Could be worth optimizing to save space on the Allocator.
    RenderPayloadPtr payload(allocator.Construct<RenderPayload>(), LinearAllocatorDeleter<RenderPayload>(&allocator));
    payload->AddStep(CreateRTExec<InternalRenderThreadRoutine>(allocator, std::bind(&RenderThread::WaitForCheckpointInternal, this, type)));
    NIPtr<Waitable> w = Execute(std::move(payload));
    if (!w->Wait())
    {
        D3D12NI_LOG_ERROR("Failed to wait for RenderThread's checkpoint");
        return false;
    }

    return true;
}

void RenderThread::WaitUntilIdle()
{
    std::unique_lock<std::mutex> lock(mPayloadQueueMutex);
    while (mPayloadQueue.size() != 0)
    {
        mQueueEmptyCV.wait(lock);
    }
}

void RenderThread::Exit()
{
    D3D12NI_ASSERT(mWorkerThread.get_id() != std::this_thread::get_id(), "RenderThread::Exit() can only be called from main thread");

    if (mWorkerDone) return;

    mWorkerDone = true;
    mPayloadAvailableCV.notify_one();

    mWorkerThread.join();

    // free up RT resources
    mContext.reset();
}

} // namespace Internal
} // namespace D3D12
