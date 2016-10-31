#pragma once

#include <iostream>

void InitConsole();
void CleanConsole();

struct ColorCoutSink {

	// mix colors, FOREGROUND_INTENSITY makes it brighter
	// Similar thing for Linux: https://github.com/KjellKod/g3sinks/blob/master/snippets/ColorCoutSink.hpp
	enum FG_Color {
		PURPLE = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
		RED = FOREGROUND_RED | FOREGROUND_INTENSITY,
		GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
		WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
	};

	FG_Color GetColor(const LEVELS level) const {
		if (level.value == WARNING.value) { return RED; }
		if (level.value == DEBUG.value) { return GREEN; }
		if (g3::internal::wasFatal(level)) { return PURPLE; }

		return WHITE;
	}

	void ReceiveLogMessage(g3::LogMessageMover logEntry) {
		auto level = logEntry.get()._level;
		auto FgColor = GetColor(level);

		// http://www.programmersheaven.com/discussion/83443/how-to-do-colors-in-c-console
		HANDLE hconsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hconsole, FgColor | 0 << 4);

		//std::cout << logEntry.get()._message << std::endl;
		std::cout << logEntry.get().toString();
	}
};