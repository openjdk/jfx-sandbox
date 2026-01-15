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

#include "D3D12CommandListPool.hpp"

#include "D3D12Config.hpp"
#include "D3D12Debug.hpp"
#include "D3D12Profiler.hpp"

#include "../D3D12NativeDevice.hpp"


namespace D3D12 {
namespace Internal {

void CommandListPool::ResetCurrentCommandList()
{
    D3D12NI_ASSERT(CurrentCommandListData().state == CommandListState::Available, "Attempted to reset available command list #%d", mCurrentCommandList);

    HRESULT hr = CurrentCommandListData().commandList->Reset(mCommandAllocators[mCurrentCommandAllocator].allocator.Get(), nullptr);
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to reset current command list");

    CurrentCommandListData().state = CommandListState::Active;
}

void CommandListPool::WaitForAvailableCommandList()
{
    Profiler::Instance().MarkEvent(mCommandListProfilerID, Profiler::Event::Wait);

    while (mCommandLists[mCurrentCommandList].state == CommandListState::Closed &&
           mNativeDevice->GetCheckpointQueue().HasCheckpoints())
    {
        mNativeDevice->GetCheckpointQueue().WaitForNextCheckpoint(CheckpointType::ANY);
    }

    // if this assertion ever triggers, there is something terribly wrong
    D3D12NI_ASSERT(mCommandLists[mCurrentCommandList].state == CommandListState::Available,
        "Waited through the entire Queue, yet current Command List is still not available. Something has gone terribly wrong."
    );
}

void CommandListPool::WaitForAvailableCommandAllocator()
{
    Profiler::Instance().MarkEvent(mCommandAllocatorProfilerID, Profiler::Event::Wait);

    while (mCommandAllocators[mCurrentCommandAllocator].state == CommandListState::Closed &&
           mNativeDevice->GetCheckpointQueue().HasCheckpoints())
    {
        mNativeDevice->GetCheckpointQueue().WaitForNextCheckpoint(CheckpointType::ENDFRAME);
    }

    // if this assertion ever triggers there is something terribly wrong
    D3D12NI_ASSERT(mCommandAllocators[mCurrentCommandList].state == CommandListState::Available,
        "Waited through the entire Queue, yet current Command Allocator is still not available. Something has gone terribly wrong."
    );
}

CommandListPool::CommandListPool(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mCommandLists()
    , mCurrentCommandList(0)
    , mCommandAllocators()
    , mCurrentCommandAllocator(0)
{
    mNativeDevice->RegisterWaitableOperation(this);
    mCommandListProfilerID = Profiler::Instance().RegisterSource("Command List Pool");
    mCommandAllocatorProfilerID = Profiler::Instance().RegisterSource("Command Allocator Pool");
}

CommandListPool::~CommandListPool()
{
    for (int i = 0; i < mCommandLists.size(); ++i)
    {
        if (mCommandLists[i].state == CommandListState::Active)
            mCommandLists[i].commandList->Close();

        mCommandLists[i].commandList.Reset();
    }

    for (int i = 0; i < mCommandAllocators.size(); ++i)
    {
        mCommandAllocators[i].allocator.Reset();
    }

    mNativeDevice->UnregisterWaitableOperation(this);
    mNativeDevice.reset();
}

bool CommandListPool::Init(D3D12_COMMAND_LIST_TYPE type, size_t commandListCount, size_t commandAllocators)
{
    #if DEBUG
    // for debugging
    wchar_t* typeStr;
    switch (type)
    {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        typeStr = L"Direct";
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        typeStr = L"Compute";
        break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        typeStr = L"Copy";
        break;
    default:
        typeStr = L"UNKNOWN";
        break;
    }
    std::wstring caNameBase(typeStr);
    caNameBase += L" Command Allocator #";

    std::wstring clNameBase(typeStr);
    clNameBase += L" Command List #";
    #endif

    HRESULT hr = S_OK;

    mCommandAllocators.resize(commandAllocators);
    for (size_t i = 0; i < mCommandAllocators.size(); ++i) {
        hr = mNativeDevice->GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&mCommandAllocators[i].allocator));
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Command Allocator");

        mCommandAllocators[i].state = CommandListState::Available;
        mCommandAllocators[i].closedFenceValue = 0;

        #if DEBUG
        std::wstring caName = caNameBase + std::to_wstring(i);
        mCommandAllocators[i].allocator->SetName(caName.c_str());
        #endif // DEBUG
    }

