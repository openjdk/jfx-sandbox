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
        argsUpdated = false;
        SHADER_LOG(@">>>> MetalShader.initWithContext()----> fragFuncName: %@", fragName);
        @autoreleasepool {
            fragTexArgsDict    = [[[NSMutableDictionary alloc] init] retain];
            fragTexSamplerDict = [[[NSMutableDictionary alloc] init] retain];
            pipeStateNonMSAANoDepthDict = [[[NSMutableDictionary alloc] init] retain];
            pipeStateNonMSAADepthDict = [[[NSMutableDictionary alloc] init] retain];
            pipeStateMSAANoDepthDict = [[[NSMutableDictionary alloc] init] retain];
            pipeStateMSAADepthDict = [[[NSMutableDictionary alloc] init] retain];
            fragArgIndicesDict = [getPRISMDict(fragName) retain];
            if (fragArgIndicesDict == nil) {
                fragArgIndicesDict = [getDECORADict(fragName) retain];
            }
        }

#ifdef SHADER_VERBOSE
        for (NSString *key in fragArgIndicesDict) {
            id value = fragArgIndicesDict[key];
            SHADER_LOG(@"-> Native: MetalShader.initWithContext() Value: %@ for key: %@", value, key);
        }
#endif

        currentRingBufferOffset = -1;
        fragFuncName = fragName;
        fragmentFunction = [[context getPipelineManager] getFunction:fragFuncName];
        NSString* key = (NSString*)[[fragArgIndicesDict allKeys] objectAtIndex:0];
        if ([fragArgIndicesDict count] == 1 && [key isEqualToString:@"UNUSED"]) {
            argumentBufferLength = 0;
        } else {
            argumentEncoder = [fragmentFunction newArgumentEncoderWithBufferIndex:0];
            argumentBufferLength = argumentEncoder.encodedLength;
            SHADER_LOG(@"-> Native: MTLShader.initWithContext()  argumentBufferLength = %lu", argumentBufferLength);
            argumentBuffer = [[context getDevice] newBufferWithLength:argumentBufferLength options:0];
            argumentBuffer.label = [NSString stringWithFormat:@"JFX Argument Buffer for fragmentFunction %@", fragFuncName];
            [argumentEncoder setArgumentBuffer:argumentBuffer offset:0];
        }
        SHADER_LOG(@"<<<< MetalShader.initWithContext()\n");
    }
    return self;
}

- (void) setArgsUpdated:(bool) updated
{
    argsUpdated = updated;
}

- (jobject) getUniformNameIdMap:(JNIEnv*)env
{
    jclass HashMapClass  = (*env)->FindClass(env, "java/util/HashMap");
    jclass IntegerClass  = (*env)->FindClass(env, "java/lang/Integer");
    if(HashMapClass == NULL || IntegerClass == NULL) {
        NSLog(@"can't find the class");
        return NULL;
    }

    jmethodID HashMapClassInitMId  = (*env)->GetMethodID(env, HashMapClass, "<init>", "()V");
    jmethodID IntegerClassInitMId  = (*env)->GetMethodID(env, IntegerClass, "<init>", "(I)V");
    if (HashMapClassInitMId == NULL || IntegerClassInitMId == NULL) {
        NSLog(@"can't find the constructor");
        (*env)->DeleteLocalRef(env, HashMapClass);
        (*env)->DeleteLocalRef(env, IntegerClass);
        return NULL;
    }

    jmethodID putMId = (*env)->GetMethodID(env, HashMapClass, "put",
                        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (putMId == NULL) {
        NSLog(@"can't find the put method");
        (*env)->DeleteLocalRef(env, HashMapClass);
        (*env)->DeleteLocalRef(env, IntegerClass);
        return NULL;
    }

    jobject nameIdMap = (*env)->NewObject(env, HashMapClass, HashMapClassInitMId);
    for (NSString *key in fragArgIndicesDict) {
        NSNumber *index = fragArgIndicesDict[key];
        jobject jKey = (*env)->NewStringUTF(env, [key UTF8String]);
        jint value = index.intValue;
        jobject jValue = (*env)->NewObject(env, IntegerClass, IntegerClassInitMId, value);
        (*env)->CallObjectMethod(env, nameIdMap, putMId, jKey, jValue);
    }
    jobject hashMapGobal = (jobject) (*env)->NewGlobalRef(env, nameIdMap);

    (*env)->DeleteLocalRef(env, nameIdMap);
    (*env)->DeleteLocalRef(env, HashMapClass);
    (*env)->DeleteLocalRef(env, IntegerClass);

    return hashMapGobal;
}

- (void) enable
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.enable()----> fragFuncName: %@", fragFuncName);
    [context setCurrentShader:self];
    SHADER_LOG(@"<<<< MetalShader.enable()\n");
}

