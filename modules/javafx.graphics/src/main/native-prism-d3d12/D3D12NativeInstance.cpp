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

#include "D3D12NativeInstance.hpp"

#include "Internal/D3D12Debug.hpp"
#include "Internal/D3D12Logger.hpp"
#include "Internal/D3D12Utils.hpp"
#include "Internal/JNIString.hpp"

#include <vector>
#include <com_sun_prism_d3d12_ni_D3D12NativeInstance.h>

#define D3D12NI_FILL_DEVICE_ERROR(hr, info) \
do { \
    _com_error __e(hr); \
    info.deviceError = hr; \
    info.deviceErrorReason = ::D3D12::Internal::Utils::ToString(__e.ErrorMessage()); \
} while (0)


namespace D3D12 {

NativeInstance::NativeInstance()
    : mDXGIFactory()
    , mDXGIAdapters()
{
}

NativeInstance::~NativeInstance()
{
    mShaderLibrary.reset();

    if (mDXGIFactory)
    {
        mDXGIAdapters.clear();
        mDXGIFactory.Reset();
    }

    D3D12NI_LOG_DEBUG("Instance destroyed");
}

bool NativeInstance::Init()
{
    UINT dxgiFlags = 0;
    if (Internal::Debug::Instance().IsEnabled())
    {
        dxgiFlags = DXGI_CREATE_FACTORY_DEBUG;
    }

    HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&mDXGIFactory));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to create DXGI Factory");

    IDXGIAdapter1* adapter;
    uint32_t i = 0;
    D3D12NI_LOG_DEBUG("DXGI enumerated adapters:");
    while (mDXGIFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        mDXGIAdapters.push_back(adapter);

        DXGI_ADAPTER_DESC1 adapterDesc;
        mDXGIAdapters.back()->GetDesc1(&adapterDesc);
        mDXGIAdapterDescs.push_back(adapterDesc);
        D3D12NI_LOG_DEBUG(" \\_ #%d: %ws (%x)", i, adapterDesc.Description, adapterDesc.Flags);

        ++i;
    }

    mShaderLibrary = std::make_shared<Internal::ShaderLibrary>();

    return true;
}

int NativeInstance::GetAdapterCount()
{
    return static_cast<int>(mDXGIAdapters.size());
}

int NativeInstance::GetAdapterOrdinal(HMONITOR monitor)
{
    int ret = -1;

    D3D12NI_LOG_DEBUG("%s: Asks for monitor %p", __func__, monitor);

    for (int adapterIdx = 0; adapterIdx < mDXGIAdapters.size(); adapterIdx++) {
        DXGI_ADAPTER_DESC1 adapterDesc;
        mDXGIAdapters[adapterIdx]->GetDesc1(&adapterDesc);

        D3D12NI_LOG_DEBUG("%s: Outputs for adapter %ws:", __func__, adapterDesc.Description);

        int outputIdx = 0;
        IDXGIOutput* output = nullptr;
        while (mDXGIAdapters[adapterIdx]->EnumOutputs(outputIdx, &output) != DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC outputDesc;
            output->GetDesc(&outputDesc);
            D3D12NI_LOG_DEBUG(" \\_ output #%d: %ws (monitor %p)", outputIdx, outputDesc.DeviceName, outputDesc.Monitor);
            if (outputDesc.Monitor == monitor) {
                ret = outputIdx;
                break;
            }

            outputIdx++;
        }

        if (ret > -1)
            break;
    }

    return ret;
}

