#include "stdafx.h"

#include <array>

#include "GameLoop.h"
#include "TimerPrecision.h"
#include "renderer/D3D11Renderer.h"

namespace game
{
	// frame limiter
	const BOOL frameLimiterEnabled = TRUE;
	const int32_t frameLimit = 120;
	const int32_t frameTimeTarget = 1000000 / frameLimit;

	// logging
	const int32_t sampleSize = 500; // number of frames until statistics are averaged and logged
	
	int32_t sampleIndex = 0;
	std::array<int32_t, sampleSize> renderTimeSamples;
	std::array<int32_t, sampleSize> sleepTimeSamples;

	struct LoggingSettings
	{
		BOOL fps = TRUE;                       // average fps
		BOOL renderTime = TRUE;                // average render time [usec]
		BOOL sleepTime = frameLimiterEnabled;              // average sleep time [usec]
		BOOL offsetTime = frameLimiterEnabled;             // average difference between target & actual frame time [usec]
		BOOL offsetTimePermille = frameLimiterEnabled;     // offsetTime as percentage of target frame time [permille]
		BOOL offsetPositivePermille = frameLimiterEnabled; // like offsetTimePermille, but only for frame times > target time
		BOOL offsetNegativePermille = frameLimiterEnabled; // like offsetTimePermille, but only for frame times < target time
	};

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

	void init(HWND hWnd)
	{
		enablePreciseTimerResolution();
		initMicrosleep();
		renderer::initD3D(hWnd);
		LOG(DEBUG) << "Statistics in microseconds";
	}

	void renderAndSleep()
	{
		const auto startTimeRender = std::chrono::high_resolution_clock::now();

		renderer::renderFrame();

		const auto endTimeRender = std::chrono::high_resolution_clock::now();
		const auto timeRender = endTimeRender - startTimeRender;
		const auto timeRenderMicros = static_cast<int32_t> (timeRender / std::chrono::microseconds(1));
		renderTimeSamples[sampleIndex] = timeRenderMicros;

		// sleep_for can wake up about once every 1,4ms
		//std::this_thread::sleep_for(std::chrono::microseconds(100));

		// microsleep can wake up about once every 0,5ms
		if (!frameLimiterEnabled)
		{
			microsleep(100);
		}
		else
		{
			const int32_t sleepTarget = frameTimeTarget - timeRenderMicros;
			if (sleepTarget > 0)
			{
				microsleep(sleepTarget + 470); // make sure we never get too short frametimes
			}
		}

		const auto endTimeSleep = std::chrono::high_resolution_clock::now();
		const auto timeSleep = endTimeSleep - endTimeRender;
		const auto timeSleepMicros = static_cast<int32_t> (timeSleep / std::chrono::microseconds(1));
		sleepTimeSamples[sampleIndex] = timeSleepMicros;

		sampleIndex++;
	}

	void logStats()
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

		
		const LoggingSettings s = {
			// overwrite logging defaults here
		};
		const std::string splitter = " | ";
		std::stringstream log;
		if (s.fps)					log << "FPS: " << fps << splitter;
		if (s.renderTime)			log << "renderTime: " << averageRenderTime << splitter;
		if (s.sleepTime)			log << "sleepTime: " << averageSleepTime << splitter;
		if (s.offsetTime)			log << "offsetTime: " << offAverage << splitter;
		if (s.offsetTimePermille)	log << "offsetTime (p): " << averagePromille << splitter;
		if (s.offsetPositivePermille) log << "offsetTime too high (p): " << averageHigherPermille << splitter;
		if (s.offsetNegativePermille) log << "offsetTime too low (p): " << averageLowerPermille << splitter;

		LOG(DEBUG) << log.str();

		// uncomment for checking actual frametime distribution for single batch of samples
		/*for (int i = 0; i < sampleSize; i++)
		{
			LOG(DEBUG) << "renderTime: " << renderTimeSamples[i] << " sleepTime: " << sleepTimeSamples[i];
		}
		Sleep(100);
		exit(0);*/
	}

	void execute()
	{
		renderAndSleep();
		if (sampleIndex >= sampleSize)
		{
			logStats();
			sampleIndex = 0;
		}
	}

	void cleanup()
	{
		renderer::cleanD3D();
		cleanupMicrosleep();
		disablePreciseTimerResolution();
	}
}