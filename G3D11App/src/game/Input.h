#pragma once

namespace game::input {

	enum class InputDevice {
		KEYBOARD,
		MOUSE
	};
	struct InputId {
		const InputDevice device;
		const std::string name;
	};

	bool isKeyUsed(const InputId& keyId);
	int16_t getAxisDelta(const std::string& axisname);

	bool onKeyUsed(UINT message, WPARAM keycode, LPARAM extendedParameters);
	bool setKeyboardInput(LPARAM lParam);
}

