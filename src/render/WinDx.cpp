#include "stdafx.h"
#include "WinDx.h"

namespace render {

	void release(IUnknown* dx11object) {
		if (dx11object != nullptr) {
			dx11object->Release();
		}
	}

	void release(const std::vector<IUnknown*>& dx11objects) {
		for (auto object : dx11objects) {
			release(object);
		}
	}

	void release(ID3D11Buffer* dx11object) {
		release((IUnknown*)dx11object);
	}

	void release(ID3D11ShaderResourceView* dx11object) {
		release((IUnknown*)dx11object);
	}
}