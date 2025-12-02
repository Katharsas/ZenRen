#pragma once

#include "Dx.h"

namespace render::gui
{
	struct GuiConstants {
		float panelWidth = 200;
		float panelPadding = 6;
		float elementWidth = panelWidth - (2 * panelPadding) - 12;
		float inputFloatWidth = 45;

		GuiConstants scale(float scaleFactor) const {
			return {
				.panelWidth = scaleFactor * panelWidth,
				.panelPadding = scaleFactor * panelPadding,
				.elementWidth = scaleFactor * elementWidth,
				.inputFloatWidth = scaleFactor * inputFloatWidth
			};
		}
	};

	struct GuiComponent
	{
		const std::function<void()> buildGui;
	};

	GuiConstants& constants();
	void onToggleVisible();
	void addWindow(const std::string& windowName, GuiComponent guiComponent);
	void addSettings(const std::string& groupName, GuiComponent guiComponent);
	void addInfo(const std::string& groupName, GuiComponent guiComponent);
	void init(WindowHandle hWnd, D3d d3d);
	void onWindowDpiChange(WindowHandle hWnd);
	void draw();
	void clean();
}

