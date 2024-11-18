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

#include "D3D12PSOManager.hpp"

#include "../D3D12NativeDevice.hpp"
#include "../D3D12Constants.hpp"

#if DEBUG
#include <codecvt>
#endif // DEBUG


namespace D3D12 {
namespace Internal {

D3D12_BLEND_DESC PSOManager::FormBlendState(CompositeMode mode)
{
    D3D12_BLEND_DESC state;
    state.AlphaToCoverageEnable = false;
    state.IndependentBlendEnable = false;
    state.RenderTarget[0].BlendEnable = true;
    state.RenderTarget[0].LogicOpEnable = false;
    // Blend Ops are ADD because D3D9 only supported Add
    // The equation is: (texel * SrcBlend) + (pixel * DestBlend)
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/alpha-texture-blending
    state.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    state.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    state.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    switch (mode)
    {
    case CompositeMode::CLEAR:
        state.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
        state.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        state.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        state.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        break;
    case CompositeMode::SRC:
        state.RenderTarget[0].BlendEnable = false;
        break;
    case CompositeMode::SRC_OVER:
        state.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        state.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        state.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        state.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        break;
    case CompositeMode::DST_OUT:
        state.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
        state.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        state.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        state.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        break;
    case CompositeMode::ADD:
        state.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        state.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        state.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        state.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        break;
    }

    return state;
}

bool PSOManager::ConstructNewPSO(const PSOParameters& params)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
    D3D12NI_ZERO_STRUCT(desc);

    if (params.vertexShader->GetMode() != params.pixelShader->GetMode())
    {
        // should not happen
        D3D12NI_LOG_ERROR("Tried to combine incompatible vertex and pixel shaders (vertex %s mode %d, pixel %s mode %d)",
            params.vertexShader->GetName().c_str(), static_cast<int>(params.vertexShader->GetMode()),
            params.pixelShader->GetName().c_str(), static_cast<int>(params.pixelShader->GetMode())
        );
        return false;
    }

    // shaders
    desc.VS = params.vertexShader->GetBytecode();
    desc.PS = params.pixelShader->GetBytecode();

    if (params.pixelShader->GetMode() == ShaderPipelineMode::UI_2D)
    {
        const NIPtr<NativeShader>& niShader = std::dynamic_pointer_cast<NativeShader>(params.pixelShader);
        desc.pRootSignature = niShader->GetRootSignature().Get();
    }
    else
    {
        desc.pRootSignature = mPhongRootSignature.Get();
    }

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    if (params.vertexShader->GetMode() == ShaderPipelineMode::UI_2D)
    {
        desc.InputLayout.pInputElementDescs = m2DInputLayout.data();
        desc.InputLayout.NumElements = static_cast<UINT>(m2DInputLayout.size());
    }
    else if (params.vertexShader->GetMode() == ShaderPipelineMode::PHONG_3D)
    {
        desc.InputLayout.pInputElementDescs = m3DInputLayout.data();
        desc.InputLayout.NumElements = static_cast<UINT>(m3DInputLayout.size());
    }

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = params.msaaSamples;
    desc.SampleDesc.Quality = 0;

    desc.SampleMask = 0xFFFFFFFF;

    desc.BlendState = FormBlendState(params.compositeMode);

    desc.RasterizerState.CullMode = params.cullMode;
    desc.RasterizerState.FillMode = params.fillMode;
    desc.RasterizerState.FrontCounterClockwise = true;

    desc.DepthStencilState.StencilEnable = false;

    if (params.enableDepthTest)
    {
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.DepthStencilState.DepthEnable = true;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    }
    else
    {
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        desc.DepthStencilState.DepthEnable = false;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    }

    D3D12PipelineStatePtr pipelineState;
    HRESULT hr = mNativeDevice->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Graphics Pipeline State");

#if DEBUG
    std::wstring name = L"GPSO-";
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    name += converter.from_bytes(params.vertexShader->GetName());
    name += L"-";
    name += converter.from_bytes(params.pixelShader->GetName());
    name += L"-";
    name += CompositeModeToWString(params.compositeMode);
    name += L"-";
    name += std::to_wstring(params.msaaSamples) + L"xMSAA";

    if (params.enableDepthTest)
    {
        name += L"-Depth";
    }

    pipelineState->SetName(name.c_str());

    D3D12NI_LOG_TRACE("--- PSO %S created ---", name.c_str());
#endif // DEBUG

    mPipelines.emplace(std::make_pair(params, std::move(pipelineState)));

