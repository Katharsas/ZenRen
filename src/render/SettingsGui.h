#pragma once

#include "Settings.h"

namespace render::gui::settings
{
	void init(RenderSettings& settings, const std::function<void()>& notifyGameSwitch);
}