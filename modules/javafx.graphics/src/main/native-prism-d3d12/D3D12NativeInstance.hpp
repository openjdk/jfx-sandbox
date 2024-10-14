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

#include "D3D12Common.hpp"
#include "D3D12NativeDevice.hpp"
#include "D3D12NativeSwapChain.hpp"

#include "Internal/D3D12ShaderLibrary.hpp"

#include <vector>


namespace D3D12 {

class NativeInstance
{
    DXGIFactoryPtr mDXGIFactory;
    std::vector<IDXGIAdapter1*> mDXGIAdapters;
    NIPtr<Internal::ShaderLibrary> mShaderLibrary;

public:
    NativeInstance();
    ~NativeInstance();

    bool Init();
    int GetAdapterCount();
    int GetAdapterOrdinal(HMONITOR monitor);
    bool LoadInternalShader(const std::string& name, ShaderPipelineMode mode, D3D12_SHADER_VISIBILITY visibility, void* code, size_t codeSize);
    NIPtr<NativeDevice>* CreateDevice(int adapterOrdinal);
    NIPtr<NativeSwapChain>* CreateSwapChain(const NIPtr<NativeDevice>& device, HWND hwnd);
};

} // namespace D3D12
