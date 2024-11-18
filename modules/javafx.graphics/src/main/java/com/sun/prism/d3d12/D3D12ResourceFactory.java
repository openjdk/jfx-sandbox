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

package com.sun.prism.d3d12;

import java.io.InputStream;
import java.lang.reflect.Method;
import java.util.Map;

import com.sun.glass.ui.Screen;
import com.sun.prism.MediaFrame;
import com.sun.prism.Mesh;
import com.sun.prism.MeshView;
import com.sun.prism.PhongMaterial;
import com.sun.prism.PixelFormat;
import com.sun.prism.Presentable;
import com.sun.prism.PresentableState;
import com.sun.prism.RTTexture;
import com.sun.prism.Texture;
import com.sun.prism.Texture.Usage;
import com.sun.prism.Texture.WrapMode;
import com.sun.prism.impl.PrismSettings;
import com.sun.prism.impl.TextureResourcePool;
import com.sun.prism.impl.ps.BaseShaderFactory;
import com.sun.prism.ps.Shader;
import com.sun.prism.ps.ShaderFactory;
import com.sun.prism.d3d12.ni.D3D12NativeInstance;
import com.sun.prism.d3d12.ni.D3D12NativeTexture;


class D3D12ResourceFactory extends BaseShaderFactory {
    private final D3D12NativeInstance mInstance;
    private final D3D12Context mContext;
    private int mMaximumTextureSize;

    D3D12ResourceFactory(D3D12NativeInstance instance, int adapterOrdinal, Screen screen) {
        super();
        mInstance = instance;
        mContext = new D3D12Context(mInstance.createDevice(adapterOrdinal), screen, this);

        mMaximumTextureSize = mContext.getDevice().getMaximumTextureSize();
    }

    D3D12Context getContext() {
        return mContext;
    }

    D3D12NativeInstance getNativeInstance() {
        return mInstance;
    }

    @Override
    public Shader createShader(String pixelShaderName, InputStream pixelShaderCode, Map<String, Integer> samplers, Map<String, Integer> params,
                               int maxTexCoordIndex, boolean isPixcoordUsed, boolean isPerVertexColorUsed) {
        if (checkDisposed()) return null;

        try {
            return D3D12Shader.create(mContext, pixelShaderName, pixelShaderCode, samplers, params);
        } catch (Exception e) {
            throw new InternalError("Failed to create shader: " + e.getMessage());
        }
    }

    @Override
    public Shader createStockShader(String name) {
        if (name == null) {
            throw new IllegalArgumentException("Shader name must be non-null");
        }
        try {
            Class klass = Class.forName("com.sun.prism.shader." + name + "_Loader");
            InputStream codeStream = D3D12ResourceFactory.class.getResourceAsStream("hlsl6/" + name + ".cso");
            Method m = klass.getMethod("loadShader",
                new Class[] { ShaderFactory.class, String.class, InputStream.class });
            return (Shader)m.invoke(null, new Object[] { this, name, codeStream });
        } catch (Throwable e) {
            e.printStackTrace();
            throw new InternalError("Error loading stock shader " + name);
        }
    }

    @Override
    public TextureResourcePool<D3D12TextureData> getTextureResourcePool() {
        return D3D12ResourcePool.instance;
    }

    static int nextPowerOfTwo(int val, int max) {
        if (val > max) {
            return 0;
        }
        int i = 1;
        while (i < val) {
            i *= 2;
        }
        return i;
    }

    @Override
    public Texture createTexture(PixelFormat formatHint, Usage usageHint, WrapMode wrapMode, int w, int h) {
        return createTexture(formatHint, usageHint, wrapMode, w, h, false);
    }

