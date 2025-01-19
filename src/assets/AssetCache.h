#pragma once

#include "render/Common.h"
#include "assets/AssetFinder.h"

#include "zenkit/MultiResolutionMesh.hh"
#include "zenkit/Model.hh"

namespace assets
{
	std::optional<const zenkit::MultiResolutionMesh*> getOrParseMrm(const std::string& assetName);
	std::optional<const zenkit::ModelHierarchy*> getOrParseMdh(const std::string& assetName);
	std::optional<const zenkit::ModelMesh*> getOrParseMdm(const std::string& assetName);
	std::optional<const zenkit::Model*> getOrParseMdl(const std::string& assetName);

	render::TexId getTexId(const std::string& texName);
	std::string_view getTexName(render::TexId texId);
}

