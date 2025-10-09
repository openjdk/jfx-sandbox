/*
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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
#include "Internal/D3D12Debug.hpp"
#include "Internal/D3D12Profiler.hpp"

#include <com_sun_prism_d3d12_ni_D3D12NativeSwapChain.h>
#include <string>


namespace D3D12 {

bool NativeSwapChain::GetSwapChainBuffers(UINT count)
{
    for (Internal::DescriptorData& rtv: mRTVs)
    {
        mNativeDevice->GetRTVDescriptorAllocator()->Free(rtv);
    }

    mBufferCount = count;

    if (mBufferCount != mTextureBuffers.size())
    {
        mTextureBuffers.resize(mBufferCount);
        mRTVs.resize(mBufferCount);
        mWaitFenceValues.resize(mBufferCount);
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    D3D12NI_ZERO_STRUCT(rtvDesc);
    rtvDesc.Format = mFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    std::wstring namePrefix(L"SwapChain Buffer #");
    for (UINT i = 0; i < mBufferCount; ++i)
    {
        D3D12ResourcePtr buffer;
        HRESULT hr = mSwapChain->GetBuffer(i, IID_PPV_ARGS(&buffer));
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to get SwapChain buffer");

        std::wstring name = namePrefix + std::to_wstring(i);
        hr = buffer->SetName(name.c_str());
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to name SwapChain buffer");

        mRTVs[i] = mNativeDevice->GetRTVDescriptorAllocator()->Allocate(1);
        if (!mRTVs[i])
        {
            D3D12NI_LOG_ERROR("Failed to allocate RTV for SwapChain buffer #%u", i);
            return false;
        }

        mNativeDevice->GetDevice()->CreateRenderTargetView(buffer.Get(), &rtvDesc, mRTVs[i].CPU(0));

        mWaitFenceValues[i] = 0;

        mTextureBuffers[i] = (std::make_shared<Internal::TextureBase>());
        mTextureBuffers[i]->Init(buffer, 1, D3D12_RESOURCE_STATE_COMMON);
    }

    return true;
}

NativeSwapChain::NativeSwapChain(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mSwapChain()
    , mTextureBuffers()
    , mRTVs()
    , mWaitFenceValues()
    , mSubmittedFrameCount(0)
    , mBufferCount(0)
    , mCurrentBufferIdx(0)
    , mDirtyRegion()
    , mFormat(DXGI_FORMAT_UNKNOWN)
    , mVSyncEnabled(true)
    , mSwapChainFlags(0)
    , mSwapInterval(1)
    , mPresentFlags(0)
    , mWidth(0)
    , mHeight(0)
    , mNullTexture()
{
    mNativeDevice->RegisterWaitableOperation(this);
    mProfilerSourceID = Internal::Profiler::Instance().RegisterSource("SwapChain");
}

NativeSwapChain::~NativeSwapChain()
{
    mNativeDevice->GetCheckpointQueue().WaitForNextCheckpoint(CheckpointType::ALL);
    // TODO: D3D12: this is placed here because of JDK-8342694
    // Otherwise it is not printed - move this to NativeDevice destructor when resolved
    mNativeDevice->GetCheckpointQueue().PrintStats();
    D3D12NI_ASSERT(mSubmittedFrameCount == 0, "SwapChain destructor: Failed to wait for all frames! Frame count = %u", mSubmittedFrameCount);

    Internal::Profiler::Instance().RemoveSource(mProfilerSourceID);
    mNativeDevice->UnregisterWaitableOperation(this);

    for (size_t i = 0; i < mTextureBuffers.size(); ++i)
    {
        mTextureBuffers[i].reset();
        mNativeDevice->GetRTVDescriptorAllocator()->Free(mRTVs[i]);
    }

    mTextureBuffers.clear();

    mSwapChain.Reset();
    mNativeDevice.reset();

    D3D12NI_LOG_DEBUG("SwapChain destroyed");
}

bool NativeSwapChain::Init(const DXGIFactoryPtr& factory, HWND hwnd)
{
    mVSyncEnabled = Internal::Config::IsVsyncEnabled();
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

    // DXGI injects its own Alt+Enter shortcut which switches to exclusive fullscreen mode
    // JFX already handles fullscreen on its own, this shortcut is not officially supported
    // by us and apps can already use it, so we want to disable it.
    hr = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to make necessary DXGI window associations");

    mFormat = desc.Format;

    if (!GetSwapChainBuffers(desc.BufferCount))
    {
        return false;
    }

    mSwapInterval = mVSyncEnabled ? 1 : 0;
    // TODO: D3D12: allowing tearing is required for supporting VRR displays - investigate if we should
    //              do this differently.
    // TODO: D3D12: Also note fullscreen related requirements
    mPresentFlags = mVSyncEnabled ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    D3D12_RESOURCE_DESC bufDesc = mTextureBuffers[0]->GetResource()->GetDesc();
    mWidth = static_cast<UINT>(bufDesc.Width);
    mHeight = static_cast<UINT>(bufDesc.Height);

    return true;
}

bool NativeSwapChain::Prepare(LONG left, LONG top, LONG right, LONG bottom)
{
    mDirtyRegion.left = left;
    mDirtyRegion.top = top;
    mDirtyRegion.right = right;
    mDirtyRegion.bottom = bottom;

    mNativeDevice->QueueTextureTransition(GetTexture(), D3D12_RESOURCE_STATE_PRESENT);
    mNativeDevice->SubmitTextureTransitions();

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

    HRESULT hr = mSwapChain->Present1(mSwapInterval, mPresentFlags, &params);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to Present on Swap Chain");

    // NOTE: We fetch fenceValue here instead in OnQueueSignal() to cover multiple-Stage
    // scenario. Each Stage has its own SwapChain and each SwapChain must know when it was
    // actually signaling the Queue. We have no way of knowing that from OnQueueSignal().
    // CloseWindowTest system test is a good confidence check for that case.
    Internal::Profiler::Instance().MarkEvent(mProfilerSourceID, Internal::Profiler::Event::Signal);
    uint64_t fenceValue = mNativeDevice->Signal(CheckpointType::ENDFRAME);
    if (fenceValue == 0)
    {
        D3D12NI_LOG_ERROR("Failed to Signal after Present");
        return false;
    }

    mWaitFenceValues[mCurrentBufferIdx] = fenceValue;
    mSubmittedFrameCount++;

    // await older frames
    while (mSubmittedFrameCount >= mBufferCount)
    {
        Internal::Profiler::Instance().MarkEvent(mProfilerSourceID, Internal::Profiler::Event::Wait);
        if (!mNativeDevice->GetCheckpointQueue().WaitForNextCheckpoint(CheckpointType::ENDFRAME))
        {
            D3D12NI_LOG_ERROR("Failed to wait for old frame to complete");
            return false;
        }
    }

    mCurrentBufferIdx = mSwapChain->GetCurrentBackBufferIndex();

    return true;
}

bool NativeSwapChain::Resize(UINT width, UINT height)
{
    // before Resize we need to wait for all frames
    mNativeDevice->GetCheckpointQueue().WaitForNextCheckpoint(CheckpointType::ALL);

    // since all frames were completed, reset all Buffer references before resizing
    for (size_t i = 0; i < mTextureBuffers.size(); ++i)
    {
        mTextureBuffers[i].reset();
    }

    HRESULT hr = mSwapChain->ResizeBuffers(mBufferCount, width, height, DXGI_FORMAT_UNKNOWN, mSwapChainFlags);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to resize SwapChain buffers");

    if (!GetSwapChainBuffers(mBufferCount))
    {
        return false;
    }

    mCurrentBufferIdx = mSwapChain->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_DESC desc = mTextureBuffers[0]->GetResource()->GetDesc();
    mWidth = static_cast<UINT>(desc.Width);
    mHeight = static_cast<UINT>(desc.Height);

    return true;
}

void NativeSwapChain::OnQueueSignal(uint64_t fenceValue)
{
    // noop
}

void NativeSwapChain::OnFenceSignaled(uint64_t fenceValue)
{
    for (size_t i = 0; i < mWaitFenceValues.size(); ++i)
    {
        if (mWaitFenceValues[i] == fenceValue)
        {
            mWaitFenceValues[i] = 0;
            mSubmittedFrameCount--;
            break;
        }
    }
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

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->GetWidth());
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeSwapChain_nGetHeight
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return false;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeSwapChain>(ptr)->GetHeight());
}

#ifdef __cplusplus
}
#endif
