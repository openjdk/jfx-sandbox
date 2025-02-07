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

namespace D3D12 {
namespace ShaderSlots {

// 2D Passthrough vertex shader, binds to index 0 in Root Signature (ensured in D3D12NativeShader.cpp)

const unsigned int PASSTHROUGH_WVP_TRANSFORM = 0;

// Below assingments must match root signature entry indexes in Mtl1PS.hlsl

const unsigned int PHONG_VS_DATA = 0;       // Root Descriptor
const unsigned int PHONG_PS_COLOR_SPEC = 1; // Root Descriptor

const unsigned int PHONG_VS_LIGHT_SPEC = 2; // Descriptor Table
const unsigned int PHONG_PS_LIGHT_SPEC = 3; // Descriptor Table

const unsigned int PHONG_PS_TEXTURE_DTABLE = 4; // Descriptor Table
const unsigned int PHONG_PS_SAMPLER_DTABLE = 5; // Descriptor Table

} // namespace ShaderSlots
} // namespace D3D12
