#include "InventoryPerformanceTelemetry.h"

#include "InventoryCore.h"

#include <Windows.h>

#include <sstream>

namespace
{
const unsigned long kTelemetrySummaryIntervalMs = 5000UL;

struct TickPerfAggregate
{
    TickPerfAggregate()
        : windowStartedMs(0)
        , samples(0)
        , controlsDisabled(0)
        , noTarget(0)
        , visibleTarget(0)
        , hoverTarget(0)
        , filterAttempted(0)
        , filterApplied(0)
        , filterSkipped(0)
        , targetCacheHit(0)
        , visibleScanAttempted(0)
        , visibleScanSkipped(0)
        , hoverScanAttempted(0)
        , totalMicros(0)
        , maxMicros(0)
        , visibleTargetMicros(0)
        , maxVisibleTargetMicros(0)
        , hoverTargetMicros(0)
        , maxHoverTargetMicros(0)
    {
    }

    unsigned long windowStartedMs;
    std::size_t samples;
    std::size_t controlsDisabled;
    std::size_t noTarget;
    std::size_t visibleTarget;
    std::size_t hoverTarget;
    std::size_t filterAttempted;
    std::size_t filterApplied;
    std::size_t filterSkipped;
    std::size_t targetCacheHit;
    std::size_t visibleScanAttempted;
    std::size_t visibleScanSkipped;
    std::size_t hoverScanAttempted;
    unsigned long totalMicros;
    unsigned long maxMicros;
    unsigned long visibleTargetMicros;
    unsigned long maxVisibleTargetMicros;
    unsigned long hoverTargetMicros;
    unsigned long maxHoverTargetMicros;
};

struct FilterPerfAggregate
{
    FilterPerfAggregate()
        : windowStartedMs(0)
        , samples(0)
        , success(0)
        , activeFilter(0)
        , forceShowAll(0)
        , totalMicros(0)
        , maxMicros(0)
        , previousTrackedEntries(0)
        , maxPreviousTrackedEntries(0)
        , rawWidgetEntries(0)
        , maxRawWidgetEntries(0)
        , boundEntries(0)
        , maxBoundEntries(0)
        , mergedEntries(0)
        , maxMergedEntries(0)
        , totalEntryCount(0)
        , maxTotalEntryCount(0)
        , visibleEntryCount(0)
        , maxVisibleEntryCount(0)
        , visibleQuantity(0)
        , maxVisibleQuantity(0)
    {
    }

    unsigned long windowStartedMs;
    std::size_t samples;
    std::size_t success;
    std::size_t activeFilter;
    std::size_t forceShowAll;
    unsigned long totalMicros;
    unsigned long maxMicros;
    std::size_t previousTrackedEntries;
    std::size_t maxPreviousTrackedEntries;
    std::size_t rawWidgetEntries;
    std::size_t maxRawWidgetEntries;
    std::size_t boundEntries;
    std::size_t maxBoundEntries;
    std::size_t mergedEntries;
    std::size_t maxMergedEntries;
    std::size_t totalEntryCount;
    std::size_t maxTotalEntryCount;
    std::size_t visibleEntryCount;
    std::size_t maxVisibleEntryCount;
    std::size_t visibleQuantity;
    std::size_t maxVisibleQuantity;
};

struct DebugProbePerfAggregate
{
    DebugProbePerfAggregate()
        : windowStartedMs(0)
        , samples(0)
        , signatureChanged(0)
        , totalMicros(0)
        , maxMicros(0)
        , localCandidates(0)
        , maxLocalCandidates(0)
        , globalBackpackContents(0)
        , maxGlobalBackpackContents(0)
    {
    }

