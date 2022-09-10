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
}