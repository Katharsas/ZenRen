#pragma once

namespace game::input {
	bool isKeyUsed(const std::string& keyname);
	bool onKeyUsed(UINT message, WPARAM keycode, LPARAM extendedParameters);
	bool setKeyboardInput(LPARAM lParam);
}

