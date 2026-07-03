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

#include "D3D12RenderThreadState.hpp"


namespace D3D12 {
namespace Internal {

bool RenderThreadState::Build2DIndexBuffer()
{
    // For 2D, Index Buffer is provided by the backend and has the same structure.
    std::array<uint16_t, Constants::MAX_BATCH_QUADS * 6> indexBufferArray;

    // stolen from D3DContext.cc
    for (uint16_t i = 0; i != Constants::MAX_BATCH_QUADS; ++i) {
        uint16_t vtx = i * 4;
        uint16_t idx = i * 6;
        indexBufferArray[idx + 0] = vtx + 0;
        indexBufferArray[idx + 1] = vtx + 1;
        indexBufferArray[idx + 2] = vtx + 2;
        indexBufferArray[idx + 3] = vtx + 2;
        indexBufferArray[idx + 4] = vtx + 1;
        indexBufferArray[idx + 5] = vtx + 3;
    }

    if (!m2DIndexBuffer.Init(indexBufferArray.data(), indexBufferArray.size() * sizeof(uint16_t),
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER))
    {
        D3D12NI_LOG_ERROR("Failed to build 2D Index Buffer");
        return false;
    }

    return true;
}

RenderThreadState::QuadVertices RenderThreadState::AssembleVertexQuadForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst)
{
    uint32_t srcWidth = src.x1 - src.x0;
    uint32_t srcHeight = src.y1 - src.y0;
    uint32_t dstWidth = dst.x1 - dst.x0;
    uint32_t dstHeight = dst.y1 - dst.y0;

    // default viewport coordinates are from -1 to 1
    // so destination has to be doubled and shifted from 0-2 range to -1-1 range
    Coords_UV_FLOAT srcTexelSize{ 1.0f / srcWidth, 1.0f / srcHeight };
    Coords_UV_FLOAT dstTexelSize{ 2.0f / dstWidth, 2.0f / dstHeight };

    // dstBox V coord is inverted because of d3d12's coordinate system
    Coords_UV_FLOAT srcBoxTexelsMin{ src.x0 * srcTexelSize.u, src.y0 * srcTexelSize.v };
    Coords_UV_FLOAT srcBoxTexelsMax{ src.x1 * srcTexelSize.u, src.y1 * srcTexelSize.v };
    Coords_UV_FLOAT dstBoxTexelsMin{ (dst.x0 * dstTexelSize.u) - 1.0f, (dst.y1 * dstTexelSize.v) - 1.0f };
    Coords_UV_FLOAT dstBoxTexelsMax{ (dst.x1 * dstTexelSize.u) - 1.0f, (dst.y0 * dstTexelSize.v) - 1.0f };

    QuadVertices result;

    // Build a fullscreen quad used for slow blit path
    result[0].pos.x = dstBoxTexelsMin.u;
    result[0].pos.y = dstBoxTexelsMin.v;
    result[0].uv1.u = srcBoxTexelsMin.u;
    result[0].uv1.v = srcBoxTexelsMin.v;

    result[1].pos.x = dstBoxTexelsMin.u;
    result[1].pos.y = dstBoxTexelsMax.v;
    result[1].uv1.u = srcBoxTexelsMin.u;
    result[1].uv1.v = srcBoxTexelsMax.v;

    result[2].pos.x = dstBoxTexelsMax.u;
    result[2].pos.y = dstBoxTexelsMin.v;
    result[2].uv1.u = srcBoxTexelsMax.u;
    result[2].uv1.v = srcBoxTexelsMin.v;

    result[3].pos.x = dstBoxTexelsMax.u;
    result[3].pos.y = dstBoxTexelsMax.v;
    result[3].uv1.u = srcBoxTexelsMax.u;
    result[3].uv1.v = srcBoxTexelsMax.v;

    for (uint32_t i = 0; i < result.size(); ++i)
    {
        result[i].pos.z = 0.0f;
        result[i].color.r = 255;
        result[i].color.g = 255;
        result[i].color.b = 255;
        result[i].color.a = 255;
        result[i].uv2 = result[i].uv1;
    }

    return result;
}

// NOTE technically we don't query buffer ptr's size, but this function assumes we reserved enough
// space already.
BBox RenderThreadState::AssembleVertexData(void* buffer, const float* vertices,
                                          const signed char* colors, size_t elementCount)
{
    Vertex_2D* bufVertices = reinterpret_cast<Vertex_2D*>(buffer);
    BBox bbox;

    size_t vertIdx = 0;
    size_t colorIdx = 0;
    for (UINT i = 0; i < elementCount; ++i)
    {
        bufVertices[i].pos.x = vertices[vertIdx++];
        bufVertices[i].pos.y = vertices[vertIdx++];
        bufVertices[i].pos.z = vertices[vertIdx++];
        bufVertices[i].color.r = colors[colorIdx++];
        bufVertices[i].color.g = colors[colorIdx++];
        bufVertices[i].color.b = colors[colorIdx++];
        bufVertices[i].color.a = colors[colorIdx++];
        bufVertices[i].uv1.u = vertices[vertIdx++];
        bufVertices[i].uv1.v = vertices[vertIdx++];
        bufVertices[i].uv2.u = vertices[vertIdx++];
        bufVertices[i].uv2.v = vertices[vertIdx++];
    }

    if (elementCount == 4)
    {
        // only create a valid bbox when we render a quad
        // quad is the only way we can be sure bbox is valid
        // TODO: D3D12: maybe we should lift that limitation some day, would require reworking
        // bbox merging though and might be too heavy CPU wise
        for (UINT i = 0; i < elementCount; ++i)
        {
            bbox.Merge(bufVertices[i].pos.x, bufVertices[i].pos.y, bufVertices[i].pos.x, bufVertices[i].pos.y);
        }
    }

    return bbox;
}

RenderThreadState::VertexSubregion RenderThreadState::GetNewRegionForVertices(uint32_t vertexCount)
{
    if (vertexCount > (Constants::MAX_BATCH_VERTICES / 2))
    {
        // rendering more vertices might utilize the Ring Buffer better if we just reserve a separate space for them
        mVertexRingBuffer.DeclareRequired(vertexCount * 8 * sizeof(float));
        Internal::RingBuffer::Region region = mVertexRingBuffer.Reserve(vertexCount * 8 * sizeof(float));
        if (!region)
        {
            D3D12NI_LOG_ERROR("2D Vertex Ring Buffer allocation failed");
            return VertexSubregion();
        }

        VertexSubregion separateRegion;
        separateRegion.subregion = region;
        separateRegion.view.BufferLocation = region.gpu;
        separateRegion.view.SizeInBytes = static_cast<UINT>(region.size);
        separateRegion.view.StrideInBytes = sizeof(Vertex_2D);

        return separateRegion;
    }

    if (!m2DVertexBatch.Valid() || vertexCount > m2DVertexBatch.Available())
    {
        // reserve space on Ring Buffer
        mVertexRingBuffer.DeclareRequired(Constants::MAX_BATCH_VERTICES * 8 * sizeof(float));

        Internal::RingBuffer::Region newVertexRegion = mVertexRingBuffer.Reserve(Constants::MAX_BATCH_VERTICES * 8 * sizeof(float));
        if (!newVertexRegion)
        {
            D3D12NI_LOG_ERROR("2D Vertex Ring Buffer allocation failed");
            return VertexSubregion();
        }

        m2DVertexBatch.AssignNewRegion(newVertexRegion, newVertexRegion);
    }

    return m2DVertexBatch.Subregion(vertexCount);
}

void RenderThreadState::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& ibView)
{
    if (indexBuffer.Get().BufferLocation == ibView.BufferLocation &&
        indexBuffer.Get().SizeInBytes == ibView.SizeInBytes &&
        indexBuffer.Get().Format == ibView.Format)
        return;

    indexBuffer.Set(ibView);
}

