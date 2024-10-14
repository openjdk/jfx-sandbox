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

#include "D3D12CommandListPool.hpp"

#include "D3D12Config.hpp"

#include "../D3D12NativeDevice.hpp"


namespace D3D12 {
namespace Internal {

void CommandListPool::ResetCurrentCommandList()
{
    D3D12NI_ASSERT(mCommandLists[mCurrentCommandList].state == CommandListState::Available, "Attempted to reset available command list #%d", mCurrentCommandList);

    HRESULT hr = mCommandLists[mCurrentCommandList].commandList->Reset(mCommandAllocator.Get(), nullptr);
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to reset current command list");

    mCommandLists[mCurrentCommandList].state = CommandListState::Active;
}

CommandListPool::CommandListPool(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mCommandLists()
    , mCurrentCommandList(0)
{
    mNativeDevice->RegisterWaitableOperation(this);
}

CommandListPool::~CommandListPool()
{
    for (int i = 0; i < mCommandLists.size(); ++i)
    {
        mCommandLists[i].commandList.Reset();
    }

    mCommandAllocator.Reset();

    mNativeDevice->UnregisterWaitableOperation(this);
    mNativeDevice.reset();
}

bool CommandListPool::Init(D3D12_COMMAND_LIST_TYPE type, size_t amount)
{
    HRESULT hr = mNativeDevice->GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&mCommandAllocator));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Command Allocator");

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
    std::wstring name(typeStr);
    name += L" Command Allocator";
    mCommandAllocator->SetName(name.c_str());

    std::wstring clNameBase(typeStr);
    clNameBase += L" Command List #";
    #endif // DEBUG

    mCommandLists.resize(amount);

    for (size_t i = 0; i < amount; ++i)
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
}

void CommandListPool::OnFenceSignaled(uint64_t fenceValue)
{
    for (int i = 0; i < mCommandLists.size(); ++i)
    {
        if (mCommandLists[i].closedFenceValue != 0 && mCommandLists[i].closedFenceValue <= fenceValue)
        {
            D3D12NI_ASSERT(mCommandLists[i].state == CommandListState::Closed, "Invalid command list state while refreshing post-fence");
            mCommandLists[i].state = CommandListState::Available;
            mCommandLists[i].closedFenceValue = 0;
        }
    }
}

bool CommandListPool::SubmitCurrentCommandList()
{
    D3D12NI_ASSERT(mCommandLists[mCurrentCommandList].state == CommandListState::Active, "Invalid Command List #%d state %d", mCurrentCommandList, mCommandLists[mCurrentCommandList].state);

    HRESULT hr = mCommandLists[mCurrentCommandList].commandList->Close();
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to close Command List");

    mCommandLists[mCurrentCommandList].state = CommandListState::Closed;

    mNativeDevice->Execute({ mCommandLists[mCurrentCommandList].commandList.Get() });

    ++mCurrentCommandList;
    if (mCurrentCommandList == mCommandLists.size()) mCurrentCommandList = 0;

    D3D12NI_ASSERT(mCommandLists[mCurrentCommandList].state == CommandListState::Available, "New command list not available");

    return true;
}

} // namespace Internal
} // namespace D3D12