    unsigned long windowStartedMs;
    std::size_t samples;
    std::size_t signatureChanged;
    unsigned long totalMicros;
    unsigned long maxMicros;
    std::size_t localCandidates;
    std::size_t maxLocalCandidates;
    std::size_t globalBackpackContents;
    std::size_t maxGlobalBackpackContents;
};

TickPerfAggregate g_tickPerfAggregate;
FilterPerfAggregate g_filterPerfAggregate;
DebugProbePerfAggregate g_debugProbePerfAggregate;

long long CurrentPerfMicros()
{
    static bool initialized = false;
    static bool useQueryPerformanceCounter = false;
    static LARGE_INTEGER frequency = {0};

    if (!initialized)
    {
        initialized = true;
        useQueryPerformanceCounter =
            QueryPerformanceFrequency(&frequency) != FALSE && frequency.QuadPart > 0;
    }

    if (useQueryPerformanceCounter)
    {
        LARGE_INTEGER counter = {0};
        if (QueryPerformanceCounter(&counter) != FALSE)
        {
            const long long wholeSeconds = counter.QuadPart / frequency.QuadPart;
            const long long remainder = counter.QuadPart % frequency.QuadPart;
            return wholeSeconds * 1000000LL + (remainder * 1000000LL) / frequency.QuadPart;
        }
    }

    return static_cast<long long>(GetTickCount()) * 1000LL;
}

unsigned long AverageUnsignedLong(unsigned long total, std::size_t samples)
{
    return samples == 0 ? 0UL : total / static_cast<unsigned long>(samples);
}

std::size_t AverageSize(std::size_t total, std::size_t samples)
{
    return samples == 0 ? 0 : total / samples;
}

void AddMaxUnsignedLong(unsigned long value, unsigned long* maxValue)
{
    if (maxValue != 0 && value > *maxValue)
    {
        *maxValue = value;
    }
}

void AddMaxSize(std::size_t value, std::size_t* maxValue)
{
    if (maxValue != 0 && value > *maxValue)
    {
        *maxValue = value;
    }
}

bool ShouldLogInventoryPerformanceTelemetry()
{
    return ShouldLogSearchDebug() || ShouldEnableDebugProbes();
}

bool ShouldFlushTelemetryWindow(unsigned long startedMs, unsigned long nowMs)
{
    return startedMs != 0
        && nowMs >= startedMs
        && nowMs - startedMs >= kTelemetrySummaryIntervalMs;
}

void ResetTickPerfAggregate(unsigned long nowMs)
{
    g_tickPerfAggregate = TickPerfAggregate();
    g_tickPerfAggregate.windowStartedMs = nowMs;
}

void ResetFilterPerfAggregate(unsigned long nowMs)
{
    g_filterPerfAggregate = FilterPerfAggregate();
    g_filterPerfAggregate.windowStartedMs = nowMs;
}

void ResetDebugProbePerfAggregate(unsigned long nowMs)
{
    g_debugProbePerfAggregate = DebugProbePerfAggregate();
    g_debugProbePerfAggregate.windowStartedMs = nowMs;
}

void RecordInventorySearchTickPerf(const InventorySearchTickPerfSample& sample)
{
    if (!ShouldLogInventoryPerformanceTelemetry())
    {
        return;
    }

    const unsigned long nowMs = GetTickCount();
    if (g_tickPerfAggregate.windowStartedMs == 0)
    {
        ResetTickPerfAggregate(nowMs);
    }

    ++g_tickPerfAggregate.samples;
    if (!sample.controlsEnabled)
    {
        ++g_tickPerfAggregate.controlsDisabled;
    }
    if (sample.controlsEnabled && !sample.visibleTarget && !sample.hoverTarget)
    {
        ++g_tickPerfAggregate.noTarget;
    }
    if (sample.visibleTarget)
    {
        ++g_tickPerfAggregate.visibleTarget;
    }
    if (sample.hoverTarget)
    {
        ++g_tickPerfAggregate.hoverTarget;
    }
    if (sample.filterAttempted)
    {
        ++g_tickPerfAggregate.filterAttempted;
    }
    if (sample.filterApplied)
    {
        ++g_tickPerfAggregate.filterApplied;
    }
    if (sample.filterSkipped)
    {
        ++g_tickPerfAggregate.filterSkipped;
    }
    if (sample.targetCacheHit)
    {
        ++g_tickPerfAggregate.targetCacheHit;
    }
    if (sample.visibleScanAttempted)
    {
        ++g_tickPerfAggregate.visibleScanAttempted;
    }
    if (sample.visibleScanSkipped)
    {
        ++g_tickPerfAggregate.visibleScanSkipped;
    }
    if (sample.hoverScanAttempted)
    {
        ++g_tickPerfAggregate.hoverScanAttempted;
    }

    g_tickPerfAggregate.totalMicros += sample.totalMicros;
    AddMaxUnsignedLong(sample.totalMicros, &g_tickPerfAggregate.maxMicros);
    g_tickPerfAggregate.visibleTargetMicros += sample.visibleTargetMicros;
    AddMaxUnsignedLong(sample.visibleTargetMicros, &g_tickPerfAggregate.maxVisibleTargetMicros);
    g_tickPerfAggregate.hoverTargetMicros += sample.hoverTargetMicros;
    AddMaxUnsignedLong(sample.hoverTargetMicros, &g_tickPerfAggregate.maxHoverTargetMicros);

    if (!ShouldFlushTelemetryWindow(g_tickPerfAggregate.windowStartedMs, nowMs))
    {
        return;
    }

    std::stringstream line;
    line << "[perf][inventory-search] tick"
         << " samples=" << g_tickPerfAggregate.samples
         << " avg_us="
         << AverageUnsignedLong(g_tickPerfAggregate.totalMicros, g_tickPerfAggregate.samples)
         << " max_us=" << g_tickPerfAggregate.maxMicros
         << " controls_disabled=" << g_tickPerfAggregate.controlsDisabled
         << " no_target=" << g_tickPerfAggregate.noTarget
         << " visible_target=" << g_tickPerfAggregate.visibleTarget
         << " hover_target=" << g_tickPerfAggregate.hoverTarget
         << " filter_attempts=" << g_tickPerfAggregate.filterAttempted
         << " filter_applied=" << g_tickPerfAggregate.filterApplied
         << " filter_skipped=" << g_tickPerfAggregate.filterSkipped
         << " target_cache_hit=" << g_tickPerfAggregate.targetCacheHit
         << " visible_scan_attempts=" << g_tickPerfAggregate.visibleScanAttempted
         << " visible_scan_skipped=" << g_tickPerfAggregate.visibleScanSkipped
         << " visible_scan_avg_us="
         << AverageUnsignedLong(g_tickPerfAggregate.visibleTargetMicros, g_tickPerfAggregate.samples)
         << " visible_scan_attempt_avg_us="
         << AverageUnsignedLong(
                g_tickPerfAggregate.visibleTargetMicros,
                g_tickPerfAggregate.visibleScanAttempted)
         << " visible_scan_max_us=" << g_tickPerfAggregate.maxVisibleTargetMicros
         << " hover_scan_attempts=" << g_tickPerfAggregate.hoverScanAttempted
         << " hover_scan_avg_us="
         << AverageUnsignedLong(g_tickPerfAggregate.hoverTargetMicros, g_tickPerfAggregate.samples)
         << " hover_scan_attempt_avg_us="
         << AverageUnsignedLong(
                g_tickPerfAggregate.hoverTargetMicros,
                g_tickPerfAggregate.hoverScanAttempted)
         << " hover_scan_max_us=" << g_tickPerfAggregate.maxHoverTargetMicros;
    LogInfoLine(line.str());
    ResetTickPerfAggregate(nowMs);
}

void RecordInventorySearchFilterPerf(const InventorySearchFilterPerfSample& sample)
{
    if (!ShouldLogInventoryPerformanceTelemetry())
    {
        return;
    }

    const unsigned long nowMs = GetTickCount();
    if (g_filterPerfAggregate.windowStartedMs == 0)
    {
        ResetFilterPerfAggregate(nowMs);
    }

    ++g_filterPerfAggregate.samples;
    if (sample.success)
    {
        ++g_filterPerfAggregate.success;
    }
    if (sample.hasActiveFilter)
    {
        ++g_filterPerfAggregate.activeFilter;
    }
    if (sample.forceShowAll)
    {
        ++g_filterPerfAggregate.forceShowAll;
    }

    g_filterPerfAggregate.totalMicros += sample.elapsedMicros;
    AddMaxUnsignedLong(sample.elapsedMicros, &g_filterPerfAggregate.maxMicros);
    g_filterPerfAggregate.previousTrackedEntries += sample.previousTrackedEntries;
    AddMaxSize(sample.previousTrackedEntries, &g_filterPerfAggregate.maxPreviousTrackedEntries);
    g_filterPerfAggregate.rawWidgetEntries += sample.rawWidgetEntries;
    AddMaxSize(sample.rawWidgetEntries, &g_filterPerfAggregate.maxRawWidgetEntries);
    g_filterPerfAggregate.boundEntries += sample.boundEntries;
    AddMaxSize(sample.boundEntries, &g_filterPerfAggregate.maxBoundEntries);
    g_filterPerfAggregate.mergedEntries += sample.mergedEntries;
    AddMaxSize(sample.mergedEntries, &g_filterPerfAggregate.maxMergedEntries);
    g_filterPerfAggregate.totalEntryCount += sample.totalEntryCount;
    AddMaxSize(sample.totalEntryCount, &g_filterPerfAggregate.maxTotalEntryCount);
    g_filterPerfAggregate.visibleEntryCount += sample.visibleEntryCount;
    AddMaxSize(sample.visibleEntryCount, &g_filterPerfAggregate.maxVisibleEntryCount);
    g_filterPerfAggregate.visibleQuantity += sample.visibleQuantity;
    AddMaxSize(sample.visibleQuantity, &g_filterPerfAggregate.maxVisibleQuantity);

    if (!ShouldFlushTelemetryWindow(g_filterPerfAggregate.windowStartedMs, nowMs))
    {
        return;
    }

    std::stringstream line;
    line << "[perf][inventory-search] filter"
         << " samples=" << g_filterPerfAggregate.samples
         << " success=" << g_filterPerfAggregate.success
         << " active_filter=" << g_filterPerfAggregate.activeFilter
         << " force_show_all=" << g_filterPerfAggregate.forceShowAll
         << " avg_us="
         << AverageUnsignedLong(g_filterPerfAggregate.totalMicros, g_filterPerfAggregate.samples)
         << " max_us=" << g_filterPerfAggregate.maxMicros
         << " prev_tracked_avg="
         << AverageSize(g_filterPerfAggregate.previousTrackedEntries, g_filterPerfAggregate.samples)
         << " prev_tracked_max=" << g_filterPerfAggregate.maxPreviousTrackedEntries
         << " raw_entries_avg="
         << AverageSize(g_filterPerfAggregate.rawWidgetEntries, g_filterPerfAggregate.samples)
         << " raw_entries_max=" << g_filterPerfAggregate.maxRawWidgetEntries
         << " bound_entries_avg="
         << AverageSize(g_filterPerfAggregate.boundEntries, g_filterPerfAggregate.samples)
         << " bound_entries_max=" << g_filterPerfAggregate.maxBoundEntries
         << " merged_entries_avg="
         << AverageSize(g_filterPerfAggregate.mergedEntries, g_filterPerfAggregate.samples)
         << " merged_entries_max=" << g_filterPerfAggregate.maxMergedEntries
         << " total_entries_avg="
         << AverageSize(g_filterPerfAggregate.totalEntryCount, g_filterPerfAggregate.samples)
         << " total_entries_max=" << g_filterPerfAggregate.maxTotalEntryCount
         << " visible_entries_avg="
         << AverageSize(g_filterPerfAggregate.visibleEntryCount, g_filterPerfAggregate.samples)
         << " visible_entries_max=" << g_filterPerfAggregate.maxVisibleEntryCount
         << " visible_quantity_avg="
         << AverageSize(g_filterPerfAggregate.visibleQuantity, g_filterPerfAggregate.samples)
         << " visible_quantity_max=" << g_filterPerfAggregate.maxVisibleQuantity;
    LogInfoLine(line.str());
    ResetFilterPerfAggregate(nowMs);
}

void RecordInventoryDebugProbePerf(const InventoryDebugProbePerfSample& sample)
{
    if (!ShouldLogInventoryPerformanceTelemetry())
    {
        return;
    }

    const unsigned long nowMs = GetTickCount();
    if (g_debugProbePerfAggregate.windowStartedMs == 0)
    {
        ResetDebugProbePerfAggregate(nowMs);
    }

    ++g_debugProbePerfAggregate.samples;
    if (sample.signatureChanged)
    {
        ++g_debugProbePerfAggregate.signatureChanged;
    }
    g_debugProbePerfAggregate.totalMicros += sample.elapsedMicros;
    AddMaxUnsignedLong(sample.elapsedMicros, &g_debugProbePerfAggregate.maxMicros);
    g_debugProbePerfAggregate.localCandidates += sample.localCandidates;
    AddMaxSize(sample.localCandidates, &g_debugProbePerfAggregate.maxLocalCandidates);
    g_debugProbePerfAggregate.globalBackpackContents += sample.globalBackpackContents;
    AddMaxSize(
        sample.globalBackpackContents,
        &g_debugProbePerfAggregate.maxGlobalBackpackContents);

    if (!ShouldFlushTelemetryWindow(g_debugProbePerfAggregate.windowStartedMs, nowMs))
    {
        return;
    }

    std::stringstream line;
    line << "[perf][inventory-search] debug_probe"
         << " samples=" << g_debugProbePerfAggregate.samples
         << " signature_changed=" << g_debugProbePerfAggregate.signatureChanged
         << " avg_us="
         << AverageUnsignedLong(g_debugProbePerfAggregate.totalMicros, g_debugProbePerfAggregate.samples)
         << " max_us=" << g_debugProbePerfAggregate.maxMicros
         << " local_candidates_avg="
         << AverageSize(g_debugProbePerfAggregate.localCandidates, g_debugProbePerfAggregate.samples)
         << " local_candidates_max=" << g_debugProbePerfAggregate.maxLocalCandidates
         << " global_backpack_content_avg="
         << AverageSize(
                g_debugProbePerfAggregate.globalBackpackContents,
                g_debugProbePerfAggregate.samples)
         << " global_backpack_content_max="
         << g_debugProbePerfAggregate.maxGlobalBackpackContents;
    LogInfoLine(line.str());
    ResetDebugProbePerfAggregate(nowMs);
}
}

