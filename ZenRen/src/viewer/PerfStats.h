#pragma once

namespace viewer::stats
{
	uint32_t toDurationMicros(const std::chrono::steady_clock::time_point start, const std::chrono::steady_clock::time_point end);
	
	struct TimeSampler;
	void createSampler(TimeSampler& sampler);
	void takeSample(const TimeSampler& sampler, std::chrono::steady_clock::time_point now);

	typedef int16_t TimeSamplerId;

	struct TimeSampler
	{
		TimeSamplerId id = -1;
		bool running = false;

		std::chrono::steady_clock::time_point last = std::chrono::high_resolution_clock::now();
		uint32_t lastTimeMicros = 0;

		TimeSampler() {
			createSampler(*this);
		}

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
			takeSample(*this, now);
			return lastTimeMicros;
		}

		uint32_t sampleRestart(std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now()) {
			uint32_t result = sample(now);
			start(now);
			return result;
		}
	};

	struct Stats {
		uint32_t averageMicros;
	};

	Stats getSampleStats(const TimeSampler& sampler);

	void sampleAndStart(TimeSampler& toStop, TimeSampler& toStart);
}
