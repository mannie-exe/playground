#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL_log.h>

#include <support/PerformanceMonitor.hpp>

const char *PerformanceMonitor::name(FramePhase phase) {
  switch (phase) {
  case FramePhase::Poll:
    return "poll";
  case FramePhase::Update:
    return "update";
  case FramePhase::Render:
    return "render";
  case FramePhase::Present:
    return "present";
  case FramePhase::Total:
    return "total";
  }
  return "unknown";
}

void PerformanceMonitor::setConfig(PerformanceConfig config) {
  if (config.historySize > maximumHistorySize)
    throw std::invalid_argument("Performance history exceeds entry limit");
  config.reportEveryFrames =
      config.reportEveryFrames == 0 ? 1 : config.reportEveryFrames;
  _gpuGroups.reserve(maximumGPUGroups);
  if (_config.enabled != config.enabled)
    resetStatistics();
  _config = config;
  while (_history.size() > _config.historySize)
    _history.pop_front();
  while (_gpuHistory.size() > _config.historySize)
    _gpuHistory.pop_front();
}

void PerformanceMonitor::setEnabled(bool enabled) {
  if (_config.enabled == enabled)
    return;
  resetStatistics();
  _config.enabled = enabled;
}

void PerformanceMonitor::end(FramePhase phase) {
  if (!_config.enabled || !_active[index(phase)])
    return;

  const std::uint64_t start{_starts[index(phase)]};
  _active[index(phase)] = false;
  _sample.milliseconds[index(phase)] +=
      milliseconds(SDL_GetPerformanceCounter() - start);
  _sample.measured[index(phase)] = true;
}

void PerformanceMonitor::endFrame() {
  if (!_config.enabled || !_active[index(FramePhase::Total)])
    return;

  end(FramePhase::Total);
  recordFrame(_sample);
}

void PerformanceMonitor::recordFrame(const PerformanceSample &sample) {
  if (!_config.enabled)
    return;
  for (std::size_t i = 0; i < sample.measured.size(); ++i)
    if (sample.measured[i] &&
        (!std::isfinite(sample.milliseconds[i]) || sample.milliseconds[i] < 0))
      throw std::invalid_argument("Invalid CPU performance sample");
  const auto nextFrame = _frameCount + 1;
  auto stats = _stats;
  for (std::size_t i = 0; i < sample.measured.size(); ++i)
    if (sample.measured[i])
      stats[i].add(sample.milliseconds[i]);
  if (_config.historySize) {
    _history.push_back(
        {nextFrame, sample, gpuTimingStatus(), _gpuSampleReceived, _uiWork});
    if (_history.size() > _config.historySize)
      _history.pop_front();
  }
  _stats = stats;
  _frameCount = nextFrame;
  for (const auto &entry : _uiWork) {
    playground::ui::accumulate(_uiSummary, entry.work);
    for (std::size_t i = 0; i < _uiMilliseconds.size(); ++i)
      _uiMilliseconds[i] += entry.milliseconds[i];
  }
  _uiWork.clear();
  _gpuSampleReceived = false;
  ++_framesSinceReport;
  if (_config.logSummary && _framesSinceReport >= _config.reportEveryFrames)
    report();
}

PerformanceReport PerformanceMonitor::snapshotReport() const {
  return {_framesSinceReport,  _stats,
          _uiSummary,          _gpuGroups,
          gpuTimingStatus(),   _gpuCollection,
          _gpuSamplesReceived, _omittedGPUSamples,
          _queryDrops,         _bufferDiscards,
          _idleWait,           _paint,
          _uiMilliseconds};
}

void PerformanceMonitor::resetReportInterval() noexcept {
  _stats = {};
  _uiSummary = {};
  _idleWait = {};
  _paint = {};
  _uiMilliseconds = {};
  _gpuGroups.clear();
  _gpuSamplesReceived = 0;
  _omittedGPUSamples = 0;
  _queryDrops = 0;
  _bufferDiscards = 0;
  _framesSinceReport = 0;
}

