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

#include "D3D12NativeDevice.hpp"

#include "D3D12Constants.hpp"

#include "Internal/D3D12Debug.hpp"
#include "Internal/D3D12Logger.hpp"
#include "Internal/D3D12MipmapGenComputeShader.hpp"
#include "Internal/D3D12TextureUploader.hpp"
#include "Internal/D3D12Utils.hpp"
#include "Internal/JNIString.hpp"

#include <com_sun_prism_d3d12_ni_D3D12NativeDevice.h>

#include <algorithm>


namespace D3D12 {

bool NativeDevice::Build2DIndexBuffer()
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

    m2DIndexBuffer = std::make_shared<Internal::Buffer>(shared_from_this());
    if (!m2DIndexBuffer->Init(indexBufferArray.data(), indexBufferArray.size() * sizeof(uint16_t),
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER))
    {
        D3D12NI_LOG_ERROR("Failed to build 2D Index Buffer");
        return false;
    }

    return true;
}

NativeDevice::QuadVertices NativeDevice::AssembleVertexQuadForBlit(const Coords_Box_UINT32& src, const Coords_Box_UINT32& dst)
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
void NativeDevice::AssembleVertexData(void* buffer, const Internal::MemoryView<float>& vertices,
                                      const Internal::MemoryView<signed char>& colors, UINT elementCount)
{
    Vertex_2D* bufVertices = reinterpret_cast<Vertex_2D*>(buffer);

    size_t vertIdx = 0;
    size_t colorIdx = 0;
    for (UINT i = 0; i < elementCount; ++i)
    {
        D3D12NI_ASSERT(vertIdx < vertices.Size(), "Exceeded vertex array size");
        D3D12NI_ASSERT(colorIdx < colors.Size(), "Exceeded color array size");
        bufVertices[i].pos.x = vertices.Data()[vertIdx++];
        bufVertices[i].pos.y = vertices.Data()[vertIdx++];
        bufVertices[i].pos.z = vertices.Data()[vertIdx++];
        bufVertices[i].color.r = colors.Data()[colorIdx++];
        bufVertices[i].color.g = colors.Data()[colorIdx++];
        bufVertices[i].color.b = colors.Data()[colorIdx++];
        bufVertices[i].color.a = colors.Data()[colorIdx++];
        bufVertices[i].uv1.u = vertices.Data()[vertIdx++];
        bufVertices[i].uv1.v = vertices.Data()[vertIdx++];
        bufVertices[i].uv2.u = vertices.Data()[vertIdx++];
        bufVertices[i].uv2.v = vertices.Data()[vertIdx++];
    }
}

const NIPtr<Internal::Shader>& NativeDevice::GetPhongPixelShader(const PhongShaderSpec& spec) const
{
    std::string name(Constants::PHONG_PS_NAME);

    uint32_t lightCount = spec.lightCount;
    if (lightCount > 3) lightCount = 3;

    if (lightCount == 0)
    {
        // no light count - only determine whether we need self illumination or not
        if (spec.isSelfIllum) return GetInternalShader(name + "_i");
        else return GetInternalShader(name);
    }

    char mapping = 0;
    switch (spec.mapping)
    {
    case PhongShaderMappingVariant::SIMPLE: mapping = 's'; break;
    case PhongShaderMappingVariant::BUMP: mapping = 'b'; break;
    }

    char specular = 0;
    switch (spec.specular)
    {
    case PhongShaderSpecularVariant::NONE: specular = 'n'; break;
    case PhongShaderSpecularVariant::TEXTURE: specular = 't'; break;
    case PhongShaderSpecularVariant::COLOR: specular = 'c'; break;
    case PhongShaderSpecularVariant::MIX: specular = 'm'; break;
    }

    char light = '0' + spec.lightCount;

    name += '_';
    name += mapping;
    name += light;
    name += specular;
    if (spec.isSelfIllum) name += 'i';

    return GetInternalShader(name);
}

NativeDevice::NativeDevice()
    : mAdapter(nullptr)
    , mDevice()
    , mCommandQueue()
    , mFence()
    , mFenceValue(0)
    , mFrameCounter(0)
    , mWaitableOps()
    , mRootSignatureManager()
    , mRenderingContext()
    , mResourceDisposer()
    , mRTVHeap()
    , mDSVHeap()
    , mShaderLibrary()
    , mPassthroughVS()
    , mPhongVS()
    , mCurrent2DShader()
    , mCommandListPool()
    , m2DIndexBuffer()
    , mRingBuffer()
    , mConstantRingBuffer()
    , mMidFrameWaitable()
{
}

NativeDevice::~NativeDevice()
{
    D3D12NI_LOG_DEBUG("Destroying device");

    // ensures the pipeline is purged
    mMidFrameWaitable.reset();
    SignalMidFrame();
    WaitMidFrame();

    if (mRingBuffer) mRingBuffer.reset();
    if (mConstantRingBuffer) mConstantRingBuffer.reset();
    if (m2DIndexBuffer) m2DIndexBuffer.reset();
    if (mCommandListPool) mCommandListPool.reset();
    if (mShaderLibrary) mShaderLibrary.reset();
    if (mRenderingContext) mRenderingContext.reset();
    if (mRTVHeap) mRTVHeap.reset();
    if (mDSVHeap) mDSVHeap.reset();
    if (mResourceDisposer) mResourceDisposer.reset();
    if (mRootSignatureManager) mRootSignatureManager.reset();

    mWaitableOps.clear();

    if (mFence) mFence.Reset();
    if (mCommandQueue) mCommandQueue.Reset();
    if (mDevice) mDevice.Reset();

    mAdapter = nullptr;
    D3D12NI_LOG_DEBUG("Device destroyed");
}