- (void) disable
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.disable()----> fragFuncName: %@", fragFuncName);
    [context setCurrentShader:nil];
    SHADER_LOG(@"<<<< MetalShader.disable()\n");
}

- (id<MTLRenderPipelineState>) getPipelineState:(bool)isMSAA
                                  compositeMode:(int)compositeMode;
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.getPipelineState()----> fragFuncName: %@", fragFuncName);
    NSMutableDictionary *psDict;
    if (isMSAA) {
        if ([context isDepthEnabled]) {
            psDict = pipeStateMSAADepthDict;
        } else {
            psDict = pipeStateMSAANoDepthDict;
        }
    } else {
        if ([context isDepthEnabled]) {
            psDict = pipeStateNonMSAADepthDict;
        } else {
            psDict = pipeStateNonMSAANoDepthDict;
        }
    }
    NSNumber *keyCompMode = [NSNumber numberWithInt:compositeMode];
    id<MTLRenderPipelineState> pipeState = psDict[keyCompMode];
    if (pipeState == nil) {
        pipeState = [[context getPipelineManager] getPipeStateWithFragFunc:fragmentFunction
                                                             compositeMode:compositeMode];
        [psDict setObject:pipeState forKey:keyCompMode];
    }
    SHADER_LOG(@"<<<< MetalShader.getPipeState()\n");
    return pipeState;
}

- (NSUInteger) getArgumentBufferLength
{
    return argumentBufferLength;
}

- (int) getRingBufferOffset
{
    return currentRingBufferOffset;
}

- (id<MTLBuffer>) getRingBuffer
{
    return argumentBufferForCB;
}

- (void) copyArgBufferToRingBuffer
{
    if (argumentBufferLength != 0 && argsUpdated) {
        currentRingBufferOffset = [[context getArgsRingBuffer] reserveBytes:argumentBufferLength];

        if (currentRingBufferOffset < 0) {
            currentRingBufferOffset = 0;
            argumentBufferForCB = [context getTransientBufferWithBytes:argumentBuffer.contents
                                                                length:argumentBufferLength];
        } else {
            argumentBufferForCB = [[context getArgsRingBuffer] getBuffer];
            memcpy(argumentBufferForCB.contents + currentRingBufferOffset,
                                argumentBuffer.contents, argumentBufferLength);
            argsUpdated = false;
        }
    }
}

- (NSMutableDictionary*) getTexutresDict
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"MetalShader.getTexutresDict()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragTexArgsDict) {
        id value = fragTexArgsDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif

    return fragTexArgsDict;
}

- (NSMutableDictionary*) getSamplersDict
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"MetalShader.getSamplersDict()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragTexSamplerDict) {
        id value = fragTexSamplerDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif

    return fragTexSamplerDict;
}

- (NSUInteger) getArgumentID:(NSString*) name
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"MetalShader.getArgumentID()----> fragFuncName: %@, name: %@", fragFuncName, name);
    return 0;
}

- (void) setInt:(int)uniformID i0:(int) i0
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setInt() : uniformID = %d, i0= %d", uniformID, i0);
    SHADER_LOG(@"     MetalShader.setInt()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif
    int *anIntPtr = [argumentEncoder constantDataAtIndex:uniformID];
    SHADER_LOG(@"    anIntPtr: %x",(unsigned int) anIntPtr);
    *anIntPtr = i0;
    SHADER_LOG(@"<<<< MetalShader.setInt()");
}


