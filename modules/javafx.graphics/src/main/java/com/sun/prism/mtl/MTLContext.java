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
import com.sun.javafx.geom.transform.BaseTransform;
import com.sun.javafx.geom.transform.GeneralTransform3D;
import com.sun.javafx.sg.prism.NGCamera;
import com.sun.javafx.sg.prism.NGDefaultCamera;
import com.sun.prism.*;
import com.sun.prism.impl.PrismSettings;
import com.sun.prism.impl.ps.BaseShaderContext;
import com.sun.prism.ps.Shader;
import java.io.File;
import java.net.URI;

public class MTLContext extends BaseShaderContext {

    public static final int NUM_QUADS = PrismSettings.superShader ? 4096 : 256;

    public static final int MTL_COMPMODE_CLEAR           = 0;
    public static final int MTL_COMPMODE_SRC             = 1;
    public static final int MTL_COMPMODE_SRCOVER         = 2;
    public static final int MTL_COMPMODE_DSTOUT          = 3;
    public static final int MTL_COMPMODE_ADD             = 4;


    private final long pContext;
    private MTLRTTexture renderTarget;
    private MTLResourceFactory resourceFactory;
    private MTLPipeline pipeline;
    private MTLPipelineManager encoderManager;

    private int targetWidth;
    private int targetHeight;

    private GeneralTransform3D scratchTx = new GeneralTransform3D(); // Column major matrix
    private GeneralTransform3D projViewTx = new GeneralTransform3D(); // Column major matrix

    private static final String shaderLibPath;
    /*
     * Locate the shader library on disk
     *
     * TODO: MTL: The loading of jfxshaders.metallib needs an adjustment
     * going forward:
     *
     *    The existing code won't locate the shader library file in all
     *    cases. There are two approaches we can take:
     *
     *    A. Treat it like a library (i.e., like a dylib). This would
     *       require a small refactoring and a new method in
     *       NativeLibLoader to return the path to a file on disk (it
     *       has all the pieces needed to do that, but the only method it
     *       provides is one that calls System.load or System.loadLibrary
     *       after locating it).
     *
     *    B. Treat it like a resource in the jar file in the same
     *       manner that .frag files for GLSL shaders are done. We would
     *       need to create a temporary file and write the resource to
     *       that file every time we start up (presuming there is no way
     *       to pass a memory copy of the metallib file to the Metal APIs,
     *       which I don't think there is).
     *
     *    Option A is probably best, since it will be faster on startup
     *    and takes advantage of existing code.
     */
    static {
        final String shaderLibName = "jfxshaders.metallib";

        // Load the native library from the same directory as the jar file
        // containing this class. This currently only works when running
        // from the SDK (see "TODO" note above).

        try {
            // Get the URL for this class, if it is a jar URL, then get
            // the filename associated with it.
            String theClassFile = "MTLContext.class";
            Class theClass = MTLContext.class;
            String classUrlString = theClass.getResource(theClassFile).toString();
            if (!classUrlString.startsWith("jar:file:") || classUrlString.indexOf('!') == -1) {
                throw new UnsatisfiedLinkError("Invalid URL for class: " + classUrlString);
            }
            // Strip out the "jar:" and everything after and including the "!"
            String tmpStr = classUrlString.substring(4, classUrlString.lastIndexOf('!'));
            // Strip everything after the last "/" to get rid of the jar filename
            int lastIndexOfSlash = tmpStr.lastIndexOf('/');

            // Location of native libraries relative to jar file
            String libDirUrlString = tmpStr.substring(0, lastIndexOfSlash);
            File libDir = new File(new URI(libDirUrlString).getPath());

            File libFile = new File(libDir, shaderLibName);
            if (!libFile.canRead()) {
                throw new UnsatisfiedLinkError("Cannot load: " + libFile);
            }
            shaderLibPath = libFile.getCanonicalPath();
        } catch (Exception ex) {
            throw new RuntimeException(ex);
        }
    }

    public void setRenderTargetTexture(MTLRTTexture rtt) {
        renderTarget = rtt;
    }