    @Override
    public Texture createTexture(PixelFormat formatHint, Usage usageHint, WrapMode wrapMode, int w, int h,
            boolean useMipmap) {
        if (checkDisposed()) return null;

        if (!isFormatSupported(formatHint)) {
            throw new UnsupportedOperationException(
                "Pixel format " + formatHint + " not supported on this device");
        }

        if (formatHint == PixelFormat.MULTI_YCbCr_420) {
            throw new UnsupportedOperationException("MULTI_YCbCr_420 textures require a MediaFrame");
        }

        int allocw, alloch;
        if (PrismSettings.forcePow2) {
            allocw = nextPowerOfTwo(w, Integer.MAX_VALUE);
            alloch = nextPowerOfTwo(h, Integer.MAX_VALUE);
        } else {
            allocw = w;
            alloch = h;
        }

        if (allocw <= 0 || alloch <= 0) {
            throw new RuntimeException("Illegal texture dimensions (" + allocw + "x" + alloch + ")");
        }

        int bpp = formatHint.getBytesPerPixelUnit();
        if (allocw >= (Integer.MAX_VALUE / alloch / bpp)) {
            throw new RuntimeException("Illegal texture dimensions (" + allocw + "x" + alloch + ")");
        }

        D3D12ResourcePool pool = D3D12ResourcePool.instance;
        long size = pool.estimateTextureSize(allocw, alloch, formatHint);
        if (!pool.prepareForAllocation(size)) {
            return null;
        }

        D3D12NativeTexture texture = mContext.getDevice().createTexture(
            allocw, alloch, formatHint, usageHint, 1, useMipmap, false
        );

        if (!texture.isValid()) {
            throw new RuntimeException("Failed to create D3D12 texture");
        }

        return D3D12Texture.create(texture, mContext, formatHint, wrapMode);
    }

    @Override
    public Texture createTexture(MediaFrame frame) {
        throw new UnsupportedOperationException("Unimplemented method 'createTexture'");
    }

    @Override
    public boolean isFormatSupported(PixelFormat format) {
        return mContext.getDevice().checkFormatSupport(format);
    }

    @Override
    public int getMaximumTextureSize() {
        return mMaximumTextureSize;
    }

    @Override
    public int getRTTWidth(int w, WrapMode wrapMode) {
        // Same as D3D9, D3D12 supports non-power-of-two dimensions for RTTs
        return w;
    }

    @Override
    public int getRTTHeight(int h, WrapMode wrapMode) {
        // Same as D3D9, D3D12 supports non-power-of-two dimensions for RTTs
        return h;
    }

    @Override
    public RTTexture createRTTexture(int width, int height, WrapMode wrapMode) {
        return createRTTexture(width, height, wrapMode, false);
    }

    @Override
    public RTTexture createRTTexture(int width, int height, WrapMode wrapMode, boolean msaa) {
        if (checkDisposed()) return null;

        int createw = width;
        int createh = height;
        if (PrismSettings.forcePow2) {
            createw = nextPowerOfTwo(createw, Integer.MAX_VALUE);
            createh = nextPowerOfTwo(createh, Integer.MAX_VALUE);
        }

        if (createw <= 0 || createh <= 0) {
            throw new RuntimeException("Illegal texture dimensions (" + createw + "x" + createh + ")");
        }

        PixelFormat format = PixelFormat.INT_ARGB_PRE;
        int bpp = format.getBytesPerPixelUnit();
        if (createw >= (Integer.MAX_VALUE / createh / bpp)) {
            throw new RuntimeException("Illegal texture dimensions (" + createw + "x" + createh + ")");
        }

        D3D12ResourcePool pool = D3D12ResourcePool.instance;
        int aaSamples = 1;
        if (msaa) {
            aaSamples = mContext.getMSAASampleSize(format);
        }

        // TODO: D3D12: 3D - Improve estimate to include if multisample rtt
        long size = pool.estimateRTTextureSize(width, height, false);
        if (!pool.prepareForAllocation(size)) {
            return null;
        }

        D3D12RTTexture tex = D3D12RTTexture.create(mContext, width, height, format, wrapMode, aaSamples);
        if (!tex.isValid()) {
            return null;
        }

        return tex;
    }

    @Override
    public boolean isCompatibleTexture(Texture tex) {
        return tex instanceof D3D12Texture;
    }

    @Override
    public Presentable createPresentable(PresentableState pState) {
        if (checkDisposed()) return null;

        return D3D12SwapChain.create(mContext, pState);
    }

    @Override
    public PhongMaterial createPhongMaterial() {
        if (checkDisposed()) return null;
        return D3D12PhongMaterial.create(mContext);
    }

    @Override
    public MeshView createMeshView(Mesh mesh) {
        if (checkDisposed()) return null;
        return D3D12MeshView.create(mContext, (D3D12Mesh)mesh);
    }

    @Override
    public Mesh createMesh() {
        if (checkDisposed()) return null;
        return D3D12Mesh.create(mContext);
    }

    @Override
    public void dispose() {
        super.dispose();
        mContext.dispose();
    }
}
