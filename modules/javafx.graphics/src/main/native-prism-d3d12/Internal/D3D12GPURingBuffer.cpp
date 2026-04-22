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

#include "D3D12GPURingBuffer.hpp"

#include "../D3D12NativeDevice.hpp"

#include "D3D12Debug.hpp"
#include "D3D12Utils.hpp"


namespace D3D12 {
namespace Internal {

GPURingBuffer::GPURingBuffer(const NIPtr<NativeDevice>& nativeDevice)
    : RingBuffer(nativeDevice)
    , mGPUBufferResource()
{
}

GPURingBuffer::~GPURingBuffer()
{
    if (mGPUBufferResource)
    {
        mGPUBufferResource.Reset();
    }
}

bool GPURingBuffer::Init(size_t flushThreshold, size_t alignment, size_t size)
{
    if (!RingBuffer::Init(flushThreshold, alignment, size)) return false;

    // create the Resource which will represent our Ring Buffer
    D3D12_RESOURCE_DESC resourceDesc;
    D3D12NI_ZERO_STRUCT(resourceDesc);
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = static_cast<UINT64>(mSize);
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;

    // Ring Buffer is a CPU-accessible passthrough to GPU for small data,
    // hence it will always be an Upload Heap type resource
    D3D12_HEAP_PROPERTIES heapProps;
    D3D12NI_ZERO_STRUCT(heapProps);
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = mNativeDevice->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mGPUBufferResource));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create GPU Ring Buffer's Default Committed Resource");

    mGPUBufferResource->SetName(L"Ring Buffer Resource (GPU)");

    mGPUResourcePtr = mGPUBufferResource->GetGPUVirtualAddress();
    mGPUResourceState = D3D12_RESOURCE_STATE_COMMON;

    mChunkToTransferStart = 0;
    mChunkToTransferSize = 0;
    mLastReserveTail = 0;

    return true;
}

GPURingBuffer::GPURegion GPURingBuffer::ReserveCPU(size_t size)
{
    GPURegion result;

    result.cpuRegion = RingBuffer::Reserve(size);
    if (!result.cpuRegion) return GPURegion();

    // fill gpu region struct based on cpu region
    // we assume both buffers
    result.gpuRegion.cpu = 0;
    result.gpuRegion.size = result.cpuRegion.size;
    result.gpuRegion.offsetFromStart = result.cpuRegion.offsetFromStart;
    result.gpuRegion.gpu = mGPUResourcePtr + result.gpuRegion.offsetFromStart;

    if (mLastReserveTail > mTail)
    {
        // wrap around
        mChunkToTransferSize += (mSize - mLastReserveTail) + mTail;
    }
    else
    {
        mChunkToTransferSize += (mTail - mLastReserveTail);
    }

    mLastReserveTail = mTail;
    return result;
}

void GPURingBuffer::RecordTransferToGPU()
{
    // we assume Current Command List is empty, this should be called right after Pool::AdvanceCommandList()
    const NIPtr<RenderingContext>& context = mNativeDevice->GetRenderingContext();

    context->QueueResourceTransition(mGPUBufferResource, mGPUResourceState, D3D12_RESOURCE_STATE_COPY_DEST);
    context->SubmitResourceTransitions();

    if (mChunkToTransferStart + mChunkToTransferSize > mSize)
    {
        // double transfer - offset + size cross buffer boundaries
        // first, copy from current offset to end
        size_t remainder = mSize - mChunkToTransferStart;

        context->CopyBufferRegion(
            mGPUBufferResource, mChunkToTransferStart,
            mBufferResource, mChunkToTransferStart,
            remainder
        );

        // second, copy from beginning to the rest of data
        context->CopyBufferRegion(
            mGPUBufferResource, 0,
            mBufferResource, 0,
            mChunkToTransferSize - remainder
        );

        mChunkToTransferStart = mChunkToTransferSize - remainder;
    }
    else
    {
        // single transfer - offset + size doesn't cross buffer boundaries
        context->CopyBufferRegion(
            mGPUBufferResource, mChunkToTransferStart,
            mBufferResource, mChunkToTransferStart,
            mChunkToTransferSize
        );

        mChunkToTransferStart += mChunkToTransferSize;
        if (mChunkToTransferStart == mSize) mChunkToTransferStart = 0;
    }

    mChunkToTransferSize = 0;

    context->QueueResourceTransition(mGPUBufferResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    context->SubmitResourceTransitions();
    mGPUResourceState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

void GPURingBuffer::SetDebugName(const std::string& name)
{
    RingBuffer::SetDebugName(name);

    if (mGPUBufferResource)
    {
        mGPUBufferResource->SetName(Utils::ToWString("(GPU) " + name).c_str());
    }
}

} // namespace Internal
} // namespace D3D12
