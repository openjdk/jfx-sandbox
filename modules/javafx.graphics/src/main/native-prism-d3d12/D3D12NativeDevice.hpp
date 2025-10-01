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

#pragma once

#include "D3D12Common.hpp"

#include "D3D12NativeMesh.hpp"
#include "D3D12NativeMeshView.hpp"
#include "D3D12NativePhongMaterial.hpp"
#include "D3D12NativeRenderTarget.hpp"
#include "D3D12NativeShader.hpp"
#include "D3D12NativeSwapChain.hpp"
#include "D3D12NativeTexture.hpp"

#include "Internal/D3D12Buffer.hpp"
#include "Internal/D3D12CheckpointQueue.hpp"
#include "Internal/D3D12CommandListPool.hpp"
#include "Internal/D3D12DescriptorAllocator.hpp"
#include "Internal/D3D12IWaitableOperation.hpp"
#include "Internal/D3D12RootSignatureManager.hpp"
#include "Internal/D3D12ResourceDisposer.hpp"
#include "Internal/D3D12RenderingContext.hpp"
#include "Internal/D3D12RingBuffer.hpp"
#include "Internal/D3D12ShaderLibrary.hpp"
#include "Internal/D3D12SamplerStorage.hpp"
#include "Internal/D3D12Waitable.hpp"

#include "Internal/D3D12Matrix.hpp"
#include "Internal/JNIBuffer.hpp"
#include "Internal/MemoryView.hpp"

#include <vector>
#include <memory>


namespace D3D12 {

class NativeDevice: public std::enable_shared_from_this<NativeDevice>
{
    using QuadVertices = std::array<Vertex_2D, 4>;

    struct VertexSubregion
    {
        uint32_t startOffset; // counted in vertices since start of entire region
        Internal::RingBuffer::Region subregion;
        D3D12_VERTEX_BUFFER_VIEW view;

        VertexSubregion()
            : startOffset(0)
            , subregion()
            , view()
        {}

        operator bool() const
        {
            return subregion.operator bool();
        }
    };

    class VertexBatch
    {
        uint32_t mTaken;
        Internal::RingBuffer::Region mRegion;
        D3D12_VERTEX_BUFFER_VIEW mView;

        size_t ElementsToBytes(size_t elements)
        {
            return elements * sizeof(Vertex_2D);
        }

    public:
        VertexBatch()
            : mTaken(0)
            , mRegion()
        {}

        inline uint32_t Available() const
        {
            return (Constants::MAX_BATCH_VERTICES - mTaken);
        }

        inline bool Valid() const
        {
            return mRegion.operator bool();
        }

        inline void Invalidate()
        {
            mRegion = Internal::RingBuffer::Region();
            mTaken = 0;
            D3D12NI_ZERO_STRUCT(mView);
        }

        void AssignNewRegion(const Internal::RingBuffer::Region& region)
        {
            mRegion = region;
            mTaken = 0;

            mView.BufferLocation = mRegion.gpu;
            mView.SizeInBytes = static_cast<UINT>(mRegion.size);
            mView.StrideInBytes = sizeof(Vertex_2D); // 3x pos, 1x uint32 color, 2x uv, 2x uv
        }

        VertexSubregion Subregion(uint32_t elements)
        {
            D3D12NI_ASSERT(elements <= (Constants::MAX_BATCH_VERTICES - mTaken), "Attempted to exceed VB Batch size");
            D3D12NI_ASSERT(mRegion == true, "No assigned vertex buffer region");

            VertexSubregion result;
            result.subregion = mRegion.Subregion(ElementsToBytes(mTaken), ElementsToBytes(elements));
            result.startOffset = mTaken;
            result.view = mView;

            mTaken += elements;
            return result;
        }
    };

    IDXGIAdapter1* mAdapter;
    D3D12DevicePtr mDevice;
    D3D12CommandQueuePtr mCommandQueue;
    D3D12FencePtr mFence;
    uint32_t mFenceValue;
    uint32_t mFrameCounter; // for debugging ex. triggering a breakpoint after X frames
    uint32_t mProfilerTransferWaitSourceID;
    uint32_t mProfilerFrameTimeID;
    bool mMidframeFlushNeeded;
    std::vector<Internal::IWaitableOperation*> mWaitableOps;
    std::vector<D3D12_RESOURCE_BARRIER> mBarrierQueue;

    Internal::CheckpointQueue mCheckpointQueue;
    NIPtr<Internal::RootSignatureManager> mRootSignatureManager;
    NIPtr<Internal::RenderingContext> mRenderingContext;
    NIPtr<Internal::ResourceDisposer> mResourceDisposer;
    NIPtr<Internal::DescriptorAllocator> mRTVAllocator;
    NIPtr<Internal::DescriptorAllocator> mDSVAllocator;
    NIPtr<Internal::DescriptorAllocator> mSRVAllocator;
    NIPtr<Internal::SamplerStorage> mSamplerStorage;
    NIPtr<Internal::ShaderLibrary> mShaderLibrary;
    NIPtr<Internal::Shader> mPassthroughVS;
    NIPtr<Internal::Shader> mPhongVS;
    NIPtr<Internal::Shader> mCurrent2DShader;
    CompositeMode m2DCompositeMode;
    VertexBatch m2DVertexBatch;
    NIPtr<Internal::CommandListPool> mCommandListPool;
    NIPtr<Internal::Buffer> m2DIndexBuffer;
    NIPtr<Internal::RingBuffer> mRingBuffer; // used for larger data (ex. 2D Vertex Buffer, texture upload)

