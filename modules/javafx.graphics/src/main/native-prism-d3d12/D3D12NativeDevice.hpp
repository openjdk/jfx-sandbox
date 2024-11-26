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

#pragma once

#include "D3D12Common.hpp"

#include "D3D12NativeBuffer.hpp"
#include "D3D12NativeMesh.hpp"
#include "D3D12NativeMeshView.hpp"
#include "D3D12NativePhongMaterial.hpp"
#include "D3D12NativeRenderTarget.hpp"
#include "D3D12NativeShader.hpp"
#include "D3D12NativeSwapChain.hpp"
#include "D3D12NativeTexture.hpp"

#include "Internal/D3D12CommandListPool.hpp"
#include "Internal/D3D12DescriptorHeap.hpp"
#include "Internal/D3D12IWaitableOperation.hpp"
#include "Internal/D3D12ResourceDisposer.hpp"
#include "Internal/D3D12RenderingContext.hpp"
#include "Internal/D3D12RingBuffer.hpp"
#include "Internal/D3D12ShaderLibrary.hpp"
#include "Internal/D3D12Waitable.hpp"

#include "Internal/D3D12Matrix.hpp"
#include "Internal/JNIBuffer.hpp"
#include "Internal/MemoryView.hpp"

#include <vector>
#include <memory>


namespace D3D12 {

class NativeDevice: public std::enable_shared_from_this<NativeDevice>
{
    IDXGIAdapter1* mAdapter;
    D3D12DevicePtr mDevice;
    D3D12CommandQueuePtr mCommandQueue;
    D3D12FencePtr mFence;
    unsigned int mFenceValue;
    unsigned int mFrameCounter; // for debugging ex. triggering a breakpoint after X frames
    std::vector<Internal::IWaitableOperation*> mWaitableOps;

    NIPtr<Internal::RenderingContext> mRenderingContext;
    NIPtr<Internal::ResourceDisposer> mResourceDisposer;
    NIPtr<Internal::DescriptorHeap> mRTVHeap;
    NIPtr<Internal::DescriptorHeap> mDSVHeap;
    NIPtr<Internal::ShaderLibrary> mShaderLibrary;
    NIPtr<Internal::Shader> mPassthroughVS;
    NIPtr<Internal::Shader> mPhongVS;
    NIPtr<Internal::CommandListPool> mCommandListPool;
    NIPtr<NativeBuffer> m2DIndexBuffer;
    NIPtr<Internal::RingBuffer> mRingBuffer; // used for larger data (ex. 2D Vertex Buffer, texture upload)
    NIPtr<Internal::RingBuffer> mConstantRingBuffer; // used purely for CBuffers for Shaders
    NIPtr<Internal::Waitable> mMidFrameWaitable; // TODO: D3D12: This is constantly (de)allocated, avoid that.

    bool Build2DIndexBuffer();
    void AssembleVertexData(void* buffer, const Internal::MemoryView<float>& vertices,
                            const Internal::MemoryView<signed char>& colors, UINT elementCount);
    const NIPtr<Internal::InternalShader>& GetPhongPixelShader(const PhongShaderSpec& spec) const;

public:
    NativeDevice();
    ~NativeDevice();

    bool Init(IDXGIAdapter1* adapter, const NIPtr<Internal::ShaderLibrary>& shaderLibrary);
    void ReleaseInternals();

    // separate, internal buffer creator
    // used for 2D Index Buffer and internals of Mesh
    NIPtr<NativeBuffer> CreateBuffer(const void* initialData, size_t size, bool cpuWriteable, D3D12_RESOURCE_STATES finalState);

