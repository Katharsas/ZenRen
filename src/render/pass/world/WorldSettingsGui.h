#pragma once

#include "render/pass/world/WorldSettings.h"
#include "render/basic/Grid.h"

namespace render::pass::world::gui
{
	void init(
		WorldSettings& worldSettings,
		uint32_t& maxDrawCalls,
		const std::function<void()>& notifyTimeOfDayChange,		
		const std::function<std::pair<GridPos, GridPos>()>& getGridMinMaxPos);
}