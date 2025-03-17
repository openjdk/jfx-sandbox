/*
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include <algorithm>
#include <codecvt>
#include <string>

namespace D3D12 {
namespace Internal {

class Utils
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> mConverter;

public:
    template <typename T>
    static inline T Align(T offset, T alignment)
    {
        return (offset + alignment - 1u) & ~(alignment - 1u);
    }

    static inline uint32_t CalcSubresource(uint32_t mipSlice, uint32_t mipLevels, uint32_t arraySlice)
    {
        // refer to https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources
        return mipSlice + (arraySlice * mipLevels);
    }

    static inline uint32_t CountZeroBitsLSB(uint32_t x, uint32_t limit)
    {
        uint32_t zeros = 0;
        while ((x & 0x1) == 0)
        {
            ++zeros;
            if (zeros >= limit) return zeros;
            x >>= 1;
        }

        return zeros;
    }

    static inline uint32_t CalcMipmapLevels(uint32_t width, uint32_t height)
    {
        return static_cast<uint32_t>(std::floor(std::log2(std::max<uint32_t>(width, height)))) + 1;
    }

    static inline std::wstring Utils::ToWString(const std::string& s)
    {
        return mConverter.from_bytes(s);
    }

    static inline std::string Utils::ToString(const std::wstring& s)
    {
        return mConverter.to_bytes(s);
    }
};

} // namespace Internal
} // namespace D3D12