void RenderThreadState::SetVertexBuffer(const D3D12_VERTEX_BUFFER_VIEW& vbView)
{
    if (vertexBuffer.Get().BufferLocation == vbView.BufferLocation &&
        vertexBuffer.Get().SizeInBytes == vbView.SizeInBytes &&
        vertexBuffer.Get().StrideInBytes == vbView.StrideInBytes)
        return;

    vertexBuffer.Set(vbView);
}

RenderThreadState::RenderThreadState(const NIPtr<NativeDevice>& nativeDevice, const CheckpointCallback& flushCallback, const CheckpointCallback& waitCallback, const CheckpointCallback& signalCallback)
    : m2DVertexBatch()
    , m2DIndexBuffer(nativeDevice)
    , mVertexRingBuffer(nativeDevice, flushCallback, waitCallback)
    , PSOManager(nativeDevice)
    , commandListPool(nativeDevice, waitCallback)
    , resourceManager(nativeDevice, flushCallback, waitCallback)
    , resourceDisposer(nativeDevice)
    , barrierQueue()
    , barrierQueueSize(0)
    , flushCommandList(flushCallback)
    , waitForCheckpoint(waitCallback)
    , signal(signalCallback)
{
}

bool RenderThreadState::Init()
{
    if (!PSOManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize PSO Manager");
        return false;
    }

    if (!resourceManager.Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Resource Manager");
        return false;
    }

    // Descriptor Heaps are set once from ResourceManager. They still need to be
    // re-applied every Command List, so we run them via CommandListSteps, but
    // Quantum-thread-side will not modify them.
    descriptorHeaps.Set({resourceManager.GetHeap(), resourceManager.GetSamplerHeap()});

    mVertexRingBuffer.SetDebugName("2D Vertex GPU Ring Buffer");
    if (!mVertexRingBuffer.Init(Config::MainRingBufferThreshold(), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT))
    {
        D3D12NI_LOG_ERROR("Failed to initialize the Vertex Ring Buffer");
        return false;
    }

    // NOTE: Command List Pool requires to have as many Command Allocators as SwapChain buffers + 1
    // This is the minimum we need (one per frame) and also ensures an extra one to pre-record more commands
    // if both SwapChain buffers are full.
    if (!commandListPool.Init(D3D12_COMMAND_LIST_TYPE_DIRECT, 16, 4))
    {
        D3D12NI_LOG_ERROR("Failed to initialize Command List Pool");
        return false;
    }

    if (!Build2DIndexBuffer())
    {
        return false;
    }

    return true;
}

