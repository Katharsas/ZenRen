#include "stdafx.h"
#include <windows.h>
#include <winternl.h>

#define STATUS_SUCCESS 0
#define STATUS_TIMER_RESOLUTION_NOT_SET 0xC0000245

const HINSTANCE LibraryNtdll = LoadLibrary(L"NTDLL.dll");
extern "C" NTSYSAPI NTSTATUS NTAPI NtSetTimerResolution(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);

namespace game
{
	// The requested resolution in 100 ns units:
	// Note: The supported resolutions can be obtained by a call to NtQueryTimerResolution()
	ULONG DesiredResolution = 2000;

	// The resolution that was actually obtained (may differ from requested resolution based on HW/OS support)
	ULONG CurrentResolution = 0;

	void enablePreciseTimerResolution()
	{
		if (!SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS))
		{
			LOG(WARNING) << "Failed to set process priority!";
		}
		if (NtSetTimerResolution(DesiredResolution, TRUE, &CurrentResolution) != STATUS_SUCCESS) {
			LOG(FATAL) << "Failed to enable precise timer resolution!";
		}
		LOG(DEBUG) << "Current timer resolution set to [100 ns units]: " << CurrentResolution;
		// should be about 0.5ms on modern platforms
	}

	void disablePreciseTimerResolution()
	{
		switch (NtSetTimerResolution(DesiredResolution, FALSE, &CurrentResolution)) {
		case STATUS_SUCCESS:
			LOG(DEBUG) << "The current timer resolution has returned to [100 ns units]:" << CurrentResolution;
			break;
		case STATUS_TIMER_RESOLUTION_NOT_SET:
			LOG(WARNING) << "The requested resolution was not set!";
			// the resolution can only return to a previous value by means of FALSE when the current resolution was set by this application
			break;
		default:
			LOG(FATAL) << "Failed to disable precise timer resolution!";
		}
		if (!SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS))
		{
			LOG(WARNING) << "Failed to reset process priority!";
		}
	}
}