#include "stdafx.h"
#include "Input.h"
#include "Util.h"

namespace game::input {

    bool rdpCompatMode = true;
    bool debugLogInputs = false;

    std::map<const uint32_t, bool> keyboardChanged;
    std::map<const uint32_t, bool> keyboardState;
    std::map<const uint32_t, bool> mouseState;

    struct MousePos {
        uint32_t x = 0;
        uint32_t y = 0;
    };
    MousePos mousePosLast;
    bool fixedCursorMode = false;

    const UINT mouseButtonDownMessages[] = { WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_XBUTTONDOWN };
    /*const UINT mouseButtonUpMessages[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP};*/

    const std::map<const uint32_t, const std::string> mouseButttonNames = {
        { MK_LBUTTON, "Mouse Left" },
        { MK_RBUTTON, "Mouse Right" },
        { MK_MBUTTON, "Mouse Middle" },
        { MK_XBUTTON1, "Mouse 4" },
        { MK_XBUTTON2, "Mouse 5" },
    };

    const std::map<const WPARAM, const uint32_t> mouseButtonCodes = {
        { WM_LBUTTONDOWN, MK_LBUTTON },
        { WM_RBUTTONDOWN, MK_RBUTTON },
        { WM_MBUTTONDOWN, MK_MBUTTON },
    };
    const std::map<const WORD, const uint32_t> mouseExtraButtonCodes = {
        { XBUTTON1, MK_XBUTTON1 },
        { XBUTTON2, MK_XBUTTON2 },
    };

    std::map<const uint32_t, const std::string> axisNames = {
        { MK_AXIS_X, "Mouse X-Axis" },
        { MK_AXIS_Y, "Mouse Y-Axis" },
    };

    bool isKeyUsed(const ButtonId& keyId) {
        if (keyId.device == InputDevice::KEYBOARD) {
            return keyboardState[keyId.code];
        }
        else {
            return mouseState[keyId.code];
        }
    }

    bool hasKeyChangedAndReset(const ButtonId& keyId) {
        if (keyId.device == InputDevice::KEYBOARD) {
            bool hasChanged = keyboardChanged[keyId.code];
            keyboardChanged[keyId.code] = false;
            return hasChanged;
        }
        else {
            return false;// TODO
        }
    }

    void setRdpCompatMode(bool isActive) {
        rdpCompatMode = isActive;
    }

    void toggleFixedHiddenCursorMode(bool isActive) {
        // SetCursorPos does not work over RDP because RDP uses client mouse to hide lag, which ignores SetCursorPos on host
        if (!rdpCompatMode) {
            ShowCursor(!isActive);
            fixedCursorMode = isActive;
        }
    }

    int32_t getAxisDelta(const AxisId& axisId) {
        POINT point;
        GetCursorPos(&point);
        if (axisId.code == MK_AXIS_X) {
            int32_t delta = point.x - mousePosLast.x;
            if (fixedCursorMode) {
                SetCursorPos(mousePosLast.x, point.y);
            }
            else {
                mousePosLast.x = point.x;
            }
            return delta;
        }
        if (axisId.code == MK_AXIS_Y) {
            int32_t delta = point.y - mousePosLast.y;
            if (fixedCursorMode) {
                SetCursorPos(point.x, mousePosLast.y);
            }
            else {
                mousePosLast.y = point.y;
            }
            return delta;
        }
    }

    std::string getKeyname(const LPARAM scancode) {
        wchar_t keyname[32];
        GetKeyNameTextW(static_cast<LONG>(scancode), keyname, 32);
        std::wstring wstring(keyname);
        return util::wideToUtf8(wstring);
    }

    std::string getKeynameFromVK(const uint32_t virtualKeycode) {
        auto scancode = MapVirtualKeyW(virtualKeycode, MAPVK_VK_TO_VSC);
        auto lparam = scancode << 16 | 0x1;// move code into upper half, then set set repeat count to 1 (probably irrelevant)
        if (virtualKeycode >= VK_PRIOR && virtualKeycode <= VK_HELP) {
            lparam |= 0x01000000;// set extended key flag
        }
        return getKeyname(lparam);
    }

    uint32_t getVk(wchar_t keyboardChar) {
        short vk = VkKeyScanW(keyboardChar);
        uint8_t keyCode = vk; // returns the virtual-key code in the low-order byte and the shift state in the high-byte
        return keyCode;
    }

    ButtonId button(wchar_t keyboardChar) {
        return { InputDevice::KEYBOARD, getVk(keyboardChar) };
    }

    const std::string getName(const ButtonId& id) {
        // TODO we should cache the names
        return getKeynameFromVK(id.code);
    }
    const std::string& getName(const AxisId& id) {
        return axisNames.find(id.code)->second;
    }

    bool onWindowMessage(UINT message, WPARAM keycode, LPARAM extendedParameters) {
        if (message == WM_KEYUP) {
            keyboardState[keycode] = false;
            keyboardChanged[keycode] = true;
            if (debugLogInputs) {
                LOG(DEBUG) << "Key released: " << getKeyname(extendedParameters);
            }
            return true;
        }
        else if (message == WM_KEYDOWN) {
            // keydown can be sent multiple times before keyup happens
            if (!keyboardState[keycode]) {
                keyboardState[keycode] = true;
                keyboardChanged[keycode] = true;
                if (debugLogInputs) {
                    LOG(DEBUG) << "Key pressed: " << getKeyname(extendedParameters);
                }
            }
            return true;
        }
        {
            auto it = std::find(std::begin(mouseButtonDownMessages), std::end(mouseButtonDownMessages), message);
            if (it != std::end(mouseButtonDownMessages)) {

                uint32_t mousecode;
                if (message != WM_XBUTTONDOWN) {
                    mousecode = mouseButtonCodes.at(message);
                }
                else {
                    // parse high-order word of keycode to get actual button as documented by WM_XBUTTONDOWN
                    WORD upper_word = (uint16_t)(keycode >> 16);
                    mousecode = mouseExtraButtonCodes.at(upper_word);
                }
                if (debugLogInputs) {
                    LOG(DEBUG) << "Button pressed: " << mouseButttonNames.at(mousecode);
                }
                mouseState[mousecode] = true;
                return true;
            }
        }
        {
            /* this is pretty hacky: we use the DOWN code as mouse button key, because the UP code is always DOWN + 1 */
            auto messageDown = message - 1;
            auto it = std::find(std::begin(mouseButtonDownMessages), std::end(mouseButtonDownMessages), messageDown);
            if (it != std::end(mouseButtonDownMessages)) {

                uint32_t mousecode;
                if (message != WM_XBUTTONUP) {
                    mousecode = mouseButtonCodes.at(messageDown);
                }
                else {
                    // parse high-order word of keycode to get actual button as documented by WM_XBUTTONUP
                    WORD upperWord = (uint16_t)(keycode >> 16);
                    mousecode = mouseExtraButtonCodes.at(upperWord);
                }
                if (debugLogInputs) {
                    LOG(DEBUG) << "Button released: " << mouseButttonNames.at(mousecode);
                }

                mouseState[mousecode] = false;
                return true;
            }
        }
        return false;
    }
}