void PerformanceMonitor::resetStatistics() {
  if (_statisticsRevision == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Performance statistics revision exhausted");
  ++_statisticsRevision;
  resetReportInterval();
  _active = {};
  _sample = {};
  _history.clear();
  _uiWork.clear();
  _gpuHistory.clear();
  _gpuCollection.reset();
  _gpuMeasured = false;
  _gpuSampleReceived = false;
  _frameCount = 0;
}

void PerformanceMonitor::report() {
  if (!_config.enabled)
    return;

  const auto summary = snapshotReport();
  std::string message{
      std::format("Performance | CPU frames={}", summary.cpuFrames)};
  for (const FramePhase phase :
       {FramePhase::Poll, FramePhase::Update, FramePhase::Render,
        FramePhase::Present, FramePhase::Total}) {
    const auto &stats = summary.cpu[index(phase)];
    if (!stats.count) {
      message += std::format(" | {} unmeasured", name(phase));
      continue;
    }
    message += std::format(
        " | {} avg={:.3f}ms min={:.3f}ms max={:.3f}ms", name(phase),
        *stats.average(), stats.minimumMilliseconds, stats.maximumMilliseconds);
  }
  const char *status = "unsupported";
  switch (summary.gpuTiming) {
  case GPUTimingStatus::Unsupported:
    break;
  case GPUTimingStatus::Disabled:
    status = "disabled";
    break;
  case GPUTimingStatus::Pending:
    status = "enabled, pending/no new sample";
    break;
  case GPUTimingStatus::Measured:
    status = "measured (collection has completed samples)";
    break;
  }
  message +=
      std::format("\nGPU | timing={} | completed samples received={} | query "
                  "drops={} | buffer discards={} | omitted samples={}",
                  status, summary.gpuSamplesReceived, summary.queryDrops,
                  summary.bufferDiscards, summary.omittedGPUSamples);
  if (summary.collection)
    message += std::format(" | current domain={} generation={} pending={}",
                           summary.collection->domain.value,
                           summary.collection->generation,
                           summary.collection->pending);
  const auto extent = [](const auto &value) {
    return value ? std::format("{}x{}", value->x, value->y)
                 : std::string{"unspecified"};
  };
  for (const auto &group : summary.gpu) {
    message += std::format(
        "\n  {} | domain={} generation={} | source={} target={} | samples={} | "
        "duration avg={:.3f}ms min={:.3f}ms max={:.3f}ms",
        group.key.label, group.key.domain.value, group.key.collectionGeneration,
        extent(group.key.context.sourcePixels),
        extent(group.key.context.targetPixels), group.duration.count,
        *group.duration.average(), group.duration.minimumMilliseconds,
        group.duration.maximumMilliseconds);
    if (group.completionLatency.count)
      message += std::format(" | completion latency samples={} avg={:.3f}ms "
                             "min={:.3f}ms max={:.3f}ms",
                             group.completionLatency.count,
                             *group.completionLatency.average(),
                             group.completionLatency.minimumMilliseconds,
                             group.completionLatency.maximumMilliseconds);
    else
      message += " | completion latency unmeasured";
  }
  message += std::format(
      "\nUI | measure={} hits={} arrange={} skips={} text={} hits={} visits={}",
      summary.ui.measured, summary.ui.measureCacheHits, summary.ui.arranged,
      summary.ui.arrangeSkips, summary.ui.textLayouts,
      summary.ui.textLayoutHits, summary.ui.invalidationVisits);
  message += std::format(" publications={} cached={}", summary.ui.publications,
                         summary.ui.publicationHits);
  constexpr std::array uiNames{"layout", "prepare",     "paint",
                               "input",  "completions", "publication"};
  for (std::size_t i = 0; i < uiNames.size(); ++i)
    message += std::format(" | {} total={:.3f}ms", uiNames[i],
                           summary.uiMilliseconds[i]);
  message +=
      std::format("\nPaint | considered={} rejected={} quads={} batches={} "
                  "upload={} bytes rect={} general={} presentation={}",
                  summary.paint.considered, summary.paint.rejected,
                  summary.paint.quads, summary.paint.drawCalls,
                  summary.paint.streamedBytes, summary.paint.rectangularQuads,
                  summary.paint.generalQuads, summary.paint.presentationQuads);
  message +=
      std::format("\nIdle | waits={} total={:.3f}ms (excluded from CPU frames)",
                  summary.idleWait.count, summary.idleWait.totalMilliseconds);
  SDL_Log("%s", message.c_str());
  resetReportInterval();
}

void PerformanceMonitor::recordUI(const playground::ui::UIWorkSample &sample) {
  if (!_config.enabled)
    return;
  if (!sample.root)
    throw std::invalid_argument("UI sample needs a root identity");
  for (double ms : sample.milliseconds)
    if (!std::isfinite(ms) || ms < 0)
      throw std::invalid_argument("Invalid UI duration");
  for (auto &entry : _uiWork)
    if (entry.root == sample.root) {
      playground::ui::accumulate(entry.work, sample.work);
      for (std::size_t i = 0; i < entry.milliseconds.size(); ++i)
        entry.milliseconds[i] += sample.milliseconds[i];
      return;
    }
  if (_uiWork.size() >= 256)
    throw std::length_error("UI diagnostic root budget exceeded");
  _uiWork.push_back(sample);
}

void PerformanceMonitor::recordGPU(
    const playground::rendering::GPUTimingSample &sample) {
  if (!_config.enabled || !_gpuTimingAvailable)
    return;
  if (_gpuCollection &&
      (!_gpuCollection->enabled || sample.domain != _gpuCollection->domain ||
       sample.collectionGeneration != _gpuCollection->generation))
    return;
  playground::rendering::validateGPUTimingLabel(sample.label);
  playground::rendering::validateGPUWorkContext(sample.context);
  if (!sample.sequence || !sample.domain ||
      !std::isfinite(sample.milliseconds) || sample.milliseconds < 0 ||
      (sample.completionLatencyMilliseconds &&
       (!std::isfinite(*sample.completionLatencyMilliseconds) ||
        *sample.completionLatencyMilliseconds < 0)))
    throw std::invalid_argument("Invalid GPU performance sample");
  GPUGroupKey key{sample.domain, sample.collectionGeneration, sample.label,
                  sample.context};
  key.context.workloadId = key.context.qualityRevision = 0;
  key.context.frameId = 0; // grouping is by workload, correlation stays in raw samples
  auto it = std::ranges::find(_gpuGroups, key, &GPUGroupReport::key);
  const bool omitted =
      it == _gpuGroups.end() && _gpuGroups.size() == maximumGPUGroups;
  GPUGroupReport group =
      it == _gpuGroups.end() ? GPUGroupReport{std::move(key)} : *it;
  if (!omitted) {
    group.duration.add(sample.milliseconds);
    if (sample.completionLatencyMilliseconds)
      group.completionLatency.add(*sample.completionLatencyMilliseconds);
  }
  if (_config.historySize) {
    _gpuHistory.push_back(sample);
    if (_gpuHistory.size() > _config.historySize)
      _gpuHistory.pop_front();
  }
  if (omitted)
    ++_omittedGPUSamples;
  else if (it == _gpuGroups.end())
    _gpuGroups.push_back(std::move(group));
  else
    *it = std::move(group);
  ++_gpuSamplesReceived;
  _gpuMeasured = true;
  _gpuSampleReceived = true;
}

void PerformanceMonitor::recordGPUCollection(
    const playground::rendering::GPUTimingCollection &collection) {
  if (!_config.enabled)
    return;
  if (collection.supported &&
      (!collection.domain || (collection.enabled && !collection.generation)))
    throw std::invalid_argument(
        "GPU collection requires domain and active generation");
  const bool same = _gpuCollection &&
                    _gpuCollection->domain == collection.domain &&
                    _gpuCollection->generation == collection.generation;
  const auto previousDrops = same ? _gpuCollection->queryDrops : 0;
  const auto previousDiscards = same ? _gpuCollection->bufferDiscards : 0;
  if (collection.queryDrops < previousDrops ||
      collection.bufferDiscards < previousDiscards)
    throw std::invalid_argument(
        "GPU collection counters cannot move backwards");
  setGPUTimingAvailable(collection.supported);
  if (!same)
    _gpuMeasured = false;
  _queryDrops += collection.queryDrops - previousDrops;
  _bufferDiscards += collection.bufferDiscards - previousDiscards;
  _gpuCollection = collection;
}

bool PerformanceMonitor::handleHotkey(const SDL_KeyboardEvent &key) {
  if (key.type != SDL_EVENT_KEY_DOWN || key.repeat)
    return false;

  if (key.key == SDLK_F10 && (key.mod & SDL_KMOD_SHIFT)) {
    if (isEnabled())
      report();
    else
      SDL_Log("Performance reporting is disabled; runtime telemetry remains active");
    return true;
  }

  if (key.key == SDLK_F10) {
    toggleEnabled();
    SDL_Log("Performance reporting %s", isEnabled() ? "enabled" : "disabled");
    return true;
  }

  if (key.key == SDLK_F11) {
    auto config = _config;
    config.reportEveryFrames = _config.reportEveryFrames == 60 ? 300 : 60;
    setConfig(config);
    SDL_Log("Performance summary interval: %u frames",
            _config.reportEveryFrames);
    return true;
  }

  return false;
}
