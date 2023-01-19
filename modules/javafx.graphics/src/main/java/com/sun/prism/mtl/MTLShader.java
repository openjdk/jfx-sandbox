/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.prism.mtl;

import com.sun.prism.ps.Shader;
import com.sun.prism.Texture;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.HashMap;
import java.util.Map;

public class MTLShader implements Shader  {

    // TODO: MTL: Use table/Dict for storing shader function pointer based on function name
    private final MTLContext context;
    private final String fragmentFunctionName;
    private final Map<Integer, String> samplers = new HashMap<>();
    private final long nMetalShaderRef;

    private static Map<String, MTLShader> shaderMap = new HashMap<>();
    private static MTLShader currentEnabledShader;


    native private static long nCreateMetalShader(long context, String fragFuncName);
    native private static long nEnable(long nMetalShader);
    native private static long nDisable(long nMetalShader);

    native private static long nSetTexture(long nMetalShader, String texName, long texPtr);

    native private static long nSetInt(long nMetalShader, String uniformName, int f0);

    native private static long nSetFloat(long nMetalShader, String uniformName, float f0);
    native private static long nSetFloat2(long nMetalShader, String uniformName, float f0, float f1);
    native private static long nSetFloat3(long nMetalShader, String uniformName, float f0, float f1, float f2);
    native private static long nSetFloat4(long nMetalShader, String uniformName, float f0, float f1, float f2, float f3);

    native private static long nSetConstants(long nMetalShader, String uniformName, float[] values, int size);

    private MTLShader(MTLContext context, String fragmentFunctionName) {
        System.err.println(">>> MTLShader(): fragFuncName = " + fragmentFunctionName);

        this.fragmentFunctionName = fragmentFunctionName;
        this.context = context;

        nMetalShaderRef = nCreateMetalShader(context.getContextHandle(), fragmentFunctionName);
        if (nMetalShaderRef != 0) {
            shaderMap.put(fragmentFunctionName, this);
        } else {
            throw new AssertionError("Failed to create Shader");
        }
        System.err.println("    shaderMap.size() : " + shaderMap.size());
        System.err.println("    shaderMap" + shaderMap);
        System.err.println("<<< MTLShader(): nMetalShaderRef = " + nMetalShaderRef);
    }

    public static Shader createShader(MTLContext ctx, String fragFuncName, Map<String, Integer> samplers,
                                      Map<String, Integer> params, int maxTexCoordIndex,
                                      boolean isPixcoordUsed, boolean isPerVertexColorUsed) {
        System.err.println(">>> MTLShader.createShader()1");
        System.err.println("    fragFuncName= " + fragFuncName);
        System.err.println("    samplers= " + samplers);
        System.err.println("    params= " + params);
        System.err.println("    maxTexCoordIndex= " + maxTexCoordIndex);
        System.err.println("    isPixcoordUsed= " + isPixcoordUsed);
        System.err.println("    isPerVertexColorUsed= " + isPerVertexColorUsed);

        if (shaderMap.containsKey(fragFuncName)) {
            System.err.println("    The shader was already created and exists in map");
            System.err.println("<<< MTLShader.createShader()");
            return shaderMap.get(fragFuncName);
        } else {
            MTLShader shader = new MTLShader(ctx, fragFuncName);
            shader.storeSamplers(samplers);
            System.err.println("<<< MTLShader.createShader()");
            return shader;
        }
    }

    public static MTLShader createShader(MTLContext ctx, String fragFuncName) {
        System.err.println(">>> MTLShader.createShader()2");
        System.err.println("    fragmentFunctionName= " + fragFuncName);
        MTLShader shader;
        if (shaderMap.containsKey(fragFuncName)) {
            System.err.println("    The shader was already created and exists in map");
            shader = shaderMap.get(fragFuncName);
        } else {
            shader = new MTLShader(ctx, fragFuncName);
        }
        System.err.println("<<< MTLShader.createShader()");
        return shader;
    }

