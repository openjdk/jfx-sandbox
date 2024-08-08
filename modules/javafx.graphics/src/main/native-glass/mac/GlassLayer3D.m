/*
 * Copyright (c) 2012, 2021, Oracle and/or its affiliates. All rights reserved.
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

#import "GlassLayer3D.h"

#import "GlassMacros.h"
#import "GlassScreen.h"

//#define VERBOSE
#ifndef VERBOSE
    #define LOG(MSG, ...)
#else
    #define LOG(MSG, ...) GLASS_LOG(MSG, ## __VA_ARGS__);
#endif

@implementation GlassLayer3D

static NSArray *allModes = nil;

- (id) init:(long)mtlCommandQueuePtr
{
    self = [super init];
    isHiDPIAware = true; // TODO : pass in this from view

    [self setAutoresizingMask:(kCALayerWidthSizable|kCALayerHeightSizable)];
    [self setContentsGravity:kCAGravityTopLeft];

    // Initially the view is not in any window yet, so using the
    // screens[0]'s scale is a good starting point (this is most probably
    // the notebook's main LCD display which is HiDPI-capable).
    // Note that mainScreen is the screen with the current app bar focus
    // in Mavericks and later OS so it will likely not match the screen
    // we initially show windows on if an app is started from an external
    // monitor.
    [self notifyScaleFactorChanged:GetScreenScaleFactor([[NSScreen screens] objectAtIndex:0])];

    [self setMasksToBounds:YES];
    [self setNeedsDisplayOnBoundsChange:YES];
    [self setAnchorPoint:CGPointMake(0.0f, 0.0f)];

    self.device = MTLCreateSystemDefaultDevice();
    self->_blitCommandQueue = (id<MTLCommandQueue>)(jlong_to_ptr(mtlCommandQueuePtr));
    // self->_blitCommandQueue = [self.device newCommandQueue];

    self.pixelFormat = MTLPixelFormatBGRA8Unorm;
    self.framebufferOnly = NO;
    self.displaySyncEnabled = NO; // to support FPS faster than 60fps (-Djavafx.animation.fullspeed=true)
    self.opaque = NO; //to support shaped window


    self->_painterOffscreen = [[GlassOffscreen alloc] initWithContext:nil andIsSwPipe:true];
    [self->_painterOffscreen setLayer:self];

    if (allModes == nil) {
        allModes = [[NSArray arrayWithObjects:NSDefaultRunLoopMode,
                                              NSEventTrackingRunLoopMode,
                                              NSModalPanelRunLoopMode, nil] retain];
    }

    self.colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);

    return self;
}

- (void)dealloc
{
    [self->_painterOffscreen release];
    self->_painterOffscreen = nil;

    [super dealloc];
}


- (void)notifyScaleFactorChanged:(CGFloat)scale
{
    if (self->isHiDPIAware) {
        if ([self respondsToSelector:@selector(setContentsScale:)]) {
            [self setContentsScale: scale];
        }
    }
}

/*
//- (void)setBounds:(CGRect)bounds
//{
//    LOG("GlassLayer3D setBounds:%s", [NSStringFromRect(NSRectFromCGRect(bounds)) UTF8String]);
//    [super setBounds:bounds];
//}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    return [self->_glassOffscreen isDirty];
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    return CGLRetainContext([self->_glassOffscreen getContext]);
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
    return CGLRetainPixelFormat(CGLGetPixelFormat([self->_glassOffscreen getContext]));
}

*/

- (void)flush
{

    if ([NSThread isMainThread]) {
        [[self->_painterOffscreen getLayer] setNeedsDisplay];
    } else {
        [[self->_painterOffscreen getLayer] performSelectorOnMainThread:@selector(setNeedsDisplay)
                                                           withObject:nil
                                                        waitUntilDone:NO
                                                            modes:allModes];
    }
}

- (GlassOffscreen*)getPainterOffscreen
{
    return self->_painterOffscreen;
}
/*
- (GlassOffscreen*)getGlassOffscreen
{
    return self->_glassOffscreen;
}

- (void)hostOffscreen:(GlassOffscreen*)offscreen
{
    [self->_glassOffscreen release];
    self->_glassOffscreen = [offscreen retain];
    [self->_glassOffscreen setLayer:self];
}*/


- (void)display {

    [self blitToScreen];

    [super display];
}

static int nextDrawableCount = 0;

- (void) blitToScreen
{
    id<MTLTexture> backBufferTex = [self->_painterOffscreen texture];

    if (backBufferTex == nil) {
        return;
    }

    int width = [self->_painterOffscreen width];
    int height = [self->_painterOffscreen height];

    if (width <= 0 || height <= 0) {
        //NSLog(@"Layer --------- backing texture not ready yet--- skipping blit.");
        return;
    }

    if (nextDrawableCount > 2) {
        //NSLog(@"Layer --------- previous drawing in progress.. skipping blit to screen.");
        return;
    }

    @autoreleasepool {
        id<MTLCommandBuffer> commandBuf = [self->_blitCommandQueue commandBuffer];
        if (commandBuf == nil) {
            return;
        }
        id<CAMetalDrawable> mtlDrawable = [self nextDrawable];
        if (mtlDrawable == nil) {
            return;
        }

        nextDrawableCount++;

        id <MTLBlitCommandEncoder> blitEncoder = [commandBuf blitCommandEncoder];

        MTLRegion region = {{0,0,0}, {width, height, 1}};

        [blitEncoder
                copyFromTexture:backBufferTex sourceSlice:0 sourceLevel:0
                sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(width, height, 1)
                toTexture:mtlDrawable.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blitEncoder endEncoding];

        [commandBuf presentDrawable:mtlDrawable];
        [commandBuf addCompletedHandler:^(id <MTLCommandBuffer> commandBuf) {
            nextDrawableCount--;
        }];

        [commandBuf commit];
        //[commandBuf waitUntilCompleted];
    }
}

- (void) updateOffscreenTexture:(void*)pixels
      layerWidth: (int)width
      layerHeight:(int)height
{
    id<MTLTexture> backBufferTex = [self->_painterOffscreen texture];

    if ((backBufferTex.width != width) ||
        (backBufferTex.height != height)) {
        return;
    }

    @autoreleasepool {
        id<MTLCommandBuffer> commandBuf = [self->_blitCommandQueue commandBuffer];
        if (commandBuf == nil) {
            return;
        }

        id <MTLBlitCommandEncoder> blitEncoder = [commandBuf blitCommandEncoder];

        id<MTLBuffer> buff = [[self.device newBufferWithBytes:pixels
                                      length:width*height*4
                                      options:0] autorelease];
            [blitEncoder copyFromBuffer:buff
                      sourceOffset:(NSUInteger)0
                 sourceBytesPerRow:(NSUInteger)width * 4
               sourceBytesPerImage:(NSUInteger)width * height * 4
                        sourceSize:MTLSizeMake(width, height, 1)
                         toTexture:backBufferTex
                  destinationSlice:(NSUInteger)0
                  destinationLevel:(NSUInteger)0
                 destinationOrigin:MTLOriginMake(0, 0, 0)];

        [blitEncoder endEncoding];

        [commandBuf addCompletedHandler:^(id <MTLCommandBuffer> commandBuf) {
            //TODO
        }];

        [commandBuf commit];
        [commandBuf waitUntilCompleted];
    }
}

@end