bool NativeDevice::Init(IDXGIAdapter1* adapter, const NIPtr<Internal::ShaderLibrary>& shaderLibrary)
{
    if (adapter == nullptr) return false;

    mAdapter = adapter;
    mShaderLibrary = shaderLibrary;

    // we're asking for FL 11_0 for highest compatibility
    // we probably won't need anything higher than that
    HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create D3D12 Device");

    D3D12NI_LOG_DEBUG("Device created");

    if (!Internal::Debug::Instance().InitDeviceDebug(shared_from_this()))
    {
        D3D12NI_LOG_ERROR("Failed to initialize debug facilities for Device");
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC cqDesc;
    D3D12NI_ZERO_STRUCT(cqDesc);
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = mDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mCommandQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Direct Command Queue");

    hr = mCommandQueue->SetName(L"Main Command Queue");
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to name Direct Command Queue");

    mFenceValue = 0;
    hr = mDevice->CreateFence(mFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create in-device Fence");

    mCommandListPool = std::make_shared<Internal::CommandListPool>(shared_from_this());
    if (!mCommandListPool->Init(D3D12_COMMAND_LIST_TYPE_DIRECT, 8, 4))
    {
        D3D12NI_LOG_ERROR("Failed to initialize Command List Pool");
        return false;
    }

    mRootSignatureManager = std::make_shared<Internal::RootSignatureManager>(shared_from_this());
    if (!mRootSignatureManager->Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Root Signatures");
        return false;
    }

    mRenderingContext = std::make_shared<Internal::RenderingContext>(shared_from_this());
    if (!mRenderingContext->Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Rendering Context");
        return false;
    }

    mResourceDisposer = std::make_shared<Internal::ResourceDisposer>(shared_from_this());
    if (!mResourceDisposer)
    {
        D3D12NI_LOG_ERROR("Failed to initialize Resource Disposer");
        return false;
    }

    // TODO: D3D12: PERF these parameters need fine-tuning once the backend is feature complete
    mRingBuffer = std::make_shared<Internal::RingBuffer>(shared_from_this());
    if (!mRingBuffer->Init(1024 * 8 * Constants::MAX_BATCH_QUADS, 1024 * 6 * Constants::MAX_BATCH_QUADS))
    {
        D3D12NI_LOG_ERROR("Failed to initialize main Ring Buffer");
        return false;
    }

    mRingBuffer->SetDebugName("Main Ring Buffer");

    mConstantRingBuffer = std::make_shared<Internal::RingBuffer>(shared_from_this());
    if (!mConstantRingBuffer->Init(1024 * 1024, 768 * 1024))
    {
        D3D12NI_LOG_ERROR("Failed to initialize constant data Ring Buffer");
        return false;
    }

    mConstantRingBuffer->SetDebugName("Constant Ring Buffer");

    mRTVHeap = std::make_shared<Internal::DescriptorHeap>(shared_from_this());
    if (!mRTVHeap->Init(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false))
    {
        D3D12NI_LOG_ERROR("Failed to allocate RTV Descriptor Heap");
        return false;
    }

    mDSVHeap = std::make_shared<Internal::DescriptorHeap>(shared_from_this());
    if (!mDSVHeap->Init(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false))
    {
        D3D12NI_LOG_ERROR("Failed to allocate DSV Descriptor Heap");
        return false;
    }

    mRTVHeap->SetName("RenderTargetView Descriptor Heap");
    mDSVHeap->SetName("DepthStencilView Descriptor Heap");

    mSamplerStorage = std::make_shared<Internal::SamplerStorage>(shared_from_this());
    if (!mSamplerStorage->Init())
    {
        D3D12NI_LOG_ERROR("Failed to initialize Sampler Storage");
        return false;
    }

    if (!Build2DIndexBuffer()) return false;

    mPassthroughVS = GetInternalShader(Constants::PASSTHROUGH_VS_NAME);
    mPhongVS = GetInternalShader(Constants::PHONG_VS_NAME);

    return true;
}

NIPtr<Internal::Buffer> NativeDevice::CreateBuffer(const void* initialData, size_t size, bool cpuWriteable, D3D12_RESOURCE_STATES finalState)
{
    NIPtr<Internal::Buffer> ret = std::make_shared<Internal::Buffer>(shared_from_this());
    if (!ret->Init(initialData, size, cpuWriteable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT, finalState))
        return nullptr;

    return ret;
}

bool NativeDevice::CheckFormatSupport(DXGI_FORMAT format)
{
    if (format == DXGI_FORMAT_UNKNOWN)
    {
        // textures' format must be known
        // it also might mean conversion from PixelFormat failed
        // (ex. we stumbled upon an unknown or unsupported enum)
        return false;
    }

    D3D12_FEATURE_DATA_FORMAT_SUPPORT fmtSupport;
    D3D12NI_ZERO_STRUCT(fmtSupport);
    fmtSupport.Format = format;

    HRESULT hr = mDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &fmtSupport, sizeof(fmtSupport));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to check format support");

    return (fmtSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D);
}

NIPtr<NativeMesh>* NativeDevice::CreateMesh()
{
    return CreateNIDeviceObject<NativeMesh>(shared_from_this());
}

NIPtr<NativeMeshView>* NativeDevice::CreateMeshView(const NIPtr<NativeMesh>& mesh)
{
    return CreateNIDeviceObject<NativeMeshView>(shared_from_this(), mesh);
}

NIPtr<NativePhongMaterial>* NativeDevice::CreatePhongMaterial()
{
    return CreateNIDeviceObject<NativePhongMaterial>(shared_from_this());
}

NIPtr<NativeRenderTarget>* NativeDevice::CreateRenderTarget(const NIPtr<NativeTexture>& texture)
{
    return CreateNIDeviceObject<NativeRenderTarget>(shared_from_this(), texture);
}

NIPtr<NativeShader>* NativeDevice::CreateShader(const std::string& name, void* buf, UINT size)
{
    return CreateNIDeviceObject<NativeShader>(shared_from_this(), name, buf, size);
}

NIPtr<NativeTexture>* NativeDevice::CreateTexture(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
                                                  TextureUsage usage, TextureWrapMode wrapMode, int samples, bool useMipmap)
{
    return CreateNIDeviceObject<NativeTexture>(shared_from_this(), width, height, format, flags, usage, wrapMode, samples, useMipmap);
}

int NativeDevice::GetMaximumMSAASampleSize(DXGI_FORMAT format) const
{
    int maxSamples = 2;

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaLevels;
    D3D12NI_ZERO_STRUCT(msaaLevels);
    msaaLevels.Format = format;

    for (int i = maxSamples; i <= Constants::MAX_MSAA_SAMPLE_COUNT; i *= 2)
    {
        msaaLevels.SampleCount = i;
        HRESULT hr = mDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaLevels, sizeof(msaaLevels));
        if (SUCCEEDED(hr))
        {
            maxSamples = i;
        }
        else
        {
            break;
        }
    }

    return maxSamples;
}

int NativeDevice::GetMaximumTextureSize() const
{
    return D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}

void NativeDevice::MarkResourceDisposed(const D3D12ResourcePtr& resource)
{
    mResourceDisposer->MarkDisposed(resource);
}

void NativeDevice::Clear(float r, float g, float b, float a)
{
    mRenderingContext->Clear(r, g, b, a);
}

void NativeDevice::ClearTextureUnit(uint32_t unit)
{
    mRenderingContext->ClearTextureUnit(unit);
}

void NativeDevice::CopyToSwapChain(const NIPtr<NativeSwapChain>& dst, const NIPtr<NativeTexture>& src)
{
    src->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    dst->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_COPY_DEST);

    GetCurrentCommandList()->CopyResource(dst->GetCurrentBuffer().Get(), src->GetResource().Get());

    dst->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_PRESENT);
}

void NativeDevice::ResolveToSwapChain(const NIPtr<NativeSwapChain>& dst, const NIPtr<NativeTexture>& src)
{
    src->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    dst->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RESOLVE_DEST);

    GetCurrentCommandList()->ResolveSubresource(dst->GetCurrentBuffer().Get(), 0, src->GetResource().Get(), 0, src->GetFormat());

    dst->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_PRESENT);
}

