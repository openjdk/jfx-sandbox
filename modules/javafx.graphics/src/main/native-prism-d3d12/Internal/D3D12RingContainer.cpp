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

#include "D3D12RingContainer.hpp"

#include "../D3D12NativeDevice.hpp"

#include "Internal/D3D12Utils.hpp"


namespace D3D12 {
namespace Internal {

void RingContainer::FlushCommandList()
{
    mNativeDevice->FlushCommandList();
    mNativeDevice->SignalMidFrame();
}

void RingContainer::CheckThreshold()
{
    if (mUsed > mFlushThreshold && !mNativeDevice->MidFrameSignaled())
    {
        FlushCommandList();
    }
}

bool RingContainer::AwaitNextCheckpoint()
{
    if (!mNativeDevice->MidFrameSignaled())
    {
        // we landed here because Ring Container couldn't allocate any more data
        // but the waitable is not set. This means either threshold is very
        // high, or we attempt to reserve large amount of data. BUT, we also
        // would not be here if the amount of space needed is too big. Try and
        // flush the buffer now and wait until it's done.
        FlushCommandList();
    }

    // await for the waitable set by FlushCommandList()
    bool waitSuccess = mNativeDevice->WaitMidFrame();
    if (!waitSuccess)
    {
        D3D12NI_LOG_WARN("Failed to wait on mid-frame waitable");
        return false;
    }

    return true;
}

RingContainer::RingContainer(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mSize(0)
    , mFlushThreshold(0)
    , mUsed(0)
    , mUncommitted(0)
    , mHead(0)
    , mTail(0)
{
    mNativeDevice->RegisterWaitableOperation(this);
    mDebugName = "Ring Container";
}

RingContainer::~RingContainer()
{
    mNativeDevice->UnregisterWaitableOperation(this);
    mNativeDevice.reset();

    D3D12NI_LOG_TRACE("%s destroyed", mDebugName.c_str());
}

bool RingContainer::InitInternal(size_t size, size_t flushThreshold)
{
    mSize = size;
    mFlushThreshold = (flushThreshold > size) ? size : flushThreshold;
    mUsed = mUncommitted = mHead = mTail = 0;
    return true;
}

RingContainer::Region RingContainer::ReserveInternal(size_t size, size_t alignment)
{
    // alignment has to be a power of two
    if ((alignment == 0) || (alignment & (alignment - 1)) != 0)
    {
        D3D12NI_LOG_ERROR("%s allocation alignment must be a power of two; was %ld", mDebugName.c_str(), alignment);
        return Region();
    }

    size = Utils::Align(size, alignment);

    if (size > mSize)
    {
        D3D12NI_LOG_ERROR("%s: Requested data too big: %ld", mDebugName.c_str(), size);
        return Region();
    }

    if (mUsed == mSize)
    {
        AwaitNextCheckpoint();
        if (mUsed == mSize)
        {
            D3D12NI_LOG_ERROR("%s fully allocated, cannot allocate %ld bytes (h: %ld t: %ld used: %ld size %ld)",
                mDebugName.c_str(), size, mHead, mTail, mUsed, mSize);
            return Region();
        }
    }

    if (mTail >= mHead)
    {
        // tail is past head, so we haven't "looped around" yet
        // figure out if we can still allocate, or do we have to loop
        if (mTail + size <= mSize)
        {
            // not crossing past buffer's total size, reserve and return
            size_t newTail = mTail + size;
            size_t allocSize = newTail - mTail;
            mUsed += allocSize;
            mUncommitted += allocSize;
            D3D12NI_ASSERT(mUsed <= mSize, "%s: Used is larger than size, probably underflowed (%ld vs %ld)", mDebugName.c_str(), mUsed, mSize);

            Region r(allocSize, mTail);
            mTail = newTail;
            CheckThreshold();
            return std::move(r);
        }
        else
        {
            if (size > mHead)
            {
                AwaitNextCheckpoint();
                if (size > mHead)
                {
                    D3D12NI_LOG_ERROR("%s: Ring Container too small to hold %ld data (h: %ld, t: %ld, size %ld)", mDebugName.c_str(), size, mHead, mTail, mSize);
                    return Region(0, 0);
                }
            }

            // loop-around - beginning of ring Container still has enough room
            size_t newTail = size;
            size_t allocSize = newTail + (mSize - mTail); // size + padding to the end of the buffer
            mUsed += allocSize;
            mUncommitted += allocSize;
            D3D12NI_ASSERT(mUsed <= mSize, "%s: Used is larger than size, probably underflowed (%ld vs %ld)", mDebugName.c_str(), mUsed, mSize);

            Region r(size, 0);
            mTail = newTail;
            CheckThreshold();
            return std::move(r);
        }
    }
    else if (mTail < mHead)
    {
        if (mTail + size >= mHead)
        {
            // Ring Container might overflow, pause and wait until last flush is done
            AwaitNextCheckpoint();
            // head could've moved behind us, so we should also check if that's the case.
            // If head is still ahead of us, double-check the condition before leaving
            if (mTail < mHead && mTail + size >= mHead)
            {
                D3D12NI_LOG_ERROR("%s: too small to hold %ld data (h: %ld, t: %ld, size %ld, used %ld)", mDebugName.c_str(), size, mHead, mTail, mSize, mUsed);
                return Region();
            }
        }

        // tail is before head but we have enough room to allocate the data
        size_t newTail = mTail + size;
        size_t allocSize = newTail - mTail; // size + padding to alignment
        mUsed += allocSize;
        mUncommitted += allocSize;
        D3D12NI_ASSERT(mUsed <= mSize, "%s: Used is larger than size, probably underflowed (%ld vs %ld)", mDebugName.c_str(), mUsed, mSize);

        Region r(allocSize, mTail);
        mTail = newTail;
        CheckThreshold();
        return std::move(r);
    }

    // Another confidence check, but we should never end up here
    D3D12NI_LOG_ERROR("%s: overflow - tried to allocate past head (h: %ld, t: %ld, size: %ld)", mDebugName.c_str(), mHead, mTail, size);
    return Region();
}

void RingContainer::OnQueueSignal(uint64_t fenceValue)
{
    if (mUncommitted > 0)
    {
        D3D12NI_LOG_DEBUG("%s: Queue signal, emplace checkpoint %u tail %u fence", mDebugName.c_str(), mTail, fenceValue);
        mCheckpoints.emplace_back(mTail, fenceValue);
        mUncommitted = 0;
    }
}

void RingContainer::OnFenceSignaled(uint64_t fenceValue)
{
    D3D12NI_LOG_DEBUG("%s: Fence signaled %u checkpoints %u", mDebugName.c_str(), fenceValue, mCheckpoints.size());

    while (!mCheckpoints.empty())
    {
        if (fenceValue < mCheckpoints.front().fenceValue)
        {
            // any remaining frames are not yet done
            break;
        }

        size_t frameTail = mCheckpoints.front().tail;

        // frameTail will be our new mHead - calculate new value of mUsed
        if (frameTail == mHead)
        {
            // corner-case - Ring Container got exactly 100% full.
            // mUsed can just drop to 0 in this case.
            mUsed = 0;
        }
        else if (frameTail > mHead)
        {
            // frame's tail is after head - not looped yet
            // subtract from frame's tail to old (current?) mHead
            D3D12NI_ASSERT(frameTail - mHead >= 0, "%s: Invalid frameTail - mHead %d", mDebugName.c_str(), frameTail - mHead);
            mUsed -= frameTail - mHead;
        }
        else
        {
            // frame's tail is before head - looped
            // subtract end-of-buffer data (size - mHeat) and beginning (frame's tail - 0)
            D3D12NI_ASSERT((mSize - mHead) + frameTail >= 0, "%s: Invalid size-head + tail %d", mDebugName.c_str(), (mSize - mHead) + frameTail);
            mUsed -= (mSize - mHead) + frameTail;
        }

        D3D12NI_ASSERT(mUsed <= mSize, "%s: Used is larger than size, probably underflowed (%ld vs %ld)", mDebugName.c_str(), mUsed, mSize);

        // update mHead to current frame's tail & discard frame checkpoint
        mHead = frameTail;
        mCheckpoints.pop_front();
    }
}

} // namespace Internal
} // namespace D3D12
