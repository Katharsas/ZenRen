#include "stdafx.h"

#include "GameLoop.h"

#include "Input.h"
#include "TimerPrecision.h"
#include "PerfStats.h"
#include "renderer/Gui.h"
#include "renderer/D3D11Renderer.h"
#include "renderer/Camera.h"
#include "imgui/imgui.h"

namespace game
{
	struct InputAction
	{
		void (*onInputActive)();
	};

	struct Settings {
		// frame limiter
		bool frameLimiterEnabled = true;
		int32_t frameLimit = 40;
	};

	Settings settings;

	// input actions
	const float cameraMoveSpeed = 0.3f;
	const float cameraTurnSpeed = 0.05f;

	std::map<std::string, InputAction> actions;
	std::map<std::string, std::string> actionsToKeys;

	void registerDigitalAction(std::string& actionName, InputAction inputAction) {
		actions[actionName] = inputAction;
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

		renderer::addGui("Graphics", {
			[]() -> void {
				ImGui::Checkbox("Framelimiter", &settings.frameLimiterEnabled);
				ImGui::InputInt("Frame Limit", &settings.frameLimit, 0);
				if (settings.frameLimit < 10) {
					settings.frameLimit = 10;
				}
			}
		});

		renderer::initD3D(hWnd);
		LOG(DEBUG) << "Statistics in microseconds";
	}

	void renderAndSleep()
	{
		// START RENDER
		stats::FrameSample frameTime;
		frameTime.renderStart();

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

		frameTime.renderEndSleepStart();
		int32_t frameTimeTarget = 0;
		
		if (settings.frameLimiterEnabled)
		{
			frameTimeTarget = 1000000 / settings.frameLimit;
			const int32_t sleepTarget = frameTimeTarget - frameTime.renderTimeMicros;
			if (sleepTarget > 0)
			{
				// sleep_for can wake up about once every 1,4ms
				//std::this_thread::sleep_for(std::chrono::microseconds(100));

				// microsleep can wake up about once every 0,5ms
				microsleep(sleepTarget + 470); // make sure we never get too short frametimes
			}
		}

		frameTime.sleepEnd();
		stats::addFrameSample(frameTime, frameTimeTarget);
	}



	void execute()
	{
		renderAndSleep();
	}

	void cleanup()
	{
		renderer::cleanD3D();
		cleanupMicrosleep();
		disablePreciseTimerResolution();
	}
}