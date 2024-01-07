#include "stdafx.h"

#include "Actions.h"

#include "Input.h"
#include "renderer/Camera.h"
#include "renderer/Gui.h"
#include "imgui/imgui.h"

namespace game {

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

	// input actions
	const float cameraMoveSpeed = 7.f;
	const float cameraMoveSpeedFast = cameraMoveSpeed * 10;

	const float cameraTurnSpeed = 2.f;
	const float cameraSensitivity = 0.2f;

	const std::string ACTION_GUI_SEPARATOR = "---";
	std::vector<std::string> actionsForGui;

	std::map<const std::string, const game::input::InputId> actionsToDigitalInput;
	std::map<const std::string, const game::input::InputId> actionsToAnalogInput;

	float deltaTime;

	bool isActive(const std::string& actionId) {
		auto& turnEnabledKey = actionsToDigitalInput.at(actionId);
		return game::input::isKeyUsed(turnEnabledKey);
	}

	float getCameraMoveSpeed() {
		if (isActive("CAMERA_MOVE_FAST")) {
			return deltaTime * cameraMoveSpeedFast;
		}
		else {
			return deltaTime * cameraMoveSpeed;
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
			renderer::camera::turnCameraHorizontal(deltaTime * cameraTurnSpeed * -1);
		} },
		ActionDigital { "CAMERA_TURN_RIGHT", []() -> void {
			renderer::camera::turnCameraHorizontal(deltaTime * cameraTurnSpeed);
		} },
	};
	std::array actionsAnalog{
		ActionAnalog {
			"CAMERA_TURN_AXIS_X", ([](InputState state) -> void {
				if (isActive("CAMERA_TURN_ANALOG")) {
					renderer::camera::turnCameraHorizontal(deltaTime * cameraSensitivity * state.axisDelta);
				}
			})
		},
		ActionAnalog {
			"CAMERA_TURN_AXIS_Y", ([](InputState state) -> void {
				if (isActive("CAMERA_TURN_ANALOG")) {
					renderer::camera::turnCameraVertical(deltaTime * cameraSensitivity * state.axisDelta);
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
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_TURN_LEFT", "Q");
		bindActionToKey("CAMERA_TURN_RIGHT", "R");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_TURN_ANALOG", "SPACE");
		bindActionToAxis("CAMERA_TURN_AXIS_X", "Mouse X-Axis");
		bindActionToAxis("CAMERA_TURN_AXIS_Y", "Mouse Y-Axis");
	}

	void initActions() {
		setDefaultInputMap();

		renderer::addWindow("Keybinds", {
			[]() -> void {
				if (ImGui::BeginTable("keybinds", 2, ImGuiTableFlags_SizingFixedFit)) {
					for (const auto& actionname : actionsForGui) {
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
	}

	void processUserInput(float deltaTime) {
		game::deltaTime = deltaTime;

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
	}
}