#pragma once

namespace game::stats {

	struct FrameSample
	{
		bool firstFrame = true;
		std::chrono::steady_clock::time_point start;
		std::chrono::steady_clock::time_point last;

		int32_t updateTimeMicros;
		int32_t renderTimeMicros;
		int32_t sleepTimeMicros;

		int32_t toDurationMicros(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) {
			const auto duration = end - start;
			return static_cast<int32_t> (duration / std::chrono::microseconds(1));
		}

		int32_t updateStart() {
			const auto now = std::chrono::high_resolution_clock::now();
			int32_t deltaTime;
			if (firstFrame) {
				deltaTime = 0;
			}
			else {
				// time between last start and current start
				deltaTime = toDurationMicros(start, now);
			}
			start = now;
			last = now;
			return deltaTime;
		}

		int32_t updateEndRenderStart() {
			const auto now = std::chrono::high_resolution_clock::now();
			updateTimeMicros = toDurationMicros(last, now);
			last = now;
			return updateTimeMicros;
		}

		int32_t renderEndSleepStart() {
			const auto now = std::chrono::high_resolution_clock::now();
			renderTimeMicros = toDurationMicros(last, now);
			last = now;
			return sleepTimeMicros;
		}

		int32_t sleepEnd() {
			firstFrame = false;

			const auto now = std::chrono::high_resolution_clock::now();
			sleepTimeMicros = toDurationMicros(last, now);
			last = now;
			return sleepTimeMicros;
		}
	};

	void addFrameSample(FrameSample sample, int32_t frameTimeTarget);
}
