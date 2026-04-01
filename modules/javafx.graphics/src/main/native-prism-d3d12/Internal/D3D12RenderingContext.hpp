/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12Buffer.hpp"
#include "D3D12CheckpointQueue.hpp"
#include "D3D12CommandListPool.hpp"
#include "D3D12GPURingBuffer.hpp"
#include "D3D12IRenderTarget.hpp"
#include "D3D12LinearAllocator.hpp"
#include "D3D12Matrix.hpp"
#include "D3D12RenderingParameter.hpp"
#include "D3D12RenderThread.hpp"
#include "D3D12PSOManager.hpp"
#include "MemoryView.hpp"

#include <unordered_set>


namespace D3D12 {
namespace Internal {

/**
 * Stores information about current state of the Renderer and records parameter
 * setting commands on a Command List as needed.
 *
 * JFX assumes the "old API" behavior - API has a context and some things that were
 * set in the Context remain there. With D3D12, due to different API design this is
 * not the case:
 *   - There is no global Context which would be responsible for such behavior
 *   - Rendering parameters (ex. VBs, IBs, PSOs...) are set via a Command
 *     List and are assumed to be local only within that Command List
 *
 * That means, if we ever want to submit a Command List and start recording to a new
 * one, many objects related to current rendering state are lost and must be re-recorded.
 *
 * This class handles that problem while making sure we don't produce any redundant API
 * calls (aka. command records).
 */
class RenderingContext
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
        Internal::RingBuffer::Region mCPURegion;
        Internal::RingBuffer::Region mGPURegion;
        D3D12_VERTEX_BUFFER_VIEW mView;

        size_t ElementsToBytes(size_t elements)
        {
            return elements * sizeof(Vertex_2D);
        }

    public:
        VertexBatch()
            : mTaken(0)
            , mCPURegion()
            , mGPURegion()
            , mView()
        {}

        inline uint32_t Available() const
        {
            return (Constants::MAX_BATCH_VERTICES - mTaken);
        }

        inline bool Valid() const
        {
            return mCPURegion.operator bool();
        }

        inline void Invalidate()
        {
            mCPURegion = Internal::RingBuffer::Region();
            mGPURegion = Internal::RingBuffer::Region();
            mTaken = 0;
            D3D12NI_ZERO_STRUCT(mView);
        }

        void AssignNewRegion(const Internal::RingBuffer::Region& cpuRegion, const Internal::RingBuffer::Region& gpuRegion)
        {
            mCPURegion = cpuRegion;
            mGPURegion = gpuRegion;
            mTaken = 0;

            mView.BufferLocation = gpuRegion.gpu;
            mView.SizeInBytes = static_cast<UINT>(gpuRegion.size);
            mView.StrideInBytes = sizeof(Vertex_2D); // 3x pos, 1x uint32 color, 2x uv, 2x uv
        }

        VertexSubregion Subregion(uint32_t elements)
        {
            D3D12NI_ASSERT(elements <= (Constants::MAX_BATCH_VERTICES - mTaken), "Attempted to exceed VB Batch size");
            D3D12NI_ASSERT(mCPURegion == true, "No assigned vertex buffer region");

            VertexSubregion result;
            result.subregion = mCPURegion.Subregion(ElementsToBytes(mTaken), ElementsToBytes(elements));
            result.startOffset = mTaken;
            result.view = mView;

            mTaken += elements;
            return result;
        }
    };

    NIPtr<NativeDevice> mNativeDevice;
    ResourceManager mResourceManager;
    LinearAllocator mPayloadAllocator;
    D3D12CommandQueuePtr mCommandQueue;
    D3D12FencePtr mFence;
    RenderThread mRenderThread;
    RenderPayloadPtr mRTPayload;
    CheckpointQueue mCheckpointQueue;
    VertexBatch m2DVertexBatch;
    NIPtr<Internal::GPURingBuffer> mVertexRingBuffer; // used for 2D Vertex data which for performance should reside on GPU-side
    std::thread::id mLastFlushTid;

    struct ClearOptState
    {
        bool clearDelayed;
        D3D12_RECT clearRect;
        bool clearDepth;
    } mClearOptState;

    // some parameters are set by the Java Runtime ex. transforms, composite mode, textures
    // whenever we need to execute some internal operation (ex. BlitTexture()) we have to
    // temporarily "stash" them to restore them later. This struct helps with that.
    // We probably won't need more than one "stash".
    struct RuntimeParametersStash
    {
        PipelineStateRenderingParameter pipelineState;
        PrimitiveTopologyRenderingParameter primitiveTopology;
        RenderTargetRenderingParameter renderTarget;
        RootSignatureRenderingParameter rootSignature;
    } mRuntimeParametersStash;

    // Graphics Pipeline
    IndexBufferRenderingParameter mIndexBuffer;
    VertexBufferRenderingParameter mVertexBuffer;
    DescriptorHeapsRenderingParameter mDescriptorHeaps;
    DescriptorsRenderingParameter mDescriptors;
    PipelineStateRenderingParameter mPipelineState;
    RootSignatureRenderingParameter mRootSignature;
    PrimitiveTopologyRenderingParameter mPrimitiveTopology;
    RenderTargetRenderingParameter mRenderTarget;
    ScissorRenderingParameter mScissor; // used when explicitly set by updateClipRect()
    ScissorRenderingParameter mDefaultScissor; // used when scissor testing is disabled
    ViewportRenderingParameter mViewport;