bool NativeInstance::CanCreateDevice(uint32_t adapterIdx, Internal::DeviceInformation& info) const
{
    if (adapterIdx >= mDXGIAdapterDescs.size())
    {
        D3D12NI_FILL_DEVICE_ERROR(DXGI_ERROR_NOT_FOUND, info);
        return false;
    }

    info.description = Internal::Utils::ToString(mDXGIAdapterDescs[adapterIdx].Description);

    HRESULT hr = D3D12CreateDevice(mDXGIAdapters[adapterIdx], D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
    D3D12NI_FILL_DEVICE_ERROR(hr, info);
    return SUCCEEDED(hr);
}

bool NativeInstance::GetAdapterInformation(uint32_t adapterIdx, Internal::AdapterInformation& info) const
{
    if (adapterIdx >= mDXGIAdapterDescs.size())
    {
        return false;
    }

    const DXGI_ADAPTER_DESC1& adapterDesc = mDXGIAdapterDescs[adapterIdx];
    info.description = Internal::Utils::ToString(adapterDesc.Description);
    info.vendorID = adapterDesc.VendorId;
    info.deviceID = adapterDesc.DeviceId;
    info.subSysID = adapterDesc.SubSysId;
    info.revision = adapterDesc.Revision;
    info.videoMemory = adapterDesc.DedicatedVideoMemory;
    info.systemMemory = adapterDesc.DedicatedSystemMemory;
    info.sharedMemory = adapterDesc.SharedSystemMemory;

    return true;
}

bool NativeInstance::GetDeviceInformation(uint32_t adapterIdx, Internal::DeviceInformation& info) const
{
    info.description = Internal::Utils::ToString(mDXGIAdapterDescs[adapterIdx].Description);

    D3D12DevicePtr device;
    HRESULT hr = D3D12CreateDevice(mDXGIAdapters[adapterIdx], D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr))
    {
        D3D12NI_LOG_ERROR("GetDeviceInformation: Failed to fetch device information for adapter %ws - D3D12CreateDevice failed", mDXGIAdapterDescs[adapterIdx].Description);
        D3D12NI_FILL_DEVICE_ERROR(hr, info);
        return false;
    }

    const D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_2,
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels;
    D3D12NI_ZERO_STRUCT(featureLevels);
    featureLevels.NumFeatureLevels = sizeof(requestedLevels)/sizeof(D3D_FEATURE_LEVEL);
    featureLevels.pFeatureLevelsRequested = requestedLevels;

    hr = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels));
    if (FAILED(hr))
    {
        D3D12NI_LOG_ERROR("GetDeviceInformation: Failed to query available feature levels");
        D3D12NI_FILL_DEVICE_ERROR(hr, info);
        return false;
    }

    info.featureLevel = Internal::D3DFeatureLevelToShortString(featureLevels.MaxSupportedFeatureLevel);

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel;
    D3D12NI_ZERO_STRUCT(shaderModel);

    // MSDN recommends calling this check in a loop because of runtime potentially not knowing SM we define here
    // suppose we run JFX on older Windows update with older D3D12 runtime, before SM 6.7 was introduced
    // Querying for D3D_SHADER_MODEL_6_7 would then return E_INVALIDARG instead of doing an actual check.
    // We will never exceed D3D_SHADER_MODEL_5_1 though - D3D12 released with guaranteed SM 5.1 support
    // See remarks on: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_shader_model
    std::array<D3D_SHADER_MODEL, 9> models{
        D3D_SHADER_MODEL_6_7,
        D3D_SHADER_MODEL_6_6,
        D3D_SHADER_MODEL_6_5,
        D3D_SHADER_MODEL_6_4,
        D3D_SHADER_MODEL_6_3,
        D3D_SHADER_MODEL_6_2,
        D3D_SHADER_MODEL_6_1,
        D3D_SHADER_MODEL_6_0,
        D3D_SHADER_MODEL_5_1,
    };

    hr = E_INVALIDARG;
    int modelIdx = 0;
    while (hr == E_INVALIDARG && modelIdx < models.size()) {
        shaderModel.HighestShaderModel = models[modelIdx];
        hr = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel));
        modelIdx++;
    }

    if (FAILED(hr))
    {
        D3D12NI_LOG_ERROR("GetDeviceInformation: Failed to query highest available Shader Model");
        D3D12NI_FILL_DEVICE_ERROR(hr, info);
        return false;
    }

    info.shaderModel = Internal::D3DShaderModelToShortString(shaderModel.HighestShaderModel);

    D3D12NI_FILL_DEVICE_ERROR(S_OK, info);
    return true;
}

bool NativeInstance::LoadInternalShader(const std::string& name, ShaderPipelineMode mode, D3D12_SHADER_VISIBILITY visibility, void* code, size_t codeSize)
{
    return mShaderLibrary->Load(name, mode, visibility, code, codeSize);
}

NIPtr<NativeDevice>* NativeInstance::CreateDevice(int adapterOrdinal)
{
    if (adapterOrdinal >= mDXGIAdapters.size()) return nullptr;

    return CreateNIObject<NativeDevice>(mDXGIAdapters[adapterOrdinal], mShaderLibrary);
}

NIPtr<NativeSwapChain>* NativeInstance::CreateSwapChain(const NIPtr<NativeDevice>& device, HWND hwnd)
{
    if (!device) return nullptr;

    return CreateNIDeviceObject<NativeSwapChain>(device, mDXGIFactory, hwnd);
}

} // namespace D3D12


#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nAllocateNativeInstance
    (JNIEnv* env, jclass)
{
    return reinterpret_cast<jlong>(D3D12::AllocateNIObject<D3D12::NativeInstance>());
}

JNIEXPORT void JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nReleaseNativeObject
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return;

   D3D12::FreeNIObject<D3D12::NativeInstance>(ptr);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nInit
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return false;

    return D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->Init();
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nGetAdapterCount
    (JNIEnv* env, jobject obj, jlong ptr)
{
    if (!ptr) return -1;

    return D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->GetAdapterCount();
}