    private void storeSamplers(Map<String, Integer> samplers) {
        for (Map.Entry<String, Integer> entry : samplers.entrySet()) {
            this.samplers.put(entry.getValue(), entry.getKey());
        }
        System.err.println(">>> MTLShader.storeSamplers() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.storeSamplers() : samplers : " + this.samplers);
    }

    @Override
    public void enable() {
        System.err.println(">> MTLShader.enable()  fragFuncName = " + fragmentFunctionName);
        currentEnabledShader = this;
        nEnable(nMetalShaderRef);
    }

    @Override
    public void disable() {
        System.err.println("MTLShader.disable()  fragFuncName = " + fragmentFunctionName);
        nDisable(nMetalShaderRef);
    }

    @Override
    public boolean isValid() {
        System.err.println("MTLShader.isValid()");
        if (nMetalShaderRef != 0) {
            return true;
        } else {
            return false;
        }
    }

    public static void setTexture(int texUnit, Texture tex) {
        System.err.println(">>> MTLShader.setTexture() : fragmentFunctionName : " + currentEnabledShader.fragmentFunctionName);
        System.err.println("    MTLShader.setTexture() texUnit = " + texUnit);
        MTLTexture mtlTex = (MTLTexture)tex;
        nSetTexture(currentEnabledShader.nMetalShaderRef,
                currentEnabledShader.samplers.get(texUnit), mtlTex.getNativeHandle());
    }

    @Override
    public void setConstant(String name, int i0) {
        System.err.println(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.setConstant() name = " + name + ", i0 = " + i0);
        nSetInt(nMetalShaderRef, name, i0);
        //throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, int i0, int i1) {
        System.err.println("MTLShader.setConstant() name = " + name + ", i0 = " + i0 + ", i1 = " + i1);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, int i0, int i1, int i2) {
        System.err.println("MTLShader.setConstant() name = " + name + ", i0 = " + i0 + ", i1 = " + i1 + ", i2 = " + i2);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, int i0, int i1, int i2, int i3) {
        System.err.println("MTLShader.setConstant() name = " + name + ", i0 = " + i0 + ", i1 = " + i1 + ", i2 = " + i2 + ", i3 = " + i3);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstants(String name, IntBuffer buf, int off, int count) {
        System.err.println("MTLShader.setConstants() name = " + name + ", buf = " + buf + ", off = " + off + ", count = " + count);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, float f0) {
        System.err.println(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.setConstant() name = " + name + ", f0 = " + f0);
        nSetFloat(nMetalShaderRef, name, f0);
        System.err.println("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstant(String name, float f0, float f1) {
        System.err.println(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.setConstant() name = " + name + ", f0 = " + f0 + ", f1 = " + f1);
        nSetFloat2(nMetalShaderRef, name, f0, f1);
        System.err.println("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstant(String name, float f0, float f1, float f2) {
        System.err.println(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.setConstant() name = " + name + ", f0 = " + f0 + ", f1 = " + f1 + ", f2 = " + f2);
        nSetFloat3(nMetalShaderRef, name, f0, f1, f2);
        System.err.println("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstant(String name, float f0, float f1, float f2, float f3) {
        System.err.println(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.setConstant() name = " + name + ", f0 = " + f0 + ", f1 = " + f1 + ", f2 = " + f2 + ", f3 = " + f3);
        nSetFloat4(nMetalShaderRef, name, f0, f1, f2, f3);
        System.err.println("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstants(String name, FloatBuffer buf, int off, int count) {
        System.err.println(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        System.err.println("    MTLShader.setConstant() name = " + name + ", buf = " + buf + ", off = " + off + ", count = " + count);
        float[] values = new float[count];
        System.err.println("MTLShader.setConstant() name = " + name + ", buf = " + values + ", off = " + off + ", count = " + count);
        buf.get(off, values, 0, count);
        nSetConstants(nMetalShaderRef, name, values, count);
    }

    @Override
    public void dispose() {
        System.err.println(">>> MTLShader.dispose() : fragmentFunctionName : " + this.fragmentFunctionName);
    }
}
