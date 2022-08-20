#include "stdafx.h"
#include "PerfStats.h"

namespace game::stats {

	struct LoggingSettings
	{
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

	// logging
	const int32_t sampleSize = 500; // number of frames until statistics are averaged and logged

	int32_t sampleIndex = 0;
	std::array<int32_t, sampleSize> renderTimeSamples;
	std::array<int32_t, sampleSize> sleepTimeSamples;


	int32_t divideOrZero(float dividend, int32_t divisor)
	{
		if (divisor == 0) return 0;
		else return static_cast<int32_t>(dividend / divisor);
	}

	int32_t average(std::array<int32_t, sampleSize> arr)
	{
		int32_t sum = 0;
		for (auto element : arr)
		{
			sum += element;
		}
		return sum / sampleSize;
	}

	void logStats(int32_t frameTimeTarget)
	{
		// log fps and frame times
		const int32_t averageRenderTime = average(renderTimeSamples);
		const int32_t averageSleepTime = average(sleepTimeSamples);
		const int32_t fps = 1000000 / (averageRenderTime + averageSleepTime);

		// log frame time divergence
		int32_t higherFrameTimeCount = 0;
		float higherFrameTimePercentSum = 0;

		int32_t lowerFrameTimeCount = 0;
		float lowerFrameTimePercentSum = 0;

		int32_t frameTimeOffCount = 0;
		int32_t frameTimeOffSum = 0;

		for (int i = 0; i < sampleSize; i++)
		{
			const int32_t ft = renderTimeSamples[i] + sleepTimeSamples[i];
			const int32_t off = ft - frameTimeTarget;
			frameTimeOffCount++;
			if (off > 0)
			{
				frameTimeOffSum += off;
				higherFrameTimePercentSum += (float)off / frameTimeTarget;
				higherFrameTimeCount++;
			}
			else
			{
				frameTimeOffSum -= off;
				lowerFrameTimePercentSum -= (float)off / frameTimeTarget;
				lowerFrameTimeCount++;
			}
		}
		const int32_t offAverage = frameTimeOffSum / frameTimeOffCount;
		const int32_t averageHigherPermille = divideOrZero(higherFrameTimePercentSum * 1000, higherFrameTimeCount);
		const int32_t averageLowerPermille = divideOrZero(lowerFrameTimePercentSum * 1000, lowerFrameTimeCount);
		const int32_t averagePromille = (int32_t)((higherFrameTimePercentSum + lowerFrameTimePercentSum) * 1000) / (higherFrameTimeCount + lowerFrameTimeCount);

		bool frameLimiterEnabled = frameTimeTarget != 0;
		const std::string splitter = " | ";
		std::stringstream log;
		if (settings.fps)					log << "FPS: " << fps << splitter;
		if (settings.renderTime)			log << "renderTime: " << averageRenderTime << splitter;
		if (frameLimiterEnabled) {
			if (settings.sleepTime)				log << "sleepTime: " << averageSleepTime << splitter;
			if (settings.offsetTime)			log << "offsetTime: " << offAverage << splitter;
			if (settings.offsetTimePermille)	log << "offsetTime (p): " << averagePromille << splitter;
			if (settings.offsetPositivePermille) log << "offsetTime too high (p): " << averageHigherPermille << splitter;
			if (settings.offsetNegativePermille) log << "offsetTime too low (p): " << averageLowerPermille << splitter;
		}

		LOG(DEBUG) << log.str();

		// uncomment for checking actual frametime distribution for single batch of samples
		/*for (int i = 0; i < sampleSize; i++)
		{
			LOG(DEBUG) << "renderTime: " << renderTimeSamples[i] << " sleepTime: " << sleepTimeSamples[i];
		}
		Sleep(100);
		exit(0);*/
	}

	void addFrameSample(FrameSample sample, int32_t frameTimeTarget) {
		renderTimeSamples[sampleIndex] = sample.renderTimeMicros;
		sleepTimeSamples[sampleIndex] = sample.sleepTimeMicros;

		sampleIndex++;

		if (sampleIndex >= sampleSize)
		{
			logStats(frameTimeTarget);
			sampleIndex = 0;
		}
	}
}