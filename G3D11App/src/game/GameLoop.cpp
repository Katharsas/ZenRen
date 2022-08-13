#include "stdafx.h"

#include <array>

#include "GameLoop.h"

#include "Input.h"
#include "TimerPrecision.h"
#include "renderer/D3D11Renderer.h"
#include "renderer/Camera.h";

namespace game
{
	struct InputAction
	{
		void (*onInputActive)();
	};

	// frame limiter
	const bool frameLimiterEnabled = true;
	const int32_t frameLimit = 40;
	const int32_t frameTimeTarget = 1000000 / frameLimit;

	// logging
	const int32_t sampleSize = 500; // number of frames until statistics are averaged and logged
	
	int32_t sampleIndex = 0;
	std::array<int32_t, sampleSize> renderTimeSamples;
	std::array<int32_t, sampleSize> sleepTimeSamples;

	// input actions
	const float cameraMoveSpeed = 0.3f;
	const float cameraTurnSpeed = 0.07f;

	std::map<std::string, InputAction> actions;
	std::map<std::string, std::string> actionsToKeys;

	void registerDigitalAction(std::string& actionName, InputAction inputAction) {
		actions[actionName] = inputAction;
	}

	struct LoggingSettings
	{
		bool fps = true;                       // average fps
		bool renderTime = true;                // average render time [usec]
		bool sleepTime = frameLimiterEnabled;              // average sleep time [usec]
		bool offsetTime = frameLimiterEnabled;             // average difference between target & actual frame time [usec]
		bool offsetTimePermille = frameLimiterEnabled;     // offsetTime as percentage of target frame time [permille]
		bool offsetPositivePermille = frameLimiterEnabled; // like offsetTimePermille, but only for frame times > target time
		bool offsetNegativePermille = frameLimiterEnabled; // like offsetTimePermille, but only for frame times < target time
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

	void registerCameraActions() {
		registerDigitalAction(std::string("CAMERA_MOVE_FORWARDS"), { []() -> void {
			renderer::camera::moveCameraDepth(cameraMoveSpeed);
		} });
		registerDigitalAction(std::string("CAMERA_MOVE_BACKWARDS"), { []() -> void {
			renderer::camera::moveCameraDepth(cameraMoveSpeed * -1);
		} });
		registerDigitalAction(std::string("CAMERA_MOVE_LEFT"), { []() -> void {
			renderer::camera::moveCameraHorizontal(cameraMoveSpeed * -1);
		} });
		registerDigitalAction(std::string("CAMERA_MOVE_RIGHT"), { []() -> void {
			renderer::camera::moveCameraHorizontal(cameraMoveSpeed);
		} });
		registerDigitalAction(std::string("CAMERA_MOVE_UP"), { []() -> void {
			renderer::camera::moveCameraVertical(cameraMoveSpeed);
		} });
		registerDigitalAction(std::string("CAMERA_MOVE_DOWN"), { []() -> void {
			renderer::camera::moveCameraVertical(cameraMoveSpeed * -1);
		} });
		registerDigitalAction(std::string("CAMERA_TURN_LEFT"), { []() -> void {
			renderer::camera::turnCameraHorizontal(cameraTurnSpeed * -1);
		} });
		registerDigitalAction(std::string("CAMERA_TURN_RIGHT"), { []() -> void {
			renderer::camera::turnCameraHorizontal(cameraTurnSpeed);
		} });
	}

	void setDefaultInputMap() {
		actionsToKeys["CAMERA_MOVE_FORWARDS"] = "R";
		actionsToKeys["CAMERA_MOVE_BACKWARDS"] = "F";

		actionsToKeys["CAMERA_MOVE_LEFT"] = "A";
		actionsToKeys["CAMERA_MOVE_RIGHT"] = "D";
		actionsToKeys["CAMERA_MOVE_UP"] = "W";
		actionsToKeys["CAMERA_MOVE_DOWN"] = "S";

		actionsToKeys["CAMERA_TURN_LEFT"] = "Q";
		actionsToKeys["CAMERA_TURN_RIGHT"] = "E";
	}

	void init(HWND hWnd)
	{
		enablePreciseTimerResolution();
		initMicrosleep();

		registerCameraActions();
		setDefaultInputMap();

		renderer::initD3D(hWnd);
		LOG(DEBUG) << "Statistics in microseconds";
	}

	void renderAndSleep()
	{
		const auto startTimeRender = std::chrono::high_resolution_clock::now();

		for (auto& [actionname, keyname] : actionsToKeys) {
			// if key is pressed
			bool isPressed = game::input::isKeyUsed(keyname);
			if (isPressed) {
				// if action exists
				auto actionIt = actions.find(actionname);
				if (actionIt != actions.end()) {
					auto action = actionIt->second;
					action.onInputActive();
				}
			}
		}

		renderer::updateObjects();
		renderer::renderFrame();

		const auto endTimeRender = std::chrono::high_resolution_clock::now();
		const auto timeRender = endTimeRender - startTimeRender;
		const auto timeRenderMicros = static_cast<int32_t> (timeRender / std::chrono::microseconds(1));
		renderTimeSamples[sampleIndex] = timeRenderMicros;
		
		if (frameLimiterEnabled)
		{
			const int32_t sleepTarget = frameTimeTarget - timeRenderMicros;
			if (sleepTarget > 0)
			{
				// sleep_for can wake up about once every 1,4ms
				//std::this_thread::sleep_for(std::chrono::microseconds(100));

				// microsleep can wake up about once every 0,5ms
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