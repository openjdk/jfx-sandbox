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

#import <jni.h>

#import "PrismShaderCommon.h"
#import "DecoraShaderCommon.h"
#import "MetalShader.h"
#import "com_sun_prism_mtl_MTLShader.h"

#ifdef SHADER_VERBOSE
#define SHADER_LOG NSLog
#else
#define SHADER_LOG(...)
#endif

NSString* jStringToNSString(JNIEnv *env, jstring string)
{
    if (string == NULL) return NULL;
    jsize length = (*env)->GetStringLength(env, string);
    NSString *result = NULL;
    const jchar *chars =(*env)->GetStringCritical(env, string, 0);
    if (chars) {
        @try {
            result = [NSString stringWithCharacters: chars length: length];
        }
        @finally {
            (*env)->ReleaseStringCritical(env, string, chars);
        }
    }
    return result;
}

@implementation MetalShader

- (id) initWithContext:(MetalContext*)ctx withFragFunc:(NSString*) fragName
{
    SHADER_LOG(@"\n");
    self = [super init];
    if (self) {
        context = ctx;
        SHADER_LOG(@">>>> MetalShader.initWithContext()----> fragFuncName: %@", fragName);
        fragArgIndicesDict = getPRISMDict(fragName);
        if (fragArgIndicesDict == nil) {
            fragArgIndicesDict = getDECORADict(fragName);
        }

        for (NSString *key in fragArgIndicesDict) {
            id value = fragArgIndicesDict[key];
            SHADER_LOG(@"-> Native: MetalShader.initWithContext() Value: %@ for key: %@", value, key);
        }

        fragFuncName = fragName;
        fragmentFunction = [[context getPipelineManager] getFunction:fragFuncName];
        argumentEncoder = [fragmentFunction newArgumentEncoderWithBufferIndex:0];
        NSUInteger argumentBufferLength = argumentEncoder.encodedLength;
        SHADER_LOG(@"-> Native: MTLShader.initWithContext()  argumentBufferLength = %lu", argumentBufferLength);
        argumentBuffer = [[context getDevice] newBufferWithLength:argumentBufferLength options:0];
        argumentBuffer.label = [NSString stringWithFormat:@"Argument Buffer for fragmentFunction %@", fragFuncName];
        [argumentEncoder setArgumentBuffer:argumentBuffer offset:0];
        pipeState = [[context getPipelineManager] getPipeStateWithFragFunc:fragmentFunction];
        SHADER_LOG(@"<<<< MetalShader.initWithContext()\n");
    }
    return self;
}

- (void) enable
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.enable()----> fragFuncName: %@", fragFuncName);
    [context setCurrentPipeState:pipeState];
    [context setCurrentArgumentBuffer:argumentBuffer];
    [context setCurrentShader:self];
    SHADER_LOG(@"<<<< MetalShader.enable()\n");
}

- (id<MTLRenderPipelineState>) getPipeState
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.getPipeState()----> fragFuncName: %@", fragFuncName);
    return pipeState;
    SHADER_LOG(@"<<<< MetalShader.getPipeState()\n");
}

- (id<MTLBuffer>) getArgumentBuffer
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.getArgumentBuffer()----> fragFuncName: %@", fragFuncName);
    return argumentBuffer;
    SHADER_LOG(@"<<<< MetalShader.getArgumentBuffer()\n");
}

- (NSUInteger) getArgumentID:(NSString*) name
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"MetalShader.getArgumentID()----> fragFuncName: %@, name: %@", fragFuncName, name);
    return 0;
}

- (void) setInt:(NSString*)argumentName i0:(int) i0
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setInt() : argumentName = %@, i0= %d", argumentName, i0);
    SHADER_LOG(@"     MetalShader.setInt()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);
    int *anIntPtr = [argumentEncoder constantDataAtIndex:index.intValue];
    SHADER_LOG(@"    anIntPtr: %x",(unsigned int) anIntPtr);
    *anIntPtr = i0;
    SHADER_LOG(@"<<<< MetalShader.setInt()");
}

- (void) setTexture:(NSString*)argumentName texture:(id<MTLTexture>) texture
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setTexture() : argumentName = %@, texture = %p", argumentName, texture);
    SHADER_LOG(@"     MetalShader.setTexture()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);

    [argumentEncoder setTexture:texture atIndex:index.intValue];

    SHADER_LOG(@"<<<< MetalShader.setTexture()");
}

- (void) setFloat:  (NSString*)argumentName f0:(float) f0
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat() : argumentName = %@, f0= %f", argumentName, f0);
    SHADER_LOG(@"     MetalShader.setFloat()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:index.intValue];
    SHADER_LOG(@"    aFloatPtr: %x",(unsigned int) aFloatPtr);
    *aFloatPtr = f0;
    SHADER_LOG(@"<<<< MetalShader.setFloat()");
}

- (void) setFloat2: (NSString*)argumentName f0:(float) f0 f1:(float) f1
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat2() : argumentName = %@, f0= %f, f1= %f", argumentName, f0, f1);
    SHADER_LOG(@"     MetalShader.setFloat2()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }

    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:index.intValue];
    SHADER_LOG(@"    aFloatPtr: %x", (unsigned int)aFloatPtr);
    *aFloatPtr++ = f0;
    *aFloatPtr = f1;
    SHADER_LOG(@"<<<< MetalShader.setFloat2()");
}

