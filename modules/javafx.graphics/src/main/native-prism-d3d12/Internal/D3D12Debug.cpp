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

const char* TranslateDREDAllocationType(D3D12_DRED_ALLOCATION_TYPE type)
{
    switch (type)
    {
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE: return "COMMAND_QUEUE";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR: return "COMMAND_ALLOCATOR";
    case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE: return "PIPELINE_STATE";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST: return "COMMAND_LIST";
    case D3D12_DRED_ALLOCATION_TYPE_FENCE: return "FENCE";
    case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP: return "DESCRIPTOR_HEAP";
    case D3D12_DRED_ALLOCATION_TYPE_HEAP: return "HEAP";
    case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP: return "QUERY_HEAP";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE: return "COMMAND_SIGNATURE";
    case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY: return "PIPELINE_LIBRARY";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER: return "VIDEO_DECODER";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR: return "VIDEO_PROCESSOR";
    case D3D12_DRED_ALLOCATION_TYPE_RESOURCE: return "RESOURCE";
    case D3D12_DRED_ALLOCATION_TYPE_PASS: return "PASS";
    case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION: return "CRYPTOSESSION";
    case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY: return "CRYPTOSESSIONPOLICY";
    case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION: return "PROTECTEDRESOURCESESSION";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP: return "VIDEO_DECODER_HEAP";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL: return "COMMAND_POOL";
    case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER: return "COMMAND_RECORDER";
    case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT: return "STATE_OBJECT";
    case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND: return "METACOMMAND";
    case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP: return "SCHEDULINGGROUP";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR: return "VIDEO_MOTION_ESTIMATOR";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP: return "VIDEO_MOTION_VECTOR_HEAP";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND: return "VIDEO_EXTENSION_COMMAND";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER: return "VIDEO_ENCODER";
    case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP: return "VIDEO_ENCODER_HEAP";
    default: return "INVALID";
    }
}

const char* TranslateDREDBreadcrumbOp(D3D12_AUTO_BREADCRUMB_OP op)
{
    switch (op)
    {
    case D3D12_AUTO_BREADCRUMB_OP_SETMARKER: return "SETMARKER";
    case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT: return "BEGINEVENT";
    case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT: return "ENDEVENT";
    case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return "DRAWINSTANCED";
    case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED: return "DRAWINDEXEDINSTANCED";
    case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT: return "EXECUTEINDIRECT";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return "DISPATCH";
    case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION: return "COPYBUFFERREGION";
    case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION: return "COPYTEXTUREREGION";
    case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return "COPYRESOURCE";
    case D3D12_AUTO_BREADCRUMB_OP_COPYTILES: return "COPYTILES";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE: return "RESOLVESUBRESOURCE";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW: return "CLEARRENDERTARGETVIEW";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW: return "CLEARUNORDEREDACCESSVIEW";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW: return "CLEARDEPTHSTENCILVIEW";
    case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return "RESOURCEBARRIER";
    case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE: return "EXECUTEBUNDLE";
    case D3D12_AUTO_BREADCRUMB_OP_PRESENT: return "PRESENT";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA: return "RESOLVEQUERYDATA";
    case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION: return "BEGINSUBMISSION";
    case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION: return "ENDSUBMISSION";
    case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME: return "DECODEFRAME";
    case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES: return "PROCESSFRAMES";
    case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT: return "ATOMICCOPYBUFFERUINT";
    case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64: return "ATOMICCOPYBUFFERUINT64";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION: return "RESOLVESUBRESOURCEREGION";
    case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE: return "WRITEBUFFERIMMEDIATE";
    case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1: return "DECODEFRAME1";
    case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION: return "SETPROTECTEDRESOURCESESSION";
    case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2: return "DECODEFRAME2";
    case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1: return "PROCESSFRAMES1";
    case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE: return "BUILDRAYTRACINGACCELERATIONSTRUCTURE";
    case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO: return "EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO";
    case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE: return "COPYRAYTRACINGACCELERATIONSTRUCTURE";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS: return "DISPATCHRAYS";
    case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND: return "INITIALIZEMETACOMMAND";
    case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND: return "EXECUTEMETACOMMAND";
    case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION: return "ESTIMATEMOTION";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP: return "RESOLVEMOTIONVECTORHEAP";
    case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1: return "SETPIPELINESTATE1";
    case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND: return "INITIALIZEEXTENSIONCOMMAND";
    case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND: return "EXECUTEEXTENSIONCOMMAND";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH: return "DISPATCHMESH";
    case D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME: return "ENCODEFRAME";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA: return "RESOLVEENCODEROUTPUTMETADATA";
    default: return "Unknown";
    }
}

} // namespace

