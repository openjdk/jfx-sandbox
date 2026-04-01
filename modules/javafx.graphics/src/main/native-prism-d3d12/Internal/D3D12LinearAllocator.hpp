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

#include <memory>
#include <list>
#include <mutex>


namespace D3D12 {
namespace Internal {

/**
 * Fast linear allocator made to reduce CPU-side allocations. Used to
 * speed up creation of RenderPayload for RenderThread.
 */
class LinearAllocator
{
    static const size_t CHUNK_SIZE = 8192 * 1024;
    static const uint32_t DATA_MAGIC = 0xD3D12A11;

    struct Chunk;

    struct DataHeader
    {
        uint32_t magic;
        uint32_t size;
        Chunk* parentChunk;

        DataHeader(uint32_t size, Chunk* parent)
            : magic(DATA_MAGIC)
            , size(size)
            , parentChunk(parent)
        {}
    };

    struct Chunk
    {
        uint8_t* mPtr;
        size_t mSize;
        size_t mTaken;
        uint32_t mAllocations;

        Chunk(size_t size)
            : mPtr(reinterpret_cast<uint8_t*>(std::malloc(size)))
            , mSize(size)
            , mTaken(0)
            , mAllocations(0)
        {}

        ~Chunk()
        {
            std::free(mPtr);
        }

        bool Fits(uint32_t size)
        {
            return (size + sizeof(DataHeader)) < (mSize - mTaken);
        }

        bool FullyFreed()
        {
            return (mAllocations == 0);
        }

        void Reclaim()
        {
            D3D12NI_ASSERT(mAllocations == 0, "Reclaim should only be called when there is 0 allocations in Chunk");

            mTaken = 0;
        }

        // assumes there is enough room
        void* Reserve(uint32_t size)
        {
            D3D12NI_ASSERT(Fits(size), "Not enough room to reserve on Allocator's Chunk");

            void* head = mPtr + mTaken;
            void* ret = mPtr + mTaken + sizeof(DataHeader);

            mTaken += size + sizeof(DataHeader);
            ++mAllocations;

            new(head) DataHeader(size, this);
            return ret;
        }

        void Free(void* ptr)
        {
            D3D12NI_ASSERT(mAllocations > 0, "Chunk was already fully freed");

            uint8_t* dataPtr = reinterpret_cast<uint8_t*>(ptr);
            DataHeader* header = reinterpret_cast<DataHeader*>(dataPtr - sizeof(DataHeader));
            D3D12NI_ASSERT(header->magic == DATA_MAGIC, "Invalid Magic, this data pointer is not valid or belonging to us");
            D3D12NI_ASSERT(header->parentChunk == this, "Invalid parent chunk in data header");

            header->~DataHeader();
            mAllocations--;
        }
    };

    std::list<Chunk> mChunks;
    size_t mSizePerChunk;
    size_t mUsedBeforeLastMove;
    Chunk* mCurrentChunk;
    std::mutex mAllocatorMutex;

    void Expand();

public:
    LinearAllocator();
    ~LinearAllocator();

    void MoveToNewChunk();
    void* Allocate(uint32_t size);
    void Free(void* ptr);

    template <typename T, typename ...Args>
    T* Construct(Args&&... args)
    {
        T* ret = reinterpret_cast<T*>(Allocate(sizeof(T)));
        new(ret) T(std::forward<Args>(args)...);
        return ret;
    }
};

template <typename T>
class LinearAllocatorDeleter
{
    LinearAllocator* mAllocator;

public:
    LinearAllocatorDeleter(LinearAllocator* a)
        : mAllocator(a)
    {}

    void operator()(T* ptr) const
    {
        if (mAllocator)
        {
            ptr->~T();
            mAllocator->Free(ptr);
        }
    }
};

} // namespace Internal
} // namespace D3D12
