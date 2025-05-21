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

import java.lang.ref.WeakReference;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.util.HashMap;
import java.util.Map;
import java.util.ArrayList;


public class MTLShader implements Shader  {

    private final MTLContext context;
    private final String fragmentFunctionName;
    private long nMetalShaderRef;
    private final Map<Integer, String> samplers = new HashMap<>();
    private final Map<String, Integer> uniformNameIdMap;
    private final Map<Integer, WeakReference<Object>> textureIdRefMap = new HashMap<>();

    private static Map<String, MTLShader> shaderMap = new HashMap<>();
    private static MTLShader currentEnabledShader;

    private MTLShader(MTLContext context, String fragmentFunctionName) {
        MTLLog.Debug(">>> MTLShader(): fragFuncName = " + fragmentFunctionName);

        this.fragmentFunctionName = fragmentFunctionName;
        this.context = context;

        nMetalShaderRef = nCreateMetalShader(context.getContextHandle(), fragmentFunctionName);
        if (nMetalShaderRef != 0) {
            shaderMap.put(fragmentFunctionName, this);
        } else {
            throw new AssertionError("Failed to create Shader");
        }
        uniformNameIdMap = nGetUniformNameIdMap(nMetalShaderRef);
        MTLLog.Debug("    uniformNameIdMap: " + uniformNameIdMap);
        MTLLog.Debug("    shaderMap.size(): " + shaderMap.size());
        MTLLog.Debug("    shaderMap" + shaderMap);
        MTLLog.Debug("<<< MTLShader(): nMetalShaderRef = " + nMetalShaderRef);
    }

    public static Shader createShader(MTLContext ctx, String fragFuncName, Map<String, Integer> samplers,
                                      Map<String, Integer> params, int maxTexCoordIndex,
                                      boolean isPixcoordUsed, boolean isPerVertexColorUsed) {
        MTLLog.Debug(">>> MTLShader.createShader()1");
        MTLLog.Debug("    fragFuncName= " + fragFuncName);
        MTLLog.Debug("    samplers= " + samplers);
        MTLLog.Debug("    params= " + params);
        MTLLog.Debug("    maxTexCoordIndex= " + maxTexCoordIndex);
        MTLLog.Debug("    isPixcoordUsed= " + isPixcoordUsed);
        MTLLog.Debug("    isPerVertexColorUsed= " + isPerVertexColorUsed);

        if (shaderMap.containsKey(fragFuncName)) {
            MTLLog.Debug("    The shader was already created and exists in map");
            MTLLog.Debug("<<< MTLShader.createShader()1");
            return shaderMap.get(fragFuncName);
        } else {
            MTLShader shader = new MTLShader(ctx, fragFuncName);
            shader.storeSamplers(samplers);
            MTLLog.Debug("<<< MTLShader.createShader()1");
            return shader;
        }
    }

    public static MTLShader createShader(MTLContext ctx, String fragFuncName) {
        MTLLog.Debug(">>> MTLShader.createShader()2");
        MTLLog.Debug("    fragmentFunctionName= " + fragFuncName);
        MTLShader shader;
        if (shaderMap.containsKey(fragFuncName)) {
            MTLLog.Debug("    The shader was already created and exists in map");
            shader = shaderMap.get(fragFuncName);
        } else {
            shader = new MTLShader(ctx, fragFuncName);
        }
        MTLLog.Debug("<<< MTLShader.createShader()2");
        return shader;
    }

    private void storeSamplers(Map<String, Integer> samplers) {
        for (Map.Entry<String, Integer> entry : samplers.entrySet()) {
            this.samplers.put(entry.getValue(), entry.getKey());
        }
        //MTLLog.Debug(">>> MTLShader.storeSamplers() : fragmentFunctionName : " + this.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.storeSamplers() : samplers : " + this.samplers);
    }

    @Override
    public void enable() {
        MTLLog.Debug(">> MTLShader.enable()  fragFuncName = " + fragmentFunctionName);
        currentEnabledShader = this;
        nEnable(nMetalShaderRef);
    }

    @Override
    public void disable() {
        MTLLog.Debug("MTLShader.disable()  fragFuncName = " + fragmentFunctionName);
        // TODO: MTL: There are no disable calls coming from BaseShaderContext.
        // So this is a no-op. We can call disable on lastShader in
        // BaseShaderContext.checkState() but that will be a common change for
        // all pipelines.
        nDisable(nMetalShaderRef);
    }

    @Override
    public boolean isValid() {
        MTLLog.Debug("MTLShader.isValid()");
        if (nMetalShaderRef != 0) {
            return true;
        } else {
            return false;
        }
    }

    public static void setTexture(int texUnit, Texture tex, boolean isLinear, int wrapMode) {
        //MTLLog.Debug(">>> MTLShader.setTexture() : fragmentFunctionName : " + currentEnabledShader.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.setTexture() texUnit = " + texUnit + ", isLinear = " + isLinear + ", wrapMode = " + wrapMode);
        if (currentEnabledShader.textureIdRefMap.get(texUnit) != null &&
            currentEnabledShader.textureIdRefMap.get(texUnit).get() == tex) return;

        currentEnabledShader.textureIdRefMap.put(texUnit, new WeakReference(tex));
        MTLTexture mtlTex = (MTLTexture)tex;
        nSetTexture(currentEnabledShader.nMetalShaderRef, texUnit,
                currentEnabledShader.uniformNameIdMap.get(currentEnabledShader.samplers.get(texUnit)),
                mtlTex.getNativeHandle(), isLinear, wrapMode);
    }

