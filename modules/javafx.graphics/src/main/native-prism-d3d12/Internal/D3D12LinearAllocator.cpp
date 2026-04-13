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
    D3D12NI_LOG_TRACE("--- LinearAllocator - expanding with new chunk size %d (current chunks %d) ---", mSizePerChunk, mChunks.size());
    mChunks.emplace_back(mSizePerChunk);
    mCurrentChunk = &mChunks.back();
}

LinearAllocator::LinearAllocator()
    : mChunks()
    , mSizePerChunk(CHUNK_SIZE)
    , mUsedBeforeLastMove(0)
    , mCurrentChunk(nullptr)
    , mAllocatorMutex()
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
    // TODO: D3D12: A small optimization would be to avoid allocating a new chunk every frame
    // (and freeing the old one). Ideally we would hit a large enough chunk size that the entire
    // frame would fit in it, and then it would be reclaimed. However, since RenderingContext always
    // assumes there is always a Payload available for use, it will always hold the Chunk alive.
    // Investigate if it's even worth it.
    while (mUsedBeforeLastMove > mSizePerChunk)
    {
        mSizePerChunk += CHUNK_SIZE;
    }

    mUsedBeforeLastMove = 0;
    Expand();
}

void* LinearAllocator::Allocate(uint32_t size)
{
    std::unique_lock<std::mutex> lock(mAllocatorMutex);

    // align to 8 bytes (64-bits)
    uint32_t alignedSize = Utils::Align<uint32_t>(size, 8);

    if (!mCurrentChunk->Fits(alignedSize))
    {
        // mark we used a whole chunk
        // this will be used next MoveToNextChunk() to preallocate larger memory chunks
        mUsedBeforeLastMove += CHUNK_SIZE;
        Expand();
    }

    return mCurrentChunk->Reserve(alignedSize);
}

void LinearAllocator::Free(void* ptr)
{
    uint8_t* dataPtr = reinterpret_cast<uint8_t*>(ptr);
    DataHeader* header = reinterpret_cast<DataHeader*>(dataPtr - sizeof(DataHeader));

    if (header->magic != DATA_MAGIC)
    {
        D3D12NI_LOG_ERROR("Cannot free, invalid magic at data header");
        return;
    }

    // NOTE: Assumes this section is shared between RenderThread and MainThread
    // However, we know that MainThread is the only one doing Allocations, so it's the
    // only one interacting with mCurrentChunk.
    //
    // If we're freeing something from outside mCurrentChunk we can lift this lock, as
    // we know Main Thread won't ever free anything or allocate it on a different chunk
    // than the current one.
    {
        std::unique_lock<std::mutex> lock(mAllocatorMutex);

        if (header->parentChunk == mCurrentChunk)
        {
            mCurrentChunk->Free(ptr);

            if (mCurrentChunk->FullyFreed())
            {
                D3D12NI_LOG_TRACE("--- LinearAllocator - Reclaiming current chunk ---");
                mCurrentChunk->Reclaim();
            }

            return;
        }
    }

    // we know the freed ptr is not part of the current chunk, browse the other Chunks
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
                // relocking to not corrupt the Chunk list
                std::unique_lock<std::mutex> lock(mAllocatorMutex);
                D3D12NI_LOG_TRACE("--- LinearAllocator - Freeing chunk size %d (currently %d chunks) ---", c.mSize, mChunks.size());
                mChunks.erase(it);
            }

            return;
        }
    }

    D3D12NI_ASSERT(0, "Trying to free a pointer not belonging to the allocator, or something went wrong somewhere. This should not happen.");
}

} // namespace Internal
} // namespace D3D12
