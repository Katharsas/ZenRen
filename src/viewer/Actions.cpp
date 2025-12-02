#include "stdafx.h"

#include "Actions.h"

#include "Input.h"
#include "Util.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "render/Gui.h"
#include <imgui.h>

namespace viewer
{
	using ::viewer::input::InputDevice;
	using ::viewer::input::ButtonId;
	using ::viewer::input::AxisId;

	struct ActionDigitalWhileActive {
		const std::string actionId;
		void (*onInputActive)();
	};
	struct ActionDigitalOnToggle {
		const std::string actionId;
		void (*onInputActive)(bool isActive);
	};

	struct AnalogInputState {
		int32_t axisDelta = 0;
	};
	struct ActionAnalog {
		const std::string actionId;
		void (*onInputActive)(AnalogInputState inputState);
		AnalogInputState inputState;
	};

	struct InputAction
	{
		void (*onInputActive)(AnalogInputState inputState);
	};

	// input actions
	const float cameraMoveSpeed = 7.f;
	const float cameraMoveSpeedFast = cameraMoveSpeed * 12;

	const float cameraTurnSpeed = 2.f;
	const float cameraSensitivity = 0.003f;

	const std::string ACTION_GUI_SEPARATOR = "---";
	std::vector<std::string> actionsForGui;

	std::map<const std::string, const viewer::input::ButtonId> actionsToDigitalInput;
	std::map<const std::string, const viewer::input::AxisId> actionsToAnalogInput;

	float deltaTime;
	bool isMouseVisible = true;

	bool isActive(const std::string& actionId) {
		auto& turnEnabledKey = actionsToDigitalInput.at(actionId);
		bool isEnabled = viewer::input::isKeyUsed(turnEnabledKey);
		return isEnabled;
	}

	float getCameraMoveSpeed() {
		if (isActive("CAMERA_MOVE_FAST")) {
			return deltaTime * cameraMoveSpeedFast;
		}
		else {
			return deltaTime * cameraMoveSpeed;
		}
	}

	std::array actionsDigitalWhileActive{
		ActionDigitalWhileActive { "CAMERA_MOVE_FORWARDS", []() -> void {
			render::camera::moveCameraDepth(getCameraMoveSpeed());
		} },
		ActionDigitalWhileActive { "CAMERA_MOVE_BACKWARDS", []() -> void {
			render::camera::moveCameraDepth(getCameraMoveSpeed() * -1);
		} },
		ActionDigitalWhileActive { "CAMERA_MOVE_LEFT", []() -> void {
			render::camera::moveCameraHorizontal(getCameraMoveSpeed() * -1);
		} },
		ActionDigitalWhileActive { "CAMERA_MOVE_RIGHT", []() -> void {
			render::camera::moveCameraHorizontal(getCameraMoveSpeed());
		} },
		ActionDigitalWhileActive { "CAMERA_MOVE_UP", []() -> void {
			render::camera::moveCameraVertical(getCameraMoveSpeed());
		} },
		ActionDigitalWhileActive { "CAMERA_MOVE_DOWN", []() -> void {
			render::camera::moveCameraVertical(getCameraMoveSpeed() * -1);
		} },
		ActionDigitalWhileActive { "CAMERA_TURN_LEFT", []() -> void {
			render::camera::turnCameraHorizontal(deltaTime * cameraTurnSpeed * -1);
		} },
		ActionDigitalWhileActive { "CAMERA_TURN_RIGHT", []() -> void {
			render::camera::turnCameraHorizontal(deltaTime * cameraTurnSpeed);
		} },
	};
	std::array actionsDigitalOnToggle{
		ActionDigitalOnToggle { "CAMERA_TURN_ANALOG", [](bool isActive) -> void {
			viewer::input::toggleFixedHiddenCursorMode(isActive);
		} },
		ActionDigitalOnToggle { "GUI_TOGGLE_VISIBLE", [](bool isActive) -> void {
			if (!isActive) {
				render::gui::onToggleVisible();
			}
		} },
		ActionDigitalOnToggle { "RELOAD_SHADERS", [](bool isActive) -> void {
;			if (!isActive) {
				render::reloadShaders();
			}
		} },
	};
	std::array actionsAnalog{
		ActionAnalog {
			"CAMERA_TURN_AXIS_X", ([](AnalogInputState state) -> void {
				if (isActive("CAMERA_TURN_ANALOG")) {
					render::camera::turnCameraHorizontal(cameraSensitivity * state.axisDelta);
				}
			})
		},
		ActionAnalog {
			"CAMERA_TURN_AXIS_Y", ([](AnalogInputState state) -> void {
				if (isActive("CAMERA_TURN_ANALOG")) {
					render::camera::turnCameraVertical(cameraSensitivity * state.axisDelta);
				}
			})
		},
	};

	void bindActionToKey(const std::string& actionName) {
		actionsForGui.push_back(actionName);
	}
	void bindActionToKey(const std::string& actionName, const ButtonId& buttonId) {
		actionsForGui.push_back(actionName);
		actionsToDigitalInput.insert({ actionName, buttonId });
	}
	void bindActionToAxis(const std::string& actionName, const AxisId& axisId) {
		actionsForGui.push_back(actionName);
		actionsToAnalogInput.insert({ actionName, axisId });
	}