    mCommandLists.resize(commandListCount);
    for (size_t i = 0; i < mCommandLists.size(); ++i)
    {
        hr = mNativeDevice->GetDevice()->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&mCommandLists[i].commandList));
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to create a Command List for the pool");

        mCommandLists[i].state = CommandListState::Available;
        mCommandLists[i].closedFenceValue = 0;

        #if DEBUG
        std::wstring clName = clNameBase + std::to_wstring(i);
        mCommandLists[i].commandList->SetName(clName.c_str());
        #endif // DEBUG
    }

    mCurrentCommandList = 0;
    mCurrentCommandAllocator = 0;

    return true;
}

void CommandListPool::OnQueueSignal(uint64_t fenceValue)
{
    size_t counter = mCurrentCommandList == 0 ? mCommandLists.size() : mCurrentCommandList;
    --counter;

    while (counter != mCurrentCommandList &&
           mCommandLists[counter].state == CommandListState::Closed &&
           mCommandLists[counter].closedFenceValue == 0)
    {
        mCommandLists[counter].closedFenceValue = fenceValue;

        if (counter == 0) counter = mCommandLists.size();
        --counter;
    }


    size_t caCounter = mCurrentCommandAllocator == 0 ? mCommandAllocators.size() : mCurrentCommandAllocator;
    --caCounter;

    while (caCounter != mCurrentCommandAllocator &&
           mCommandAllocators[caCounter].state == CommandListState::Closed &&
           mCommandAllocators[caCounter].closedFenceValue == 0)
    {
        mCommandAllocators[caCounter].closedFenceValue = fenceValue;

        if (caCounter == 0) caCounter = mCommandAllocators.size();
        --caCounter;
    }
}

void CommandListPool::OnFenceSignaled(uint64_t fenceValue)
{
    for (size_t i = 0; i < mCommandLists.size(); ++i)
    {
        if (mCommandLists[i].closedFenceValue != 0 && mCommandLists[i].closedFenceValue <= fenceValue)
        {
            D3D12NI_ASSERT(mCommandLists[i].state == CommandListState::Closed, "Invalid command list state while refreshing post-fence");
            mCommandLists[i].state = CommandListState::Available;
            mCommandLists[i].closedFenceValue = 0;
        }
    }

    for (size_t i = 0; i < mCommandAllocators.size(); ++i)
    {
        if (mCommandAllocators[i].closedFenceValue != 0 && mCommandAllocators[i].closedFenceValue <= fenceValue)
        {
            D3D12NI_ASSERT(mCommandAllocators[i].state == CommandListState::Closed, "Invalid command allocator state while refreshing post-fence");
            mCommandAllocators[i].state = CommandListState::Available;
            mCommandAllocators[i].closedFenceValue = 0;
        }
    }
}

bool CommandListPool::SubmitCurrentCommandList()
{
    D3D12NI_ASSERT(CurrentCommandListData().state == CommandListState::Active, "Invalid Command List #%d state %d", mCurrentCommandList, CurrentCommandListData().state);

    HRESULT hr = CurrentCommandListData().commandList->Close();
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to close Command List");

    CurrentCommandListData().state = CommandListState::Closed;

    mNativeDevice->Execute({ CurrentCommandListData().commandList.Get() });

    ++mCurrentCommandList;
    if (mCurrentCommandList == mCommandLists.size()) mCurrentCommandList = 0;

    if (CurrentCommandListData().state == CommandListState::Available)
    {
        ResetCurrentCommandList();
    }

    return true;
}

void CommandListPool::AdvanceAllocator()
{
    mCommandAllocators[mCurrentCommandAllocator].state = CommandListState::Closed;

    mCurrentCommandAllocator++;
    if (mCurrentCommandAllocator == mCommandAllocators.size()) mCurrentCommandAllocator = 0;

    // Command Allocators should be advanced per-frame by a NativeSwapChain instance.
    // If we run out of CAs we have to wait for one of previous frames to be completed.
    if (mCommandAllocators[mCurrentCommandAllocator].state != CommandListState::Available)
    {
        WaitForAvailableCommandAllocator();
    }

    D3D12NI_ASSERT(mCommandAllocators[mCurrentCommandAllocator].state == CommandListState::Available,
        "About to reset a Command Allocator that's still in use, which should never happen. Something is terribly wrong.");
    mCommandAllocators[mCurrentCommandAllocator].allocator->Reset();
}

} // namespace Internal
} // namespace D3D12