- (void) setFloat3: (NSString*)argumentName f0:(float) f0 f1:(float) f1 f2:(float) f2
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat3() : argumentName = %@, f0= %f, f1= %f, f2= %f", argumentName, f0, f1, f2);
    SHADER_LOG(@"     MetalShader.setFloat3()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }

    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:index.intValue];
    SHADER_LOG(@"    aFloatPtr: %x",(unsigned int)aFloatPtr);
    *aFloatPtr++ = f0;
    *aFloatPtr++ = f1;
    *aFloatPtr = f2;
    SHADER_LOG(@"<<<< MetalShader.setFloat3()");
}

- (void) setFloat4: (NSString*)argumentName f0:(float) f0 f1:(float) f1 f2:(float) f2  f3:(float) f3
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat4() : argumentName = %@, f0= %f, f1= %f, f2= %f, f3= %f",
                argumentName, f0, f1, f2, f3);
    SHADER_LOG(@"     MetalShader.setFloat4()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }

    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:index.intValue];
    SHADER_LOG(@"    aFloatPtr: %x", (unsigned int)aFloatPtr);
    *aFloatPtr++ = f0;
    *aFloatPtr++ = f1;
    *aFloatPtr++ = f2;
    *aFloatPtr = f3;
    SHADER_LOG(@"<<<< MetalShader.setFloat4()");
}

- (void) setConstants: (NSString*)argumentName values:(float[]) values size:(int) size
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setConstants() : argumentName = %@, size = %d", argumentName, size);
    SHADER_LOG(@"     MetalShader.setConstants()----> fragFuncName: %@", fragFuncName);
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }

    NSNumber *index = fragArgIndicesDict[argumentName];
    SHADER_LOG(@"    index.intValue: %d", index.intValue);
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:index.intValue];
    SHADER_LOG(@"    aFloatPtr: %x", (unsigned int)aFloatPtr);
    for (int i = 0; i < size; i++) {
        *aFloatPtr++ = values[i];
    }
    SHADER_LOG(@"<<<< MetalShader.setConstants()");
}

@end // MetalShader


JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nCreateMetalShader
  (JNIEnv *env, jclass jClass, jlong ctx, jstring fragFuncName)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> JNICALL Native: MTLShader_nCreateMetalShader");
    MetalContext* context = (MetalContext*)jlong_to_ptr(ctx);
    NSString *nameString = jStringToNSString(env, fragFuncName);
    MetalShader* shader = [[MetalShader alloc] initWithContext:context withFragFunc:nameString];
    jlong shader_ptr = ptr_to_jlong(shader);
    SHADER_LOG(@"<<<< Native: MTLShader_nCreateMetalShader");
    return shader_ptr;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nEnable
  (JNIEnv *env, jclass jClass, jlong shader)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> JNICALL Native: MTLShader_nEnable");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader enable];
    SHADER_LOG(@"<<<< Native: MTLShader_nEnable");
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nDisable
  (JNIEnv *env, jclass jClass, jlong shader)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nDisable");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    //[mtlShader disable];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetTexture
  (JNIEnv *env, jclass jClass, jlong shader, jstring name, jlong nTexturePtr)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetTexture");
    MetalShader* mtlShader = (MetalShader*)jlong_to_ptr(shader);
    NSString* nameString   = jStringToNSString(env, name);
    MetalTexture* mtlTex   = (MetalTexture*)jlong_to_ptr(nTexturePtr);
    id<MTLTexture> tex     = [mtlTex getTexture];

    [mtlShader setTexture:nameString texture:tex];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetInt
  (JNIEnv *env, jclass jClass, jlong shader, jstring name, jint i0)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetInt");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    NSString *nameString = jStringToNSString(env, name);
    [mtlShader setInt:nameString i0:i0];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat
  (JNIEnv *env, jclass jClass, jlong shader, jstring name, jfloat f0)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    NSString *nameString = jStringToNSString(env, name);
    [mtlShader setFloat:nameString f0:f0];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat2
  (JNIEnv *env, jclass jClass, jlong shader, jstring name, jfloat f0, jfloat f1)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat2");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    NSString *nameString = jStringToNSString(env, name);
    [mtlShader setFloat2:nameString f0:f0 f1:f1];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat3
  (JNIEnv *env, jclass jClass, jlong shader, jstring name,
    jfloat f0, jfloat f1, jfloat f2)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat3");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    NSString *nameString = jStringToNSString(env, name);
    [mtlShader setFloat3:nameString f0:f0 f1:f1 f2:f2];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat4
  (JNIEnv *env, jclass jClass, jlong shader, jstring name,
    jfloat f0, jfloat f1, jfloat f2, jfloat f3)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat4");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    NSString *nameString = jStringToNSString(env, name);
    [mtlShader setFloat4:nameString f0:f0 f1:f1 f2:f2 f3:f3];
    return 1;
}

JNIEXPORT jlong JNICALL Java_com_sun_prism_mtl_MTLShader_nSetConstants
  (JNIEnv *env, jclass jClass, jlong shader, jstring name,
    jfloatArray valuesArray, jint size)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetConstants");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    NSString *nameString = jStringToNSString(env, name);
    jfloat* values = (*env)->GetFloatArrayElements(env, valuesArray, 0);
    [mtlShader setConstants:nameString values:values size:size];
    return 1;
}
