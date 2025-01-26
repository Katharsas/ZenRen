#pragma once

#include "Common.h"

#include "zenkit/Mmap.hh" 

namespace assets
{
	struct LoadDebugFlags {
		bool vobsTint = false;
		bool staticLights = false;
		bool staticLightRays = false;
		bool staticLightTintUnreached = false;
	};
}
//namespace zenkit
//{
//	class Mmap;
//}
namespace render
{
	struct VobLighting
	{
		DirectX::XMVECTOR direction;// pre-inverted
		COLOR color;
		bool receiveLightSun;
	};

	struct StaticInstance {
		std::string meshName;// TODO rename to modelName or visualAsset or visualFile
		DirectX::XMMATRIX transform;
		std::array<DirectX::XMVECTOR, 2> bbox;// pos_min, pos_max
		VobLighting lighting;
	};

	struct Light {
		// TODO falloff and type surely play a role
		VEC3 pos;
		bool isStatic;
		COLOR color;
		float range;
	};

	struct FileData {
	public:
		// TODO wrap mmap in unique pointer
		// TODO move Mmap.h include to .cpp (should be possible when behind unique_ptr but does not work for some reason)

		// not copyable, must be moved
		FileData(const FileData&) = delete;
		FileData(FileData&&) noexcept;
		~FileData() noexcept;

		// backed by some static buffer
		FileData(const std::string& name, const std::byte * data, uint64_t size);

		// backed by mmap which is kept open for this objects lifetime
		FileData(const std::string& name, const std::byte * data, uint64_t size,
			zenkit::Mmap&& mmap);

		// backed by heap buffer which is kept for this objects lifetime
		FileData(const std::string& name, const std::byte * data, uint64_t size,
			std::unique_ptr<std::vector<std::uint8_t>>&& buffer);

		// backed by heap buffer with unknown lifetime
		FileData(const std::string& name, const std::byte* data, uint64_t size,
			const std::shared_ptr<std::vector<std::uint8_t>>& buffer);

		const std::string name;
		const std::byte * const data = nullptr;
		const uint64_t size = 0;
	private:
		// std::unique_ptr (and also zenkit::Mmap) is not copyable, making this struct also not copyable
		// both are automatically deleted when this struct goes out of scope or is deleted
	
		// TODO consider using variant?
		std::optional<zenkit::Mmap> mmap = std::nullopt;
		std::optional<std::unique_ptr<std::vector<std::uint8_t>>> buffer = std::nullopt;
		std::optional<std::shared_ptr<std::vector<std::uint8_t>>> bufferShared = std::nullopt;
	};

	struct RenderData {
		bool isOutdoorLevel = false;
		VERT_CHUNKS_BY_MAT worldMesh;
		VERT_CHUNKS_BY_MAT staticMeshes;
		VERTEX_DATA_BY_MAT dynamicMeshes;
		std::vector<FileData> worldMeshLightmaps;
	};
}