InventoryPerfTimer::InventoryPerfTimer()
    : m_startedMicros(CurrentPerfMicros())
{
}

unsigned long InventoryPerfTimer::ElapsedMs() const
{
    return (ElapsedMicros() + 500UL) / 1000UL;
}

unsigned long InventoryPerfTimer::ElapsedMicros() const
{
    const long long elapsedMicros = CurrentPerfMicros() - m_startedMicros;
    if (elapsedMicros <= 0)
    {
        return 0UL;
    }

    return static_cast<unsigned long>(elapsedMicros);
}

InventorySearchTickPerfSample::InventorySearchTickPerfSample()
    : controlsEnabled(false)
    , visibleTarget(false)
    , hoverTarget(false)
    , filterAttempted(false)
    , filterApplied(false)
    , filterSkipped(false)
    , targetCacheHit(false)
    , visibleScanAttempted(false)
    , visibleScanSkipped(false)
    , hoverScanAttempted(false)
    , visibleTargetMicros(0)
    , hoverTargetMicros(0)
    , totalMicros(0)
{
}

InventorySearchTickPerfScope::InventorySearchTickPerfScope()
    : m_timer()
    , m_sample()
{
}

InventorySearchTickPerfScope::~InventorySearchTickPerfScope()
{
    m_sample.totalMicros = m_timer.ElapsedMicros();
    RecordInventorySearchTickPerf(m_sample);
}

