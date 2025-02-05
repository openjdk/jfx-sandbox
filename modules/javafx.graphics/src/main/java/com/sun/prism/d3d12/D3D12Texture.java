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

import java.nio.Buffer;
import java.nio.ByteBuffer;

import com.sun.prism.MediaFrame;
import com.sun.prism.PixelFormat;
import com.sun.prism.Texture;
import com.sun.prism.impl.BaseTexture;
import com.sun.prism.d3d12.ni.D3D12NativeTexture;

class D3D12Texture extends BaseTexture<D3D12Resource<D3D12TextureData>> {
    protected D3D12Context mContext;

    protected D3D12Texture(D3D12Resource<D3D12TextureData> resource, D3D12Context context, PixelFormat format, WrapMode wrapMode,
                           int width, int height) {
        super(resource, format, wrapMode, width, height);
        mContext = context;
    }

    private D3D12Texture(D3D12Texture sharedTex, WrapMode altMode) {
        super(sharedTex, altMode, false);
    }

    public static D3D12Texture create(D3D12NativeTexture nativeTexture, D3D12Context context,
                                      PixelFormat format, WrapMode wrapMode) {
        return new D3D12Texture(new D3D12Resource<D3D12TextureData>(
                                    new D3D12TextureData(
                                        nativeTexture, null, nativeTexture.getWidth(), nativeTexture.getHeight(), format
                                    )
                                ),
                                context,
                                format,
                                wrapMode,
                                nativeTexture.getWidth(),
                                nativeTexture.getHeight());
    }

    D3D12NativeTexture getNativeTexture() {
        return resource.getResource().getNativeTexture();
    }

    @Override
    public void update(Buffer buffer, PixelFormat format, int dstx, int dsty, int srcx, int srcy, int srcw, int srch,
                       int srcscan, boolean skipFlush) {
        checkUpdateParams(buffer, format, dstx, dsty, srcx, srcy, srcw, srch, srcscan);

        if (!skipFlush) {
            mContext.flushVertexBuffer();
        }

        int contentX = getContentX();
        int contentY = getContentY();
        int contentW = getContentWidth();
        int contentH = getContentHeight();
        int texWidth = getPhysicalWidth();
        int texHeight = getPhysicalHeight();
        boolean res = mContext.getDevice().updateTexture(getNativeTexture(),
                                                         buffer, format,
                                                         dstx, dsty,
                                                         srcx, srcy, srcw, srch, srcscan);
        if (!res) {
            new Exception(String.format("D3D12: Texture update failed. Stack trace:")).printStackTrace(System.err);
        }

        // TODO: D3D12: getWrapMode() and simulate
    }

    @Override
    public void update(MediaFrame frame, boolean skipFlush) {
        if (frame.getPixelFormat() == PixelFormat.MULTI_YCbCr_420) {
            throw new IllegalArgumentException("Unsupported format " + frame.getPixelFormat());
        }

        try (D3D12Utils.AutoReleasableMediaFrame mf =
                new D3D12Utils.AutoReleasableMediaFrame(frame)) {
            ByteBuffer pixels = mf.get().getBufferForPlane(0);

            if (!skipFlush) {
                mContext.flushVertexBuffer();
            }

            PixelFormat format = mf.get().getPixelFormat();
            boolean res;

            if (format.getDataType() == PixelFormat.DataType.INT) {
                res = mContext.getDevice().updateTexture(getNativeTexture(),
                                                        pixels.asIntBuffer(), format,
                                                        0, 0, 0, 0,
                                                        mf.get().getEncodedWidth(), mf.get().getEncodedHeight(),
                                                        mf.get().strideForPlane(0));
            } else {
                res = mContext.getDevice().updateTexture(getNativeTexture(),
                                                        pixels, format,
                                                        0, 0, 0, 0,
                                                        mf.get().getEncodedWidth(), mf.get().getEncodedHeight(),
                                                        mf.get().strideForPlane(0));
            }

            if (!res) {
                new Exception(String.format("D3D12: Texture update failed. Stack trace:")).printStackTrace(System.err);
            }
        }
    }

    @Override
    protected Texture createSharedTexture(WrapMode newMode) {
        return new D3D12Texture(this, newMode);
    }

}
