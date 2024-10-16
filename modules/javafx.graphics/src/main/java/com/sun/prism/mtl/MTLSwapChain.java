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

package com.sun.prism.mtl;

import com.sun.glass.ui.Screen;
import com.sun.javafx.geom.Rectangle;
import com.sun.prism.Graphics;
import com.sun.prism.Presentable;
import com.sun.prism.PresentableState;
import com.sun.prism.CompositeMode;
import com.sun.prism.impl.PrismSettings;


public class MTLSwapChain extends MTLResource
        implements MTLRenderTarget, Presentable {

    private PresentableState pState;
    private MTLRTTexture stableBackbuffer;
    private final float pixelScaleFactorX;
    private final float pixelScaleFactorY;
    private boolean needsResize;
    private int w, h;

    public MTLSwapChain(MTLContext context, PresentableState state) {
        super(new MTLRecord(context, 0l));
        pState = state;
        pixelScaleFactorX = state.getRenderScaleX();
        pixelScaleFactorY = state.getRenderScaleY();

        w = state.getRenderWidth();
        h = state.getRenderHeight();

        // System.err.println("MTLSwapChain - constructor()");
    }

    @Override
    public boolean lockResources(PresentableState state) {

        if (this.pState != state ||
            pixelScaleFactorX != state.getRenderScaleX() ||
            pixelScaleFactorY != state.getRenderScaleY())
        {
            return true;
        }
        needsResize = (w != state.getRenderWidth() || h != state.getRenderHeight());

        // the stableBackbuffer will be used as the render target
        if (stableBackbuffer != null && !needsResize) {
            stableBackbuffer.lock();
            if (stableBackbuffer.isSurfaceLost()) {
                stableBackbuffer = null;
                // For resizes we can keep the back buffer, but if we lose
                // the back buffer then we need the caller to know that a
                // new buffer is coming so that the entire scene can be
                // redrawn.  To force this, we return true and the Presentable
                // is recreated and repainted in its entirety.
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean prepare(Rectangle dirtyregion) {

        MTLContext context = getContext();
        context.flushVertexBuffer();
        MTLGraphics g = (MTLGraphics) MTLGraphics.create(context, stableBackbuffer);
        if (g == null) {
            return false;
        }
        /*int sw = stableBackbuffer.getContentWidth();
        int sh = stableBackbuffer.getContentHeight();
        int dw = this.getContentWidth();
        int dh = this.getContentHeight();
        if (isMSAA()) {
            context.flushVertexBuffer();
            g.blit(stableBackbuffer, null, 0, 0, sw, sh, 0, 0, dw, dh);
        } else {
            g.setCompositeMode(CompositeMode.SRC);
            g.drawTexture(stableBackbuffer, 0, 0, dw, dh, 0, 0, sw, sh);
        }
        context.flushVertexBuffer();*/
        stableBackbuffer.unlock();
        return true;
    }

    public MTLContext getContext() {
        return mtlResRecord.getContext();
    }

    @Override
    public boolean present() {

        MTLContext context = getContext();
        if (context.isDisposed()) {
            return false;
        }

        context.commitCurrentCommandBuffer();

        return true;
    }

    @Override
    public float getPixelScaleFactorX() {
        return pixelScaleFactorX;
    }

    @Override
    public float getPixelScaleFactorY() {
        return pixelScaleFactorY;
    }

    @Override
    public Screen getAssociatedScreen() {
        return null;
    }

    @Override
    public Graphics createGraphics() {

        if (pState.getNativeFrameBuffer() == 0) {
            //TODO: MTL : handle error gracefully
            //System.err.println("Native backbuffer texture from Glass is nil.");
            return null;
        }

        needsResize = (w != pState.getRenderWidth() || h != pState.getRenderHeight());
        // the stableBackbuffer will be used as the render target
        if (stableBackbuffer == null || needsResize) {
            // note that we will take care of calling
            // forceRenderTarget() for the hardware backbuffer and
            // reset the needsResize flag at present() time...
            if (stableBackbuffer != null) {
                stableBackbuffer.dispose();
                stableBackbuffer = null;
            } /*else {
                // RT-27554
                // TODO: this implementation was done to make sure there is a
                // context current for the hardware backbuffer before we start
                // attempting to use the FBO associated with the
                // RTTexture "backbuffer"...
                ES2Graphics.create(context, this);
            }*/
            w = pState.getRenderWidth();
            h = pState.getRenderHeight();

            long pTex = pState.getNativeFrameBuffer();

            MTLVramPool pool = MTLVramPool.getInstance();
            long size = pool.estimateRTTextureSize(w, h, false);
            if (!pool.prepareForAllocation(size)) {
                return null;
            }
            stableBackbuffer = (MTLRTTexture)MTLRTTexture.create(getContext(), pTex, w, h, size);
            if (PrismSettings.dirtyOptsEnabled) {
                stableBackbuffer.contentsUseful();
            }
            //copyFullBuffer = true;
        }

        Graphics g = MTLGraphics.create(getContext(), stableBackbuffer);
        g.scale(pixelScaleFactorX, pixelScaleFactorY);
        return g;
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
        return pState.getOutputWidth();
    }

    @Override
    public int getPhysicalHeight() {
        return pState.getOutputHeight();
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
        return pState.getOutputWidth();
    }

    @Override
    public int getContentHeight() {
        return pState.getOutputHeight();
    }

    @Override
    public long getResourceHandle() {
        return 0;
    }

    @Override
    public void dispose() {
        if (stableBackbuffer != null) {
            stableBackbuffer.dispose();
            stableBackbuffer = null;
        }
    }
}
