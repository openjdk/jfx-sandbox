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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <comdef.h>
#include <wrl/client.h>
#include <jni.h>
#include <memory>
#include <string>
#include <cassert>

#include "Internal/D3D12Logger.hpp"

#include "Common_D3D12ShaderResourceDataHeader.hpp"

namespace D3D12 {

// smart-pointer container shorthand for D3D12 objects
template <typename T>
using Ptr = Microsoft::WRL::ComPtr<T>;

// aliases for DXGI-specific objects used by the backend
// this is to help with versioning the interfaces in a unified manner
using DXGIFactoryPtr = Ptr<IDXGIFactory3>;
using DXGIDebugPtr = Ptr<IDXGIDebug1>;
using DXGIInfoQueuePtr = Ptr<IDXGIInfoQueue>;
using DXGISwapChainPtr = Ptr<IDXGISwapChain3>;

// Non-D3D-specific Blob object pointer
using D3DBlobPtr = Ptr<ID3DBlob>;

// aliases for D3D12-specific interfaces
using D3D12DevicePtr = Ptr<ID3D12Device10>;
using D3D12CommandAllocatorPtr = Ptr<ID3D12CommandAllocator>;
using D3D12CommandQueuePtr = Ptr<ID3D12CommandQueue>;
using D3D12DebugPtr = Ptr<ID3D12Debug5>;
using D3D12InfoQueuePtr = Ptr<ID3D12InfoQueue1>;
using D3D12DescriptorHeapPtr = Ptr<ID3D12DescriptorHeap>;
using D3D12FencePtr = Ptr<ID3D12Fence1>;
using D3D12GraphicsCommandListPtr = Ptr<ID3D12GraphicsCommandList7>;
using D3D12CommandListPtr = Ptr<ID3D12CommandList>;
using D3D12PipelineStatePtr = Ptr<ID3D12PipelineState>;
using D3D12ResourcePtr = Ptr<ID3D12Resource2>;
using D3D12RootSignaturePtr = Ptr<ID3D12RootSignature>;
using D3D12ShaderReflectionPtr = Ptr<ID3D12ShaderReflection>;

// DXC-specific aliases
// NOTE: those might have to be removed
//using DXCUtilsPtr = Ptr<IDxcUtils>;
//using DXCBlobPtr = Ptr<IDxcBlobEncoding>;

// smart-pointer container for internal objects
template <typename T>
using NIPtr = std::shared_ptr<T>;

// forward declaration of potentially the most important object
class NativeDevice;

// fast & easy allocation routines
template <typename T>
NIPtr<T>* AllocateNIObject()
{
    NIPtr<T>* newPtr = new NIPtr<T>();
    *newPtr = std::make_shared<T>();
    return newPtr;
}

template <typename T>
NIPtr<T>* AllocateNIDeviceObject(const NIPtr<NativeDevice>& device)
{
    NIPtr<T>* newPtr = new NIPtr<T>();
    *newPtr = std::make_shared<T>(device);
    return newPtr;
}

template <typename T, typename ...Args>
NIPtr<T>* CreateNIObject(Args&&... args)
{
    NIPtr<T>* niPtr = AllocateNIObject<T>();
    NIPtr<T>& obj = *niPtr;
    if (!obj->Init(std::forward<Args>(args)...))
    {
        delete niPtr;
        return nullptr;
    }

    return niPtr;
}

template <typename T, typename ...Args>
NIPtr<T>* CreateNIDeviceObject(const NIPtr<NativeDevice>& device, Args&&... args)
{
    NIPtr<T>* niPtr = AllocateNIDeviceObject<T>(device);
    NIPtr<T>& obj = *niPtr;
    if (!obj->Init(std::forward<Args>(args)...))
    {
        delete niPtr;
        return nullptr;
    }

    return niPtr;
}

template <typename T>
const NIPtr<T>& GetNIObject(jlong ptr)
{
    return *reinterpret_cast<NIPtr<T>*>(ptr);
}

template <typename T>
void FreeNIObject(jlong ptr)
{
    NIPtr<T>* niPtr = reinterpret_cast<NIPtr<T>*>(ptr);
    delete niPtr;
}

// bytes per pixel calculator
// only covers pixel formats supported by JFX
constexpr uint32_t GetDXGIFormatBPP(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return sizeof(float) * 4;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
        return sizeof(uint8_t) * 4;
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_NV12:
        return sizeof(uint8_t) * 1;
    case DXGI_FORMAT_R16_UINT:
        return sizeof(uint16_t) * 1;
    case DXGI_FORMAT_R32_UINT:
        return sizeof(uint32_t) * 1;
    // TODO: D3D12: DXGI_FORMAT_NV12
    default:
        return 0;
    }
}

