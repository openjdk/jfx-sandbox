/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12RootSignatureManager.hpp"

#include "D3D12NativeDevice.hpp"

#include "../D3D12Constants.hpp"


namespace D3D12 {
namespace Internal {

RootSignatureManager::RootSignatureManager(const NIPtr<NativeDevice>& nativeDevice)
    : mNativeDevice(nativeDevice)
{
}

bool RootSignatureManager::Init()
{
    // Prepare Root Signature for Phong Shaders
    // See hlsl/ShaderCommon.hlsl for details
    std::vector<D3D12_ROOT_PARAMETER> rsParams;
    std::vector<D3D12_STATIC_SAMPLER_DESC> rsSamplers;
    D3D12_ROOT_PARAMETER rsParam;
    D3D12_STATIC_SAMPLER_DESC rsSampler;
    D3D12_DESCRIPTOR_RANGE CBVRange;
    D3D12_DESCRIPTOR_RANGE UAVRange;
    D3D12_DESCRIPTOR_RANGE SRVRange;

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
        D3D12NI_LOG_ERROR("Failed to serialize Internal Shader Root Signature: %s", rsErrorBlob->GetBufferPointer());
        return false;
    }

    hr = mNativeDevice->GetDevice()->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&mGraphicsRootSignature));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Internal Shader Root Signature");


    // Prepare Root Signature for Compute Shaders
    // See hlsl/ShaderCommon.hlsl for details
    rsParams.clear();
    rsSamplers.clear();
    D3D12NI_ZERO_STRUCT(rsParam);
    D3D12NI_ZERO_STRUCT(rsSampler);

    // Shaders share the same static sampler
    rsSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rsSampler.RegisterSpace = 0;
    rsSampler.ShaderRegister = 0;
    rsSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    rsSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    rsSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    rsSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    rsSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    rsSampler.MinLOD = 0;
    rsSampler.MaxLOD = D3D12_FLOAT32_MAX;
    rsSampler.MipLODBias = 0;
    rsSampler.MaxAnisotropy = 1;
    rsSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    rsSamplers.emplace_back(rsSampler);

    // CBuffer View for any constant data needed
    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rsParam.Descriptor.ShaderRegister = 0;
    rsParam.Descriptor.RegisterSpace = 0;
    rsParams.emplace_back(rsParam);

    // UAV Table
    D3D12NI_ZERO_STRUCT(UAVRange);
    UAVRange.BaseShaderRegister = 0;
    UAVRange.RegisterSpace = 0;
    UAVRange.NumDescriptors = 4;
    UAVRange.OffsetInDescriptorsFromTableStart = 0;
    UAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rsParam.DescriptorTable.pDescriptorRanges = &UAVRange;
    rsParam.DescriptorTable.NumDescriptorRanges = 1;
    rsParams.emplace_back(rsParam);

    // Texture table
    D3D12NI_ZERO_STRUCT(SRVRange);
    SRVRange.BaseShaderRegister = 0;
    SRVRange.RegisterSpace = 0;
    SRVRange.NumDescriptors = 4;
    SRVRange.OffsetInDescriptorsFromTableStart = 0;
    SRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    rsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rsParam.DescriptorTable.pDescriptorRanges = &SRVRange;
    rsParam.DescriptorTable.NumDescriptorRanges = 1;
    rsParams.emplace_back(rsParam);

    D3D12NI_ZERO_STRUCT(rsDesc);
    rsDesc.pParameters = rsParams.data();
    rsDesc.NumParameters = static_cast<UINT>(rsParams.size());
    rsDesc.pStaticSamplers = rsSamplers.data();
    rsDesc.NumStaticSamplers = static_cast<UINT>(rsSamplers.size());

    D3DBlobPtr computeRsBlob;
    D3DBlobPtr computeRsErrorBlob;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &computeRsBlob, &computeRsErrorBlob);
    if (FAILED(hr))
    {
        D3D12NI_LOG_ERROR("Failed to serialize Compute Shader Root Signature: %s", computeRsErrorBlob->GetBufferPointer());
        return false;
    }

    hr = mNativeDevice->GetDevice()->CreateRootSignature(0, computeRsBlob->GetBufferPointer(), computeRsBlob->GetBufferSize(), IID_PPV_ARGS(&mComputeRootSignature));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create Compute Shader Root Signature");

    return true;
}

} // namespace Internal
} // namespace D3D12