    @Override
    public void setConstant(String name, int i0) {
       //MTLLog.Debug(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
       //MTLLog.Debug("    MTLShader.setConstant() name = " + name + ", i0 = " + i0);
        nSetInt(nMetalShaderRef, uniformNameIdMap.get(name), i0);
    }

    @Override
    public void setConstant(String name, int i0, int i1) {
        //MTLLog.Debug("MTLShader.setConstant() name = " + name + ", i0 = " + i0 + ", i1 = " + i1);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, int i0, int i1, int i2) {
        //MTLLog.Debug("MTLShader.setConstant() name = " + name + ", i0 = " + i0 + ", i1 = " + i1 + ", i2 = " + i2);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, int i0, int i1, int i2, int i3) {
        //MTLLog.Debug("MTLShader.setConstant() name = " + name + ", i0 = " + i0 + ", i1 = " + i1 + ", i2 = " + i2 + ", i3 = " + i3);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstants(String name, IntBuffer buf, int off, int count) {
        //MTLLog.Debug("MTLShader.setConstants() name = " + name + ", buf = " + buf + ", off = " + off + ", count = " + count);
        throw new UnsupportedOperationException("Not implemented yet.");
    }

    @Override
    public void setConstant(String name, float f0) {
        //MTLLog.Debug(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.setConstant() name = " + name + ", f0 = " + f0);
        nSetFloat1(nMetalShaderRef, uniformNameIdMap.get(name), f0);
        //MTLLog.Debug("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstant(String name, float f0, float f1) {
        //MTLLog.Debug(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.setConstant() name = " + name + ", f0 = " + f0 + ", f1 = " + f1);
        nSetFloat2(nMetalShaderRef, uniformNameIdMap.get(name), f0, f1);
        //MTLLog.Debug("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstant(String name, float f0, float f1, float f2) {
        //MTLLog.Debug(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.setConstant() name = " + name + ", f0 = " + f0 + ", f1 = " + f1 + ", f2 = " + f2);
        nSetFloat3(nMetalShaderRef, uniformNameIdMap.get(name), f0, f1, f2);
        //MTLLog.Debug("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstant(String name, float f0, float f1, float f2, float f3) {
        //MTLLog.Debug(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.setConstant() name = " + name + ", f0 = " + f0 + ", f1 = " + f1 + ", f2 = " + f2 + ", f3 = " + f3);
        nSetFloat4(nMetalShaderRef, uniformNameIdMap.get(name), f0, f1, f2, f3);
        //MTLLog.Debug("<< MTLShader.setConstant()");
    }

    @Override
    public void setConstants(String name, FloatBuffer buf, int off, int count) {
        //MTLLog.Debug(">>> MTLShader.setConstant() : fragmentFunctionName : " + this.fragmentFunctionName);
        //MTLLog.Debug("    MTLShader.setConstant() name = " + name + ", buf = " + buf + ", off = " + off + ", count = " + count);
        boolean direct = buf.isDirect();
        if (direct) {
            nSetConstantsBuf(nMetalShaderRef, uniformNameIdMap.get(name),
                                buf, buf.position() * 4, count * 4);
        } else {
            count = 4 * count;
            float[] values = new float[count];
            buf.get(off, values, 0, count);
            nSetConstants(nMetalShaderRef, uniformNameIdMap.get(name), values, count);
        }
    }

    @Override
    public void dispose() {
        MTLLog.Debug(">>> MTLShader.dispose() : fragmentFunctionName : " + this.fragmentFunctionName);
        if (isValid()) {
            context.disposeShader(nMetalShaderRef);
            nMetalShaderRef = 0;
            textureIdRefMap.clear();
        }
    }

    // Native methods

    native private static long nCreateMetalShader(long context, String fragFuncName);
    native private static Map  nGetUniformNameIdMap(long nMetalShader);
    native private static void nEnable(long nMetalShader);
    native private static void nDisable(long nMetalShader);

    native private static void nSetTexture(long nMetalShader, int texID, int uniformID,
                                           long texPtr, boolean isLinear, int wrapMode);

    native private static void nSetInt(long nMetalShader, int uniformID, int i0);

    native private static void nSetFloat1(long nMetalShader, int uniformID, float f0);
    native private static void nSetFloat2(long nMetalShader, int uniformID,
                                            float f0, float f1);
    native private static void nSetFloat3(long nMetalShader, int uniformID,
                                            float f0, float f1, float f2);
    native private static void nSetFloat4(long nMetalShader, int uniformID,
                                            float f0, float f1, float f2, float f3);

    native private static void nSetConstants(long nMetalShader, int uniformID,
                                            float[] values, int size);
    native private static void nSetConstantsBuf(long nMetalShader, int uniformID,
                                    Object values, int valuesByteOffset, int size);
}