// Helper structs for in case we need to shuffle components
// ex. see NativeDevice::UpdateTexture()
struct Coords_XYZ_FLOAT
{
    float x;
    float y;
    float z;
};

struct Coords_XYZW_FLOAT
{
    float x;
    float y;
    float z;
    float w;
};

struct Coords_UV_FLOAT
{
    float u;
    float v;
};

struct Pixel_RGB8_UNORM
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Pixel_RGBA8_UNORM
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct Pixel_RGB32_FLOAT
{
    float r;
    float g;
    float b;
};

struct Pixel_RGBA32_FLOAT
{
    float r;
    float g;
    float b;
    float a;
};

struct Pixel_BGRA8_UNORM
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
};

// Light spec matching Shader definitions
// TODO: D3D12: cleanup - try avoiding code duplication and share below structs with HLSL headers
struct VSLightSpec
{
    Coords_XYZW_FLOAT pos;
    Coords_XYZW_FLOAT normDir;
};

struct PSColorSpec
{
    Pixel_RGBA32_FLOAT diffuse;
    Pixel_RGBA32_FLOAT specular;
    Pixel_RGBA32_FLOAT ambientLight;
};

struct PSLightSpec
{
    Pixel_RGBA32_FLOAT color;
    Pixel_RGBA32_FLOAT attenuation; // { r=constant, g=linear, b=quadratic, a=on/off }
    Pixel_RGBA32_FLOAT maxRange; // { r=maxRange, _, _, _ }
    Pixel_RGBA32_FLOAT spotLightFactors; // precalculated factors based on input data
                                         // { r=cos(outer), g=cos(inner)-cos(outer), b=falloff, _ }
};

// Phong Shader configuration
enum class PhongShaderMappingVariant: uint8_t
{
    SIMPLE,
    BUMP
};

enum class PhongShaderSpecularVariant: uint8_t
{
    NONE,
    TEXTURE,
    COLOR,
    MIX
};

struct PhongShaderSpec
{
    uint32_t lightCount;
    bool isSelfIllum;
    PhongShaderMappingVariant mapping;
    PhongShaderSpecularVariant specular;
};

// vertex definition for 2D
struct Vertex_2D
{
    Coords_XYZ_FLOAT pos;
    Pixel_RGBA8_UNORM color;
    Coords_UV_FLOAT uv1;
    Coords_UV_FLOAT uv2;
};

// Determines which rendering mode Shader belongs to.
// Mostly used to ensure vertex-pixel shader combination is correct and to
// initialize PSOs correctly
enum class ShaderPipelineMode: unsigned char
{
    UI_2D = 0,
    PHONG_3D,
    MAX_ENUM
};

// mirrors CompositeMode.java
enum class CompositeMode: unsigned char
{
    CLEAR = 0,
    SRC,
    SRC_OVER,
    DST_OUT,
    ADD,
    MAX_ENUM
};

// mirrors CullFace.java
enum class CullFace: unsigned char
{
    NONE = 0,
    BACK,
    FRONT,
    MAX_ENUM,
};