    struct Transforms
    {
        Coords_XYZW_FLOAT cameraPos;
        Internal::Matrix<float> worldTransform;
        Internal::Matrix<float> viewProjTransform;
    } mTransforms;

    bool Build2DIndexBuffer();
    BBox AssembleVertexData(void* buffer, const Internal::MemoryView<float>& vertices,
                            const Internal::MemoryView<signed char>& colors, UINT elementCount);
    QuadVertices AssembleVertexQuadForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst);
    const NIPtr<Internal::Shader>& GetPhongPixelShader(const PhongShaderSpec& spec) const;
    VertexSubregion GetNewRegionForVertices(uint32_t elementCount);

public:
    NativeDevice();
    ~NativeDevice();

    bool Init(IDXGIAdapter1* adapter, const NIPtr<Internal::ShaderLibrary>& shaderLibrary);
    void Release();

    // separate, internal buffer creator
    // used for 2D Index Buffer and internals of Mesh
    NIPtr<Internal::Buffer> CreateBuffer(const void* initialData, size_t size, bool cpuWriteable, D3D12_RESOURCE_STATES finalState);

    bool CheckFormatSupport(DXGI_FORMAT format);
    NIPtr<NativeMesh>* CreateMesh();
    NIPtr<NativeMeshView>* CreateMeshView(const NIPtr<NativeMesh>& mesh);
    NIPtr<NativePhongMaterial>* CreatePhongMaterial();
    NIPtr<NativeRenderTarget>* CreateRenderTarget(const NIPtr<NativeTexture>& texture);
    NIPtr<NativeShader>* CreateShader(const std::string& name, void* buf, UINT size);
    NIPtr<NativeTexture>* CreateTexture(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
                                        TextureUsage usage, TextureWrapMode wrapMode, int samples, bool useMipmap);
    int GetMaximumMSAASampleSize(DXGI_FORMAT format) const;
    int GetMaximumTextureSize() const;
    void MarkResourceDisposed(const D3D12PageablePtr& pageable);

    void Clear(float r, float g, float b, float a, bool clearDepth);
    void ClearTextureUnit(uint32_t unit);
    void RenderQuads(const Internal::MemoryView<float>& vertices, const Internal::MemoryView<signed char>& colors,
                     UINT vertexCount);
    void RenderMeshView(const NIPtr<NativeMeshView>& meshView);
    void SetCompositeMode(CompositeMode mode);
    void UnsetPixelShader();
    void SetPixelShader(const NIPtr<NativeShader>& ps);
    void SetRenderTarget(const NIPtr<NativeRenderTarget>& target, bool enableDepthTest);
    void SetScissor(bool enabled, int x1, int y1, int x2, int y2);
    bool SetShaderConstants(const NIPtr<NativeShader>& shader, const std::string& name, const void* data, size_t size);
    void SetTexture(uint32_t unit, const NIPtr<NativeTexture>& texture);
    void SetCameraPos(const Coords_XYZW_FLOAT& pos);
    void SetWorldTransform(const Internal::Matrix<float>& matrix);
    void SetViewProjTransform(const Internal::Matrix<float>& matrix);
    bool Blit(const NIPtr<NativeRenderTarget>& srcRT, const Coords_Box_UINT32& src,
              const NIPtr<Internal::IRenderTarget>& dstRT, const Coords_Box_UINT32& dst);
    bool ReadTexture(const NIPtr<NativeTexture>& texture, void* buffer, size_t pixelCount,
                     UINT srcx, UINT srcy, UINT srcw, UINT srch);
    bool GenerateMipmaps(const NIPtr<NativeTexture>& texture);
    bool UpdateTexture(const NIPtr<NativeTexture>& texture, const void* data, size_t pixelCount, PixelFormat srcFormat,
                       UINT dstx, UINT dsty, UINT srcx, UINT srcy, UINT srcw, UINT srch, UINT srcstride);

    void FinishFrame();
    void FlushCommandList();
    void Execute(const std::vector<ID3D12CommandList*>& commandLists);
    uint64_t Signal(CheckpointType type);
    void AdvanceCommandAllocator();
    void RegisterWaitableOperation(Internal::IWaitableOperation* waitableOp);
    void UnregisterWaitableOperation(Internal::IWaitableOperation* waitableOp);
    void QueueTextureTransition(const NIPtr<Internal::TextureBase>& tex, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void SubmitTextureTransitions();

    const D3D12DevicePtr& GetDevice()
    {
        return mDevice;
    }

    Internal::CheckpointQueue& GetCheckpointQueue()
    {
        return mCheckpointQueue;
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

    const NIPtr<Internal::RootSignatureManager>& GetRootSignatureManager() const
    {
        return mRootSignatureManager;
    }

    const NIPtr<Internal::DescriptorAllocator>& GetRTVDescriptorAllocator() const
    {
        return mRTVAllocator;
    }

    const NIPtr<Internal::DescriptorAllocator>& GetDSVDescriptorAllocator() const
    {
        return mDSVAllocator;
    }

    const NIPtr<Internal::DescriptorAllocator>& GetSRVDescriptorAllocator() const
    {
        return mSRVAllocator;
    }

    const NIPtr<Internal::SamplerStorage>& GetSamplerStorage() const
    {
        return mSamplerStorage;
    }

    const NIPtr<Internal::Shader>& GetInternalShader(const std::string& name) const
    {
        return mShaderLibrary->GetShaderData(name);
    }

    // TODO This can be removed?
    inline void NotifyMidframeFlushNeeded()
    {
        mMidframeFlushNeeded = true;
    }
};

} // namespace D3D12