void RenderThreadState::QueueTextureTransition(const NIPtr<Internal::ITrackedResource>& resource, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    if (resource->GetD3D12ResourceState(subresource) == newState) return;

    QueueResourceTransition(resource->GetD3D12Resource(), resource->GetD3D12ResourceState(subresource), newState, subresource);
    resource->SetD3D12ResourceState(newState, subresource);
}

void RenderThreadState::QueueResourceTransition(const D3D12ResourcePtr& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState, uint32_t subresource)
{
    D3D12NI_ZERO_STRUCT(barrierQueue[barrierQueueSize]);
    barrierQueue[barrierQueueSize].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierQueue[barrierQueueSize].Transition.pResource = resource.Get();
    barrierQueue[barrierQueueSize].Transition.StateBefore = oldState;
    barrierQueue[barrierQueueSize].Transition.StateAfter = newState;
    barrierQueue[barrierQueueSize].Transition.Subresource = subresource;
    ++barrierQueueSize;

    if (barrierQueueSize == barrierQueue.size()) SubmitResourceTransitions(commandListPool.CurrentCommandListRef());
}

void RenderThreadState::SubmitResourceTransitions(const D3D12GraphicsCommandListPtr& commandList)
{
    if (barrierQueueSize == 0) return;

    commandList->ResourceBarrier(barrierQueueSize, barrierQueue.data());
    barrierQueueSize = 0;
}

void RenderThreadState::ApplySteps(const D3D12GraphicsCommandListPtr& commandList)
{
    indexBuffer.Apply(commandList);
    vertexBuffer.Apply(commandList);
    primitiveTopology.Apply(commandList);
    scissor.Apply(commandList);
    viewport.Apply(commandList);
    renderTarget.Apply(commandList);
    pipelineState.Apply(commandList);
    descriptorHeaps.Apply(commandList);
    graphicsRootSignature.Apply(commandList);
}

void RenderThreadState::ApplyComputeSteps(const D3D12GraphicsCommandListPtr& commandList)
{
    pipelineState.Apply(commandList);
    descriptorHeaps.Apply(commandList);
    computeRootSignature.Apply(commandList);
}

void RenderThreadState::ClearAppliedSteps()
{
    indexBuffer.ClearApplied();
    vertexBuffer.ClearApplied();
    primitiveTopology.ClearApplied();
    scissor.ClearApplied();
    viewport.ClearApplied();
    renderTarget.ClearApplied();
    pipelineState.ClearApplied();
    descriptorHeaps.ClearApplied();
    graphicsRootSignature.ClearApplied();
    computeRootSignature.ClearApplied();
}

void RenderThreadState::Draw(const D3D12GraphicsCommandListPtr& commandList, uint32_t elements, uint32_t vbOffset)
{
    BBox invalidBox;
    Draw(commandList, elements, vbOffset, invalidBox);
}

