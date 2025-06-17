/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12DescriptorAllocator.hpp"

#include "../D3D12NativeDevice.hpp"

#include "D3D12Debug.hpp"


namespace D3D12 {
namespace Internal {

std::string DescriptorAllocator::HeapSpecificName(uint32_t id) const
{
    return mName + '_' + std::to_string(id);
}

bool DescriptorAllocator::AddHeap()
{
    mLastHeapID++;
    if (mLastHeapID == 0) mLastHeapID++;

    // first try and allocate the new heap
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    D3D12NI_ZERO_STRUCT(desc);
    desc.NumDescriptors = DescriptorHeap::MAX_DESCRIPTOR_SLOT_COUNT;
    desc.Type = mType;
    desc.Flags = (mShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    desc.NodeMask = 0;

    D3D12DescriptorHeapPtr heap;
    HRESULT hr = mNativeDevice->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to allocate new Descriptor Heap");

    uint32_t increment = mNativeDevice->GetDevice()->GetDescriptorHandleIncrementSize(mType);

    mHeaps.emplace(std::make_pair(mLastHeapID, DescriptorHeap(heap, increment, mLastHeapID, HeapSpecificName(mLastHeapID))));
    return true;
}

DescriptorAllocator::DescriptorAllocator(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mHeaps()
    , mLastHeapID(0)
    , mType(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES)
    , mShaderVisible(false)
    , mName("Descriptor Heap")
{
}

bool DescriptorAllocator::Init(D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
    mType = type;
    mShaderVisible = shaderVisible;

    return AddHeap();
}

DescriptorData DescriptorAllocator::Allocate(uint32_t count)
{
    if (count > DescriptorHeap::MAX_DESCRIPTOR_SLOT_COUNT)
    {
        D3D12NI_LOG_ERROR("Tried to allocate too many descriptors (%d, max allowed %d)", count, DescriptorHeap::MAX_DESCRIPTOR_SLOT_COUNT);
        return DescriptorData();
    }

    auto& heapIt = mHeaps.find(mLastHeapID);
    D3D12NI_ASSERT(heapIt != mHeaps.end(), "Cannot find available descriptor allocator!");

    DescriptorData data = heapIt->second.Allocate(count);
    if (!data)
    {
        D3D12NI_LOG_TRACE("Current heap must be full or too fragmented, advancing to a new one");
        if (!AddHeap())
        {
            D3D12NI_LOG_ERROR("Failed to add new Descriptor Heap for allocation of %d descriptors", count);
            return DescriptorData();
        }

        // retry the allocation
        heapIt = mHeaps.find(mLastHeapID);
        data = heapIt->second.Allocate(count);
        if (!data)
        {
            D3D12NI_LOG_ERROR("Failed to allocate %d descriptors", count);
            return DescriptorData();
        }
    }

    return data;
}

void DescriptorAllocator::Free(const DescriptorData& data)
{
    const auto& heapIt = mHeaps.find(data.allocatorId);
    D3D12NI_ASSERT(heapIt != mHeaps.end(), "Tried to free a block with invalid allocator ID");

    DescriptorHeap& heap = heapIt->second;

    heap.Free(data);
    if (heap.Empty() && heapIt->first != mLastHeapID)
    {
        // we advanced past this heap as our "recent useable" one and it has been completely freed
        // which means we can dispose of it
        mNativeDevice->MarkResourceDisposed(heap.GetHeap());
        mHeaps.erase(heapIt);
    }
}

void DescriptorAllocator::SetName(const std::string& name)
{
    mName = name;
    for (auto& heap: mHeaps)
    {
        heap.second.SetName(HeapSpecificName(heap.first));
    }
}

} // namespace Internal
} // namespace D3D12
