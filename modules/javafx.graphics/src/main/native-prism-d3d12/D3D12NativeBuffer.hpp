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

#pragma once

#include "D3D12Common.hpp"


namespace D3D12 {

class NativeBuffer
{
    static uint64_t counter;

    NIPtr<NativeDevice> mNativeDevice;
    D3D12ResourcePtr mBufferResource;
    size_t mSize;
    D3D12_HEAP_TYPE mHeapType;
    std::wstring mDebugName;

public:
    NativeBuffer(const NIPtr<NativeDevice>& nativeDevice);
    ~NativeBuffer();

    /**
     * Creates the Buffer and fills it with data if needed.
     *
     * If the buffer is supposed to be preinitialized with data, @p data is necessary.
     * Otherwise, it should be nullptr.
     *
     * @p size is in bytes.
     *
     * @p heapType determines on which heap the buffer will be allocated and can have
     * performance implications.
     *
     * @p finalState only matters when @p cpuWriteable is false - otherwise the Buffer is
     * allocated on Upload Heap which only allows GENERIC_READ state (which also is the only
     * state it can be initialized in).
     */
    bool Init(const void* data, size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES finalState);

    /**
     * Map D3D12 resource to CPU-visible memory. Returns a pointer to Resource's memory.
     *
     * After use, Unmap() must be called.
     */
    void* Map();

    /**
     * Unmap D3D12 resource from CPU-visible memory.
     */
    void Unmap();

    inline D3D12_GPU_VIRTUAL_ADDRESS GetGPUPtr() const
    {
        return mBufferResource->GetGPUVirtualAddress();
    }

    inline const D3D12ResourcePtr& GetResource() const
    {
        return mBufferResource;
    }

    inline size_t Size() const
    {
        return mSize;
    }
};

} // namespace D3D12