namespace D3D12 {
namespace Internal {

void Debug::DREDProcessBreadcrumbNode(const D3D12_AUTO_BREADCRUMB_NODE* node)
{
    D3D12NI_LOG_INFO("  Breadcrumbs on Command List %s (Queue %s):", node->pCommandListDebugNameA, node->pCommandQueueDebugNameA);
    int commands = node->BreadcrumbCount;
    int lastCompleted = *node->pLastBreadcrumbValue - 1;
    for (int i = 0; i < commands; ++i)
    {
        D3D12NI_LOG_INFO("   -%c  %s", (lastCompleted == i) ? '>' : ' ', TranslateDREDBreadcrumbOp(node->pCommandHistory[i]));
    }
}

void Debug::DREDProcessPageFaultNode(const D3D12_DRED_ALLOCATION_NODE* node)
{
    D3D12NI_LOG_INFO("    - %s (%s)", TranslateDREDAllocationType(node->AllocationType), (node->ObjectNameA != nullptr ? node->ObjectNameA : "UNNAMED"));
}

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
}

Debug& Debug::Instance()
{
    static Debug instance;
    return instance;
}

bool Debug::Init()
{
    // first, conditionally enable DRED - this will be useful even with debug layers disabled
    // we must change those settings _before_ D3D12 device is created
    if (Config::IsDREDEnabled())
    {
        D3D12DeviceRemovedExtendedDataSettings dredSettings;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));
        D3D12NI_RET_IF_FAILED(hr, false, "DRED was requested but failed to acquire its interface. DRED might not be available on this system.");

        dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        mIsDREDEnabled = true;
        D3D12NI_LOG_INFO("Enabled DRED analysis");
    }

    mIsEnabled = Config::IsDebugLayerEnabled();
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
    mD3D12Debug->SetEnableGPUBasedValidation(Config::IsGpuDebugEnabled());
    // NOTE: here we can potentially disable state-tracking for GPU-based valiadtion.
    // This saves a lot of performance but shuts down resource state validation.
    // Use: mDebugLayer->SetGPUBasedValidationFlags(...);
    // Might be worthwhile for some scenarios.

    // NOTE: DXGIGetDebugInterface1 CAN return E_NOINTERFACE when Windows SDK is not
    // installed in the system.
    hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&mDXGIInfoQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get DXGI Info Queue interface");

    if (Config::IsBreakOnErrorEnabled())
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
    mD3D12Device = device->GetDevice();

    if (!mD3D12Device)
    {
        D3D12NI_LOG_ERROR("Failed to initialize Debug class - D3D12 device is NULL");
        return false;
    }

    if (!mIsEnabled)
    {
        // quiet exit, since debugging is disabled
        return true;
    }

    HRESULT hr = mD3D12Device->QueryInterface(IID_PPV_ARGS(&mD3D12DebugDevice));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get D3D12 Debug Device interface");

    hr = mD3D12Device->QueryInterface(IID_PPV_ARGS(&mD3D12InfoQueue));
    D3D12NI_RET_IF_FAILED(hr, false, "Failed to get D3D12 Info Queue interface");

    // a list of debug messages to filter out
    D3D12_MESSAGE_ID filterMsgs[] =
    {
        // D3D12 provides a very minor and not even guaranteed perf boost when CreateCommittedResource
        // is called with "initial clear value" when creating a RenderTarget or DepthStencil resource.
        // That clear value then has to be provided to ClearRenderTargetView and if it ever differs,
        // it will still work but might be ever-so-slightly (potentially) slower and produces a warning
        // in debug layers.
        // In case of JFX it is VERY difficult to guesstimate which color would be the most frequently
        // used as clear color, so we provide zeros when creating RTTs hoping it will speed things up
        // in some cases. To reduce the noise in cases we don't clear to zeros we silence the warning.
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

    if (Config::IsBreakOnErrorEnabled())
    {
        hr = mD3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to set break on D3D12 errors");
        hr = mD3D12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        D3D12NI_RET_IF_FAILED(hr, false, "Failed to set break on D3D12 corruptions");
    }

    D3D12NI_LOG_INFO("D3D12 Device debugging set up");
    return true;
}