    public MTLRTTexture getRenderTargetTexture() {
        return renderTarget;
    }

    MTLContext(Screen screen, MTLResourceFactory factory) {
        super(screen, factory, NUM_QUADS);
        resourceFactory = factory;
        pContext = nInitialize(shaderLibPath);
    }

    public MTLResourceFactory getResourceFactory() {
        return resourceFactory;
    }

    long getContextHandle() {
        return pContext;
    }

    @Override
    protected State updateRenderTarget(RenderTarget target, NGCamera camera, boolean depthTest) {
        System.err.println("MTLContext.updateRenderTarget() :target = " + target + ", camera = " + camera + ", depthTest = " + depthTest);
        System.err.println("MTLContext.updateRenderTarget() projViewTx:1:-->\n" + projViewTx);
        renderTarget = (MTLRTTexture)target;
        nUpdateRenderTarget(pContext, renderTarget.getNativeHandle());

        targetWidth = target.getPhysicalWidth();
        targetHeight = target.getPhysicalHeight();

        // Need to validate the camera before getting its computed data.
        if (camera instanceof NGDefaultCamera) {
            ((NGDefaultCamera) camera).validate(targetWidth, targetHeight);
            projViewTx = camera.getProjViewTx(projViewTx);
            System.err.println("MTLContext.updateRenderTarget() projViewTx:2:-->\n" + projViewTx);
        } else {
            projViewTx = camera.getProjViewTx(projViewTx);
            double vw = camera.getViewWidth();
            double vh = camera.getViewHeight();
            if (targetWidth != vw || targetHeight != vh) {
                projViewTx.scale(vw / targetWidth, vh / targetHeight, 1.0);
            }
        }

        // ------------------------------------------------------------------------------------------
        // TODO: MTL: This scale transformation is to accomodate HiDPi scale. It is a temporary hack to match screen scaling.
        // This hack shoule be removed once the MTLSwapChain is properly implemented to work with PresentingPainter
        // ------------------------------------------------------------------------------------------
        projViewTx.scale(getAssociatedScreen().getRecommendedOutputScaleX(),
                         getAssociatedScreen().getRecommendedOutputScaleY(), 1.0);

        System.err.println("MTLContext.updateRenderTarget() projViewTx:3:-->\n" + projViewTx);

        // Set projection view matrix
        nSetProjViewMatrix(pContext, depthTest,
            projViewTx.get(0),  projViewTx.get(1),  projViewTx.get(2),  projViewTx.get(3),
            projViewTx.get(4),  projViewTx.get(5),  projViewTx.get(6),  projViewTx.get(7),
            projViewTx.get(8),  projViewTx.get(9),  projViewTx.get(10), projViewTx.get(11),
            projViewTx.get(12), projViewTx.get(13), projViewTx.get(14), projViewTx.get(15));

        // cameraPos = camera.getPositionInWorld(cameraPos);

        return new State();
    }

    @Override
    protected void updateTexture(int texUnit, Texture tex) {
        new Exception().printStackTrace();
        System.err.println("MTLContext.updateTexture() :texUnit = " + texUnit + ", tex = " + tex);
        MTLTexture tex0 = (MTLTexture)tex;
        MTLShader.setTexture(texUnit, tex);
        nSetTex0(pContext, tex0.getNativeHandle());
    }

    @Override
    protected void updateShaderTransform(Shader shader, BaseTransform xform) {
        System.err.println("MTLContext.updateShaderTransform() :shader = " + shader + ", xform = " + xform);
        if (xform == null) {
            xform = BaseTransform.IDENTITY_TRANSFORM;
        }

        scratchTx.set(projViewTx);
        final GeneralTransform3D perspectiveTransform = getPerspectiveTransformNoClone();
        if (perspectiveTransform.isIdentity()) {
            scratchTx = scratchTx.mul(xform);
        } else {
            scratchTx = scratchTx.mul(xform).mul(perspectiveTransform);
        }
        nSetProjViewMatrix(pContext, true,
            scratchTx.get(0),  scratchTx.get(1),  scratchTx.get(2),  scratchTx.get(3),
            scratchTx.get(4),  scratchTx.get(5),  scratchTx.get(6),  scratchTx.get(7),
            scratchTx.get(8),  scratchTx.get(9),  scratchTx.get(10), scratchTx.get(11),
            scratchTx.get(12), scratchTx.get(13), scratchTx.get(14), scratchTx.get(15));
    }

