/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "D3D12Profiler.hpp"

#include "D3D12Common.hpp"
#include "D3D12Config.hpp"


namespace {

inline void PrintEventCounter(const char* name, uint64_t hits, uint64_t frameCount)
{
    if (hits)
    {
        D3D12NI_LOG_WARN("   - %d %s hits (avg %.2f per frame)", hits, name, static_cast<float>(hits) / static_cast<float>(frameCount));
    }
}

} // namespace

namespace D3D12 {
namespace Internal {

Profiler::Profiler()
    : mEventSources()
    , mSourceCount(0)
    , mFrameCount(0)
{
}

Profiler::~Profiler()
{
}

Profiler& Profiler::Instance()
{
    static Profiler instance;
    return instance;
}

uint32_t Profiler::RegisterSource(const std::string& name)
{
    uint32_t id = mSourceCount++;
    mEventSources.emplace_back(id, name);
    return id;
}

void Profiler::RenameSource(uint32_t sourceID, const std::string& name)
{
    D3D12NI_ASSERT(sourceID < mSourceCount, "Invalid source ID provided");
    mEventSources[sourceID].name = name;
}

void Profiler::MarkEvent(uint32_t sourceID, Event event)
{
    D3D12NI_ASSERT(sourceID < mSourceCount, "Invalid source ID provided");
    mEventSources[sourceID].totalHits++;
    mEventSources[sourceID].hits[static_cast<uint32_t>(event)]++;
}

void Profiler::MarkFrameEnd()
{
    mFrameCount++;
}

void Profiler::PrintSummary()
{
    if (!Config::Instance().IsProfilerSummaryEnabled()) return;

    if (mFrameCount == 0) mFrameCount = 1;

    D3D12NI_LOG_WARN("===   Profiler summary   ===");
    D3D12NI_LOG_WARN("D3D12 Profiler registered hits from %d sources across %d frames (not-hit events are skipped):", mSourceCount, mFrameCount);
    for (const EventSource& source: mEventSources)
    {
        D3D12NI_LOG_WARN("%d. %s - %d hits (avg %.2f per frame)", source.id, source.name.c_str(), source.totalHits, static_cast<float>(source.totalHits) / static_cast<float>(mFrameCount));
        PrintEventCounter("Event", source.hits[static_cast<uint32_t>(Event::Event)], mFrameCount);
        PrintEventCounter("Signal", source.hits[static_cast<uint32_t>(Event::Signal)], mFrameCount);
        PrintEventCounter("Wait", source.hits[static_cast<uint32_t>(Event::Wait)], mFrameCount);
    }
    D3D12NI_LOG_WARN("=== Profiler summary end ===");
}

} // namespace Internal
} // namespace D3D12