    return true;
}

PSOManager::PSOManager(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
    , m2DInputLayout()
    , m3DInputLayout()
    , mPipelines()
    , mNullPipeline(nullptr)
{
    m2DInputLayout[0].SemanticName = "POSITION";
    m2DInputLayout[0].SemanticIndex = 0;
    m2DInputLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    m2DInputLayout[0].InputSlot = 0;
    m2DInputLayout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m2DInputLayout[0].AlignedByteOffset = 0;

    m2DInputLayout[1].SemanticName = "COLOR";
    m2DInputLayout[1].SemanticIndex = 0;
    m2DInputLayout[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    m2DInputLayout[1].InputSlot = 0;
    m2DInputLayout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m2DInputLayout[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    m2DInputLayout[2].SemanticName = "TEXCOORD";
    m2DInputLayout[2].SemanticIndex = 0;
    m2DInputLayout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
    m2DInputLayout[2].InputSlot = 0;
    m2DInputLayout[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m2DInputLayout[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    m2DInputLayout[3].SemanticName = "TEXCOORD";
    m2DInputLayout[3].SemanticIndex = 1;
    m2DInputLayout[3].Format = DXGI_FORMAT_R32G32_FLOAT;
    m2DInputLayout[3].InputSlot = 0;
    m2DInputLayout[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m2DInputLayout[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


    m3DInputLayout[0].SemanticName = "POSITION";
    m3DInputLayout[0].SemanticIndex = 0;
    m3DInputLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    m3DInputLayout[0].InputSlot = 0;
    m3DInputLayout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m3DInputLayout[0].AlignedByteOffset = 0;

    m3DInputLayout[1].SemanticName = "TEXCOORD";
    m3DInputLayout[1].SemanticIndex = 0;
    m3DInputLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    m3DInputLayout[1].InputSlot = 0;
    m3DInputLayout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m3DInputLayout[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    m3DInputLayout[2].SemanticName = "TEXCOORD";
    m3DInputLayout[2].SemanticIndex = 1;
    m3DInputLayout[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    m3DInputLayout[2].InputSlot = 0;
    m3DInputLayout[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    m3DInputLayout[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
}

PSOManager::~PSOManager()
{
    for (auto& it: mPipelines)
    {
        it.second.Reset();
    }

    mPipelines.clear();

    mNativeDevice.reset();

    D3D12NI_LOG_DEBUG("PSOManager destroyed");
}

bool PSOManager::Init()
{
    // Prepare Root Signature for Phong Shaders
    // See hlsl/*.hlsl files for reference
    std::vector<D3D12_ROOT_PARAMETER> rsParams;
    std::vector<D3D12_STATIC_SAMPLER_DESC> rsSamplers;
    D3D12_ROOT_PARAMETER rsParam;
    D3D12_STATIC_SAMPLER_DESC rsSampler;
    D3D12NI_ZERO_STRUCT(rsParam);
    D3D12NI_ZERO_STRUCT(rsSampler);

    // Shaders share the same static sampler
    rsSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rsSampler.RegisterSpace = 0;
    rsSampler.ShaderRegister = 0;
    rsSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rsSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rsSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    rsSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    rsSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    rsSampler.MinLOD = 0;
    rsSampler.MaxLOD = D3D12_FLOAT32_MAX;
    rsSampler.MipLODBias = 0;
    rsSampler.MaxAnisotropy = 1;
    rsSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    rsSamplers.emplace_back(rsSampler);

    // Vertex shader root CBuffer View - gData
    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rsParam.Descriptor.ShaderRegister = 0;
    rsParam.Descriptor.RegisterSpace = 0;
    rsParams.emplace_back(rsParam);

    // Similar for Pixel Shader - gColorSpec
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rsParams.emplace_back(rsParam);

    // Vertex Shader Descriptor Table - gLightSpec
    D3D12_DESCRIPTOR_RANGE CBVRange;
    D3D12NI_ZERO_STRUCT(CBVRange);
    CBVRange.BaseShaderRegister = 1;
    CBVRange.RegisterSpace = 0;
    CBVRange.NumDescriptors = Constants::MAX_LIGHTS;
    CBVRange.OffsetInDescriptorsFromTableStart = 0;
    CBVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rsParam.DescriptorTable.pDescriptorRanges = &CBVRange;
    rsParam.DescriptorTable.NumDescriptorRanges = 1;
    rsParams.emplace_back(rsParam);

    // Similarly in Pixel Shader - gLightSpec
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rsParams.emplace_back(rsParam);

    // Pixel Shader textures/maps
    D3D12_DESCRIPTOR_RANGE SRVRange;
    D3D12NI_ZERO_STRUCT(SRVRange);
    SRVRange.BaseShaderRegister = 0;
    SRVRange.RegisterSpace = 0;
    SRVRange.NumDescriptors = 4; // diffuse, specular, bump, selfIllum
    SRVRange.OffsetInDescriptorsFromTableStart = 0;
    SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rsParam.DescriptorTable.pDescriptorRanges = &SRVRange;
    rsParam.DescriptorTable.NumDescriptorRanges = 1;
    rsParams.emplace_back(rsParam);

    D3D12_ROOT_SIGNATURE_DESC rsDesc;
    D3D12NI_ZERO_STRUCT(rsDesc);
    rsDesc.pParameters = rsParams.data();
    rsDesc.NumParameters = static_cast<UINT>(rsParams.size());
    rsDesc.pStaticSamplers = rsSamplers.data();
    rsDesc.NumStaticSamplers = static_cast<UINT>(rsSamplers.size());
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3DBlobPtr rsBlob;
    D3DBlobPtr rsErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rsBlob, &rsErrorBlob);
    if (FAILED(hr))
    {
        D3D12NI_LOG_ERROR("Failed to serialize Phong Shader Root Signature: %s", rsErrorBlob->GetBufferPointer());
        return false;
    }

    hr = mNativeDevice->GetDevice()->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&mPhongRootSignature));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Phong Shader Root Signature");

    return true;
}

const D3D12PipelineStatePtr& PSOManager::GetPSO(const PSOParameters& params)
{
    const auto psoIter = mPipelines.find(params);
    if (psoIter == mPipelines.end())
    {
        if (!ConstructNewPSO(params))
        {
            D3D12NI_LOG_ERROR("Failed to construct new PSO");
            return mNullPipeline;
        }

        return mPipelines[params];
    }
    else
    {
        return psoIter->second;
    }
}

} // namespace Internal
} // namespace D3D12

