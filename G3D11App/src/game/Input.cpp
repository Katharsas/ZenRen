#include "stdafx.h"
#include "Input.h"
#include "Util.h"

namespace game::input {

    std::map<const std::string, bool> keyState;

    struct MousePos {
        uint16_t x = 0;
        uint16_t y = 0;
    };
    MousePos mousePosCurrent;
    MousePos mousePosLast;

    const UINT mouseButtonDownMessages[] = { WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_XBUTTONDOWN };
    /*const UINT mouseButtonUpMessages[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP};*/

    const std::map<const WPARAM, const std::string> mouseButtonNames = {
        { WM_LBUTTONDOWN, "Mouse Left" },
        { WM_RBUTTONDOWN, "Mouse Right" },
        { WM_MBUTTONDOWN, "Mouse Middle" },
    };
    const std::map<const WORD, const std::string> mouseExtraButtonNames = {
        { XBUTTON1, "Mouse 4" },
        { XBUTTON2, "Mouse 5" },
    };

    bool isKeyUsed(const InputId& keyId) {
        return keyState[keyId.name];
    }

    int16_t getAxisDelta(const std::string& axisname) {
        if (axisname == "Mouse X-Axis") {
            int16_t delta = mousePosCurrent.x - mousePosLast.x;
            mousePosLast.x = mousePosCurrent.x;
            return delta;
        }
        if (axisname == "Mouse Y-Axis") {
            int16_t delta = mousePosCurrent.y - mousePosLast.y;
            mousePosLast.y = mousePosCurrent.y;
            return delta;
        }
    }

    std::string getKeyname(LPARAM extendedParameters) {
        wchar_t keyname[32];
        GetKeyNameTextW(static_cast<LONG>(extendedParameters), keyname, 32);
        std::wstring wstring(keyname);
        return util::wideToUtf8(wstring);
    }

    bool onKeyUsed(UINT message, WPARAM keycode, LPARAM extendedParameters) {
        if (message == WM_KEYUP) {
            auto keyname = getKeyname(extendedParameters);
            //LOG(DEBUG) << "Key released: " << keyname;

            keyState[keyname] = false;
            return true;
        }
        else if (message == WM_KEYDOWN) {
            auto keyname = getKeyname(extendedParameters);
            //LOG(DEBUG) << "Key pressed: " << keyname;

            keyState[keyname] = true;
            return true;
        }

        {
            auto it = std::find(std::begin(mouseButtonDownMessages), std::end(mouseButtonDownMessages), message);
            if (it != std::end(mouseButtonDownMessages)) {

                std::string keyname;
                if (message != WM_XBUTTONDOWN) {
                    keyname = mouseButtonNames.at(message);
                }
                else {
                    // parse high-order word of keycode to get actual button as documented by WM_XBUTTONDOWN
                    WORD upper_word = (uint16_t)(keycode >> 16);
                    keyname = mouseExtraButtonNames.at(upper_word);
                }
                //LOG(DEBUG) << "Button pressed: " << keyname;

                keyState[keyname] = true;
                return true;
            }
        }
        {
            /* this is pretty hacky: we use the DOWN code as mouse button key, because the UP code is always DOWN + 1 */
            auto messageDown = message - 1;
            auto it = std::find(std::begin(mouseButtonDownMessages), std::end(mouseButtonDownMessages), messageDown);
            if (it != std::end(mouseButtonDownMessages)) {

                std::string keyname;
                if (message != WM_XBUTTONUP) {
                    keyname = mouseButtonNames.at(messageDown);
                }
                else {
                    // parse high-order word of keycode to get actual button as documented by WM_XBUTTONUP
                    WORD upperWord = (uint16_t)(keycode >> 16);
                    keyname = mouseExtraButtonNames.at(upperWord);
                }
                //LOG(DEBUG) << "Button released: " << keyname;

                keyState[keyname] = false;
                return true;
            }

            // TODO there is a bug with pressin multiple mouse buttons and then releasing them
        }
        {
            if (message == WM_MOUSEMOVE) {
                WORD lowerWordX = (uint16_t)extendedParameters;
                WORD upperWordY = (uint16_t)(extendedParameters >> 16);

                //LOG(DEBUG) << "Mouse moved - X: " << lowerWordX << ", Y: " << upperWordY;
                mousePosCurrent.x = lowerWordX;
                mousePosCurrent.y = upperWordY;
                return true;
            }
        }
        return false;
    }

    bool setKeyboardInput(LPARAM lParam)
    {
        //RAWINPUT raw;

        //ZeroMemory(&raw, size);

        UINT size = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
        LPBYTE buffer = new BYTE[size];
        UINT actualSize = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER));
        if (buffer == NULL)
        {
            return false;
        }
        if (actualSize == -1) {
            auto error = GetLastError();
            std::string message = std::system_category().message(error);
            //LOG(WARNING) << "Error " << error << ": " << message;
            return false;
        }
        if (actualSize != size)
        {
            //LOG(WARNING) << "GetRawInputData does not return correct size! Expected: " << size << ", Actual: " << actualSize;
            return false;
        }
        RAWINPUT* raw = (RAWINPUT*)buffer;

        if (raw->header.dwType == RIM_TYPEKEYBOARD)
        {
            const RAWKEYBOARD& rawKeyboard = raw->data.keyboard;

            unsigned int scanCode = rawKeyboard.MakeCode;
            unsigned int flags = rawKeyboard.Flags;
            const bool E0 = ((flags & RI_KEY_E0) != 0);
            const bool E1 = ((flags & RI_KEY_E1) != 0);
            const bool KeyDown = !((flags & RI_KEY_BREAK) != 0);

            UINT key = (scanCode << 16) | (E0 << 24);
            wchar_t keyname[32];
            GetKeyNameTextW((LONG)key, keyname, 32);
            if (KeyDown)
            {
                LOG(DEBUG) << "KEY_DOWN: " << keyname;
                //myKey[keyname] = true;
            }
            else
            {
                LOG(DEBUG) << "KEY_UP: " << keyname;
                //myKey[keyname] = false;
            }
        }
        delete[] buffer;
        return true;
    }
}