void Debug::ReleaseAndReportLiveObjects()
{
    // This function should be the last resource release section when NativeDevice gets removed.
    // If below reports differ from what logs suggest we have a leak that needs fixing.

    if (!mIsEnabled) return;

    D3D12NI_LOG_DEBUG(" ======= Starting Live Object report =======");
    D3D12NI_LOG_DEBUG("Note that this only reports app-used live objects, ignoring internal ones.");

    mD3D12Device.Reset();
    mD3D12InfoQueue.Reset();
    mD3D12Debug.Reset();

    if (mD3D12DebugDevice)
    {
        D3D12NI_LOG_DEBUG("Live D3D12 objects at Debug Release (there should be only one ID3D12Device with Refcount: 1):");
        mD3D12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        mD3D12DebugDevice.Reset();
    }

    mDXGIInfoQueue.Reset();

    if (mDXGIDebug)
    {
        D3D12NI_LOG_DEBUG("Live DXGI objects at Debug Release (this list should be empty):");
        mDXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    }

    D3D12NI_LOG_DEBUG(" ======= Live Object report complete =======");

    mIsEnabled = false;
}

bool Debug::IsEnabled()
{
    return mIsEnabled;
}

void Debug::ExamineDeviceRemoved()
{
    // Device removed reason can always be fetched
    HRESULT reason = mD3D12Device->GetDeviceRemovedReason();
    if (SUCCEEDED(reason))
    {
        // quietly exit, device is OK
        return;
    }

    _com_error comReason(reason);
    D3D12NI_LOG_ERROR("Device removed reason: %x (%ws)", reason, comReason.ErrorMessage());

    // fetch as much data as possible in hopes of getting some debugging information
    if (!mIsDREDEnabled)
    {
        D3D12NI_LOG_ERROR("DRED disabled - no more device removed information could be fetched.");
        D3D12NI_LOG_ERROR("To get more information, re-launch with -Dprism.d3d12.dred=true");
        return;
    }

    D3D12DeviceRemovedExtendedData dred;
    HRESULT hr = mD3D12Device->QueryInterface(IID_PPV_ARGS(&dred));
    D3D12NI_VOID_RET_IF_FAILED(hr, "Failed to fetch DRED interface");

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT dredBreadcrumbs;
    D3D12_DRED_PAGE_FAULT_OUTPUT dredPageFault;

    HRESULT hrBreadcrumbs = dred->GetAutoBreadcrumbsOutput(&dredBreadcrumbs);
    HRESULT hrPageFault = dred->GetPageFaultAllocationOutput(&dredPageFault);

    if (SUCCEEDED(hrBreadcrumbs))
    {
        D3D12NI_LOG_INFO("DRED breadcrumbs:");
        const D3D12_AUTO_BREADCRUMB_NODE* node = dredBreadcrumbs.pHeadAutoBreadcrumbNode;
        while (node != nullptr)
        {
            DREDProcessBreadcrumbNode(node);
            node = node->pNext;
        }
    }

    if (SUCCEEDED(hrPageFault))
    {
        D3D12NI_LOG_INFO("DRED page fault information (VA %x):", dredPageFault.PageFaultVA);
        D3D12NI_LOG_INFO("  Existing allocation nodes:");
        const D3D12_DRED_ALLOCATION_NODE* node = dredPageFault.pHeadExistingAllocationNode;
        while (node != nullptr)
        {
            DREDProcessPageFaultNode(node);
            node = node->pNext;
        }

        D3D12NI_LOG_INFO("  Recently freed allocation nodes:");
        node = dredPageFault.pHeadRecentFreedAllocationNode;
        while (node != nullptr)
        {
            DREDProcessPageFaultNode(node);
            node = node->pNext;
        }
    }
}

} // namespace Internal
} // namespace D3D12
