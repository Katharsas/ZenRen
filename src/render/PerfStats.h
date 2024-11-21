#pragma once

namespace render::stats
{
	// sample arbitrary values (counts etc.)

	typedef int16_t SamplerId;

	struct Stats {
		uint32_t average;
	};

	uint32_t toDurationMicros(const std::chrono::steady_clock::time_point start, const std::chrono::steady_clock::time_point end);

	SamplerId createSampler();
	void takeSample(SamplerId samplerId, uint32_t value, const std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now());
	Stats getSamplerStats(SamplerId samplerId);

	// sample time durations, returned values are in microseconds (us)

	struct TimeSampler;

	TimeSampler createTimeSampler();
	void takeTimeSample(const TimeSampler& sampler, const std::chrono::steady_clock::time_point now);
	void sampleAndStart(TimeSampler& toStop, TimeSampler& toStart);
	Stats getTimeSamplerStats(const TimeSampler& sampler);

	struct TimeSampler
	{
		SamplerId id = -1;
		bool running = false;

		std::chrono::steady_clock::time_point last = std::chrono::high_resolution_clock::now();
		uint32_t lastTimeMicros = 0;

		void start(std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now()) {
			assert(!running);
			running = true;
			last = now;
		}

		uint32_t stop(std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now()) {
			assert(running);
			running = false;
			lastTimeMicros = toDurationMicros(last, now);
			last = now;
			return lastTimeMicros;
		}

		uint32_t sample(std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now()) {
			if (running) {
				stop(now);
			}
			takeTimeSample(*this, now);
			return lastTimeMicros;
		}

		uint32_t sampleRestart(std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now()) {
			uint32_t result = sample(now);
			start(now);
			return result;
		}

		void logMillisAndRestart(const std::string& message)
		{
			LOG(INFO) << message << ": " << stop() / 1000 << "ms";
			start();
		}
		void logMicrosAndRestart(const std::string& message)
		{
			LOG(INFO) << message << ": " << stop() << "us";
			start();
		}
	};
}
