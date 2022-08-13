#include "stdafx.h"
#include "Input.h"
#include "Util.h"

namespace game::input {

    std::map<const std::string, bool> keyState;

    bool isKeyUsed(const std::string& keyname) {
        return keyState[keyname];
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
            LOG(DEBUG) << "Key released: " << keyname;

            keyState[keyname] = false;

            return true;
        }
        else if (message == WM_KEYDOWN) {
            auto keyname = getKeyname(extendedParameters);
            LOG(DEBUG) << "Key pressed: " << keyname;

            keyState[keyname] = true;

            return true;
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
