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

#include "../D3D12Common.hpp"


namespace D3D12 {
namespace Internal {

class Debug
{
    D3D12DevicePtr mD3D12Device;
    DXGIDebugPtr mDXGIDebug;
    DXGIInfoQueuePtr mDXGIInfoQueue;
    D3D12DebugPtr mD3D12Debug;
    D3D12InfoQueuePtr mD3D12InfoQueue;
    D3D12DebugDevicePtr mD3D12DebugDevice;
    DWORD mD3D12MessageCallbackCookie;
    bool mIsEnabled;
    bool mIsDREDEnabled;

    Debug();
    ~Debug();

    Debug(const Debug&) = delete;
    Debug(Debug&&) = delete;
    Debug& operator=(const Debug&) = delete;
    Debug& operator=(Debug&&) = delete;

    void DREDProcessBreadcrumbNode(const D3D12_AUTO_BREADCRUMB_NODE* node);
    void DREDProcessPageFaultNode(const D3D12_DRED_ALLOCATION_NODE* node);

public:
    static Debug& Instance();

    bool Init();
    bool InitDeviceDebug(const NIPtr<NativeDevice>& debug);
    void ReleaseAndReportLiveObjects();
    bool IsEnabled();
    void ExamineDeviceRemoved();
};

} // namespace Internal
} // namespace D3D12