void NativeDevice::RenderQuads(const Internal::MemoryView<float>& vertices, const Internal::MemoryView<signed char>& colors,
                               UINT elementCount)
{
    // index buffer size check - should not cross 4096 quads rendered
    // also serves as an overflow check
    if ((elementCount / 4) > Constants::MAX_BATCH_QUADS)
    {
        D3D12NI_LOG_WARN("Provided %d quads to render (max %d)", elementCount / 4, Constants::MAX_BATCH_QUADS);
        return;
    }

    // reserve space on Ring Buffer
    // This can cause a command list flush, so better do it early
    Internal::RingBuffer::Region vertexRegion = mRingBuffer->Reserve(elementCount * 8 * sizeof(float), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    if (!vertexRegion)
    {
        D3D12NI_LOG_ERROR("Ring Buffer allocation failed");
        return;
    }

    // move data to our Ring Buffer
    AssembleVertexData(vertexRegion.cpu, vertices, colors, elementCount);

    D3D12_VERTEX_BUFFER_VIEW vbView;
    vbView.BufferLocation = vertexRegion.gpu;
    vbView.SizeInBytes = static_cast<UINT>(vertexRegion.size);
    vbView.StrideInBytes = 8 * sizeof(float); // 3x pos, 1x uint32 color, 2x uv, 2x uv
    mRenderingContext->SetVertexBuffer(vbView);

    D3D12_INDEX_BUFFER_VIEW ibView;
    ibView.BufferLocation = m2DIndexBuffer->GetGPUPtr();
    ibView.SizeInBytes = static_cast<UINT>(m2DIndexBuffer->Size());
    ibView.Format = DXGI_FORMAT_R16_UINT;
    mRenderingContext->SetIndexBuffer(ibView);

    mRenderingContext->SetVertexShader(mPassthroughVS);
    mRenderingContext->SetPixelShader(mCurrent2DShader);
    mRenderingContext->SetCullMode(D3D12_CULL_MODE_NONE);
    mRenderingContext->SetFillMode(D3D12_FILL_MODE_SOLID);

    // apply Rendering Context details
    if (!mRenderingContext->Apply())
    {
        D3D12NI_LOG_ERROR("Failed to apply Rendering Context settings. Rendering skipped.");
        return;
    }

    // we separately ensure that textures bound to the Context are in correct state
    // there can be a situation where a Texture was bound to the Context and then updated
    // via updateTexture(). Its state will have to be re-set back to PIXEL_SHADER_RESOURCE
    // before the draw call.
    mRenderingContext->EnsureBoundTextureStates(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // draw the quads
    GetCurrentCommandList()->DrawIndexedInstanced((elementCount / 4) * 6, 1, 0, 0, 0);
}

void NativeDevice::RenderMeshView(const NIPtr<NativeMeshView>& meshView)
{
    // set shaders and their constants
    mRenderingContext->SetVertexShader(mPhongVS);

    const NIPtr<NativePhongMaterial>& material = meshView->GetMaterial();

    PhongShaderSpec spec;
    spec.mapping = material->GetMappingVariant();
    spec.lightCount = meshView->GetEnabledLightCount();
    spec.specular = material->GetSpecularVariant();
    spec.isSelfIllum = material->IsSelfIllum();

    const NIPtr<Internal::Shader>& ps = GetPhongPixelShader(spec);
    mRenderingContext->SetPixelShader(ps);

    // Transform data is set by RenderingContext, so here just set the Light data from MeshView
    uint32_t lightCount = meshView->GetEnabledLightCount();

    for (uint32_t i = 0; i < lightCount; ++i)
    {
        mPhongVS->SetConstantsInArray("gLight", i, meshView->GetVSLightSpecPtr(i), sizeof(VSLightSpec));
        ps->SetConstantsInArray("gLight", i, meshView->GetPSLightSpecPtr(i), sizeof(PSLightSpec));
    }

    ps->SetConstants("gColor", &meshView->GetPSColorSpec(), sizeof(PSColorSpec));

    const NIPtr<NativeMesh>& mesh = meshView->GetMesh();

    D3D12_VERTEX_BUFFER_VIEW vbView;
    vbView.BufferLocation = mesh->GetVertexBuffer()->GetGPUPtr();
    vbView.SizeInBytes = static_cast<UINT>(mesh->GetVertexBuffer()->Size());
    vbView.StrideInBytes = 9 * sizeof(float); // 3 * modelVertexPos; 2 * texD; 4 * modelVertexNormal
    mRenderingContext->SetVertexBuffer(vbView);

    D3D12_INDEX_BUFFER_VIEW ibView;
    ibView.BufferLocation = mesh->GetIndexBuffer()->GetGPUPtr();
    ibView.SizeInBytes = static_cast<UINT>(mesh->GetIndexBuffer()->Size());
    ibView.Format = mesh->GetIndexBufferFormat();
    mRenderingContext->SetIndexBuffer(ibView);

    mRenderingContext->SetCompositeMode(CompositeMode::SRC_OVER);
    mRenderingContext->SetCullMode(meshView->GetCullMode());
    mRenderingContext->SetFillMode(meshView->GetFillMode());

    for (uint32_t i = 0; i < static_cast<uint32_t>(TextureMapType::MAX_ENUM); ++i)
    {
        mRenderingContext->SetTexture(i, material->GetMap(static_cast<TextureMapType>(i)));
    }

    if (!mRenderingContext->Apply())
    {
        D3D12NI_LOG_ERROR("Failed to apply Rendering Context settings. Skipping mesh view rendering.");
        return;
    }

    mRenderingContext->EnsureBoundTextureStates(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    GetCurrentCommandList()->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
}

void NativeDevice::SetCompositeMode(CompositeMode mode)
{
    mRenderingContext->SetCompositeMode(mode);
}

void NativeDevice::UnsetPixelShader()
{
    mCurrent2DShader.reset();
}

void NativeDevice::SetPixelShader(const NIPtr<NativeShader>& ps)
{
    mCurrent2DShader = ps;
}

void NativeDevice::SetRenderTarget(const NIPtr<NativeRenderTarget>& target, bool enableDepthTest)
{
    target->SetDepthTestEnabled(enableDepthTest);
    mRenderingContext->SetRenderTarget(target);
}

void NativeDevice::SetScissor(bool enabled, int x1, int y1, int x2, int y2)
{
    D3D12_RECT scissor;
    scissor.left = x1;
    scissor.top = y1;
    scissor.right = x2;
    scissor.bottom = y2;
    mRenderingContext->SetScissor(enabled, scissor);
}

bool NativeDevice::SetShaderConstants(const NIPtr<NativeShader>& shader, const std::string& name, const void* data, size_t size)
{
    bool ret = shader->SetConstants(name, data, size);

    if (ret)
    {
        mRenderingContext->ClearResourcesApplied();
    }

    return ret;
}

void NativeDevice::SetTexture(uint32_t unit, const NIPtr<NativeTexture>& texture)
{
    mRenderingContext->SetTexture(unit, texture);
}

void NativeDevice::SetCameraPos(const Coords_XYZW_FLOAT& pos)
{
    mRenderingContext->SetCameraPos(pos);
}

void NativeDevice::SetWorldTransform(const Internal::Matrix<float>& matrix)
{
    mRenderingContext->SetWorldTransform(matrix);
}

void NativeDevice::SetViewProjTransform(const Internal::Matrix<float>& matrix)
{
    mRenderingContext->SetViewProjTransform(matrix);
}

bool NativeDevice::BlitTexture(const NIPtr<NativeRenderTarget>& srcRT, const Coords_Box_UINT32& src,
                               const NIPtr<NativeRenderTarget>& dstRT, const Coords_Box_UINT32& dst)
{
    D3D12NI_LOG_DEBUG("BlitTexture: from RT %S %ux%u - %ux%u %uxMSAA to RT %S %ux%u - %ux%u %uxMSAA",
        srcRT->GetTexture()->GetDebugName().c_str(), src.x0, src.y0, src.x1, src.y1, srcRT->GetMSAASamples(),
        dstRT->GetTexture()->GetDebugName().c_str(), dst.x0, dst.y0, dst.x1, dst.y1, dstRT->GetMSAASamples()
    );

    if (dstRT->GetMSAASamples() > 1)
    {
        // TODO: D3D12: should it? I'm pretty sure D3D9's StretchRect did not support it.
        D3D12NI_LOG_ERROR("BlitTexture() does not support MSAA destination textures");
        return false;
    }

    uint32_t srcWidth = src.x1 - src.x0;
    uint32_t srcHeight = src.y1 - src.y0;
    uint32_t dstWidth = dst.x1 - dst.x0;
    uint32_t dstHeight = dst.y1 - dst.y0;

    if (srcWidth == dstWidth && srcHeight == dstHeight)
    {
        if (srcRT->GetMSAASamples() == 1)
        {
            D3D12NI_LOG_DEBUG("BlitTexture: Fast non-MSAA path");

            D3D12_TEXTURE_COPY_LOCATION srcLoc;
            D3D12NI_ZERO_STRUCT(srcLoc);
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            srcLoc.pResource = srcRT->GetResource().Get();
            srcLoc.SubresourceIndex = 0;

            D3D12_BOX srcBox;
            D3D12NI_ZERO_STRUCT(srcBox);
            srcBox.left = src.x0;
            srcBox.top = src.y0;
            srcBox.right = src.x1;
            srcBox.bottom = src.y1;
            srcBox.front = 0;
            srcBox.back = 1;

            D3D12_TEXTURE_COPY_LOCATION dstLoc;
            D3D12NI_ZERO_STRUCT(dstLoc);
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.pResource = dstRT->GetResource().Get();
            dstLoc.SubresourceIndex = 0;

            srcRT->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_COPY_SOURCE);
            dstRT->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_COPY_DEST);

            GetCurrentCommandList()->CopyTextureRegion(&dstLoc, dst.x0, dst.y0, 0, &srcLoc, &srcBox);
        }
        else
        {
            D3D12NI_LOG_DEBUG("BlitTexture: Fast MSAA path");

            // use ResolveSubresourceRegion
            D3D12_RECT srcRect;
            srcRect.left = src.x0;
            srcRect.top = src.y0;
            srcRect.right = src.x1;
            srcRect.bottom = src.y1;

            srcRT->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            dstRT->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RESOLVE_DEST);

            GetCurrentCommandList()->ResolveSubresourceRegion(
                dstRT->GetResource().Get(), 0, dst.x0, dst.y0,
                srcRT->GetResource().Get(), 0, &srcRect,
                dstRT->GetTexture()->GetFormat(), D3D12_RESOLVE_MODE_AVERAGE
            );
        }
    }
    else
    {
        NIPtr<NativeTexture> sourceTexture;
        NIPtr<NativeTexture> intermediateTexture;

        // prepare quad vertices for blitting
        // we do this early to make potential ring buffer flush happen before a batch of GPU commands
        QuadVertices fsQuad = AssembleVertexQuadForBlit(src, dst);

        Internal::RingBuffer::Region vertexRegion = mRingBuffer->Reserve(fsQuad.size() * sizeof(Vertex_2D), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        if (vertexRegion.cpu == 0)
        {
            D3D12NI_LOG_ERROR("BlitTexture: Ring Buffer allocation failed");
            return false;
        }

        memcpy(vertexRegion.cpu, fsQuad.data(), fsQuad.size() * sizeof(Vertex_2D));

        if (srcRT->GetMSAASamples() > 1)
        {
            D3D12NI_LOG_DEBUG("BlitTexture: Slow MSAA path");

            // create intermediate tex, use ResolveSubresource() to populate it
            intermediateTexture = std::make_shared<NativeTexture>(shared_from_this());
            if (!intermediateTexture->Init(srcWidth, srcHeight, srcRT->GetTexture()->GetFormat(),
                D3D12_RESOURCE_FLAG_NONE, TextureUsage::DEFAULT, TextureWrapMode::CLAMP_NOT_NEEDED, 1, false))
            {
                D3D12NI_LOG_ERROR("BlitTexture: Failed to create intermediate texture for source RT resolve");
                return false;
            }

            srcRT->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
            intermediateTexture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RESOLVE_DEST);

            GetCurrentCommandList()->ResolveSubresource(
                intermediateTexture->GetResource().Get(), 0,
                srcRT->GetResource().Get(), 0, srcRT->GetTexture()->GetFormat()
            );

            sourceTexture = intermediateTexture;
        }
        else
        {
            D3D12NI_LOG_DEBUG("BlitTexture: Slow non-MSAA path");
            sourceTexture = srcRT->GetTexture();
        }

        // prepare rendering context for blit
        D3D12_VERTEX_BUFFER_VIEW vbView;
        vbView.BufferLocation = vertexRegion.gpu;
        vbView.SizeInBytes = static_cast<UINT>(vertexRegion.size);
        vbView.StrideInBytes = sizeof(Vertex_2D);
        mRenderingContext->SetVertexBuffer(vbView);

        D3D12_INDEX_BUFFER_VIEW ibView;
        ibView.BufferLocation = m2DIndexBuffer->GetGPUPtr();
        ibView.SizeInBytes = static_cast<UINT>(m2DIndexBuffer->Size());
        ibView.Format = DXGI_FORMAT_R16_UINT;
        mRenderingContext->SetIndexBuffer(ibView);

        const NIPtr<Internal::Shader>& blitShader = GetInternalShader("BlitPS");

        // temporarily store whatever context state we have right now
        mRenderingContext->StashParamters();

        mRenderingContext->SetVertexShader(mPassthroughVS);
        mRenderingContext->SetPixelShader(blitShader);

        mRenderingContext->SetCullMode(D3D12_CULL_MODE_NONE);
        mRenderingContext->SetFillMode(D3D12_FILL_MODE_SOLID);

        mRenderingContext->SetTexture(0, sourceTexture);
        mRenderingContext->SetRenderTarget(dstRT);
        mRenderingContext->SetWorldTransform(Internal::Matrix<float>::IDENTITY);
        mRenderingContext->SetViewProjTransform(Internal::Matrix<float>::IDENTITY);
        mRenderingContext->SetCompositeMode(CompositeMode::SRC);

        mRenderingContext->Apply();

        sourceTexture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dstRT->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_RENDER_TARGET);

        GetCurrentCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

        // restore original context parameters
        mRenderingContext->RestoreStashedParameters();
    }

    return true;
}

