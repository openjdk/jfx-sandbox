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

#include "D3D12RenderThread.hpp"

#include "../D3D12NativeDevice.hpp"

#include "D3D12Config.hpp"
#include "D3D12Profiler.hpp"


namespace D3D12 {
namespace Internal {

// Thread

RenderThread::RenderThread(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mCommandQueue()
    , mState(nativeDevice)
{
}

bool RenderThread::Init()
{
    if (!mState.PSOManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize PSO Manager");
        return false;
    }

    if (!mState.resourceManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Resource Manager");
        return false;
    }

    return true;
}

void RenderThread::Execute(RenderPayloadPtr&& payload)
{
    RenderPayloadPtr curPayload(std::move(payload));

    // TODO move below to separate thread
    {
        // apply any resource changes the Payload brought us
        curPayload->ApplyResourceSteps(mState);

        switch (curPayload->GetType())
        {
        case RenderPayload::Type::GRAPHICS:
        {
            // go through ResourceManager and prepare the descriptors and shader constants
            mState.resourceManager.DeclareRingResources();
            if (!mState.resourceManager.PrepareResources())
            {
                D3D12NI_LOG_ERROR("Failed to prepare resources for recording Command List data");
                return;
            }
            break;
        }
        case RenderPayload::Type::COMPUTE:
        {
            mState.resourceManager.DeclareComputeRingResources();
            if (!mState.resourceManager.PrepareComputeResources())
            {
                D3D12NI_LOG_ERROR("Failed to prepare resources for recording Command List data");
                return;
            }
            break;
        }
        default:
            // assume this payload is not ending with a draw or a dispatch, no resources are needed
            break;
        }

        // record what is needed on the Command List
        const D3D12GraphicsCommandListPtr& commandList = mNativeDevice->GetCurrentCommandList();
        if (!commandList) return;

        curPayload->ApplySteps(commandList, mState);
    }
}

void RenderThread::WaitForCompletion()
{
    // TODO for now noop, will be filled in later
}

} // namespace Internal
} // namespace D3D12
