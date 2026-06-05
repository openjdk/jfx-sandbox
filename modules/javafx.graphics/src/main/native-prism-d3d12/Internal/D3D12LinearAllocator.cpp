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

#include "D3D12LinearAllocator.hpp"

#include "D3D12Utils.hpp"


namespace D3D12 {
namespace Internal {

void LinearAllocator::Expand()
{
    D3D12NI_ASSERT(std::this_thread::get_id() == mInitThreadId, "Expand() can only be called by the initializing (main) thread");

    D3D12NI_LOG_TRACE("--- LinearAllocator - expanding with new chunk size %d (current chunks %d) ---", mSizePerChunk, mChunks.size());
    mChunks.emplace_back(mSizePerChunk);
    mCurrentChunk = &mChunks.back();
}

LinearAllocator::LinearAllocator()
    : mChunks()
    , mSizePerChunk(CHUNK_SIZE)
    , mCurrentChunk(nullptr)
    , mInitThreadId(std::this_thread::get_id())
{
    Expand();
}

LinearAllocator::~LinearAllocator()
{
}

// called when frame is finished; goal is to allocate a new large enough
// chunk to prevent further allocations
void LinearAllocator::MoveToNewChunk()
{
    Expand();
}

void* LinearAllocator::Allocate(uint32_t size)
{
    D3D12NI_ASSERT(std::this_thread::get_id() == mInitThreadId, "Allocate() can only be called by the initializing (main) thread");

    // align to 8 bytes (64-bits)
    uint32_t alignedSize = Utils::Align<uint32_t>(size, 8);

    if (!mCurrentChunk->Fits(alignedSize))
    {
        // increase chunk size
        mSizePerChunk += CHUNK_SIZE;

        // TODO: D3D12: This log could just be a TRACE log. After debugging issues change it back.
        D3D12NI_LOG_WARN("LinearAllocator: must expand! increasing used to %d", mSizePerChunk);
        Expand();
    }

    void* ret = mCurrentChunk->Reserve(alignedSize);
    return ret;
}

void LinearAllocator::Free(void* ptr)
{
    // NOTE: Below assertion is right 99.9999% of the time. The only exception is when
    // we close the application and cleanup leftover resources, ex. in ResourceManager.
    // Those will be done after RenderThread completed and we rejoined it though.
    // For cleaner exit messaging in DebugNative the assertion is commented out. Restore
    // it if there are threading issues to be debugged in this area.
    //D3D12NI_ASSERT(std::this_thread::get_id() != mInitThreadId, "Free() can only be called by the worker/render thread");

    uint8_t* dataPtr = reinterpret_cast<uint8_t*>(ptr);
    DataHeader* header = reinterpret_cast<DataHeader*>(dataPtr - sizeof(DataHeader));

    if (header->magic != DATA_MAGIC)
    {
        D3D12NI_LOG_ERROR("Cannot free, invalid magic at data header for pointer %p", ptr);
        assert(false); // LKDEBUG
        return;
    }

    Chunk* chunk = header->parentChunk; // preserving the pointer, Free() will destroy it
    chunk->Free(ptr);
    if (chunk->FullyFreed())
    {
        for (std::list<Chunk>::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
        {
            Chunk& c = *it;
            if (chunk == &c)
            {
                D3D12NI_LOG_TRACE("--- LinearAllocator - Freeing chunk size %d (currently %d chunks) ---", c.mSize, mChunks.size());
                mChunks.erase(it);
                return;
            }
        }

        D3D12NI_ASSERT(0, "Couldn't find parent chunk on the chunk list. This should not happen.");
    }

    return;
/*
    if (header->parentChunk == mCurrentChunk)
    {
        mCurrentChunk->Free(ptr);
        if (mCurrentChunk->FullyFreed())
        {
            D3D12NI_LOG_TRACE("--- LinearAllocator - Reclaiming current chunk size %d ---", mCurrentChunk->mSize);
            mCurrentChunk->Reclaim();
        }

        return;
    }

    // we know the freed ptr is not part of the current chunk, browse the other dormant Chunks
    // we store and free it from there. We do this the "long way" instead of just peeking
    // into header->parentChunk to make sure ptr was allocated by us.
    for (std::list<Chunk>::iterator it = mChunks.begin(); it != mChunks.end(); ++it)
    {
        Chunk& c = *it;
        if (header->parentChunk == &c)
        {
            c.Free(ptr);
            if (c.FullyFreed())
            {
                D3D12NI_LOG_TRACE("--- LinearAllocator - Freeing chunk size %d (currently %d chunks) ---", c.mSize, mChunks.size());
                mChunks.erase(it);
            }

            return;
        }
    }
*/
    D3D12NI_ASSERT(0, "Trying to free a pointer not belonging to the allocator, or something went wrong somewhere. This should not happen.");
}

} // namespace Internal
} // namespace D3D12