bool NativeDevice::ReadTexture(const NIPtr<NativeTexture>& texture, void* buffer, size_t bufferSize,
                               UINT srcx, UINT srcy, UINT srcw, UINT srch)
{
    DXGI_FORMAT format = texture->GetFormat();
    size_t bpp = GetDXGIFormatBPP(format);

    size_t readbackStride = Internal::Utils::Align<size_t>(srcw * bpp, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    size_t readbackBufferSize = srch * readbackStride;

    // path here is reverse to UpdateTexture but using a READBACK buffer
    // TODO: D3D12: consider using a separate Command Queue for transfer operations
    // TODO: D3D12: maybe this should be more than a simple Readback resource? investigate
    //              performance reasons, maybe we could avoid allocation here
    Internal::Buffer readbackBuffer(shared_from_this());
    if (!readbackBuffer.Init(nullptr, readbackBufferSize, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST))
    {
        D3D12NI_LOG_ERROR("Failed to initialize readback buffer for texture read");
        return false;
    }

    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.pResource = texture->GetResource().Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    D3D12_BOX srcBox;
    srcBox.left = srcx;
    srcBox.top = srcy;
    srcBox.right = srcx + srcw;
    srcBox.bottom = srcy + srch;
    srcBox.front = 0;
    srcBox.back = 1;

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    D3D12NI_ZERO_STRUCT(dstLoc);
    dstLoc.pResource = readbackBuffer.GetResource().Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint.Footprint.Width = srcw;
    dstLoc.PlacedFootprint.Footprint.Height = srch;
    dstLoc.PlacedFootprint.Footprint.Depth = 1;
    dstLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(readbackStride);
    dstLoc.PlacedFootprint.Footprint.Format = format;
    dstLoc.PlacedFootprint.Offset = 0;

    texture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_COPY_SOURCE);

    GetCurrentCommandList()->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

    // Flush the Command Queue to ensure data was read and wait for it
    // mid-frame waitable is reset here to ensure our copy is included in the execution
    FlushCommandList();

    mMidFrameWaitable.reset();
    SignalMidFrame();
    WaitMidFrame();

    void* readbackPtr = readbackBuffer.Map();
    if (readbackPtr == nullptr)
    {
        D3D12NI_LOG_ERROR("Failed to map readback buffer for texture read");
        return false;
    }

    if (format == DXGI_FORMAT_B8G8R8X8_UNORM)
    {
        size_t srcStrideElems = readbackStride / bpp;

        for (size_t y = 0; y < srch; ++y)
        {
            const Pixel_BGRA8_UNORM* srcp = reinterpret_cast<const Pixel_BGRA8_UNORM*>(readbackPtr) + (y * srcStrideElems);
            Pixel_RGB8_UNORM* dstp = reinterpret_cast<Pixel_RGB8_UNORM*>(buffer) + (y * srcw);
            for (size_t x = 0; x < srcw; ++x)
            {
                dstp[x].r = srcp[x].r;
                dstp[x].g = srcp[x].g;
                dstp[x].b = srcp[x].b;
            }
        }
    }
    else
    {
        for (size_t y = 0; y < srch; ++y)
        {
            const uint8_t* srcPtr = reinterpret_cast<const uint8_t*>(readbackPtr) + (y * readbackStride);
            uint8_t* dstPtr = reinterpret_cast<uint8_t*>(buffer) + (y * srcw * bpp);

            memcpy(dstPtr, srcPtr, srcw * bpp);
        }
    }

    readbackBuffer.Unmap();

    return true;
}

