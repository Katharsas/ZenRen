#include "stdafx.h"
#include "PerfStats.h"

#include <numeric>

namespace viewer::stats
{
	using std::array;
	using std::vector;
	using std::pair;
	using std::chrono::steady_clock;

	uint32_t toDurationMicros(const std::chrono::steady_clock::time_point start, const std::chrono::steady_clock::time_point end) {
		const auto duration = end - start;
		assert(duration.count() > 0);
		return static_cast<uint32_t> (duration / std::chrono::microseconds(1));
	}
	uint32_t toDurationMillis(const std::chrono::steady_clock::time_point start, const std::chrono::steady_clock::time_point end) {
		const auto duration = end - start;
		assert(duration.count() > 0);
		return static_cast<uint32_t> (duration / std::chrono::milliseconds(1));
	}

	struct LoggingSettings
	{
		bool detailedLoggingEnabled = false;
		bool fps = true;                    // average fps
		bool renderTime = true;             // average render time [usec]
		bool sleepTime = true;              // average sleep time [usec]
		bool offsetTime = true;             // average difference between target & actual frame time [usec]
		bool offsetTimePermille = true;     // offsetTime as percentage of target frame time [permille]
		bool offsetPositivePermille = true; // like offsetTimePermille, but only for frame times > target time
		bool offsetNegativePermille = true; // like offsetTimePermille, but only for frame times < target time
	};

	const LoggingSettings settings = {
		// overwrite logging defaults here
	};

	const int32_t sampleSize = 1000; // max number of frames before samples are averaged to update stats
	const int32_t maxDurationMillis = 1000; // max ms duration before samples are averaged to update stats

	vector<array<uint32_t, sampleSize>> sampleBuffers;
	vector<pair<int32_t, steady_clock::time_point>> lastUpdated;
	vector<Stats> lastStats;


	int32_t divideOrZero(float dividend, int32_t divisor)
	{
		if (divisor == 0) return 0;
		else return static_cast<int32_t>(dividend / divisor);
	}

	uint32_t average(const std::array<uint32_t, sampleSize>& samples, uint32_t sampleCount)
	{
		return std::accumulate(samples.begin(), (samples.begin() + sampleCount), 0) / sampleCount;
	}

	void createSampler(TimeSampler& sampler)
	{
		sampler.id = (int16_t) sampleBuffers.size();
		sampleBuffers.push_back({});
		lastUpdated.push_back({ 0, std::chrono::high_resolution_clock::now() });
		lastStats.push_back({ 0 });
	}

	void updateStats(const TimeSamplerId& id, int32_t currentSampleCount)
	{
		lastStats[id].averageMicros = average(sampleBuffers[id], currentSampleCount);
	}

	void takeSample(const TimeSampler& sampler, std::chrono::steady_clock::time_point now)
	{
		// save sample into buffer
		assert(sampler.id >= 0 && sampler.id < lastUpdated.size());
		auto& [currentSampleCount, lastUpdate] = lastUpdated[sampler.id];

		sampleBuffers[sampler.id][currentSampleCount] = sampler.lastTimeMicros;
		currentSampleCount++;

		// update stats
		uint32_t millisSinceLastUpdated = toDurationMillis(lastUpdate, now);
		if (currentSampleCount >= sampleSize || millisSinceLastUpdated >= maxDurationMillis) {
			updateStats(sampler.id, currentSampleCount);
			currentSampleCount = 0;
			lastUpdate = now;
		}
	}

	Stats getSampleStats(const TimeSampler& sampler) {
		return lastStats[sampler.id];
	}

	void sampleAndStart(TimeSampler& toStop, TimeSampler& toStart)
	{
		toStop.sample();
		toStart.start(toStop.last);
	}
}