void RenderThreadState::Draw(const D3D12GraphicsCommandListPtr& commandList, uint32_t elements, uint32_t vbOffset, const BBox& dirtyBBox)
{
    /* LKTODO clear opts restore
    bool clearDiscarded = false;
    CompositeMode currentCompositeMode = pipelineState.Get().compositeMode;

    if (mClearOptState.clearDelayed)
    {
        // Check if we can discard this clear.
        // The clear can be discarded if we use composite mode SRC_OVER and
        // this draw call will overwrite the entire to-be-cleared area of the RTT.
        //
        // NOTE: compared to other parts related to Clear optimization here we're being
        // a bit more cautions with coordinates - min bbox gets ceil-ed while max bbox gets floor-ed.
        // There can be situations where despite coordinates crossing the 0.5 rounding "barrier" the
        // runtime won't actually render and overwrite pixels on the RTT (happens occasionally in CircleBlendAdd
        // renderperf test). This will create single-frame artifacts, because old RTT contents won't be
        // overwritten by the primitive we want to draw. To prevent those occasional artifacts we must push
        // a Clear() through here - under-estimating BBox coordinates makes it possible and ensures visual
        // correctness when using clear optimizations.
        if (currentCompositeMode == CompositeMode::SRC_OVER && dirtyBBox.Valid() &&
            std::ceil(dirtyBBox.min.x) <= mClearOptState.clearRect.left  && std::ceil(dirtyBBox.min.y) <= mClearOptState.clearRect.top &&
            std::floor(dirtyBBox.max.x) >= mClearOptState.clearRect.right && std::floor(dirtyBBox.max.y) >= mClearOptState.clearRect.bottom)
        {
            clearDiscarded = true;
            SetCompositeMode(CompositeMode::SRC);
        }
        else
        {
            RecordClear(0.0f, 0.0f, 0.0f, 0.0f, mClearOptState.clearDepth, mClearOptState.clearRect);
        }

        mClearOptState.clearDelayed = false;
    }*/

    // Do the draw
    commandList->DrawIndexedInstanced(elements, 1, 0, vbOffset, 0);

    /* LKTODO restore clear opts
    mRenderTarget.Get()->UpdateDirtyBBox(dirtyBBox);

    if (clearDiscarded)
    {
        // restore original composite mode
        SetCompositeMode(currentCompositeMode);
    }*/
}

BBox RenderThreadState::PrepareQuadsDraw(float* vertices, signed char* colors, uint32_t vertexCount, uint32_t& vbOffset)
{
    D3D12_INDEX_BUFFER_VIEW ibView;
    ibView.BufferLocation = m2DIndexBuffer.GetGPUPtr();
    ibView.SizeInBytes = static_cast<UINT>(m2DIndexBuffer.Size());
    ibView.Format = DXGI_FORMAT_R16_UINT;
    SetIndexBuffer(ibView);

    BBox box;

    VertexSubregion vertexRegion = GetNewRegionForVertices(vertexCount);
    if (!vertexRegion) return box;

    box = AssembleVertexData(vertexRegion.subregion.cpu, vertices, colors, vertexCount);
    SetVertexBuffer(vertexRegion.view);

    vbOffset = vertexRegion.startOffset;
    return box;
}

void RenderThreadState::PrepareMeshViewDraw(const NIPtr<NativeMeshView>& meshView)
{
    const NIPtr<NativeMesh>& mesh = meshView->GetMesh();

    D3D12_VERTEX_BUFFER_VIEW vbView;
    vbView.BufferLocation = mesh->GetVertexBuffer()->GetGPUPtr();
    vbView.SizeInBytes = static_cast<UINT>(mesh->GetVertexBuffer()->Size());
    vbView.StrideInBytes = 9 * sizeof(float); // 3 * modelVertexPos; 2 * texD; 4 * modelVertexNormal
    SetVertexBuffer(vbView);

    D3D12_INDEX_BUFFER_VIEW ibView;
    ibView.BufferLocation = mesh->GetIndexBuffer()->GetGPUPtr();
    ibView.SizeInBytes = static_cast<UINT>(mesh->GetIndexBuffer()->Size());
    ibView.Format = mesh->GetIndexBufferFormat();
    SetIndexBuffer(ibView);
}

} // namespace Internal
} // namespace D3D12
