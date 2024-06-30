#pragma once

#include "vdfs/fileIndex.h"
#include "zenload/zTypes.h"
#include "zenload/zCProgMeshProto.h"
#include "zenload/zCModelMeshLib.h"

namespace assets
{
	struct MeshData {
		bool hasPacked;
		ZenLib::ZenLoad::zCProgMeshProto mesh;
		ZenLib::ZenLoad::PackedMesh packed;
	};

	struct MeshLibData {
		bool hasPacked;
		ZenLib::ZenLoad::zCModelMeshLib meshLib;
		std::vector<ZenLib::ZenLoad::PackedMesh> meshesPacked;
		std::vector<ZenLib::ZenLoad::PackedMesh> attachementsPacked;
	};

	const MeshData& getOrParseMesh(ZenLib::VDFS::FileIndex& vdf, std::string meshName, bool pack);
	const MeshLibData& getOrParseMeshLib(ZenLib::VDFS::FileIndex& vdf, std::string meshName, bool pack);
}