	void setDefaultInputMap() {
		bindActionToKey(ACTION_GUI_SEPARATOR + "General");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("GUI_TOGGLE_VISIBLE", { InputDevice::KEYBOARD, VK_F11 });
		bindActionToKey("RELOAD_SHADERS", { InputDevice::KEYBOARD, VK_F9 });// F10 and F12 are hooked by VS during debugging which sucks
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey(ACTION_GUI_SEPARATOR + "Camera");
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_MOVE_FORWARDS", input::button('W'));
		bindActionToKey("CAMERA_MOVE_BACKWARDS", input::button('S'));
		bindActionToKey("CAMERA_MOVE_LEFT", input::button('A'));
		bindActionToKey("CAMERA_MOVE_RIGHT", input::button('D'));
		bindActionToKey("CAMERA_MOVE_UP", input::button('E'));
		bindActionToKey("CAMERA_MOVE_DOWN", input::button('C'));
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_MOVE_FAST", { InputDevice::KEYBOARD, VK_SHIFT });
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_TURN_LEFT", input::button('Q'));
		bindActionToKey("CAMERA_TURN_RIGHT", input::button('R'));
		bindActionToKey(ACTION_GUI_SEPARATOR);
		bindActionToKey("CAMERA_TURN_ANALOG", { InputDevice::KEYBOARD, VK_SPACE });
		bindActionToAxis("CAMERA_TURN_AXIS_X", { InputDevice::MOUSE, MK_AXIS_X });
		bindActionToAxis("CAMERA_TURN_AXIS_Y", { InputDevice::MOUSE, MK_AXIS_Y });
	}

	void initActions() {
		setDefaultInputMap();

		render::gui::addWindow("Keybinds", {
			[]() -> void {
				if (ImGui::BeginTable("keybinds", 2, ImGuiTableFlags_SizingFixedFit)) {
					for (const auto& actionname : actionsForGui) {
						if (actionname == ACTION_GUI_SEPARATOR) {
							ImGui::TableNextColumn();
								ImGui::NewLine();
								ImGui::TableNextColumn();
						}
						else if (util::startsWith(actionname, ACTION_GUI_SEPARATOR)) {
							auto separatorName = actionname.substr(ACTION_GUI_SEPARATOR.length(), actionname.length() - ACTION_GUI_SEPARATOR.length());
								ImGui::TableNextColumn();
								ImGui::Text(separatorName.c_str());
								ImGui::TableNextColumn();
						}
						else {
							auto buttonIt = actionsToDigitalInput.find(actionname);
							auto axisIt = actionsToAnalogInput.find(actionname);
							bool buttonExists = buttonIt != actionsToDigitalInput.end();
							bool axisExits = axisIt != actionsToAnalogInput.end();
							if (!buttonExists && !axisExits) {
								break;
							}

							ImGui::TableNextColumn();
							ImGui::Indent();
							ImGui::Text(actionname.c_str());
							ImGui::Unindent();
							ImGui::TableNextColumn();

							if (buttonExists) {
								const auto name = input::getName(buttonIt->second);
								ImGui::Text(name.c_str());
							} else {
								const auto& name = input::getName(axisIt->second);
								ImGui::Text(name.c_str());
							}
						}
					}
					ImGui::EndTable();
				}
			}
		});
	}

	void processUserInput(float deltaTime) {
		viewer::deltaTime = deltaTime;

		// check if input is mapped to a button and activate while pressed
		for (auto& action : actionsDigitalWhileActive) {
			auto inputIt = actionsToDigitalInput.find(action.actionId);
			if (inputIt != actionsToDigitalInput.end()) {
				auto& inputId = inputIt->second;
				bool isPressed = viewer::input::isKeyUsed(inputId);
				if (isPressed) {
					action.onInputActive();
				}
			}
		}
		// check if input is mapped to a button and activate on button down/release
		for (auto& action : actionsDigitalOnToggle) {
			auto inputIt = actionsToDigitalInput.find(action.actionId);
			if (inputIt != actionsToDigitalInput.end()) {
				auto& inputId = inputIt->second;
				// this makes it impossible to use single key for two toggle actions,
				// could be changed by resetting all buttons only after evaluating all toggle actions
				bool hasChanged = viewer::input::hasKeyChangedAndReset(inputId);
				if (hasChanged) {
					bool isPressed = viewer::input::isKeyUsed(inputId);
					action.onInputActive(isPressed);
				}
			}
		}
		// analog inputs are always evaluated if mapped
		for (auto& action : actionsAnalog) {
			auto inputIt = actionsToAnalogInput.find(action.actionId);
			if (inputIt != actionsToAnalogInput.end()) {
				auto& inputId = inputIt->second;
				action.inputState.axisDelta = viewer::input::getAxisDelta(inputId);
				action.onInputActive(action.inputState);
			}
		}
	}
}