bool NativeDevice::GenerateMipmaps(const NIPtr<NativeTexture>& texture)
{
    if (!texture->HasMipmaps()) return true;

    uint32_t mipLevels = texture->GetMipLevels();

    const NIPtr<Internal::Shader>& cs = GetInternalShader("MipmapGenCS");
    mRenderingContext->SetComputeShader(cs);
    mRenderingContext->SetTexture(0, texture);

    uint32_t srcWidth = static_cast<uint32_t>(texture->GetWidth());
    uint32_t srcHeight = static_cast<uint32_t>(texture->GetHeight());

    // transition entire texture with mips to UAV state
    texture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // starting from 1, level 0 is our base mip level
    // also note, we're divinding by 2 a lot, so to make it faster we'll bit-shift instead
    Internal::MipmapGenComputeShader::CBuffer constants;
    uint32_t mipMapCount = mipLevels - 1; // mipLevels includes base level so we need to skip it
    for (uint32_t mipBase = 0; mipBase < mipMapCount; mipBase += constants.numLevels)
    {
        // dimensions of first mipmap
        uint32_t mip1Width = (srcWidth >> 1);
        uint32_t mip1Height = (srcHeight >> 1);

        // check how many times we can downsample at once
        // we guarantee at least one downsample (MipmapGenCS has the initial "heavy" path for that purpose)
        // but if both mip1Width and mip1Height have any zero-LSBs past that, we can do more downsamples within
        // one dispatch and save some CS invocations. Worst case scenario our texture dimensions have all LSB
        // bits set to 1, but it is an extremely rare occurence.
        uint32_t widthZeros = Internal::Utils::CountZeroBitsLSB(mip1Width, 3);
        uint32_t heightZeros = Internal::Utils::CountZeroBitsLSB(mip1Height, 3);
        uint32_t levels = 1 + std::min<uint32_t>(widthZeros, heightZeros);

        constants.sourceLevel = mipBase; // base level is one higher than our first mip
        constants.numLevels = std::min<uint32_t>(levels, mipMapCount - mipBase);
        constants.texelSizeMip1[0] = 1.0f / mip1Width;
        constants.texelSizeMip1[1] = 1.0f / mip1Height;
        cs->SetConstants("gData", &constants, sizeof(constants));

        mRenderingContext->ClearComputeResourcesApplied();

        if (!mRenderingContext->ApplyCompute())
        {
            D3D12NI_LOG_ERROR("Failed to apply Rendering Context Compute settings");
            return false;
        }

        // transition base level to non-PS-resource
        texture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                             Internal::Utils::CalcSubresource(mipBase, texture->GetMipLevels(), 0));

        // each thread group manages an 8x8 square, so we need to dispatch (width/8) X groups and (height/8) Y groups
        GetCurrentCommandList()->Dispatch(std::max<UINT>(srcWidth >> 3, 1), std::max<UINT>(srcHeight >> 3, 1), 1);

        // transition base level back to UAV
        // this should make all subresources have the same state again
        texture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                             Internal::Utils::CalcSubresource(mipBase, texture->GetMipLevels(), 0));

        srcWidth >>= constants.numLevels;
        srcHeight >>= constants.numLevels;
    }

    return true;
}

