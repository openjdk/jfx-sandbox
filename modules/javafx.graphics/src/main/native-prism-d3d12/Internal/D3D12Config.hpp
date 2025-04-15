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

#pragma once

#include <jni.h>
#include <string>

namespace D3D12 {
namespace Internal {

class Config
{
    JNIEnv* mJNIEnv;
    jclass mPrismSettingsClass;

    jclass mSystemClass;
    jmethodID mGetPropertyMethodID;

    struct {
        bool verbose;
        bool debug;
        bool trace;
        bool debugLayers;
        bool gpuDebug;
        bool breakOnError;
        bool colorLogs;
        bool vsync;
        bool fileLog;
        bool apiOpts;
        bool dred;
    } mSettings;

    Config();
    ~Config() = default;

    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;

    std::string GetPropertyInternal(const char* propertyName);
    jfieldID GetSettingsFieldID(const char* name, const char* signature);
    bool GetBool(const char* name);
    int GetInt(const char* name);
    bool GetBoolProperty(const char* name); // defaults to false when TryGet fails
    int GetIntProperty(const char* name); // defaults to 0 when TryGet fails

    // below TryGet's return whether property exists or not
    // true when @p result is valid, false when getting property failed
    bool TryGetBoolProperty(const char* name, bool& result);
    bool TryGetIntProperty(const char* name, int& result);

public:
    static Config& Instance();

    bool LoadConfiguration(JNIEnv* env, jclass psClass);

    //pre-fetched properties and settings
    inline bool IsVerbose()
    {
        return mSettings.verbose;
    }

    inline bool IsDebug()
    {
        return mSettings.debug;
    }

    inline bool IsTrace()
    {
        return mSettings.trace;
    }

    inline bool IsDebugLayerEnabled()
    {
        return mSettings.debugLayers;
    }

    inline bool IsGpuDebugEnabled()
    {
        return mSettings.gpuDebug;
    }

    inline bool IsBreakOnErrorEnabled()
    {
        return mSettings.breakOnError;
    }

    inline bool IsColorLogsEnabled()
    {
        return mSettings.colorLogs;
    }

    inline bool IsFileLogEnabled()
    {
        return mSettings.fileLog;
    }

    inline bool IsVsyncEnabled()
    {
        return mSettings.vsync;
    }

    inline bool IsApiOptsEnabled()
    {
        return mSettings.apiOpts;
    }

    inline bool IsDREDEnabled()
    {
        return mSettings.dred;
    }
};

} // namespace Internal
} // namespace D3D12
