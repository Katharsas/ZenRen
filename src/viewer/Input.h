#pragma once

#include "Win.h"

namespace viewer::input
{
	#define MK_AXIS_X 0x8000
	#define MK_AXIS_Y 0x8001

	enum class InputDevice {
		KEYBOARD,
		MOUSE
	};
	struct ButtonId {
		const InputDevice device;
		const uint32_t code;
	};
	struct AxisId {
		const InputDevice device;
		const uint32_t code;
	};

	bool isKeyUsed(const ButtonId& keyId);
	bool hasKeyChangedAndReset(const ButtonId& keyId);
	void setRdpCompatMode(bool isActive);
	void toggleFixedHiddenCursorMode(bool isActive);
	int32_t getAxisDelta(const AxisId& axisId);

	ButtonId button(wchar_t letter);

	const std::string getName(const ButtonId& id);
	const std::string& getName(const AxisId& id);

	bool onWindowMessage(UINT message, WPARAM keycode, LPARAM extendedParameters);
	bool setKeyboardInput(LPARAM lParam);
}