- (void) setTexture:(int)texID
          uniformID:(int)uniformID
            texture:(id<MTLTexture>)texture
           isLinear:(bool)isLinear
           wrapMode:(int)wrapMode
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setTexture() : uniformID = %d, texture = %p", uniformID, texture);
    SHADER_LOG(@"     MetalShader.setTexture()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif

    // texture.label = [NSString stringWithFormat:@"%@_%@", fragFuncName, argumentName];
    NSNumber *idNum = [NSNumber numberWithInt:uniformID];
    [fragTexArgsDict setObject:texture forKey:idNum];

    [argumentEncoder setTexture:texture atIndex:uniformID];

    id<MTLSamplerState> sampler = [context getSampler:isLinear wrapMode:wrapMode];
    [fragTexSamplerDict setObject:sampler forKey:[NSNumber numberWithInt:texID]];

    SHADER_LOG(@"<<<< MetalShader.setTexture()");
}

- (void) setFloat1:(int)uniformID f0:(float) f0
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat() : uniformID = %d, f0= %f", uniformID, f0);
    SHADER_LOG(@"     MetalShader.setFloat()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:uniformID];
    SHADER_LOG(@"    aFloatPtr: %x",(unsigned int) aFloatPtr);
    *aFloatPtr = f0;
    SHADER_LOG(@"<<<< MetalShader.setFloat()");
}

- (void) setFloat2:(int)uniformID f0:(float) f0 f1:(float) f1
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat2() : uniformID = %d, f0= %f, f1= %f", uniformID, f0, f1);
    SHADER_LOG(@"     MetalShader.setFloat2()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:uniformID];
    SHADER_LOG(@"    aFloatPtr: %x", (unsigned int)aFloatPtr);
    *aFloatPtr++ = f0;
    *aFloatPtr = f1;
    SHADER_LOG(@"<<<< MetalShader.setFloat2()");
}

- (void) setFloat3:(int)uniformID f0:(float) f0 f1:(float) f1 f2:(float) f2
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat3() : uniformID = %d, f0= %f, f1= %f, f2= %f", uniformID, f0, f1, f2);
    SHADER_LOG(@"     MetalShader.setFloat3()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:uniformID];
    SHADER_LOG(@"    aFloatPtr: %x",(unsigned int)aFloatPtr);
    *aFloatPtr++ = f0;
    *aFloatPtr++ = f1;
    *aFloatPtr = f2;
    SHADER_LOG(@"<<<< MetalShader.setFloat3()");
}

- (void) setFloat4:(int)uniformID f0:(float) f0 f1:(float) f1 f2:(float) f2  f3:(float) f3
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setFloat4() : uniformID = %d, f0= %f, f1= %f, f2= %f, f3= %f",
                uniformID, f0, f1, f2, f3);
    SHADER_LOG(@"     MetalShader.setFloat4()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:uniformID];
    SHADER_LOG(@"    aFloatPtr: %x", (unsigned int)aFloatPtr);
    *aFloatPtr++ = f0;
    *aFloatPtr++ = f1;
    *aFloatPtr++ = f2;
    *aFloatPtr = f3;
    SHADER_LOG(@"<<<< MetalShader.setFloat4()");
}

- (void) setConstants:(int)uniformID values:(float[]) values size:(int) size
{
    argsUpdated = true;
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.setConstants() : uniformID = %d, size = %d", uniformID, size);
    SHADER_LOG(@"     MetalShader.setConstants()----> fragFuncName: %@", fragFuncName);

#ifdef SHADER_VERBOSE
    for (NSString *key in fragArgIndicesDict) {
        id value = fragArgIndicesDict[key];
        SHADER_LOG(@"    Value: %@ for key: %@", value, key);
    }
#endif
    float *aFloatPtr = [argumentEncoder constantDataAtIndex:uniformID];
    SHADER_LOG(@"    aFloatPtr: %x", (unsigned int)aFloatPtr);
    memcpy(aFloatPtr, values, size * 4);
    SHADER_LOG(@"<<<< MetalShader.setConstants()");
}

