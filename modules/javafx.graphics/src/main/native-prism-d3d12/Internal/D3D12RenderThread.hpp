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

#include "../D3D12NativeTexture.hpp"

#include "D3D12CommandListPool.hpp"
#include "D3D12IRenderTarget.hpp"
#include "D3D12LinearAllocator.hpp"
#include "D3D12Matrix.hpp"
#include "D3D12RenderPayload.hpp"
#include "D3D12PSOManager.hpp"

#include <unordered_set>
#include <forward_list>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>


namespace D3D12 {
namespace Internal {

// processes RenderPayload objects on separate thread
class RenderThread
{
    NIPtr<NativeDevice> mNativeDevice;
    RenderThreadState mState;
    RenderPayloadPtr mNullPayload; // used to leave FetchPayload() without returning a temporary ref

    std::condition_variable mPayloadAvailableCV;
    std::condition_variable mQueueEmptyCV;
    std::mutex mPayloadQueueMutex;
    std::queue<RenderPayloadPtr> mPayloadQueue;
    std::atomic<bool> mWorkerDone;
    std::thread mWorkerThread;

    RenderPayloadPtr& FetchPayload();
    void WorkerMain();

public:
    RenderThread(const NIPtr<NativeDevice>& nativeDevice);
    ~RenderThread() = default;

    bool Init();
    void Execute(RenderPayloadPtr&& payload);
    void WaitForCompletion();
    D3D12GraphicsCommandListPtr FinalizeCommandList(LinearAllocator& allocator, RenderPayloadPtr&& payload);
    void Exit();

    inline void AdvanceCommandAllocator()
    {
        mState.commandListPool.AdvanceAllocator();
    }
};

} // namespace Internal
} // namespace D3D12
