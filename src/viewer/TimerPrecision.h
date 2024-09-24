#pragma once

namespace viewer
{
	void enablePreciseTimerResolution();
	void disablePreciseTimerResolution();

	void initMicrosleep();
	void cleanupMicrosleep();
	void microsleep(int64_t usec);
}
