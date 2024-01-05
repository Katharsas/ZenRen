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
	struct InputState {
		int16_t axisDelta = 0;
	};
	struct ActionDigital {
		const std::string actionId;
		void (*onInputActive)();
	};
	struct ActionAnalog {
		const std::string actionId;
		void (*onInputActive)(InputState inputState);
		InputState inputState;
	};

	struct InputAction
	{
		void (*onInputActive)(InputState inputState);
	};

	struct Settings {
		// frame limiter
		bool frameLimiterEnabled = true;
		int32_t frameLimit = 40;
	};

	InputState inputState;
	Settings settings;

	// input actions
	const float cameraMoveSpeed = 0.34f;
	const float cameraMoveSpeedFast = cameraMoveSpeed * 8;

	const float cameraTurnSpeed = 0.05f;
	const float cameraSensitivity = 0.01f;

	std::string ACTION_GUI_SEPARATOR = "---";
	std::vector<std::string> actionsForGui;

	std::map<const std::string, const game::input::InputId> actionsToDigitalInput;
	std::map<const std::string, const game::input::InputId> actionsToAnalogInput;

	bool isActive(const std::string& actionId) {
		auto& turnEnabledKey = actionsToDigitalInput.at(actionId);
		return game::input::isKeyUsed(turnEnabledKey);
	}

	float getCameraMoveSpeed() {
		if (isActive("CAMERA_MOVE_FAST")) {
			return cameraMoveSpeedFast;
		}
		else {
			return cameraMoveSpeed;
		}
	}

	std::array actionsDigital{
		ActionDigital { "CAMERA_MOVE_FORWARDS", []() -> void {
			renderer::camera::moveCameraDepth(getCameraMoveSpeed());
		} },
		ActionDigital { "CAMERA_MOVE_BACKWARDS", []() -> void {
			renderer::camera::moveCameraDepth(getCameraMoveSpeed() * -1);
		} },
		ActionDigital { "CAMERA_MOVE_LEFT", []() -> void {
			renderer::camera::moveCameraHorizontal(getCameraMoveSpeed() * -1);
		} },
		ActionDigital { "CAMERA_MOVE_RIGHT", []() -> void {
			renderer::camera::moveCameraHorizontal(getCameraMoveSpeed());
		} },
		ActionDigital { "CAMERA_MOVE_UP", []() -> void {
			renderer::camera::moveCameraVertical(getCameraMoveSpeed());
		} },
		ActionDigital { "CAMERA_MOVE_DOWN", []() -> void {
			renderer::camera::moveCameraVertical(getCameraMoveSpeed() * -1);
		} },
		ActionDigital { "CAMERA_TURN_LEFT", []() -> void {
			renderer::camera::turnCameraHorizontal(cameraTurnSpeed * -1);
		} },
		ActionDigital { "CAMERA_TURN_RIGHT", []() -> void {
			renderer::camera::turnCameraHorizontal(cameraTurnSpeed);
		} },
	};
	std::array actionsAnalog {
		ActionAnalog {
			"CAMERA_TURN_AXIS_X", ([](InputState state) -> void {
				if (isActive("CAMERA_TURN_ANALOG")) {
					renderer::camera::turnCameraHorizontal(cameraSensitivity * state.axisDelta);
				}
			})
		},
		ActionAnalog {
			"CAMERA_TURN_AXIS_Y", ([](InputState state) -> void {
				if (isActive("CAMERA_TURN_ANALOG")) {
					renderer::camera::turnCameraVertical(cameraSensitivity * state.axisDelta);
				}
			})
		},
	};

	void bindActionToKey(const std::string& actionName, const std::string& key = std::string()) {
		actionsForGui.push_back(actionName);
		if (!key.empty()) {
			actionsToDigitalInput.insert({ actionName, { input::InputDevice::KEYBOARD, key } });
		}
	}
	void bindActionToAxis(const std::string& actionName, const std::string& key = std::string()) {
		actionsForGui.push_back(actionName);
		if (!key.empty()) {
			actionsToAnalogInput.insert({ actionName, { input::InputDevice::MOUSE, key } });
		}
	}

	void setDefaultInputMap() {
		bindActionToKey(ACTION_GUI_SEPARATOR + "Camera");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_MOVE_FORWARDS", "W");
		bindActionToKey("CAMERA_MOVE_BACKWARDS", "S");
		bindActionToKey("CAMERA_MOVE_LEFT", "A");
		bindActionToKey("CAMERA_MOVE_RIGHT", "D");
		bindActionToKey("CAMERA_MOVE_UP", "E");
		bindActionToKey("CAMERA_MOVE_DOWN", "C");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_MOVE_FAST", "SHIFT");
		//bindActionToKey(ACTION_GUI_SEPARATOR);
		//bindActionToKey("CAMERA_TURN_LEFT", "Q");
		//bindActionToKey("CAMERA_TURN_RIGHT", "E");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_TURN_ANALOG", "CTRL");
		bindActionToAxis("CAMERA_TURN_AXIS_X", "Mouse X-Axis");
		bindActionToAxis("CAMERA_TURN_AXIS_Y", "Mouse Y-Axis");
	}

	void init(HWND hWnd)
	{
		enablePreciseTimerResolution();
		initMicrosleep();

		//registerCameraActions();
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
							auto it = actionsToDigitalInput.find(actionname);
							if (it == actionsToDigitalInput.end()) {
								it = actionsToAnalogInput.find(actionname);
								if (it == actionsToAnalogInput.end()) {
									break;
								}
							}
							ImGui::TableNextColumn();
							ImGui::Indent();
							ImGui::Text(actionname.c_str());
							ImGui::Unindent();
							ImGui::TableNextColumn();
							auto& inputId = it->second;
							ImGui::Text(inputId.name.c_str());
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

		// check if input is mapped to a button and activate if pressed
		for (auto& action : actionsDigital) {
			auto inputIt = actionsToDigitalInput.find(action.actionId);
			if (inputIt != actionsToDigitalInput.end()) {
				auto& inputId = inputIt->second;
				bool isPressed = game::input::isKeyUsed(inputId);
				if (isPressed) {
					action.onInputActive();
				}
			}
		}
		// analog inputs are always evaluated if mapped
		for (auto& action : actionsAnalog) {
			auto inputIt = actionsToAnalogInput.find(action.actionId);
			if (inputIt != actionsToAnalogInput.end()) {
				auto& inputId = inputIt->second;
				action.inputState.axisDelta = game::input::getAxisDelta(inputId.name);
				action.onInputActive(action.inputState);
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

	void onWindowResized(uint32_t width, uint32_t height)
	{
		LOG(DEBUG) << "Resizing game window! Width: " << width << ", Height: " << height;
		renderer::onWindowResize(width, height);
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