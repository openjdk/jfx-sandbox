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

#include "D3D12Config.hpp"

#include "D3D12Common.hpp"


namespace D3D12 {
namespace Internal {

Config::Config()
    : mJNIEnv(nullptr)
    , mPrismSettingsClass(nullptr)
    , mSystemClass(nullptr)
    , mGetPropertyMethodID(nullptr)
{
    D3D12NI_ZERO_STRUCT(mSettings);
}

std::string Config::GetPropertyInternal(const char* propertyName)
{
    jstring propStr = mJNIEnv->NewStringUTF(propertyName);
    jstring propValueStr = reinterpret_cast<jstring>(mJNIEnv->CallStaticObjectMethod(mSystemClass, mGetPropertyMethodID, propStr));
    if (mJNIEnv->ExceptionCheck() == JNI_TRUE)
    {
        mJNIEnv->ExceptionClear();
        return std::string();
    }

    if (propValueStr == nullptr) return std::string();

    const char* propValue = mJNIEnv->GetStringUTFChars(propValueStr, false);
    std::string ret(propValue);
    mJNIEnv->ReleaseStringUTFChars(propValueStr, propValue);
    return ret;
}

jfieldID Config::GetSettingsFieldID(const char* name, const char* signature)
{
    jfieldID id = mJNIEnv->GetStaticFieldID(mPrismSettingsClass, name, signature);
    if (mJNIEnv->ExceptionCheck() == JNI_TRUE)
    {
        mJNIEnv->ExceptionClear();
        return nullptr;
    }

    return id;
}

Config& Config::Instance()
{
    static Config instance;
    return instance;
}

bool Config::LoadConfiguration(JNIEnv* env, jclass psClass)
{
    // to fetch Prism settings
    mJNIEnv = env;
    mPrismSettingsClass = psClass;

    // to fetch System properties from JVM
    mSystemClass = mJNIEnv->FindClass("java/lang/System");
    if (mSystemClass == nullptr) return false;

    mGetPropertyMethodID = mJNIEnv->GetStaticMethodID(mSystemClass, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
    if (mGetPropertyMethodID == nullptr) return false;

    // fetch debug configuration (these settings should only be acquired when
    // JFX is build in DebugNative configuration
#if DEBUG
    mSettings.verbose = GetBool("verbose");
    mSettings.debug = GetBool("debug");
    mSettings.trace = GetBoolProperty("prism.d3d12.trace");
    mSettings.debugLayers = GetBoolProperty("prism.d3d12.debugLayers");
    mSettings.gpuDebug = GetBoolProperty("prism.d3d12.gpuDebug");
    mSettings.breakOnError = GetBoolProperty("prism.d3d12.breakOnError");
    mSettings.colorLogs = GetBoolProperty("prism.d3d12.colorLogs");
    mSettings.fileLog = GetBoolProperty("prism.d3d12.fileLog");
#endif

    mSettings.vsync = GetBool("isVsyncEnabled");

    // cleanup leftovers
    mJNIEnv = nullptr;
    mPrismSettingsClass = nullptr;
    mSystemClass = nullptr;
    mGetPropertyMethodID = nullptr;

    return true;
}

int Config::GetInt(const char* name)
{
    jfieldID id = GetSettingsFieldID(name, "I");
    return id ? mJNIEnv->GetStaticIntField(mPrismSettingsClass, id) : 0;
}

bool Config::GetBool(const char* name)
{
    jfieldID id = GetSettingsFieldID(name, "Z");
    return id && mJNIEnv->GetStaticBooleanField(mPrismSettingsClass, id);
}

bool Config::GetBoolProperty(const char* name)
{
    std::string propValue = GetPropertyInternal(name);
    if (propValue.empty()) return false;

    if (propValue.compare("true") == 0) return true;
    else if (propValue.compare("false") == 0) return false;
    else return std::atoi(propValue.c_str()) > 0;
}

int Config::GetIntProperty(const char* name)
{
    std::string propValue = GetPropertyInternal(name);
    return propValue.empty() ? 0 : std::atoi(propValue.c_str());
}

} // namespace Internal
} // namespace D3D12
