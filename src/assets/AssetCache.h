#pragma once

#include "render/Common.h"
#include "assets/AssetFinder.h"

#include "zenkit/MultiResolutionMesh.hh"
#include "zenkit/Model.hh"

namespace assets
{
	template <typename L> concept HasLoad =
		requires(L loadable) {
			{ loadable.load((zenkit::Read*)nullptr) } -> std::same_as<void>;
	};

	template<HasLoad T>
	std::optional<const T*> getOrParse(const std::string& assetName);

	render::TexId getTexId(const std::string& texName);
	std::string_view getTexName(render::TexId texId);
}