// mirrors MapType in PhongMaterial.java
enum class TextureMapType: unsigned char
{
    DIFFUSE = 0,
    SPECULAR,
    BUMP,
    SELF_ILLUM,
    MAX_ENUM,
};

// mirrors PixelFormat in PixelFormat.java
enum class PixelFormat: unsigned char
{
    INT_ARGB_PRE = 0,
    BYTE_BGRA_PRE,
    BYTE_RGB,
    BYTE_GRAY,
    BYTE_ALPHA,
    MULTI_YCbCr_42,
    BYTE_APPLE_422,
    FLOAT_XYZW,
};

// decides how Shader resource should be bound
// see D3D12Shader.hpp
enum class ResourceAssignmentType: uint32_t
{
    ROOT_CONSTANT,
    DESCRIPTOR,
    DESCRIPTOR_TABLE_TEXTURES,
    DESCRIPTOR_TABLE_CBUFFERS
};

inline const char* ResourceAssignmentTypeToString(ResourceAssignmentType type)
{
    switch (type)
    {
    case ResourceAssignmentType::ROOT_CONSTANT: return "ROOT_CONSTANT";
    case ResourceAssignmentType::DESCRIPTOR: return "DESCRIPTOR";
    case ResourceAssignmentType::DESCRIPTOR_TABLE_TEXTURES: return "DESCRIPTOR_TABLE_TEXTURES";
    case ResourceAssignmentType::DESCRIPTOR_TABLE_CBUFFERS: return "DESCRIPTOR_TABLE_CBUFFERS";
    default: return "UNKNOWN";
    }
}

inline const wchar_t* CompositeModeToWString(CompositeMode mode)
{
    switch (mode)
    {
    case CompositeMode::CLEAR: return L"CLEAR";
    case CompositeMode::SRC: return L"SRC";
    case CompositeMode::SRC_OVER: return L"SRC_OVER";
    case CompositeMode::DST_OUT: return L"DST_OUT";
    case CompositeMode::ADD: return L"ADD";
    default: return L"UNKNOWN";
    }
}

inline size_t GetPixelFormatBPP(PixelFormat f)
{
    switch (f) {
    case PixelFormat::BYTE_GRAY:
    case PixelFormat::BYTE_ALPHA:
        return 1;
    case PixelFormat::BYTE_RGB:
        return 3;
    case PixelFormat::INT_ARGB_PRE:
    case PixelFormat::BYTE_BGRA_PRE:
        return 4;
    case PixelFormat::FLOAT_XYZW:
        return 16;
    default:
        return 0;
    }
}

} // namespace D3D12


// checks provided hr, if it fails prints errMsg with hr's value and returns ret
#define D3D12NI_RET_IF_FAILED(hr, ret, errMsg) do { \
    if (FAILED(hr)) { \
        _com_error __e(hr); \
        D3D12NI_LOG_ERROR("%s: %x (%ws)", errMsg, hr, __e.ErrorMessage()); \
        return (ret); \
    } \
} while (0)

// same as above but for void-returning calls
#define D3D12NI_VOID_RET_IF_FAILED(hr, errMsg) do { \
    if (FAILED(hr)) { \
        _com_error __e(hr); \
        D3D12NI_LOG_ERROR("%s: %x (%ws)", errMsg, hr, __e.ErrorMessage()); \
        return; \
    } \
} while (0)

// zeros a structure, frequently used to initialize D3D12_*_DESC structs
#define D3D12NI_ZERO_STRUCT(x) do { \
    memset(&x, 0x0, sizeof(x)); \
} while (0)

// custom assert call that also logs an error message to the logger
#ifdef DEBUG

#define D3D12NI_ASSERT(x, msg, ...) do { \
    if (!(x)) \
    { \
        D3D12NI_LOG_ERROR(msg, __VA_ARGS__); \
        assert(0 == msg); \
    } \
} while (0)

#else // DEBUG

#define D3D12NI_ASSERT(x, msg, ...) do { } while (0)

#endif // DEBUG

