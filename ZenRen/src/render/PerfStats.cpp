#include "stdafx.h"
#include "PerfStats.h"

#include <numeric>

namespace render::stats
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
		uint64_t sum = std::accumulate(samples.begin(), (samples.begin() + sampleCount), (uint64_t) 0);
		return (uint32_t) ((sum / (double) sampleCount) + .5f);
		
	}

	void updateStats(SamplerId id, int32_t currentSampleCount)
	{
		lastStats[id].average = average(sampleBuffers[id], currentSampleCount);
	}

	SamplerId createSampler()
	{
		SamplerId id = (SamplerId) sampleBuffers.size();
		sampleBuffers.push_back({});
		lastUpdated.push_back({ 0, std::chrono::high_resolution_clock::now() });
		lastStats.push_back({ 0 });
		return id;
	}

	void takeSample(SamplerId samplerId, uint32_t value, const std::chrono::steady_clock::time_point now)
	{
		// save sample into buffer
		assert(samplerId >= 0 && (size_t) samplerId < lastUpdated.size());
		auto& [currentSampleCount, lastUpdate] = lastUpdated[samplerId];

		sampleBuffers[samplerId][currentSampleCount] = value;
		currentSampleCount++;

		// update stats
		uint32_t millisSinceLastUpdated = toDurationMillis(lastUpdate, now);
		if (currentSampleCount >= sampleSize || millisSinceLastUpdated >= maxDurationMillis) {
			updateStats(samplerId, currentSampleCount);
			currentSampleCount = 0;
			lastUpdate = now;
		}
	}

	Stats getSamplerStats(SamplerId samplerId) {
		return lastStats[samplerId];
	}

	TimeSampler createTimeSampler()
	{
		TimeSampler sampler;
		sampler.id = createSampler();
		return sampler;
	}

	void takeTimeSample(const TimeSampler& sampler, const std::chrono::steady_clock::time_point now)
	{
		takeSample(sampler.id, sampler.lastTimeMicros, now);
	}

	void sampleAndStart(TimeSampler& toStop, TimeSampler& toStart)
	{
		toStop.sample();
		toStart.start(toStop.last);
	}

	Stats getTimeSamplerStats(const TimeSampler& sampler) {
		return lastStats[sampler.id];
	}
}