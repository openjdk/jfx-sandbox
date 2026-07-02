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
#include "../D3D12NativeMeshView.hpp"

#include "D3D12RenderingStep.hpp"

#include "D3D12Buffer.hpp"
#include "D3D12PSOManager.hpp"
#include "D3D12CheckpointQueue.hpp"
#include "D3D12CommandListPool.hpp"
#include "D3D12ResourceManager.hpp"
#include "D3D12ResourceDisposer.hpp"


namespace D3D12 {
namespace Internal {

template <typename T>
class CommandListDataStep: public RenderingDataStep<T>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const = 0;

public:
    void Apply(const D3D12GraphicsCommandListPtr& commandList)
    {
        if (this->CanBeSkipped()) return;

        this->ApplyImpl(commandList);
        this->mIsApplied = true;
    }
};

class IndexBufferCommandListStep: public CommandListDataStep<D3D12_INDEX_BUFFER_VIEW>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->IASetIndexBuffer(&this->mParameter);
    }
};

class VertexBufferCommandListStep: public CommandListDataStep<D3D12_VERTEX_BUFFER_VIEW>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->IASetVertexBuffers(0, 1, &this->mParameter);
    }
};

class PrimitiveTopologyCommandListStep: public CommandListDataStep<D3D12_PRIMITIVE_TOPOLOGY>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->IASetPrimitiveTopology(this->mParameter);
    }
};

class ScissorCommandListStep: public CommandListDataStep<D3D12_RECT>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->RSSetScissorRects(1, &this->mParameter);
    }
};

class ViewportCommandListStep: public CommandListDataStep<D3D12_VIEWPORT>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->RSSetViewports(1, &this->mParameter);
    }
};

class PipelineStateCommandListStep: public CommandListDataStep<D3D12PipelineStatePtr>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->SetPipelineState(mParameter.Get());
    }
};

class GraphicsRootSignatureCommandListStep: public CommandListDataStep<D3D12RootSignaturePtr>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->SetGraphicsRootSignature(mParameter.Get());
    }
};

class ComputeRootSignatureCommandListStep: public CommandListDataStep<D3D12RootSignaturePtr>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->SetComputeRootSignature(mParameter.Get());
    }
};

struct RenderTargetCommandListData
{
    UINT count;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;

    RenderTargetCommandListData()
        : RenderTargetCommandListData(0, D3D12_CPU_DESCRIPTOR_HANDLE{0}, D3D12_CPU_DESCRIPTOR_HANDLE{0})
    {}

    RenderTargetCommandListData(UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE& rtv)
        : RenderTargetCommandListData(count, rtv, D3D12_CPU_DESCRIPTOR_HANDLE{0})
    {}

    RenderTargetCommandListData(UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE& rtv, const D3D12_CPU_DESCRIPTOR_HANDLE& dsv)
        : count(count), rtv(rtv), dsv(dsv)
    {}
};

class RenderTargetCommandListStep: public CommandListDataStep<RenderTargetCommandListData>
{
protected:
    virtual void ApplyImpl(const D3D12GraphicsCommandListPtr& commandList) const override
    {
        commandList->OMSetRenderTargets(
            mParameter.count, &mParameter.rtv, true, mParameter.dsv.ptr > 0 ? &mParameter.dsv : nullptr
        );
    }
};

/**
 * Stores information about current state of the Command List and records parameter
 * setting commands as needed.
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
class RenderThreadState
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

    VertexBatch m2DVertexBatch;
    Internal::Buffer m2DIndexBuffer;
    Internal::RingBuffer mVertexRingBuffer; // used for 2D Vertex data

    bool Build2DIndexBuffer();
    QuadVertices AssembleVertexQuadForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst);
    BBox AssembleVertexData(void* buffer, const float* vertices, const signed char* colors, size_t elementCount);
    VertexSubregion GetNewRegionForVertices(uint32_t vertexCount);
    void EnsureBoundTextureStates(const D3D12GraphicsCommandListPtr& commandList, D3D12_RESOURCE_STATES state);
    void SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbView);
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView);

public:
    PSOManager PSOManager;
    CommandListPool commandListPool; // TODO move to RenderThread internals
    ResourceManager resourceManager;
    ResourceDisposer resourceDisposer;
    std::array<D3D12_RESOURCE_BARRIER, 8> barrierQueue;
    uint32_t barrierQueueSize;
    bool heapsApplied;

    // RT callbacks to control command lists
    CheckpointCallback flushCommandList;
    CheckpointCallback waitForCheckpoint;
    CheckpointCallback signal;

    // RenderThread-side Command List parameters. This mostly clones what RenderingContext has.
    // RenderingContext updates these via RTExecutables and those are reflected on CommandLists.
    // The reason for duplicating these is to both reduce Command List records, as well as to
    // restore the Command List to its "known state" when internally RenderThread flushes
    // the Command List

    // Graphics
    IndexBufferCommandListStep indexBuffer;
    VertexBufferCommandListStep vertexBuffer;
    PrimitiveTopologyCommandListStep primitiveTopology;
    ScissorCommandListStep scissor;
    ViewportCommandListStep viewport;
    PipelineStateCommandListStep pipelineState;
    RenderTargetCommandListStep renderTarget;
    GraphicsRootSignatureCommandListStep graphicsRootSignature;

    // Compute
    ComputeRootSignatureCommandListStep computeRootSignature;

    RenderThreadState(const NIPtr<NativeDevice>& nativeDevice, const CheckpointCallback& flushCallback, const CheckpointCallback& waitCallback, const CheckpointCallback& signalCallback);
    bool Init();

    void QueueTextureTransition(const NIPtr<Internal::ITrackedResource>& resource, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void SubmitResourceTransitions(const D3D12GraphicsCommandListPtr& commandList);

    void ApplySteps(const D3D12GraphicsCommandListPtr& commandList);
    void ApplyComputeSteps(const D3D12GraphicsCommandListPtr& commandList);
    void ClearAppliedSteps();

    BBox PrepareQuadsDraw(float* vertices, signed char* colors, uint32_t vertexCount, uint32_t& vbOffset);
    void PrepareMeshViewDraw(const NIPtr<NativeMeshView>& meshView);
    void Draw(const D3D12GraphicsCommandListPtr& commandList, uint32_t elements, uint32_t vbOffset);
    void Draw(const D3D12GraphicsCommandListPtr& commandList, uint32_t elements, uint32_t vbOffset, const BBox& dirtyBBox);

    inline void Invalidate2DVertexBatch()
    {
        m2DVertexBatch.Invalidate();
    }
};

} // namespace Internal
} // namespace D3D12
