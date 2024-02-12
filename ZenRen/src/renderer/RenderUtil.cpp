#include "stdafx.h"
#include "RenderUtil.h"

namespace renderer::util {

	void dumpVerts(std::string& matName, std::vector<POS_NORMAL_UV>& verts) {
		std::ostringstream buffer;
		buffer << matName << std::endl;
		for (auto& vert : verts) {
			buffer << "    " << vert << std::endl;
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