InventorySearchTickPerfSample& InventorySearchTickPerfScope::Sample()
{
    return m_sample;
}

InventorySearchFilterPerfSample::InventorySearchFilterPerfSample()
    : success(false)
    , forceShowAll(false)
    , hasActiveFilter(false)
    , blueprintOnly(false)
    , previousTrackedEntries(0)
    , rawWidgetEntries(0)
    , boundEntries(0)
    , mergedEntries(0)
    , totalEntryCount(0)
    , visibleEntryCount(0)
    , visibleQuantity(0)
    , elapsedMicros(0)
{
}

InventorySearchFilterPerfScope::InventorySearchFilterPerfScope()
    : m_timer()
    , m_sample()
{
}

InventorySearchFilterPerfScope::~InventorySearchFilterPerfScope()
{
    m_sample.elapsedMicros = m_timer.ElapsedMicros();
    RecordInventorySearchFilterPerf(m_sample);
}

InventorySearchFilterPerfSample& InventorySearchFilterPerfScope::Sample()
{
    return m_sample;
}

InventoryDebugProbePerfSample::InventoryDebugProbePerfSample()
    : signatureChanged(false)
    , localCandidates(0)
    , globalBackpackContents(0)
    , elapsedMicros(0)
{
}

InventoryDebugProbePerfScope::InventoryDebugProbePerfScope()
    : m_timer()
    , m_sample()
{
}

InventoryDebugProbePerfScope::~InventoryDebugProbePerfScope()
{
    m_sample.elapsedMicros = m_timer.ElapsedMicros();
    RecordInventoryDebugProbePerf(m_sample);
}

InventoryDebugProbePerfSample& InventoryDebugProbePerfScope::Sample()
{
    return m_sample;
}
