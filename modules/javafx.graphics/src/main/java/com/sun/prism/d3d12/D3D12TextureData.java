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

import com.sun.prism.PixelFormat;
import com.sun.prism.impl.Disposer;
import com.sun.prism.impl.PrismTrace;
import com.sun.prism.d3d12.ni.D3D12NativeRenderTarget;
import com.sun.prism.d3d12.ni.D3D12NativeTexture;

class D3D12TextureData implements Disposer.Record {

    private D3D12NativeTexture mTexture;
    private D3D12NativeRenderTarget mRenderTarget;
    private final long mSize;

    static long estimateSize(int physicalWidth, int physicalHeight, PixelFormat format)
    {
        return (long) physicalWidth * physicalHeight * format.getBytesPerPixelUnit();
    }

    static long estimateRTSize(int physicalWidth, int physicalHeight, boolean hasDepth)
    {
        return 4L * physicalWidth * physicalHeight;
    }

    D3D12TextureData(D3D12NativeTexture tex, D3D12NativeRenderTarget rt, int width, int height, PixelFormat format) {
        mTexture = tex;
        mRenderTarget = rt;

        mSize = isRTT()
            ? estimateRTSize(width, height, false)
            : estimateSize(width, height, format);

        if (isRTT()) {
            PrismTrace.rttCreated(mRenderTarget.getPtr(), width, height, mSize);
        } else {
            PrismTrace.textureCreated(mTexture.getPtr(), width, height, mSize);
        }
    }

    @Override
    public void dispose() {
        if (isRTT()) {
            PrismTrace.rttDisposed(mRenderTarget.getPtr());
            mRenderTarget.close();
            mTexture.close();
        } else {
            PrismTrace.textureDisposed(mTexture.getPtr());
            mTexture.close();
        }
    }

    public boolean isValid() {
        if (isRTT()) {
            return mRenderTarget.isValid() && mTexture.isValid();
        } else {
            return mTexture.isValid();
        }
    }

    public final boolean isRTT() {
        return mRenderTarget != null;
    }

    public long getSize() {
        if (isRTT()) {
            // TODO: D3D12: include bpp calculations
            return mRenderTarget.getHeight() * mRenderTarget.getHeight() * 4;
        } else {
            return mTexture.getSize();
        }
    }

    D3D12NativeTexture getNativeTexture() {
        return mTexture;
    }

    D3D12NativeRenderTarget getRenderTarget() {
        return mRenderTarget;
    }
}