JNIEXPORT jint JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nGetAdapterOrdinal
    (JNIEnv* env, jobject obj, jlong ptr, jlong screenNativeHandle)
{
    if (!ptr) return -1;
    if (!screenNativeHandle) return -1;

    return D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->GetAdapterOrdinal(reinterpret_cast<HMONITOR>(screenNativeHandle));
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nCanCreateDevice
    (JNIEnv* env, jobject obj, jlong ptr, jint adapterIdx, jobject deviceInfoStruct)
{
    if (!ptr) return false;
    if (adapterIdx < 0) return false;
    if (deviceInfoStruct == nullptr) return false;

    D3D12::Internal::DeviceInformation deviceInfo;
    bool result = D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->CanCreateDevice(static_cast<uint32_t>(adapterIdx), deviceInfo);

    if (!deviceInfo.ToJObject(env, deviceInfoStruct)) return false;

    return result;
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nGetAdapterInformation
    (JNIEnv* env, jobject obj, jlong ptr, jint adapterIdx, jobject adapterInfoStruct)
{
    if (!ptr) return false;
    if (adapterIdx < 0) return false;
    if (adapterInfoStruct == nullptr) return false;

    D3D12::Internal::AdapterInformation adapterInfo;
    if (!D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->GetAdapterInformation(static_cast<uint32_t>(adapterIdx), adapterInfo))
    {
        return false;
    }

    return adapterInfo.ToJObject(env, adapterInfoStruct);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nGetDeviceInformation
    (JNIEnv* env, jobject obj, jlong ptr, jint adapterIdx, jobject deviceInfoStruct)
{
    if (!ptr) return false;
    if (adapterIdx < 0) return false;
    if (deviceInfoStruct == nullptr) return false;

    D3D12::Internal::DeviceInformation deviceInfo;
    if (!D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->GetDeviceInformation(static_cast<uint32_t>(adapterIdx), deviceInfo))
    {
        return false;
    }

    return deviceInfo.ToJObject(env, deviceInfoStruct);
}

JNIEXPORT jboolean JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nLoadInternalShader
    (JNIEnv* env, jobject obj, jlong ptr, jstring name, jint mode, jint visibility, jobject codeBBuf)
{
    if (!ptr) return false;
    if (!name) return false;
    if (!codeBBuf) return false;

    if (mode < static_cast<jint>(D3D12::ShaderPipelineMode::UI_2D) ||
        mode >= static_cast<jint>(D3D12::ShaderPipelineMode::MAX_ENUM))
    {
        D3D12NI_LOG_ERROR("Invalid shader pipeline mode provided for internal shader: %d", mode);
        return false;
    }

    void* codeBuf = env->GetDirectBufferAddress(codeBBuf);
    jlong codeBufSize = env->GetDirectBufferCapacity(codeBBuf);
    if (codeBuf == nullptr || codeBufSize <= 0)
    {
        D3D12NI_LOG_ERROR("Failed to get internal shader code buffer address");
        return false;
    }

    D3D12::Internal::JNIString nameJString(env, name);
    if (nameJString == nullptr)
    {
        D3D12NI_LOG_ERROR("Failed to get internal shader name string");
        return false;
    }

    std::string nameStr(nameJString);

    return D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->LoadInternalShader(
        nameStr, static_cast<D3D12::ShaderPipelineMode>(mode), static_cast<D3D12_SHADER_VISIBILITY>(visibility), codeBuf, static_cast<size_t>(codeBufSize)
    );
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nCreateDevice
    (JNIEnv* env, jobject obj, jlong ptr, jint adapterOrdinal)
{
    if (!ptr) return 0;
    if (adapterOrdinal < 0) return 0;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->CreateDevice(adapterOrdinal));
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_d3d12_ni_D3D12NativeInstance_nCreateSwapChain
    (JNIEnv* env, jobject obj, jlong ptr, jlong devicePtr, jlong hwnd)
{
    if (!ptr) return 0;
    if (!devicePtr) return 0;
    if (!hwnd) return 0;

    const D3D12::NIPtr<D3D12::NativeDevice>& device = D3D12::GetNIObject<D3D12::NativeDevice>(devicePtr);
    HWND windowHandle = reinterpret_cast<HWND>(hwnd);
    if (!device) return 0;
    if (!windowHandle) return 0;

    return reinterpret_cast<jlong>(D3D12::GetNIObject<D3D12::NativeInstance>(ptr)->CreateSwapChain(device, windowHandle));
}

#ifdef __cplusplus
}
#endif
