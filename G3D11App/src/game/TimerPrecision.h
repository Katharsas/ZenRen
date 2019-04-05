#pragma once

namespace game
{
	void enablePreciseTimerResolution();
	void disablePreciseTimerResolution();

	void initMicrosleep();
	void cleanupMicrosleep();
	void microsleep(int64_t usec);
}