    bool CheckFormatSupport(DXGI_FORMAT format);
    NIPtr<NativeMesh>* CreateMesh();
    NIPtr<NativeMeshView>* CreateMeshView(const NIPtr<NativeMesh>& mesh);
    NIPtr<NativePhongMaterial>* CreatePhongMaterial();
    NIPtr<NativeRenderTarget>* CreateRenderTarget(const NIPtr<NativeTexture>& texture);
    NIPtr<NativeShader>* CreateShader(const std::string& name, void* buf, UINT size);
    NIPtr<NativeTexture>* CreateTexture(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, TextureUsage usage, int samples, bool useMipmap);
    int GetMaximumMSAASampleSize(DXGI_FORMAT format) const;
    int GetMaximumTextureSize() const;
    void MarkResourceDisposed(const D3D12ResourcePtr& texture);

    void Clear(float r, float g, float b, float a);
    void ClearTextureUnit(uint32_t unit);
    void CopyToSwapChain(const NIPtr<NativeSwapChain>& dst, const NIPtr<NativeTexture>& src);
    void ResolveToSwapChain(const NIPtr<NativeSwapChain>& dst, const NIPtr<NativeTexture>& src);
    void RenderQuads(const Internal::MemoryView<float>& vertices, const Internal::MemoryView<signed char>& colors,
                     UINT elementCount);
    void RenderMeshView(const NIPtr<NativeMeshView>& meshView);
    void SetCompositeMode(CompositeMode mode);
    void SetPixelShader(const NIPtr<NativeShader>& ps);
    void SetRenderTarget(const NIPtr<NativeRenderTarget>& target, bool enableDepthTest);
    void SetScissor(bool enabled, int x1, int y1, int x2, int y2);
    bool SetShaderConstants(const NIPtr<NativeShader>& shader, const std::string& name, const void* data, size_t size);
    void SetTexture(uint32_t unit, const NIPtr<NativeTexture>& texture);
    void SetCameraPos(const Coords_XYZW_FLOAT& pos);
    void SetWorldTransform(const Internal::Matrix<float>& matrix);
    void SetViewProjTransform(const Internal::Matrix<float>& matrix);
    bool ReadTexture(const NIPtr<NativeTexture>& texture, void* buffer, size_t pixelCount,
                     UINT srcx, UINT srcy, UINT srcw, UINT srch);
    bool UpdateTexture(const NIPtr<NativeTexture>& texture, const void* data, size_t pixelCount, PixelFormat srcFormat,
                       UINT dstx, UINT dsty, UINT srcx, UINT srcy, UINT srcw, UINT srch, UINT srcstride);

    void FinishFrame();
    void FlushCommandList();
    void Execute(const std::vector<ID3D12CommandList*>& commandLists);
    NIPtr<Internal::Waitable> Signal();
    void AdvanceCommandAllocator();
    void RegisterWaitableOperation(Internal::IWaitableOperation* waitableOp);
    void UnregisterWaitableOperation(Internal::IWaitableOperation* waitableOp);

    // for classes that need to flush data mid-frame ex. Ring Buffer
    void SignalMidFrame();
    bool WaitMidFrame();
    inline bool MidFrameSignaled() const
    {
        return (bool)mMidFrameWaitable;
    }

    const D3D12DevicePtr& GetDevice()
    {
        return mDevice;
    }

    const D3D12CommandQueuePtr& GetCommandQueue()
    {
        return mCommandQueue;
    }

    const D3D12GraphicsCommandListPtr& GetCurrentCommandList()
    {
        return mCommandListPool->CurrentCommandList();
    }

    const NIPtr<Internal::RingBuffer>& GetRingBuffer() const
    {
        return mRingBuffer;
    }

    const NIPtr<Internal::RingBuffer>& GetConstantRingBuffer() const
    {
        return mConstantRingBuffer;
    }

    const NIPtr<Internal::DescriptorHeap>& GetRTVDescriptorHeap() const
    {
        return mRTVHeap;
    }

    const NIPtr<Internal::DescriptorHeap>& GetDSVDescriptorHeap() const
    {
        return mDSVHeap;
    }

    const NIPtr<Internal::InternalShader>& GetInternalShader(const std::string& name) const
    {
        return mShaderLibrary->GetShaderData(name);
    }
};

} // namespace D3D12
