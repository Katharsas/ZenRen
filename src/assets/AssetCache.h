#pragma once

#include "render/Common.h"
#include "assets/AssetFinder.h"

#include "zenkit/MultiResolutionMesh.hh"
#include "zenkit/Model.hh"

#include "zenload/zTypes.h"
#include "zenload/zCProgMeshProto.h"
#include "zenload/zCModelMeshLib.h"

namespace assets
{
	struct MeshData {
		ZenLib::ZenLoad::zCProgMeshProto mesh;
	};

	struct MeshLibData {
		ZenLib::ZenLoad::zCModelMeshLib meshLib;
	};

	std::optional<const zenkit::MultiResolutionMesh*> getOrParseMrm(const std::string& assetName);
	std::optional<const zenkit::ModelHierarchy*> getOrParseMdh(const std::string& assetName);
	std::optional<const zenkit::ModelMesh*> getOrParseMdm(const std::string& assetName);
	std::optional<const zenkit::Model*> getOrParseMdl(const std::string& assetName);

	const MeshData& getOrParseMesh(const Vfs& vfs, const std::string& meshName);
	const MeshLibData& getOrParseMeshLib(const Vfs& vfs, const std::string& meshName);

	render::TexId getTexId(const std::string& texName);
	std::string_view getTexName(render::TexId texId);
}

