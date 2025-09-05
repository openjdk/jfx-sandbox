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

#include "D3D12NativeRenderTarget.hpp"

#include "D3D12NativeDevice.hpp"

#include <com_sun_prism_d3d12_ni_D3D12NativeRenderTarget.h>


namespace D3D12 {

NativeRenderTarget::NativeRenderTarget(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , mTexture(nullptr)
    , mDescriptors()
    , mWidth(0)
    , mHeight(0)
{
}

NativeRenderTarget::~NativeRenderTarget()
{
    if (mDescriptors)
    {
        mNativeDevice->GetRTVDescriptorAllocator()->Free(mDescriptors);
    }

    if (mDSVDescriptor)
    {
        mNativeDevice->GetDSVDescriptorAllocator()->Free(mDSVDescriptor);
    }

    mTexture.reset();
    mDepthTexture.reset();
    mNativeDevice.reset();

    D3D12NI_LOG_TRACE("--- RenderTarget destroyed (%ux%u) ---", mWidth, mHeight);
}

bool NativeRenderTarget::Init(const NIPtr<NativeTexture>& texture)
{
    mTexture = texture;
    mTextureBase = mTexture;
    mDescriptors = mNativeDevice->GetRTVDescriptorAllocator()->Allocate(1);

    return Refresh();
}

bool NativeRenderTarget::EnsureHasDepthBuffer()
{
    // if Render Target needs depth testing, we must create a resource which will be our depth buffer
    // note that if it's already created we don't have to do anything
    if (mDepthTexture) return true;

    mDepthTexture = std::make_shared<NativeTexture>(mNativeDevice);
    if (!mDepthTexture->Init(static_cast<int>(mWidth), static_cast<int>(mHeight), DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
            TextureUsage::STATIC, TextureWrapMode::CLAMP_NOT_NEEDED, mTexture->GetMSAASamples(), false))
    {
        D3D12NI_LOG_ERROR("Failed to create Depth Texture");
        return false;
    }

    mDepthTextureBase = mDepthTexture;

    mDSVDescriptor = mNativeDevice->GetDSVDescriptorAllocator()->Allocate(1);
    if (!mDSVDescriptor)
    {
        D3D12NI_LOG_ERROR("Failed to allocate DSV descriptor for Depth Buffer");
        return false;
    }

    Refresh();

    // Schedule a clear to initialize this Depth Buffer so it doesn't contain garbage
    mNativeDevice->QueueTextureTransition(mDepthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    mNativeDevice->SubmitTextureTransitions();
    mNativeDevice->GetCurrentCommandList()->ClearDepthStencilView(mDSVDescriptor.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12NI_LOG_TRACE("--- RenderTarget %s uses depth texture %s ---", mTexture->GetName().c_str(), mDepthTexture->GetName().c_str());
    return true;
}

bool NativeRenderTarget::Refresh()
{
    D3D12_RENDER_TARGET_VIEW_DESC desc;
    D3D12NI_ZERO_STRUCT(desc);
    desc.Format = mTexture->GetFormat();
    desc.ViewDimension = mTexture->GetMSAASamples() > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = 0;
    desc.Texture2D.PlaneSlice = 0;

    mNativeDevice->GetDevice()->CreateRenderTargetView(mTexture->GetResource().Get(), &desc, mDescriptors.CPU(0));

    mWidth = mTexture->GetWidth();
    mHeight = mTexture->GetHeight();

    if (mDepthTexture)
    {
        // resize Depth Bufer
        mDepthTexture->Resize(static_cast<UINT>(mWidth), static_cast<UINT>(mHeight));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        D3D12NI_ZERO_STRUCT(dsvDesc);
        dsvDesc.ViewDimension = mDepthTexture->GetMSAASamples() > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.Texture2D.MipSlice = 0;
        mNativeDevice->GetDevice()->CreateDepthStencilView(mDepthTexture->GetResource().Get(), &dsvDesc, mDSVDescriptor.cpu);
    }

    return true;
}

void NativeRenderTarget::SetDepthTestEnabled(bool enabled)
{
    mDepthTestEnabled = enabled;

    if (mDepthTestEnabled) EnsureHasDepthBuffer();
}

} // namespace D3D12


#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeRenderTarget_nReleaseNativeObject
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

    D3D12::FreeNIObject<D3D12::NativeRenderTarget>(ptr);
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeRenderTarget_nGetWidth
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return 0;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeRenderTarget>(ptr)->GetWidth());
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeRenderTarget_nGetHeight
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return 0;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeRenderTarget>(ptr)->GetHeight());
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeRenderTarget_nRefresh
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return 0;

    return D3D12::GetNIObject<D3D12::NativeRenderTarget>(ptr)->Refresh();
}

#ifdef __cplusplus
}
#endif