bool NativeDevice::UpdateTexture(const NIPtr<NativeTexture>& texture, const void* data, size_t dataSizeBytes, PixelFormat srcFormat,
                                 UINT dstx, UINT dsty, UINT srcx, UINT srcy, UINT srcw, UINT srch, UINT srcstride)
{
    size_t targetSize = Internal::TextureUploader::EstimateTargetSize(srcw, srch, texture->GetFormat());

    // first, source data must land on the Ring Buffer
    // TODO: D3D12: consider using a separate Command Queue for transfer operations

    Internal::TextureUploader uploader;
    uploader.SetSource(data, dataSizeBytes, srcFormat, srcx, srcy, srcw, srch, srcstride);

    // TODO: D3D12: this threshold might be another optimization point
    size_t copyThreshold = mRingBuffer->Size() / 2;
    bool useStagingBuffer = targetSize > copyThreshold;

    Internal::RingBuffer::Region ringRegion;
    Internal::Buffer stagingBuffer(shared_from_this());
    if (useStagingBuffer)
    {
        // for larger textures allocate a dedicated staging buffer
        // uploader will handle its initialization
        if (!stagingBuffer.Init(nullptr, targetSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ))
        {
            D3D12NI_LOG_ERROR("Failed to allocate a staging buffer for large texture upload");
            return false;
        }

        uploader.SetTarget(stagingBuffer.Map(), stagingBuffer.Size(), texture->GetFormat());
    }
    else
    {
        // Alignment can be reduced if we update a "small" texture.
        // There's plenty of requirements which really boil down to target size being
        // lower than default alignment, which is 64K.
        // This optimizes memory consumption a bit in the Ring Buffer.
        size_t alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        if (targetSize < D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
            alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;

        // copying smaller textures will go via the Ring Buffer to prevent unnecessary allocation
        ringRegion = mRingBuffer->Reserve(targetSize, alignment);
        if (ringRegion.cpu == nullptr)
        {
            D3D12NI_LOG_ERROR("Failed to reserve space in the Ring Buffer (full?)");
            return false;
        }

        uploader.SetTarget(ringRegion.cpu, ringRegion.size, texture->GetFormat());
    }

    if (!uploader.Upload())
    {
        D3D12NI_LOG_ERROR("Failed to upload texture data to Ring Buffer");
        return false;
    }

    if (useStagingBuffer)
    {
        stagingBuffer.Unmap();
    }

    // now we can record a data move from staging or Ring Buffer to our Texture
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    D3D12NI_ZERO_STRUCT(srcLoc);
    srcLoc.pResource = useStagingBuffer ? stagingBuffer.GetResource().Get() : mRingBuffer->GetResource().Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Footprint.Width = srcw;
    srcLoc.PlacedFootprint.Footprint.Height = srch;
    srcLoc.PlacedFootprint.Footprint.Depth = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = uploader.GetTargetStride();
    srcLoc.PlacedFootprint.Footprint.Format = uploader.GetTargetFormat();
    srcLoc.PlacedFootprint.Offset = useStagingBuffer ? 0 : ringRegion.offsetFromStart;

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    D3D12NI_ZERO_STRUCT(dstLoc);
    dstLoc.pResource = texture->GetResource().Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    // Ensure we are in COPY_DEST state. Texture can be now bound to RenderingContext
    // and exist in a different state.
    texture->EnsureState(GetCurrentCommandList(), D3D12_RESOURCE_STATE_COPY_DEST);

    GetCurrentCommandList()->CopyTextureRegion(&dstLoc, dstx, dsty, 0, &srcLoc, nullptr);

    GenerateMipmaps(texture);

    return true;
}

void NativeDevice::FlushCommandList()
{
    mCommandListPool->SubmitCurrentCommandList();
    mRenderingContext->ClearAppliedFlags();
}

void NativeDevice::FinishFrame()
{
    FlushCommandList();

    // in a short bit we're going to wait for Present to complete
    // so whichever last mid-frame wait we have stored will be done in SwapChain regardless
    mMidFrameWaitable.reset();

    mFrameCounter++;
}

void NativeDevice::Execute(const std::vector<ID3D12CommandList*>& commandLists)
{
    mCommandQueue->ExecuteCommandLists(static_cast<UINT>(commandLists.size()), commandLists.data());
}

void NativeDevice::RegisterWaitableOperation(Internal::IWaitableOperation* waitableOp)
{
    mWaitableOps.emplace_back(waitableOp);
}

void NativeDevice::UnregisterWaitableOperation(Internal::IWaitableOperation* waitableOp)
{
    for (size_t i = 0; i < mWaitableOps.size(); ++i)
    {
        if (mWaitableOps[i] == waitableOp)
        {
            if (i != mWaitableOps.size() - 1)
            {
                mWaitableOps[i] = mWaitableOps[mWaitableOps.size() - 1];
            }

            mWaitableOps.pop_back();
        }
    }
}

// Signal() is separate and not called everytime Execute() is called
// because we need to call it in SwapChain after we Present()
NIPtr<Internal::Waitable> NativeDevice::Signal()
{
    mFenceValue++;
    if (mFenceValue == 0) mFenceValue++;

    // mark this point in time in places that need it
    for (Internal::IWaitableOperation* op: mWaitableOps)
    {
        op->OnQueueSignal(mFenceValue);
    }

    NIPtr<Internal::Waitable> waitable = std::make_shared<Internal::Waitable>(mFenceValue);

    HRESULT hr = mFence->SetEventOnCompletion(mFenceValue, waitable->GetHandle());
    D3D12NI_RET_IF_FAILED(hr, nullptr, "Failed to set Fence event on completion");

    hr = mCommandQueue->Signal(mFence.Get(), mFenceValue);
    D3D12NI_RET_IF_FAILED(hr, nullptr, "Failed to signal event on completion");

    waitable->SetFinishedCallback([this](uint64_t fenceValue) -> bool
    {
        for (Internal::IWaitableOperation* op: mWaitableOps)
        {
            op->OnFenceSignaled(fenceValue);
        }

        return true;
    });

    return waitable;
}

void NativeDevice::AdvanceCommandAllocator()
{
    mCommandListPool->AdvanceCommandAllocator(mFenceValue);
}

void NativeDevice::SignalMidFrame()
{
    if (MidFrameSignaled())
        return;

    mMidFrameWaitable = Signal();
}

bool NativeDevice::WaitMidFrame()
{
    if (!mMidFrameWaitable) return true;

    bool result = mMidFrameWaitable->Wait();
    mMidFrameWaitable.reset();
    return result;
}

} // namespace D3D12


