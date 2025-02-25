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

#include "D3D12DescriptorHeap.hpp"

#include "../D3D12NativeDevice.hpp"

#include <codecvt>


namespace D3D12 {
namespace Internal {

DescriptorHeap::DescriptorHeap(const NIPtr<NativeDevice>& device)
    : mDevice(device)
    , mHeap()
    , mShaderVisible(false)
    , mCPUStartHandle{0}
    , mGPUStartHandle{0}
    , mSlotAvailability()
    , mFirstFreeSlot(0)
    , mReady(false)
{
    for (bool& slot: mSlotAvailability)
    {
        slot = true;
    }
}

bool DescriptorHeap::Init(D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    D3D12NI_ZERO_STRUCT(desc);
    desc.Type = type;
    // TODO: D3D12: this is static count for now, to be automatically increased later on
    desc.NumDescriptors = MAX_DESCRIPTOR_SLOT_COUNT;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    // TODO: D3D12: for multi-adapters, we need to set below to non-zero.
    // See: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_descriptor_heap_desc
    desc.NodeMask = 0;

    HRESULT hr = mDevice->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Descriptor Heap");

    mShaderVisible = shaderVisible;

    mCPUStartHandle = mHeap->GetCPUDescriptorHandleForHeapStart();
    mIncrementSize = mDevice->GetDevice()->GetDescriptorHandleIncrementSize(type);

    if (mShaderVisible)
    {
        mGPUStartHandle = mHeap->GetGPUDescriptorHandleForHeapStart();
    }

    mAllocatedCountTotal = 0;
    mSize = MAX_DESCRIPTOR_SLOT_COUNT;
    mReady = true;

    return true;
}

DescriptorData DescriptorHeap::Allocate(UINT count)
{
    if (!mReady) return DescriptorData();

    if (count > mSlotAvailability.size())
    {
        D3D12NI_ASSERT(count > mSlotAvailability.size(), "Too many descriptors requested for alloc");
        return DescriptorData();
    }

    size_t i = mFirstFreeSlot;

    do
    {
        // if we don't have enough slots till the end of the array to allocate requested
        // descriptor count, skip those and continue searching from the beginning
        if ((mSlotAvailability.size() - i) < count)
        {
            i = 0;
            continue;
        }

        bool enoughSpace = true;
        for (size_t x = i; x < i + count; ++x)
        {
            if (mSlotAvailability[x] == false)
            {
                // not enough space here, continue on
                i = x + 1;
                enoughSpace = false;
                break;
            }
        }

        if (enoughSpace)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE retCPUHandle = mCPUStartHandle;
            retCPUHandle.ptr += i * mIncrementSize;

            D3D12_GPU_DESCRIPTOR_HANDLE retGPUHandle = mGPUStartHandle;
            if (mShaderVisible)
            {
                // increment GPU handle only if our Heap is shader-visible
                // otherwise we should keep it as NULL
                retGPUHandle.ptr += i * mIncrementSize;
            }

            // mark slots as used and update counters
            for (size_t x = i; x < i + count; ++x)
            {
                mSlotAvailability[x] = false;
            }
            mFirstFreeSlot = i + count;

            mAllocatedCountTotal += count;
            D3D12NI_LOG_TRACE("%s: Allocated %u descriptors, %u/%u taken", mName.c_str(), count, mAllocatedCountTotal, mSize);

            return DescriptorData(retCPUHandle, retGPUHandle, count, mIncrementSize);
        }
    }
    while (i != mFirstFreeSlot);

    D3D12NI_ASSERT(false, "Failed to find enough room to allocate %u descriptors", count);
    return DescriptorData();
}

void DescriptorHeap::Free(const DescriptorData& data)
{
    if (!mReady) return;

    size_t slot = (data.cpu.ptr - mCPUStartHandle.ptr) / mIncrementSize;
    size_t finalSlot = slot + data.count;

    uint32_t freed = 0;
    for (slot; slot < finalSlot; ++slot)
    {
        mSlotAvailability[slot] = true;
        freed++;
    }

    mAllocatedCountTotal -= freed;
    D3D12NI_LOG_TRACE("%s: Freed %u descriptors, %u/%u taken ", mName.c_str(), freed, mAllocatedCountTotal, mSize);
}

void DescriptorHeap::SetName(const std::string& name)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    mName = name;
    mHeap->SetName(converter.from_bytes(name).c_str());
}

} // namespace Internal
} // namespace D3D12
