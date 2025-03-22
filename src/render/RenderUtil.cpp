#include "stdafx.h"
#include "RenderUtil.h"

#include <Psapi.h>

namespace render::util {

	void initViewport(BufferSize& size, D3D11_VIEWPORT* viewport)
	{
		ZeroMemory(viewport, sizeof(D3D11_VIEWPORT));

		viewport->TopLeftX = 0.f;
		viewport->TopLeftY = 0.f;
		viewport->Width = (float)size.width;
		viewport->Height = (float)size.height;
		viewport->MinDepth = 0.f;
		viewport->MaxDepth = 1.f;
	}

	void dumpVerts(const std::string& matName, const std::vector<VertexPos>& vertPos, const std::vector<NORMAL_UV_LUV>& vertOther)
	{
		std::ostringstream buffer;
		buffer << matName << '\n';
		for (uint32_t i = 0; i < vertPos.size(); i++) {
			buffer << "    " << vertPos[i] << "  " << vertOther[i] << '\n';
		}
		LOG(INFO) << buffer.str();
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO getVramInfo(IDXGIAdapter3* adapter)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
		adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
		return videoMemoryInfo;
	}

	std::string getVramUsage(IDXGIAdapter3* adapter)
	{
		const auto vramInfo = getVramInfo(adapter);

		uint32_t currentMb = (uint32_t) (vramInfo.CurrentUsage / 1024 / 1024);
		uint32_t totalMb = (uint32_t) (vramInfo.Budget / 1024 / 1024);
		uint32_t percentage = (uint32_t) (((float) currentMb / totalMb) * 100);

		std::ostringstream buffer;
		buffer << percentage << " %% (" << currentMb << " / " << totalMb << " MB)";
		return buffer.str();
	}

	std::string getMemUsage()
	{
		PROCESS_MEMORY_COUNTERS_EX2 memCounters = {};
		GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&memCounters, sizeof(memCounters));

		// Private working set: How much the process is actually using (non-shared)
		uint32_t privateWorkingSetMb = (uint32_t)memCounters.PrivateWorkingSetSize / 1024 / 1024;

		// Private usage / commit: Entire disk-swappable (comitted) virtual memory space (non-shared):
		// - includes memory mapped files
		// - includes VRAM for 64bit binaries only
		float privateUsageGb = memCounters.PrivateUsage / 1024 / 1024 / 1024.f;

		std::ostringstream buffer;
		buffer << privateWorkingSetMb << " MB (" << std::format("{:.2f}", privateUsageGb) << " GB Commit)";
		return buffer.str();
	}
}