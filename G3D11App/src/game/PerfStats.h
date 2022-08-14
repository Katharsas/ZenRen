#pragma once

namespace game::stats {

	struct FrameSample
	{
		std::chrono::steady_clock::time_point temp;

		int32_t renderTimeMicros;
		int32_t sleepTimeMicros;

		int32_t toDurationMicros(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) {
			const auto duration = end - start;
			return static_cast<int32_t> (duration / std::chrono::microseconds(1));
		}

		void renderStart() {
			temp = std::chrono::high_resolution_clock::now();
		}

		int32_t renderEndSleepStart() {
			const auto end = std::chrono::high_resolution_clock::now();
			renderTimeMicros = toDurationMicros(temp, end);
			temp = end;
			return sleepTimeMicros;
		}

		int32_t sleepEnd() {
			const auto end = std::chrono::high_resolution_clock::now();
			sleepTimeMicros = toDurationMicros(temp, end);
			//temp = 
			return sleepTimeMicros;
		}
	};

	void addFrameSample(FrameSample sample, int32_t frameTimeTarget);
}