#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nReleaseNativeObject
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

    D3D12::FreeNIObject<D3D12::NativeDevice>(ptr);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCheckFormatSupport
    (JNIEnv* env, jobject obj, jlong ptr, jint format)
{
    if (!ptr) return false;
    if (format < 0) return false;

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CheckFormatSupport(static_cast<DXGI_FORMAT>(format));
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCreateMesh
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return 0;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CreateMesh());
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCreateMeshView
    (JNIEnv* env, jobject obj, jlong ptr, jlong meshPtr)
{
    if (!ptr) return 0;
    if (!meshPtr) return 0;

    const D3D12::NIPtr<D3D12::NativeMesh>& mesh = D3D12::GetNIObject<D3D12::NativeMesh>(meshPtr);

    if (!mesh) return 0;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CreateMeshView(mesh));
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCreatePhongMaterial
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return 0;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CreatePhongMaterial());
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCreateRenderTarget
    (JNIEnv* env, jobject obj, jlong ptr, jlong texturePtr)
{
    if (!ptr) return 0;
    if (!texturePtr) return 0;

    const D3D12::NIPtr<D3D12::NativeTexture>& texture = D3D12::GetNIObject<D3D12::NativeTexture>(texturePtr);
    if (!texture) return 0;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CreateRenderTarget(texture));
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCreateShader
    (JNIEnv* env, jobject obj, jlong ptr, jstring name, jobject codeBBuf)
{
    if (!ptr) return 0;

    void* codeBuf = env->GetDirectBufferAddress(codeBBuf);
    jlong codeBufSize = env->GetDirectBufferCapacity(codeBBuf);
    if (codeBuf == nullptr || codeBufSize <= 0)
    {
        D3D12NI_LOG_ERROR("Failed to get shader code buffer address");
        return 0;
    }

    D3D12::Internal::JNIString nameJString(env, name);
    if (nameJString == nullptr)
    {
        D3D12NI_LOG_ERROR("Failed to get shader name string");
        return false;
    }

    std::string nameStr(nameJString);

    return reinterpret_cast<jlong>(
        D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CreateShader(nameStr, codeBuf, static_cast<UINT>(codeBufSize))
    );
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCreateTexture
    (JNIEnv* env, jobject obj, jlong ptr, jint width, jint height, jint format,
     jint usage, jint wrapMode, jint samples, jboolean useMipmap, jboolean isRTT)
{
    if (!ptr) return 0;

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (isRTT) flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CreateTexture(
        static_cast<UINT>(width), static_cast<UINT>(height), static_cast<DXGI_FORMAT>(format), flags,
        static_cast<D3D12::TextureUsage>(usage), static_cast<D3D12::TextureWrapMode>(wrapMode),
        samples, static_cast<bool>(useMipmap)
    ));
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nGetMaximumMSAASampleSize
    (JNIEnv* env, jobject obj, jlong ptr, jint dxgiFormat)
{
    if (!ptr) return 0;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->GetMaximumMSAASampleSize(static_cast<DXGI_FORMAT>(dxgiFormat)));
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nGetMaximumTextureSize
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return 0;

    return static_cast<jint>(D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->GetMaximumTextureSize());
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nClear
    (JNIEnv* env, jobject obj, jlong ptr, jfloat r, jfloat g, jfloat b, jfloat a)
{
    if (!ptr) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->Clear(r, g, b, a);
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nCopyToSwapChain
    (JNIEnv* env, jobject obj, jlong ptr, jlong swapchainDstPtr, jlong textureSrcPtr)
{
    if (!ptr) return;
    if (!swapchainDstPtr) return;
    if (!textureSrcPtr) return;

    const D3D12::NIPtr<D3D12::NativeSwapChain>& swapchainDst = D3D12::GetNIObject<D3D12::NativeSwapChain>(swapchainDstPtr);
    const D3D12::NIPtr<D3D12::NativeTexture>& textureSrc = D3D12::GetNIObject<D3D12::NativeTexture>(textureSrcPtr);

    if (!swapchainDst) return;
    if (!textureSrc) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->CopyToSwapChain(swapchainDst, textureSrc);
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nResolveToSwapChain
    (JNIEnv* env, jobject obj, jlong ptr, jlong swapchainDstPtr, jlong textureSrcPtr)
{
    if (!ptr) return;
    if (!swapchainDstPtr) return;
    if (!textureSrcPtr) return;

    const D3D12::NIPtr<D3D12::NativeSwapChain>& swapchainDst = D3D12::GetNIObject<D3D12::NativeSwapChain>(swapchainDstPtr);
    const D3D12::NIPtr<D3D12::NativeTexture>& textureSrc = D3D12::GetNIObject<D3D12::NativeTexture>(textureSrcPtr);

    if (!swapchainDst) return;
    if (!textureSrc) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->ResolveToSwapChain(swapchainDst, textureSrc);
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nRenderQuads
    (JNIEnv* env, jobject obj, jlong ptr, jfloatArray vertices, jbyteArray colors, jint elementCount)
{
    if (!ptr) return;
    if (elementCount <= 0) return;

    D3D12::Internal::JNIBuffer<jfloatArray> vertsArray(env, nullptr, vertices);
    D3D12::Internal::JNIBuffer<jbyteArray> colorsArray(env, nullptr, colors);

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->RenderQuads(
        D3D12::Internal::MemoryView<float>(reinterpret_cast<const float*>(vertsArray.Data()), vertsArray.Size()),
        D3D12::Internal::MemoryView<signed char>(reinterpret_cast<const signed char*>(colorsArray.Data()), colorsArray.Size()),
        static_cast<UINT>(elementCount)
    );
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nRenderMeshView
    (JNIEnv* env, jobject obj, jlong ptr, jlong meshViewPtr)
{
    if (!ptr) return;
    if (!meshViewPtr) return;

    const D3D12::NIPtr<D3D12::NativeMeshView>& meshView = D3D12::GetNIObject<D3D12::NativeMeshView>(meshViewPtr);
    if (!meshView) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->RenderMeshView(meshView);
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetCompositeMode
    (JNIEnv* env, jobject obj, jlong ptr, jint compositeMode)
{
    if (!ptr) return;

    if (compositeMode < static_cast<jint>(D3D12::CompositeMode::CLEAR) ||
        compositeMode >= static_cast<jint>(D3D12::CompositeMode::MAX_ENUM))
    {
        D3D12NI_LOG_ERROR("Invalid compositeMode received on native backend");
        return;
    }

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetCompositeMode(static_cast<D3D12::CompositeMode>(compositeMode));
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetPixelShader
    (JNIEnv* env, jobject obj, jlong ptr, jlong pixelShaderPtr)
{
    if (!ptr) return;

    if (!pixelShaderPtr)
    {
        // NOTE: I didn't observe this path being taken by Prism ever. It might not work
        //       or cause some unexpected issues if it ever gets hit.
        D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->UnsetPixelShader();
    }

    const D3D12::NIPtr<D3D12::NativeShader>& ps = D3D12::GetNIObject<D3D12::NativeShader>(pixelShaderPtr);
    if (!ps) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetPixelShader(ps);
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetRenderTarget
    (JNIEnv* env, jobject obj, jlong ptr, jlong renderTargetPtr, jboolean enableDepthTest)
{
    if (!ptr) return;
    if (!renderTargetPtr) return;

    const D3D12::NIPtr<D3D12::NativeRenderTarget>& renderTarget = D3D12::GetNIObject<D3D12::NativeRenderTarget>(renderTargetPtr);
    if (!renderTarget) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetRenderTarget(renderTarget, static_cast<bool>(enableDepthTest));
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetScissor
    (JNIEnv* env, jobject obj, jlong ptr, jboolean enabled, jint x1, jint y1, jint x2, jint y2)
{
    if (!ptr) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetScissor(enabled, x1, y1, x2, y2);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetShaderConstantsF
    (JNIEnv* env, jobject obj, jlong ptr, jlong shaderPtr, jstring name, jobject floatBuf, jint offset, jint count)
{
    if (!ptr) return false;
    if (!shaderPtr) return false;
    if (!name) return false;
    if (!floatBuf) return false;
    if (offset < 0) return false;
    if (count <= 0) return false;

    const D3D12::NIPtr<D3D12::NativeShader>& shader = D3D12::GetNIObject<D3D12::NativeShader>(shaderPtr);
    if (!shader) return false;

    D3D12::Internal::JNIBuffer<jfloatArray> buffer(env, floatBuf, nullptr);
    D3D12::Internal::JNIString nameJStr(env, name);
    std::string nameStr(nameJStr);

    if (buffer.Data() == nullptr) return false;
    if (offset + count > buffer.Size()) return false;

    size_t sizeBytes = static_cast<size_t>(count) * sizeof(jfloat);
    size_t offsetBytes = static_cast<size_t>(offset) * sizeof(jfloat);

    const uint8_t* srcPtr = reinterpret_cast<const uint8_t*>(buffer.Data()) + offsetBytes;

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetShaderConstants(shader, nameStr, srcPtr, sizeBytes);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetShaderConstantsI
    (JNIEnv* env, jobject obj, jlong ptr, jlong shaderPtr, jstring name, jobject intBuf, jint offset, jint count)
{
    if (!ptr) return false;
    if (!shaderPtr) return false;
    if (!name) return false;
    if (!intBuf) return false;
    if (offset < 0) return false;
    if (count <= 0) return false;

    const D3D12::NIPtr<D3D12::NativeShader>& shader = D3D12::GetNIObject<D3D12::NativeShader>(shaderPtr);
    if (!shader) return false;

    D3D12::Internal::JNIBuffer<jintArray> buffer(env, intBuf, nullptr);
    D3D12::Internal::JNIString nameJStr(env, name);
    std::string nameStr(nameJStr);

    if (buffer.Data() == nullptr) return false;
    if (offset + count > buffer.Size()) return false;

    size_t sizeBytes = static_cast<size_t>(count) * sizeof(jint);
    size_t offsetBytes = static_cast<size_t>(offset) * sizeof(jint);

    const uint8_t* srcPtr = reinterpret_cast<const uint8_t*>(buffer.Data()) + offsetBytes;

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetShaderConstants(shader, nameStr, srcPtr, sizeBytes);
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetTexture
    (JNIEnv* env, jobject obj, jlong ptr, jint unit, jlong texturePtr)
{
    if (!ptr) return;
    if (unit < 0) return;

    if (texturePtr)
    {
        const D3D12::NIPtr<D3D12::NativeTexture>& tex = D3D12::GetNIObject<D3D12::NativeTexture>(texturePtr);
        if (!tex) return;

        D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetTexture(static_cast<uint32_t>(unit), tex);
    }
    else
    {
        D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->ClearTextureUnit(static_cast<uint32_t>(unit));
    }
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetCameraPos
    (JNIEnv* env, jobject obj, jlong ptr, jdouble x, jdouble y, jdouble z)
{
    if (!ptr) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetCameraPos(D3D12::Coords_XYZW_FLOAT{
        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1.0f
    });
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetViewProjTransform
    (JNIEnv* env, jobject obj, jlong ptr,
     jdouble m00, jdouble m01, jdouble m02, jdouble m03,
     jdouble m10, jdouble m11, jdouble m12, jdouble m13,
     jdouble m20, jdouble m21, jdouble m22, jdouble m23,
     jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    if (!ptr) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetViewProjTransform(D3D12::Internal::Matrix<float>(
        static_cast<float>(m00), static_cast<float>(m01), static_cast<float>(m02), static_cast<float>(m03),
        static_cast<float>(m10), static_cast<float>(m11), static_cast<float>(m12), static_cast<float>(m13),
        static_cast<float>(m20), static_cast<float>(m21), static_cast<float>(m22), static_cast<float>(m23),
        static_cast<float>(m30), static_cast<float>(m31), static_cast<float>(m32), static_cast<float>(m33)
    ));
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nSetWorldTransform
    (JNIEnv* env, jobject obj, jlong ptr,
     jdouble m00, jdouble m01, jdouble m02, jdouble m03,
     jdouble m10, jdouble m11, jdouble m12, jdouble m13,
     jdouble m20, jdouble m21, jdouble m22, jdouble m23,
     jdouble m30, jdouble m31, jdouble m32, jdouble m33)
{
    if (!ptr) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->SetWorldTransform(D3D12::Internal::Matrix<float>(
        static_cast<float>(m00), static_cast<float>(m01), static_cast<float>(m02), static_cast<float>(m03),
        static_cast<float>(m10), static_cast<float>(m11), static_cast<float>(m12), static_cast<float>(m13),
        static_cast<float>(m20), static_cast<float>(m21), static_cast<float>(m22), static_cast<float>(m23),
        static_cast<float>(m30), static_cast<float>(m31), static_cast<float>(m32), static_cast<float>(m33)
    ));
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nBlitTexture
    (JNIEnv* env, jobject obj, jlong ptr, jlong srcRTPtr, jint srcX0, jint srcY0, jint srcX1, jint srcY1,
                                          jlong dstRTPtr, jint dstX0, jint dstY0, jint dstX1, jint dstY1)
{
    if (!ptr) return false;
    if (!srcRTPtr) return false;
    if (!dstRTPtr) return false;
    if (srcX0 < 0 || srcY0 < 0 || srcX1 < 0 || srcY1 < 0) return false;
    if (srcX0 > srcX1 || srcY0 > srcY1) return false;
    if (dstX0 < 0 || dstY0 < 0 || dstX1 < 0 || dstY1 < 0) return false;
    if (dstX0 > dstX1 || dstY0 > dstY1) return false;

    const D3D12::NIPtr<D3D12::NativeRenderTarget>& srcRT = D3D12::GetNIObject<D3D12::NativeRenderTarget>(srcRTPtr);
    if (!srcRT) return false;

    const D3D12::NIPtr<D3D12::NativeRenderTarget>& dstRT = D3D12::GetNIObject<D3D12::NativeRenderTarget>(dstRTPtr);
    if (!dstRT) return false;

    D3D12::Coords_Box_UINT32 srcBox{static_cast<uint32_t>(srcX0), static_cast<uint32_t>(srcY0),
                                    static_cast<uint32_t>(srcX1), static_cast<uint32_t>(srcY1)};
    D3D12::Coords_Box_UINT32 dstBox{static_cast<uint32_t>(dstX0), static_cast<uint32_t>(dstY0),
                                    static_cast<uint32_t>(dstX1), static_cast<uint32_t>(dstY1)};

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->BlitTexture(srcRT, srcBox, dstRT, dstBox);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nReadTextureB
    (JNIEnv* env, jobject obj, jlong ptr, jlong srcTexturePtr, jobject buf, jbyteArray array, jint x, jint y, jint w, jint h)
{
    if (!ptr) return false;
    if (!srcTexturePtr) return false;
    if (x < 0 || y < 0 || w < 0 || h < 0) return false;

    const D3D12::NIPtr<D3D12::NativeTexture>& tex = D3D12::GetNIObject<D3D12::NativeTexture>(srcTexturePtr);
    if (!tex) return false;

    D3D12::Internal::JNIBuffer<jbyteArray> data(env, buf, array);

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->ReadTexture(tex, data.Data(), data.Size(),
        static_cast<UINT>(x), static_cast<UINT>(y), static_cast<UINT>(w), static_cast<UINT>(h)
    );
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nReadTextureI
    (JNIEnv* env, jobject obj, jlong ptr, jlong srcTexturePtr, jobject buf, jintArray array, jint x, jint y, jint w, jint h)
{
    if (!ptr) return false;
    if (!srcTexturePtr) return false;
    if (x < 0 || y < 0 || w < 0 || h < 0) return false;

    const D3D12::NIPtr<D3D12::NativeTexture>& tex = D3D12::GetNIObject<D3D12::NativeTexture>(srcTexturePtr);
    if (!tex) return false;

    D3D12::Internal::JNIBuffer<jintArray> data(env, buf, array);

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->ReadTexture(tex, data.Data(), data.Size(),
        static_cast<UINT>(x), static_cast<UINT>(y), static_cast<UINT>(w), static_cast<UINT>(h)
    );
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nUpdateTextureF
    (JNIEnv* env, jobject obj, jlong ptr, jlong texturePtr, jobject dataBuf, jfloatArray dataArray, jint pixelFormat,
     jint dstx, jint dsty, jint srcx, jint srcy, jint srcw, jint srch, jint srcscan)
{
    if (!ptr) return false;
    if (!texturePtr) return false;

    const D3D12::NIPtr<D3D12::NativeTexture>& tex = D3D12::GetNIObject<D3D12::NativeTexture>(texturePtr);
    if (!tex) return false;

    D3D12::Internal::JNIBuffer<jfloatArray> data(env, dataBuf, dataArray);

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->UpdateTexture(tex, data.Data(), data.Size(),
        static_cast<D3D12::PixelFormat>(pixelFormat), dstx, dsty, srcx, srcy, srcw, srch, srcscan
    );
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nUpdateTextureI
    (JNIEnv* env, jobject obj, jlong ptr, jlong texturePtr, jobject dataBuf, jintArray dataArray, jint pixelFormat,
     jint dstx, jint dsty, jint srcx, jint srcy, jint srcw, jint srch, jint srcscan)
{
    if (!ptr) return false;
    if (!texturePtr) return false;

    const D3D12::NIPtr<D3D12::NativeTexture>& tex = D3D12::GetNIObject<D3D12::NativeTexture>(texturePtr);
    if (!tex) return false;

    D3D12::Internal::JNIBuffer<jintArray> data(env, dataBuf, dataArray);

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->UpdateTexture(tex, data.Data(), data.Size(),
        static_cast<D3D12::PixelFormat>(pixelFormat), dstx, dsty, srcx, srcy, srcw, srch, srcscan
    );
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nUpdateTextureB
    (JNIEnv* env, jobject obj, jlong ptr, jlong texturePtr, jobject dataBuf, jbyteArray dataArray, jint pixelFormat,
     jint dstx, jint dsty, jint srcx, jint srcy, jint srcw, jint srch, jint srcscan)
{
    if (!ptr) return false;
    if (!texturePtr) return false;

    const D3D12::NIPtr<D3D12::NativeTexture>& tex = D3D12::GetNIObject<D3D12::NativeTexture>(texturePtr);
    if (!tex) return false;

    D3D12::Internal::JNIBuffer<jbyteArray> data(env, dataBuf, dataArray);

    return D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->UpdateTexture(tex, data.Data(), data.Size(),
        static_cast<D3D12::PixelFormat>(pixelFormat), dstx, dsty, srcx, srcy, srcw, srch, srcscan
    );
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeDevice_nFinishFrame
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

    D3D12::GetNIObject<D3D12::NativeDevice>(ptr)->FinishFrame();
}

#ifdef __cplusplus
}
#endif