- (void) dealloc
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> MetalShader.dispose()----> fragFuncName: %@", fragFuncName);
    for (NSNumber *keyPipeState in pipeStateNonMSAANoDepthDict) {
        [pipeStateNonMSAANoDepthDict[keyPipeState] release];
    }
    for (NSNumber *keyPipeState in pipeStateNonMSAADepthDict) {
        [pipeStateNonMSAADepthDict[keyPipeState] release];
    }
    for (NSNumber *keyPipeState in pipeStateMSAANoDepthDict) {
        [pipeStateMSAANoDepthDict[keyPipeState] release];
    }
    for (NSNumber *keyPipeState in pipeStateMSAADepthDict) {
        [pipeStateMSAADepthDict[keyPipeState] release];
    }
    for (NSNumber *keyTexArg in fragTexArgsDict) {
        [fragTexArgsDict[keyTexArg] release];
    }
    for (NSNumber *keyTexSampler in fragTexSamplerDict) {
        [fragTexSamplerDict[keyTexSampler] release];
    }
    for (NSNumber *keyArgIndex in fragArgIndicesDict) {
        [fragArgIndicesDict[keyArgIndex] release];
    }
    [pipeStateNonMSAANoDepthDict release];
    [pipeStateNonMSAADepthDict release];
    [pipeStateMSAANoDepthDict release];
    [pipeStateMSAADepthDict release];
    [fragTexArgsDict release];
    [fragTexSamplerDict release];
    [fragArgIndicesDict release];
    SHADER_LOG(@"<<<< MetalShader.dispose()");
    [super dealloc];
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

JNIEXPORT jobject JNICALL Java_com_sun_prism_mtl_MTLShader_nGetUniformNameIdMap
  (JNIEnv *env, jclass jClass, jlong shader)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> JNICALL Native: MTLShader_nGetUniformNameIdMap");
    MetalShader *mtlShader = (MetalShader*)jlong_to_ptr(shader);
    return [mtlShader getUniformNameIdMap:env];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nEnable
  (JNIEnv *env, jclass jClass, jlong shader)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@">>>> JNICALL Native: MTLShader_nEnable");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader enable];
    SHADER_LOG(@"<<<< Native: MTLShader_nEnable");
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nDisable
  (JNIEnv *env, jclass jClass, jlong shader)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nDisable");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader disable];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetTexture
  (JNIEnv *env, jclass jClass, jlong shader, jint texID, jint uniformID,
    jlong nTexturePtr, jboolean isLinear, jint wrapMode)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetTexture");
    MetalShader* mtlShader = (MetalShader*)jlong_to_ptr(shader);
    MetalTexture* mtlTex   = (MetalTexture*)jlong_to_ptr(nTexturePtr);
    id<MTLTexture> tex     = [mtlTex getTexture];

    [mtlShader setTexture:texID uniformID:uniformID texture:tex isLinear:isLinear wrapMode:wrapMode];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetInt
  (JNIEnv *env, jclass jClass, jlong shader, jint uniformID, jint i0)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetInt");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader setInt:uniformID i0:i0];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat1
  (JNIEnv *env, jclass jClass, jlong shader, jint uniformID, jfloat f0)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader setFloat1:uniformID f0:f0];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat2
  (JNIEnv *env, jclass jClass, jlong shader, jint uniformID, jfloat f0, jfloat f1)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat2");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader setFloat2:uniformID f0:f0 f1:f1];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat3
  (JNIEnv *env, jclass jClass, jlong shader, jint uniformID,
    jfloat f0, jfloat f1, jfloat f2)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat3");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader setFloat3:uniformID f0:f0 f1:f1 f2:f2];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetFloat4
  (JNIEnv *env, jclass jClass, jlong shader, jint uniformID,
    jfloat f0, jfloat f1, jfloat f2, jfloat f3)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetFloat4");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    [mtlShader setFloat4:uniformID f0:f0 f1:f1 f2:f2 f3:f3];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetConstantsBuf
(JNIEnv *env, jclass class, jlong shader, jint uniformID,
        jobject values, jint valuesByteOffset, jint size) {
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    float *valuesPtr = (float *) (((char *) (*env)->GetDirectBufferAddress(env, values)) + valuesByteOffset);
    [mtlShader setConstants:uniformID values:valuesPtr size:size];
}

JNIEXPORT void JNICALL Java_com_sun_prism_mtl_MTLShader_nSetConstants
  (JNIEnv *env, jclass jClass, jlong shader, jint uniformID,
    jfloatArray valuesArray, jint size)
{
    SHADER_LOG(@"\n");
    SHADER_LOG(@"-> JNICALL Native: MTLShader_nSetConstants");
    MetalShader *mtlShader = (MetalShader *)jlong_to_ptr(shader);
    jfloat* values = (*env)->GetFloatArrayElements(env, valuesArray, 0);
    [mtlShader setConstants:uniformID values:values size:size];
    (*env)->ReleaseFloatArrayElements(env, valuesArray, values, 0);
}
