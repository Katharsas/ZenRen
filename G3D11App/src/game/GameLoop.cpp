#include "stdafx.h"

#include "GameLoop.h"

#include "Input.h"
#include "TimerPrecision.h"
#include "PerfStats.h"
#include "renderer/Gui.h"
#include "renderer/Renderer.h"
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

	std::string ACTION_GUI_SEPARATOR = "---";
	std::vector<std::string> actionsForGui;

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

	void bindActionToKey(const std::string& actionName, const std::string& key = std::string()) {
		actionsForGui.push_back(actionName);
		if (actionName != ACTION_GUI_SEPARATOR) {
			actionsToKeys[actionName] = key;
		}
	}

	void setDefaultInputMap() {
		bindActionToKey(ACTION_GUI_SEPARATOR + "Camera");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_MOVE_FORWARDS", "R");
		bindActionToKey("CAMERA_MOVE_BACKWARDS", "F");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_MOVE_LEFT", "A");
		bindActionToKey("CAMERA_MOVE_RIGHT", "D");
		bindActionToKey("CAMERA_MOVE_UP", "W");
		bindActionToKey("CAMERA_MOVE_DOWN", "S");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_TURN_LEFT", "Q");
		bindActionToKey("CAMERA_TURN_RIGHT", "E");
	}

	void init(HWND hWnd)
	{
		enablePreciseTimerResolution();
		initMicrosleep();

		registerCameraActions();
		setDefaultInputMap();

		renderer::addSettings("Graphics", {
			[]() -> void {
				ImGui::Checkbox("Framelimiter", &settings.frameLimiterEnabled);
				ImGui::PushItemWidth(40);
				ImGui::InputInt("Frame Limit", &settings.frameLimit, 0);
				if (settings.frameLimit < 10) {
					settings.frameLimit = 10;
				}
				ImGui::PopItemWidth();
			}
		});
		renderer::addWindow("Keybinds", {
			[]() -> void {
				if (ImGui::BeginTable("keybinds", 2, ImGuiTableFlags_SizingFixedFit)) {
					for (auto& actionname : actionsForGui) {
						if (actionname == ACTION_GUI_SEPARATOR) {
							ImGui::TableNextColumn();
								ImGui::NewLine();
								ImGui::TableNextColumn();
						}
						else if (actionname._Starts_with(ACTION_GUI_SEPARATOR)) {
							auto separatorName = actionname.substr(ACTION_GUI_SEPARATOR.length(), actionname.length() - ACTION_GUI_SEPARATOR.length());
								ImGui::TableNextColumn();
								ImGui::Text(separatorName.c_str());
								ImGui::TableNextColumn();
						}
						else {
							ImGui::TableNextColumn();
							ImGui::Indent();
							auto& keyname = actionsToKeys[actionname];
							ImGui::Text(actionname.c_str());
							ImGui::Unindent();
							ImGui::TableNextColumn();
							ImGui::Text(keyname.c_str());
						}
					}
					ImGui::EndTable();
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

		renderer::update();
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