    @Override
    protected void updateWorldTransform(BaseTransform xform) {
        System.err.println("MTLContext.updateWorldTransform() :xform = " + xform);
    }

    @Override
    protected void updateClipRect(Rectangle clipRect) {
        System.err.println("MTLContext.updateClipRect() :clipRect = " + clipRect);
    }

    @Override
    protected void updateCompositeMode(CompositeMode mode) {
        System.err.println("MTLContext.updateCompositeMode() :mode = " + mode);

        int mtlCompMode;
        switch (mode) {
            case CLEAR:
                mtlCompMode = MTL_COMPMODE_CLEAR;
                break;

            case SRC:
                mtlCompMode = MTL_COMPMODE_SRC;
                break;

            case SRC_OVER:
                mtlCompMode = MTL_COMPMODE_SRCOVER;
                break;

            case DST_OUT:
                mtlCompMode = MTL_COMPMODE_DSTOUT;
                break;

            case ADD:
                mtlCompMode = MTL_COMPMODE_ADD;
                break;

            default:
                throw new InternalError("Unrecognized composite mode: " + mode);
        }
        nSetCompositeMode(getContextHandle(), mtlCompMode);
    }

    @Override
    public void blit(RTTexture srcRTT, RTTexture dstRTT, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1) {
        System.err.println("MTLContext.blit() :srcRTT = " + srcRTT + ", dstRTT = " + dstRTT + ", srcX0 = " +
                srcX0 + ", srcY0 = " + srcY0 + ", srcX1 = " + srcX1 + ", srcY1 = " + srcY1 + ", dstX0 = " +
                dstX0 + ", dstY0 = " + dstY0 + ", dstX1 = " + dstX1 + ", dstY1 = " + dstY1);
    }

    @Override
    protected void renderQuads(float[] coordArray, byte[] colorArray, int numVertices) {
        System.err.println("\n\nnumVertices = " + numVertices);
        System.err.println("coordArray : size = "+coordArray.length);
        for (int i = 0; i < numVertices * 7; i += 7) {
            System.err.println(
                    "xyz: x: " + coordArray[i] + ", y: " + coordArray[i + 1] + ", z: " + coordArray[i + 2]
                    + ",  uv1: u: " + coordArray[i + 3] + ", v: " + coordArray[i + 4]
                    + ",  uv2: u: " + coordArray[i + 5] + ", v: " + coordArray[i + 6]);
        }
        System.err.println("\ncolorArray : size = "+ colorArray.length);
        for (int i = 0; i < numVertices * 4; i += 4) {
            int r = colorArray[i] & 0xFF;
            int g = colorArray[i + 1] & 0xFF;
            int b = colorArray[i + 2] & 0xFF;
            int a = colorArray[i + 3] & 0xFF;
            System.err.println(r + ", " + g + ", " + b + ", " + a);
        }

        nDrawIndexedQuads(getContextHandle(), coordArray, colorArray, numVertices);
    }

    native private static long nInitialize(String shaderLibPathStr);
    native private static int nDrawIndexedQuads(long context, float coords[], byte volors[], int numVertices);
    native private static void nUpdateRenderTarget(long context, long texPtr);
    native private static int nResetTransform(long context);
    native private static void nSetTex0(long context, long texPtr);
    private static native int nSetProjViewMatrix(long pContext, boolean isOrtho,
        double m00, double m01, double m02, double m03,
        double m10, double m11, double m12, double m13,
        double m20, double m21, double m22, double m23,
        double m30, double m31, double m32, double m33);
    native private static void nSetCompositeMode(long context, int mode);
}
