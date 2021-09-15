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

import com.sun.glass.ui.Screen;
import com.sun.javafx.geom.Rectangle;
import com.sun.prism.Graphics;
import com.sun.prism.Presentable;
import com.sun.prism.PresentableState;

/**
 * // TODO: MTL:
 * MTLSwapChain is unused as of now and implementation is not complete.
 * The implementation should be completed after we make relevant changes
 * in PresentingPainter and on Glass Side.
 */
public class MTLSwapChain extends MTLResource
        implements MTLRenderTarget, Presentable {
    private MTLRTTexture backBuffer;
    private final float pixelScaleFactorX;
    private final float pixelScaleFactorY;


    private MTLSwapChain() {
        super(null);
        pixelScaleFactorX = 1;
        pixelScaleFactorY = 1;
    }

    public MTLSwapChain(MTLContext context, long pResource, MTLRTTexture rtt,
                 float pixelScaleX, float pixelScaleY) {
        super(new MTLRecord(context, pResource));
        backBuffer = rtt;
        pixelScaleFactorX = pixelScaleX;
        pixelScaleFactorY = pixelScaleY;
    }

    @Override
    public boolean lockResources(PresentableState pState) {
        return false;
    }

    @Override
    public boolean prepare(Rectangle dirtyregion) {
        MTLContext context = getContext();
        context.flushVertexBuffer();
        return true;
    }

    public MTLContext getContext() {
        return mtlResRecord.getContext();
    }

    @Override
    public boolean present() {
        return false;
    }

    @Override
    public float getPixelScaleFactorX() {
        return 0;
    }

    @Override
    public float getPixelScaleFactorY() {
        return 0;
    }

    @Override
    public Screen getAssociatedScreen() {
        return null;
    }

    @Override
    public Graphics createGraphics() {
        return null;
    }

    @Override
    public boolean isOpaque() {
        return false;
    }

    @Override
    public void setOpaque(boolean opaque) {

    }

    @Override
    public boolean isMSAA() {
        return false;
    }

    @Override
    public int getPhysicalWidth() {
        return 0;
    }

    @Override
    public int getPhysicalHeight() {
        return 0;
    }

    @Override
    public int getContentX() {
        return 0;
    }

    @Override
    public int getContentY() {
        return 0;
    }

    @Override
    public int getContentWidth() {
        return 0;
    }

    @Override
    public int getContentHeight() {
        return 0;
    }

    @Override
    public long getResourceHandle() {
        return 0;
    }
}
