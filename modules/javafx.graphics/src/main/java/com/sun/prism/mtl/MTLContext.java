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
import com.sun.javafx.geom.Vec3d;
import com.sun.javafx.geom.transform.Affine3D;
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

    private Vec3d cameraPos = new Vec3d();
    private static float rawMatrix[] = new float[16];
    private GeneralTransform3D worldTx = new GeneralTransform3D();
    private static final Affine3D scratchAffine3DTx = new Affine3D();
    private GeneralTransform3D scratchTx = new GeneralTransform3D(); // Column major matrix
    private GeneralTransform3D projViewTx = new GeneralTransform3D(); // Column major matrix

    private static double[] tempAdjustClipSpaceMat = new double[16];

    private static final String shaderLibPath;
    public final static int CULL_BACK                  = 110;
    public final static int CULL_FRONT                 = 111;
    public final static int CULL_NONE                  = 112;
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

    private GeneralTransform3D adjustClipSpace(GeneralTransform3D projViewTx) {
        double[] m = projViewTx.get(tempAdjustClipSpaceMat);
        m[8] = (m[8] + m[12])/2;
        m[9] = (m[9] + m[13])/2;
        m[10] = (m[10] + m[14])/2;
        m[11] = (m[11] + m[15])/2;
        projViewTx.set(m);
        return projViewTx;
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
            // TODO: MTL: Check whether we need to adjust clip space
            projViewTx = camera.getProjViewTx(projViewTx);
            System.err.println("MTLContext.updateRenderTarget() projViewTx:2:-->\n" + projViewTx);
        } else {
            projViewTx = adjustClipSpace(camera.getProjViewTx(projViewTx));
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

        cameraPos = camera.getPositionInWorld(cameraPos);
        return new State();
    }

    @Override
    protected void updateTexture(int texUnit, Texture tex) {
        System.err.println("MTLContext.updateTexture() :texUnit = " + texUnit + ", tex = " + tex);
        MTLTexture tex0 = (MTLTexture)tex;
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
        worldTx.setIdentity();
        if ((xform != null) && (!xform.isIdentity())) {
            worldTx.mul(xform);
        }
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

    private static native void nSetWorldTransformToIdentity(long pContext);
    private static native void nSetWorldTransform(long pContext,
                                                  double m00, double m01, double m02, double m03,
                                                  double m10, double m11, double m12, double m13,
                                                  double m20, double m21, double m22, double m23,
                                                  double m30, double m31, double m32, double m33);
    private static native int nSetDeviceParametersFor3D(long pContext);
    private static native int nSetCameraPosition(long pContext, double x, double y, double z);
    private static native long nCreateMTLMesh(long pContext);
    private static native void nReleaseMTLMesh(long pContext, long nativeHandle);
    private static native boolean nBuildNativeGeometryShort(long pContext, long nativeHandle,
                                                            float[] vertexBuffer, int vertexBufferLength, short[] indexBuffer, int indexBufferLength);
    private static native boolean nBuildNativeGeometryInt(long pContext, long nativeHandle,
                                                          float[] vertexBuffer, int vertexBufferLength, int[] indexBuffer, int indexBufferLength);
    private static native long nCreateMTLPhongMaterial(long pContext);
    private static native void nReleaseMTLPhongMaterial(long pContext, long nativeHandle);
    private static native void nSetDiffuseColor(long pContext, long nativePhongMaterial,
                                                float r, float g, float b, float a);
    private static native void nSetSpecularColor(long pContext, long nativePhongMaterial,
                                                 boolean set, float r, float g, float b, float a);
    private static native void nSetMap(long pContext, long nativePhongMaterial,
                                       int mapType, long texID);
    private static native long nCreateMTLMeshView(long pContext, long nativeMesh);
    private static native void nReleaseMTLMeshView(long pContext, long nativeHandle);
    private static native void nSetCullingMode(long pContext, long nativeMeshView,
                                               int cullingMode);
    private static native void nSetMaterial(long pContext, long nativeMeshView,
                                            long nativePhongMaterialInfo);
    private static native void nSetWireframe(long pContext, long nativeMeshView,
                                             boolean wireframe);
    private static native void nSetAmbientLight(long pContext, long nativeMeshView,
                                                float r, float g, float b);
    private static native void nSetLight(long pContext, long nativeMeshView,
                                         int index, float x, float y, float z, float r, float g, float b, float w, float ca, float la, float qa,
                                         float isAttenuated, float maxRange, float dirX, float dirY, float dirZ, float innerAngle, float outerAngle,
                                         float falloff);
    private static native void nRenderMeshView(long pContext, long nativeMeshView);

    @Override
    protected void setDeviceParametersFor3D() {
        if (checkDisposed()) return;
        System.err.println("3D : MTLContext:setDeviceParametersFor3D()");
        nSetDeviceParametersFor3D(pContext);
    }

    long createMTLMesh() {
        if (checkDisposed()) return 0;
        System.err.println("3D : MTLContext:createMTLMesh()");
        return nCreateMTLMesh(pContext);
    }

    void releaseMTLMesh(long nativeHandle) {
        System.err.println("3D : MTLContext:releaseMTLMesh()");
        nReleaseMTLMesh(pContext, nativeHandle);
    }

    boolean buildNativeGeometry(long nativeHandle, float[] vertexBuffer, int vertexBufferLength,
                                short[] indexBuffer, int indexBufferLength) {
        System.err.println("3D : MTLContext:buildNativeGeometryShort()");
        System.err.println("VertexBuffer");
        int i = 0;
        int index = 0;
        while (i < vertexBuffer.length) {
            System.err.println("Index " + index + " : " + vertexBuffer[i]
                + " " + vertexBuffer[i + 1] + " " + vertexBuffer[i + 2]);
            i = i + 9;
            index++;
        }
        i = 0;
        index = 0;
        System.err.println("IndexBuffer");
        while (i < indexBuffer.length) {
            System.err.println("Triangle " + index + " : " + indexBuffer[i]
                + " " + indexBuffer[i + 1] + " " + indexBuffer[i + 2]);
            i = i + 3;
            index++;
        }
        return nBuildNativeGeometryShort(pContext, nativeHandle, vertexBuffer,
            vertexBufferLength, indexBuffer, indexBufferLength);
    }

    boolean buildNativeGeometry(long nativeHandle, float[] vertexBuffer, int vertexBufferLength,
                                int[] indexBuffer, int indexBufferLength) {
        // TODO: MTL: Complete the implementation
        System.err.println("3D : MTLContext:buildNativeGeometryInt()");
        return false;
        //return nBuildNativeGeometryInt(pContext, nativeHandle, vertexBuffer,
            //vertexBufferLength, indexBuffer, indexBufferLength);
    }

    long createMTLPhongMaterial() {
        System.err.println("3D : MTLContext:createMTLPhongMaterial()");
        return nCreateMTLPhongMaterial(pContext);
    }

    void releaseMTLPhongMaterial(long nativeHandle) {
        // TODO: MTL: Complete the implementation
        System.err.println("3D : MTLContext:createMTLPhongMaterial()");
        //nReleaseMTLPhongMaterial(pContext, nativeHandle);
    }

    void setDiffuseColor(long nativePhongMaterial, float r, float g, float b, float a) {
        System.err.println("3D : MTLContext:setDiffuseColor()");
        nSetDiffuseColor(pContext, nativePhongMaterial, r, g, b, a);
    }

    void setSpecularColor(long nativePhongMaterial, boolean set, float r, float g, float b, float a) {
        System.err.println("3D : MTLContext:setSpecularColor()");
        nSetSpecularColor(pContext, nativePhongMaterial, set, r, g, b, a);
    }

    void setMap(long nativePhongMaterial, int mapType, long nativeTexture) {
        // TODO: MTL: Complete the implementation
        System.err.println("3D : MTLContext:setMap()");
        //nSetMap(pContext, nativePhongMaterial, mapType, nativeTexture);
    }

    long createMTLMeshView(long nativeMesh) {
        System.err.println("3D : MTLContext:createMTLMeshView()");
        return nCreateMTLMeshView(pContext, nativeMesh);
    }

    void releaseMTLMeshView(long nativeMeshView) {
        // TODO: MTL: Complete the implementation
        System.err.println("3D : MTLContext:releaseMTLMeshView()");
        //nReleaseMTLMeshView(pContext, nativeMeshView);
    }

    void setCullingMode(long nativeMeshView, int cullMode) {
        System.err.println("3D : MTLContext:setCullingMode()");
        int cm;
        if (cullMode == MeshView.CULL_NONE) {
            cm = CULL_NONE;
        } else if (cullMode == MeshView.CULL_BACK) {
            cm = CULL_BACK;
        } else if (cullMode == MeshView.CULL_FRONT) {
            cm = CULL_FRONT;
        } else {
            throw new IllegalArgumentException("illegal value for CullMode: " + cullMode);
        }
        nSetCullingMode(pContext, nativeMeshView, cm);
    }

    void setMaterial(long nativeMeshView, long nativePhongMaterial) {
        System.err.println("3D : MTLContext:setMaterial()");
        nSetMaterial(pContext, nativeMeshView, nativePhongMaterial);
    }

    void setWireframe(long nativeMeshView, boolean wireframe) {
        System.err.println("3D : MTLContext:setWireframe()");
        nSetWireframe(pContext, nativeMeshView, wireframe);
    }

    void setAmbientLight(long nativeMeshView, float r, float g, float b) {
        System.err.println("3D : MTLContext:setAmbientLight()");
        nSetAmbientLight(pContext, nativeMeshView, r, g, b);
    }

    void setLight(long nativeMeshView, int index, float x, float y, float z, float r, float g, float b, float w,
                  float ca, float la, float qa, float isAttenuated, float maxRange, float dirX, float dirY, float dirZ,
                  float innerAngle, float outerAngle, float falloff) {
        System.err.println("3D : MTLContext:setLight()");
        nSetLight(pContext, nativeMeshView, index, x, y, z, r, g, b, w,  ca, la, qa, isAttenuated, maxRange,
            dirX, dirY, dirZ, innerAngle, outerAngle, falloff);
    }

    void renderMeshView(long nativeMeshView, Graphics g) {
        System.err.println("3D : MTLContext:renderMeshView()");
        // Support retina display by scaling the projViewTx and pass it to the shader.
        float pixelScaleFactorX = g.getPixelScaleFactorX();
        float pixelScaleFactorY = g.getPixelScaleFactorY();
        if (pixelScaleFactorX != 1.0 || pixelScaleFactorY != 1.0) {
            scratchTx = scratchTx.set(projViewTx);
            scratchTx.scale(pixelScaleFactorX, pixelScaleFactorY, 1.0);
            updateRawMatrix(scratchTx);
        } else {
            updateRawMatrix(projViewTx);
        }
        printRawMatrix("Projection");
        // Set projection view matrix
        int res = nSetProjViewMatrix(pContext, g.isDepthTest(),
            rawMatrix[0], rawMatrix[1], rawMatrix[2], rawMatrix[3],
            rawMatrix[4], rawMatrix[5], rawMatrix[6], rawMatrix[7],
            rawMatrix[8], rawMatrix[9], rawMatrix[10], rawMatrix[11],
            rawMatrix[12], rawMatrix[13], rawMatrix[14], rawMatrix[15]);

        // TODO: MTL: Implement eye position
        //res = nSetCameraPosition(pContext, cameraPos.x, cameraPos.y, cameraPos.z);

        // Undo the SwapChain scaling done in createGraphics() because 3D needs
        // this information in the shader (via projViewTx)
        BaseTransform xform = g.getTransformNoClone();
        if (pixelScaleFactorX != 1.0 || pixelScaleFactorY != 1.0) {
            scratchAffine3DTx.setToIdentity();
            scratchAffine3DTx.scale(1.0 / pixelScaleFactorX, 1.0 / pixelScaleFactorY);
            scratchAffine3DTx.concatenate(xform);
            updateWorldTransform(scratchAffine3DTx);
        } else {
            updateWorldTransform(xform);
        }

        updateRawMatrix(worldTx);
        printRawMatrix("World");
        nSetWorldTransform(pContext,
            rawMatrix[0], rawMatrix[1], rawMatrix[2], rawMatrix[3],
            rawMatrix[4], rawMatrix[5], rawMatrix[6], rawMatrix[7],
            rawMatrix[8], rawMatrix[9], rawMatrix[10], rawMatrix[11],
            rawMatrix[12], rawMatrix[13], rawMatrix[14], rawMatrix[15]);
        nRenderMeshView(pContext, nativeMeshView);
    }

    void printRawMatrix(String mesg) {
        System.err.println(mesg + " = ");
        for (int i = 0; i < 4; i++) {
            System.err.println(rawMatrix[i] + ", " + rawMatrix[i+4]
                + ", " + rawMatrix[i+8] + ", " + rawMatrix[i+12]);
        }
    }
    private void updateRawMatrix(GeneralTransform3D src) {
        rawMatrix[0]  = (float)src.get(0); // Scale X
        rawMatrix[1]  = (float)src.get(4); // Shear Y
        rawMatrix[2]  = (float)src.get(8);
        rawMatrix[3]  = (float)src.get(12);
        rawMatrix[4]  = (float)src.get(1); // Shear X
        rawMatrix[5]  = (float)src.get(5); // Scale Y
        rawMatrix[6]  = (float)src.get(9);
        rawMatrix[7]  = (float)src.get(13);
        rawMatrix[8]  = (float)src.get(2);
        rawMatrix[9]  = (float)src.get(6);
        rawMatrix[10] = (float)src.get(10);
        rawMatrix[11] = (float)src.get(14);
        rawMatrix[12] = (float)src.get(3);  // Translate X
        rawMatrix[13] = (float)src.get(7);  // Translate Y
        rawMatrix[14] = (float)src.get(11);
        rawMatrix[15] = (float)src.get(15);
    }
}
