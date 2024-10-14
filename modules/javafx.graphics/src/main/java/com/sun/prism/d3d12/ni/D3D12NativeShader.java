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

import java.nio.FloatBuffer;
import java.nio.IntBuffer;

public class D3D12NativeShader extends D3D12NativeObject {

    // mimics internal ShaderPipelineMode enum from D3D12Common.hpp
    public enum PipelineMode {
        UI_2D,
        PHONG_3D
    }

    // Mimicks D3D12_SHADER_VISIBILITY from d3d12.h
    public enum Visibility {
        ALL,
        VERTEX,
        HULL,
        DOMAIN,
        GEOMETRY,
        PIXEL,
    };

    D3D12NativeShader(long ptr) {
        super(ptr);
    }

    @Override protected native void nReleaseNativeObject(long ptr);
    private native boolean nSetConstantsF(long ptr, String name, FloatBuffer buf, int off, int count);
    private native boolean nSetConstantsI(long ptr, String name, IntBuffer buf, int off, int count);

    public boolean setConstants(String name, FloatBuffer buf, int off, int count) {
        return nSetConstantsF(ptr, name, buf, off, count);
    }

    public boolean setConstants(String name, IntBuffer buf, int off, int count) {
        return nSetConstantsI(ptr, name, buf, off, count);
    }
}