    inline ScissorRenderingParameter& GetScissor()
    {
        if (mScissor.IsSet()) return mScissor;
        else return mDefaultScissor;
    }

    // Compute Pipeline
    ComputePipelineStateRenderingParameter mComputePipelineState;
    ComputeRootSignatureRenderingParameter mComputeRootSignature;
    DescriptorsRenderingParameter mComputeDescriptors;

    // Used RTTs for finish-frame-time BBox invalidation
    // Prism can "juggle" the RTTs between frames, so our dirty-bbox optimization can
    // only apply for one frame - afterwards we need to invalidate those bboxes and
    // start tracking anew
    std::unordered_set<NIPtr<Internal::IRenderTarget>> mUsedRTs;

    ResourceBarrierArray mBarrierQueue;
    uint32_t mBarrierQueueSize;

    void RecordClear(float r, float g, float b, float a, bool clearDepth, const D3D12_RECT& clearRect);
    void EnsureBoundTextureStates(D3D12_RESOURCE_STATES state);
    RenderPayloadPtr ReplaceRTPayload(); // creates new payload, returns old one
    void SubmitRTPayload();


    QuadVertices AssembleVertexQuadForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst);
    BBox AssembleVertexData(void* buffer, const Internal::MemoryView<float>& vertices,
                            const Internal::MemoryView<signed char>& colors, UINT elementCount);
    VertexSubregion GetNewRegionForVertices(uint32_t vertexCount);
    void SyncToRenderThread();
    void ExecuteCurrentCommandList();
    void ClearAppliedFlags();

public:
    RenderingContext(const NIPtr<NativeDevice>& nativeDevice);
    ~RenderingContext();

    bool Init();

    bool Apply();
    bool ApplyCompute();
    void Clear(float r, float g, float b, float a, bool clearDepth);
    void Draw(uint32_t elements, uint32_t vbOffset);
    void Draw(uint32_t elements, uint32_t vbOffset, const BBox& dirtyBBox);
    void Dispatch(uint32_t x, uint32_t y, uint32_t z);

    void Resolve(const NIPtr<TextureBase>& dstTexture, const NIPtr<TextureBase>& srcTexture, DXGI_FORMAT resolveFormat);
    void ResolveRegion(const NIPtr<TextureBase>& dstTexture, uint32_t dstx, uint32_t dsty,
                       const NIPtr<TextureBase>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch,
                       DXGI_FORMAT resolveFormat);
    void CopyTexture(const NIPtr<TextureBase>& dstTexture, uint32_t dstx, uint32_t dsty,
                     const NIPtr<TextureBase>& srcTexture, uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch);
    void CopyTexture(const Buffer& dstBuffer, uint32_t dstStride, const NIPtr<NativeTexture>& srcTexture,
                     uint32_t srcx, uint32_t srcy, uint32_t srcw, uint32_t srch);
    void CopyToTexture(const NIPtr<TextureBase>& dstTexture, uint32_t dstx, uint32_t dsty,
                       ID3D12Resource* srcResource, uint32_t srcw, uint32_t srch, uint64_t srcOffset,
                       uint32_t srcStride, DXGI_FORMAT format);
    void CopyBufferRegion(const D3D12ResourcePtr& dst, uint64_t dstOffset, const D3D12ResourcePtr& src, uint64_t srcOffset, uint64_t size);
    void CopyResource(const D3D12ResourcePtr& dst, const D3D12ResourcePtr& src);
    bool GenerateMipmaps(const NIPtr<NativeTexture>& texture);

    void QueueTextureTransition(const NIPtr<Internal::TextureBase>& tex, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void SubmitResourceTransitions();

    void ClearTextureUnit(uint32_t unit);
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView);
    void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbView);
    BBox SetVertexBufferForQuads(const Internal::MemoryView<float>& vertices, const Internal::MemoryView<signed char>& colors,
                                 uint32_t vertexCount, uint32_t& retOffset);
    BBox SetVertexBufferForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst, uint32_t& retOffset);
    void SetRenderTarget(const NIPtr<IRenderTarget>& renderTarget);
    void SetScissor(bool enabled, const D3D12_RECT& scissor);
    void SetTexture(uint32_t unit, const NIPtr<TextureBase>& texture);

    void SetCompositeMode(CompositeMode mode);
    void SetCullMode(D3D12_CULL_MODE mode);
    void SetFillMode(D3D12_FILL_MODE mode);
    void SetVertexShader(const NIPtr<Shader>& vertexShader);
    void SetPixelShader(const NIPtr<Shader>& pixelShader);
    void SetComputeShader(const NIPtr<Shader>& computeShader);

    void StashParamters();
    void RestoreStashedParameters();

    void FlushCommandList(CheckpointType type);
    bool WaitForNextCheckpoint(CheckpointType type);
    void Signal(uint64_t fenceValue, CheckpointType type, Waitable&& waitable);
    void FinishFrame();

    // exposed for SwapChain
    inline const D3D12CommandQueuePtr& GetCommandQueue() const
    {
        return mCommandQueue;
    }

    inline bool CheckpointQueueEmpty() const
    {
        return mCheckpointQueue.Empty();
    }
};

} // namespace Internal
} // namespace D3D12
