#include "stdafx.h"
#include "RenderUtil.h"

namespace render::util {

	void dumpVerts(const std::string& matName, const std::vector<VERTEX_POS>& vertPos, const std::vector<NORMAL_UV_LUV>& vertOther) {
		std::ostringstream buffer;
		buffer << matName << std::endl;
		for (uint32_t i = 0; i < vertPos.size(); i++) {
			buffer << "    " << vertPos[i] << "  " << vertOther[i] << std::endl;
		}
		LOG(INFO) << buffer.str();
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO getVramInfo(IDXGIAdapter3* adapter) {
		DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
		adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
		return videoMemoryInfo;
	}

	std::string getVramUsage(IDXGIAdapter3* adapter) {
		const auto vramInfo = getVramInfo(adapter);
		uint32_t currentMb = vramInfo.CurrentUsage / 1024 / 1024;
		uint32_t totalMb = vramInfo.Budget / 1024 / 1024;
		uint32_t percentage = ((float) currentMb / totalMb) * 100;
		std::ostringstream buffer;
		buffer << percentage << " %% (" << currentMb << " / " << totalMb << " MB)";
		return buffer.str();
	}
}