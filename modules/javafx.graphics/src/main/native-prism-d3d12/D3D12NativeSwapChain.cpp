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

#include "D3D12NativeSwapChain.hpp"

#include "D3D12NativeDevice.hpp"
#include "Internal/D3D12Config.hpp"

#include <com_sun_prism_d3d12_ni_D3D12NativeSwapChain.h>
#include <string>


namespace D3D12 {

bool NativeSwapChain::GetSwapChainBuffers(UINT count)
{
    mBufferCount = count;

    if (mBufferCount != mBuffers.size())
    {
        mBuffers.resize(mBufferCount);
        mStates.resize(mBufferCount);
    }

    std::wstring namePrefix(L"SwapChain Buffer #");
    for (UINT i = 0; i < mBufferCount; ++i)
    {
        HRESULT hr = mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBuffers[i]));
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to get SwapChain buffer");

        std::wstring name = namePrefix + std::to_wstring(i);
        hr = mBuffers[i]->SetName(name.c_str());
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to name SwapChain buffer");
    }

    return true;
}

NativeSwapChain::NativeSwapChain(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mSwapChain()
    , mBuffers()
    , mStates()
    , mCurrentBufferIdx(0)
    , mDirtyRegion()
    , mFormat(DXGI_FORMAT_UNKNOWN)
    , mVSyncEnabled(true)
    , mSwapChainFlags(0)
    , mSwapInterval(1)
    , mPresentFlags(0)
    , mWidth(0)
    , mHeight(0)
{
}

NativeSwapChain::~NativeSwapChain()
{
    NIPtr<Internal::Waitable> frameWaitable;
    while (!mPastFrameWaitables.empty())
    {
        frameWaitable = std::move(mPastFrameWaitables.front());
        mPastFrameWaitables.pop_front();
        frameWaitable->Wait();
    }

    for (size_t i = 0; i < mBuffers.size(); ++i)
    {
        mBuffers[i].Reset();
    }

    mBuffers.clear();

    mSwapChain.Reset();
    mNativeDevice.reset();

    D3D12NI_LOG_DEBUG("SwapChain destroyed");
}

bool NativeSwapChain::Init(const DXGIFactoryPtr& factory, HWND hwnd)
{
    mVSyncEnabled = Internal::Config::Instance().IsVsyncEnabled();
    mSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    DXGI_SWAP_CHAIN_DESC1 desc;
    D3D12NI_ZERO_STRUCT(desc);
    desc.Width = 0; // TODO: D3D12: - for now it's taken from HWND
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = mSwapChainFlags;

    // NOTE: Technically we could make SwapChain buffers multi-sampled here and let the
    // SwapChain handle resolving it for us, but it is not recommended. There are two
    // reasons why Microsoft advises against this:
    //   - Calling ResolveSubresource() explicitly gives us more control over the pipeline
    //   - UWP does not support it (although we don't use UWP in JFX)
    // Since this is the recommendation and we have to use an offscreen RTT for dirty
    // region opts anyway, we might as well follow it with very little effort added.
    //
    // See official MSAA sample code for more details:
    //   https://github.com/microsoft/Xbox-ATG-Samples/blob/main/PCSamples/IntroGraphics/SimpleMSAA_PC12/SimpleMSAA_PC12.cpp#L42
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    Ptr<IDXGISwapChain1> tmpSwapchain;
    HRESULT hr = factory->CreateSwapChainForHwnd(mNativeDevice->GetCommandQueue().Get(), hwnd, &desc, nullptr, nullptr, &tmpSwapchain);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create SwapChain");

    hr = tmpSwapchain.As(&mSwapChain);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to up-version SwapChain");

    if (!GetSwapChainBuffers(desc.BufferCount))
    {
        return false;
    }

    mFormat = desc.Format;
    mSwapInterval = mVSyncEnabled ? 1 : 0;
    // TODO: D3D12: allowing tearing is required for supporting VRR displays - investigate if we should
    //              do this differently.
    // TODO: D3D12: Also note fullscreen related requirements
    mPresentFlags = mVSyncEnabled ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    D3D12_RESOURCE_DESC bufDesc = mBuffers[0]->GetDesc();
    mWidth = static_cast<UINT>(bufDesc.Width);
    mHeight = static_cast<UINT>(bufDesc.Height);

    return true;
}

void NativeSwapChain::EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState)
{
    if (newState == mStates[mCurrentBufferIdx]) return;

    D3D12_RESOURCE_BARRIER barrier;
    D3D12NI_ZERO_STRUCT(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mBuffers[mCurrentBufferIdx].Get();
    barrier.Transition.StateBefore = mStates[mCurrentBufferIdx];
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    mStates[mCurrentBufferIdx] = newState;
}

bool NativeSwapChain::Prepare(LONG left, LONG top, LONG right, LONG bottom)
{
    mDirtyRegion.left = left;
    mDirtyRegion.top = top;
    mDirtyRegion.right = right;
    mDirtyRegion.bottom = bottom;
    return true;
}

bool NativeSwapChain::Present()
{
    DXGI_PRESENT_PARAMETERS params;
    D3D12NI_ZERO_STRUCT(params);

    if (mDirtyRegion.left >= 0 && mDirtyRegion.top >= 0 &&
        mDirtyRegion.right >= 0 && mDirtyRegion.bottom >= 0)
    {
        params.DirtyRectsCount = 1;
        params.pDirtyRects = &mDirtyRegion;
    }

    NIPtr<Internal::Waitable> oldWaitable;
    // await older frames
    if (mPastFrameWaitables.size() >= mBufferCount)
    {
        oldWaitable = std::move(mPastFrameWaitables.front());
        mPastFrameWaitables.pop_front();

        if (!oldWaitable->Wait())
        {
            D3D12NI_LOG_ERROR("Failed to wait for old frame to complete");
            return false;
        }
    }

    HRESULT hr = mSwapChain->Present1(mSwapInterval, mPresentFlags, &params);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to Present on Swap Chain");

    NIPtr<Internal::Waitable> waitable = mNativeDevice->Signal();
    // store current waitable onto our collection, we'll wait for them some other time
    mPastFrameWaitables.push_back(std::move(waitable));

    mCurrentBufferIdx = mSwapChain->GetCurrentBackBufferIndex();

    return true;
}

bool NativeSwapChain::Resize(UINT width, UINT height)
{
    // before Resize we need to wait for all frames
    NIPtr<Internal::Waitable> waitable;
    while (!mPastFrameWaitables.empty())
    {
        waitable = std::move(mPastFrameWaitables.front());
        mPastFrameWaitables.pop_front();

        if (!waitable->Wait())
        {
            D3D12NI_LOG_ERROR("Failed to wait for frames to complete");
            return false;
        }
    }

    // since all frames were completed, reset all Buffer references before resizing
    for (size_t i = 0; i < mBuffers.size(); ++i)
    {
        mBuffers[i].Reset();
    }

    HRESULT hr = mSwapChain->ResizeBuffers(mBufferCount, width, height, DXGI_FORMAT_UNKNOWN, mSwapChainFlags);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to resize SwapChain buffers");

    if (!GetSwapChainBuffers(mBufferCount))
    {
        return false;
    }

    mCurrentBufferIdx = mSwapChain->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_DESC desc = mBuffers[0]->GetDesc();
    mWidth = static_cast<UINT>(desc.Width);
    mHeight = static_cast<UINT>(desc.Height);

    return true;
}

} // namespace D3D12


#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nReleaseNativeObject
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

    D3D12::FreeNIObject<D3D12::NativeSwapChain>(ptr);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nPrepare
    (JNIEnv* env, jobject obj, jlong ptr, jlong left, jlong top, jlong right, jlong bottom)
{
    if (!ptr) return false;

    return D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->Prepare(
        static_cast<LONG>(left),
        static_cast<LONG>(top),
        static_cast<LONG>(right),
        static_cast<LONG>(bottom)
    );
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nPresent
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return false;

    return D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->Present();
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nResize
    (JNIEnv* env, jobject obj, jlong ptr, jint width, jint height)
{
    if (!ptr) return false;
    if (width < 0 || height < 0) return false;

    return D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->Resize(static_cast<UINT>(width), static_cast<UINT>(height));
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nGetWidth
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return false;

    return D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->GetWidth();
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nGetHeight
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return false;

    return D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->GetHeight();
}

#ifdef __cplusplus
}
#endif
