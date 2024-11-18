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

package com.sun.prism.d3d12.ni;

import java.nio.ByteBuffer;

public final class D3D12NativeInstance extends D3D12NativeObject {
    public D3D12NativeInstance() {
        super(nAllocateNativeInstance());
    }

    private static native long nAllocateNativeInstance();
    @Override protected native void nReleaseNativeObject(long ptr);

    private native boolean nInit(long ptr);
    private native int nGetAdapterCount(long ptr);
    private native int nGetAdapterOrdinal(long ptr, long screenNativeHandle);
    private native boolean nLoadInternalShader(long ptr, String name, int mode, int visibility, ByteBuffer code);
    private native long nCreateDevice(long ptr, int adapterOrdinal);
    private native long nCreateSwapChain(long ptr, long devicePtr, long hwnd);

    public boolean Init() {
        return nInit(this.ptr);
    }

    public int getAdapterCount() {
        return nGetAdapterCount(this.ptr);
    }

    public int getAdapterOrdinal(long screenNativeHandle) {
        return nGetAdapterOrdinal(this.ptr, screenNativeHandle);
    }

    public boolean loadInternalSahder(String name, D3D12NativeShader.PipelineMode mode, D3D12NativeShader.Visibility visibility, ByteBuffer code) {
        return nLoadInternalShader(this.ptr, name, mode.ordinal(), visibility.ordinal(), code);
    }

    public D3D12NativeDevice createDevice(int adapterOrdinal) {
        return new D3D12NativeDevice(nCreateDevice(this.ptr, adapterOrdinal));
    }

    public D3D12NativeSwapChain createSwapChain(D3D12NativeDevice device, long hwnd) {
        return new D3D12NativeSwapChain(nCreateSwapChain(this.ptr, device.ptr, hwnd));
    }
}
