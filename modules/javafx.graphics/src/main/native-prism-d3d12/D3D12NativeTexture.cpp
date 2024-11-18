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

#include "D3D12NativeTexture.hpp"

#include "D3D12NativeDevice.hpp"

#include <com_sun_prism_d3d12_ni_D3D12NativeTexture.h>


namespace D3D12 {

uint64_t NativeTexture::textureCounter = 0;
uint64_t NativeTexture::depthTextureCounter = 0;
uint64_t NativeTexture::rttextureCounter = 0;

bool NativeTexture::InitInternal(const D3D12_RESOURCE_DESC1& desc)
{
    mResourceDesc = desc;

    D3D12_HEAP_PROPERTIES heapProps;
    D3D12NI_ZERO_STRUCT(heapProps);
    // Texture resources require DEFAULT heap type
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = mNativeDevice->GetDevice()->CreateCommittedResource2(&heapProps, D3D12_HEAP_FLAG_NONE,
        &mResourceDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, nullptr, IID_PPV_ARGS(&mTextureResource));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Texture's Committed Resource");

    D3D12NI_ZERO_STRUCT(mSRVDesc);
    mSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mSRVDesc.Format = mResourceDesc.Format;
    mSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    mSRVDesc.Texture2D.MipLevels = 1;
    mSRVDesc.Texture2D.MostDetailedMip = 0;
    mSRVDesc.Texture2D.PlaneSlice = 0;
    mSRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    // Texture will be separately loaded with data via Java's Texture.update() calls
    // Fill in remaining members and leave
    mState = D3D12_RESOURCE_STATE_COMMON;

    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        mDebugName = L"RTTexture_#";
        mDebugName += std::to_wstring(rttextureCounter++);
        mTextureResource->SetName(mDebugName.c_str());
    }
    else if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        mDebugName = L"DepthTexture_#";
        mDebugName += std::to_wstring(depthTextureCounter++);
        mTextureResource->SetName(mDebugName.c_str());
    }
    else
    {
        mDebugName = L"Texture_#";
        mDebugName += std::to_wstring(textureCounter++);
        mTextureResource->SetName(mDebugName.c_str());
    }

    D3D12NI_LOG_TRACE("--- Texture %S created (%ux%u format %s %uxMSAA) ---",
        mDebugName.c_str(), mResourceDesc.Width, mResourceDesc.Height, Internal::DXGIFormatToString(mResourceDesc.Format), mResourceDesc.SampleDesc.Count);

    return true;
}

NativeTexture::NativeTexture(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mTextureResource(nullptr)
    , mResourceDesc()
    , mState(D3D12_RESOURCE_STATE_COMMON)
    , mSRVDesc()
{
}

NativeTexture::~NativeTexture()
{
    mNativeDevice->MarkResourceDisposed(mTextureResource);
    mNativeDevice.reset();

    if (mTextureResource)
    {
        // Trace log only if we actually allocated the resource
        // with mBufferResource being null we never called Init (or it failed)
        D3D12NI_LOG_TRACE("--- Texture %S destroyed (%ux%u format %s %uxMSAA) ---",
            mDebugName.c_str(), mResourceDesc.Width, mResourceDesc.Height, Internal::DXGIFormatToString(mResourceDesc.Format), mResourceDesc.SampleDesc.Count);
    }
}

bool NativeTexture::Init(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, TextureUsage usage, int samples, bool useMipmap)
{
    if (useMipmap)
    {
        D3D12NI_LOG_WARN("Mipmaps not yet implemented");
        useMipmap = false;
    }

    if (width <= 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        height <= 0 || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        D3D12NI_LOG_ERROR("Invalid width and/or height");
        return false;
    }

    D3D12_RESOURCE_DESC1 desc;
    D3D12NI_ZERO_STRUCT(desc);
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.Flags = flags;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.SampleDesc.Count = samples;
    desc.SampleDesc.Quality = 0;

    return InitInternal(desc);
}

UINT64 NativeTexture::GetSize()
{
    if (!mTextureResource) return -1;

    D3D12_RESOURCE_DESC1 resDesc;
    resDesc = mTextureResource->GetDesc1();

    return resDesc.Width * resDesc.Height * resDesc.DepthOrArraySize * GetDXGIFormatBPP(resDesc.Format);
}

bool NativeTexture::Resize(UINT width, UINT height)
{
    if (width == mResourceDesc.Width && height == mResourceDesc.Height) return true;

    mResourceDesc.Width = width;
    mResourceDesc.Height = height;

    mNativeDevice->MarkResourceDisposed(mTextureResource);

    return InitInternal(mResourceDesc);
}

void NativeTexture::WriteToDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& descriptorCpu)
{
    mNativeDevice->GetDevice()->CreateShaderResourceView(mTextureResource.Get(), &mSRVDesc, descriptorCpu);
}

void NativeTexture::EnsureState(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES newState)
{
    if (newState == mState) return;

    D3D12_RESOURCE_BARRIER barrier;
    D3D12NI_ZERO_STRUCT(barrier);
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = mTextureResource.Get();
    barrier.Transition.StateBefore = mState;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    mState = newState;
}

} // namespace D3D12


#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeTexture_nReleaseNativeObject
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

    D3D12::FreeNIObject<D3D12::NativeTexture>(ptr);
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeTexture_nGetSize
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return -1;

    return static_cast<jlong>(D3D12::GetNIObject<D3D12::NativeTexture>(ptr)->GetSize());
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeTexture_nGetWidth
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return -1;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeTexture>(ptr)->GetWidth());
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeTexture_nGetHeight
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return -1;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeTexture>(ptr)->GetHeight());
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeTexture_nResize
    (JNIEnv* env, jobject obj, jlong ptr, jint width, jint height)
{
    if (!ptr) return false;
    if (width <= 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) return false;
    if (height <= 0 || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) return false;

    return static_cast<jboolean>(D3D12::GetNIObject<D3D12::NativeTexture>(ptr)->Resize(static_cast<UINT>(width), static_cast<UINT>(height)));
}

#ifdef __cplusplus
}
#endif
