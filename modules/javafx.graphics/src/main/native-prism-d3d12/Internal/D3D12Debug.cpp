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

#include "D3D12Debug.hpp"

#include "D3D12Config.hpp"
#include "D3D12Logger.hpp"

#include "../D3D12NativeDevice.hpp"

namespace {

const char* D3D12MessageCategoryToString(D3D12_MESSAGE_CATEGORY category)
{
    switch (category)
    {
        case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED: return "APPLICATION_DEFINED";
        case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS: return "MISCELLANEOUS";
        case D3D12_MESSAGE_CATEGORY_INITIALIZATION: return "INITIALIZATION";
        case D3D12_MESSAGE_CATEGORY_CLEANUP: return "CLEANUP";
        case D3D12_MESSAGE_CATEGORY_COMPILATION: return "COMPILATION";
        case D3D12_MESSAGE_CATEGORY_STATE_CREATION: return "STATE_CREATION";
        case D3D12_MESSAGE_CATEGORY_STATE_SETTING: return "STATE_SETTING";
        case D3D12_MESSAGE_CATEGORY_STATE_GETTING: return "STATE_GETTING";
        case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION: return "RESOURCE_MANIPULATION";
        case D3D12_MESSAGE_CATEGORY_EXECUTION: return "EXECUTION";
        case D3D12_MESSAGE_CATEGORY_SHADER: return "SHADER";
        default: return "UNKNOWN";
    }
}

void D3D12DebugMessageCallback(D3D12_MESSAGE_CATEGORY Category,
                               D3D12_MESSAGE_SEVERITY Severity,
                               D3D12_MESSAGE_ID ID,
                               LPCSTR pDescription,
                               void* pContext)
{
    switch (Severity)
    {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        D3D12NI_LOG_ERROR("D3D12 %s Corruption: %s", D3D12MessageCategoryToString(Category), pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_ERROR:
        D3D12NI_LOG_ERROR("D3D12 %s Error: %s", D3D12MessageCategoryToString(Category), pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_WARNING:
        D3D12NI_LOG_WARN("D3D12 %s Warning: %s", D3D12MessageCategoryToString(Category), pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_INFO:
        D3D12NI_LOG_INFO("D3D12 %s Info: %s", D3D12MessageCategoryToString(Category), pDescription);
        break;
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
        D3D12NI_LOG_DEBUG("D3D12 %s Message: %s", D3D12MessageCategoryToString(Category), pDescription);
        break;
    default:
        return;
    }
}

} // namespace

namespace D3D12 {
namespace Internal {

Debug::Debug()
    : mDXGIDebug()
    , mDXGIInfoQueue()
    , mD3D12Debug()
    , mD3D12InfoQueue()
    , mIsEnabled(false)
{
}

Debug::~Debug()
{
    if (mIsEnabled && mDXGIDebug)
    {
        D3D12NI_LOG_INFO("Reporting live objects at Debug destructor:");
        mDXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    }
}

Debug& Debug::Instance()
{
    static Debug instance;
    return instance;
}

bool Debug::Init()
{
    mIsEnabled = Config::Instance().IsDebugLayerEnabled();
    if (!mIsEnabled)
    {
        D3D12NI_LOG_INFO("Debug facilities disabled");
        return true;
    }

    HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&mDXGIDebug));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get DXGI Debug interface");

    hr = D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&mD3D12Debug));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get Debug Layers interface");

    mD3D12Debug->EnableDebugLayer();
    mD3D12Debug->SetEnableAutoName(true);

    mD3D12Debug->SetEnableGPUBasedValidation(Config::Instance().IsGpuDebugEnabled());
    // NOTE: here we can potentially disable state-tracking for GPU-based valiadtion.
    // This saves a lot of performance but shuts down resource state validation.
    // Use: mDebugLayer->SetGPUBasedValidationFlags(...);
    // Might be worthwhile for some scenarios.

    // NOTE: DXGIGetDebugInterface1 CAN return E_NOINTERFACE when Windows SDK is not
    // installed in the system.
    hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&mDXGIInfoQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get DXGI Info Queue interface");

    if (Config::Instance().IsBreakOnErrorEnabled())
    {
        hr = mDXGIInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to set break on DXGI errors");
        hr = mDXGIInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to set break on DXGI corruptions");
    }

    D3D12NI_LOG_INFO("Debug facilities enabled");
    return true;
}

bool Debug::InitDeviceDebug(const NIPtr<NativeDevice>& device)
{
    if (!mIsEnabled)
    {
        // quiet exit, since debugging is disabled
        return true;
    }

    mNativeDevice = device;

    HRESULT hr = mNativeDevice->GetDevice()->QueryInterface(IID_PPV_ARGS(&mD3D12InfoQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get DXGI Info Queue interface");

    // a list of debug messages to filter out
    D3D12_MESSAGE_ID filterMsgs[] =
    {
        // D3D12 provides a very minor and not even guaranteed perf boost when CreateCommittedResource
        // is called with "initial clear value" when creating a RenderTarget or DepthStencil resource.
        // That clear value then has to be provided to ClearRenderTargetView and if it ever differs,
        // it will still work but might be ever-so-slightly slower and produces a warning in debug
        // layers.
        // In case of JFX it is VERY difficult to guesstimate which color would be the most frequently
        // used as clear color, so we just don't provide it when creating RTTs. Warning still exists
        // regardless (debug layer bug?), so we silence it by default.
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
        D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
    };

    D3D12_INFO_QUEUE_FILTER filter;
    D3D12NI_ZERO_STRUCT(filter);
    filter.DenyList.NumIDs = _countof(filterMsgs);
    filter.DenyList.pIDList = filterMsgs;

    hr = mD3D12InfoQueue->AddStorageFilterEntries(&filter);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to apply D3D12 info queue filters");

    // register our own message callback to use D3D12NI's logging facilities
    hr = mD3D12InfoQueue->RegisterMessageCallback(D3D12DebugMessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &mD3D12MessageCallbackCookie);
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to register D3D12 debug message callback");

    if (mD3D12MessageCallbackCookie == 0)
    {
        D3D12NI_LOG_ERROR("Failed to register D3D12 debug message callback (cookie is empty)");
        return false;
    }

    if (Config::Instance().IsBreakOnErrorEnabled())
    {
        hr = mD3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to set break on D3D12 errors");
        hr = mD3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to set break on D3D12 corruptions");
    }

    D3D12NI_LOG_INFO("D3D12 Device debugging set up");
    return true;
}

bool Debug::IsEnabled()
{
    return mIsEnabled;
}

} // namespace Internal
} // namespace D3D12
