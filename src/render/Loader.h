#pragma once

#include "Common.h"

#include "zenkit/Mmap.hh" 
#include "zenkit/Material.hh"

namespace assets
{
	struct LoadDebugFlags {
		bool validateMeshData = true;

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
	enum class VisualType : std::uint8_t {
		DECAL = 0,
		MESH = 1,
		MULTI_RESOLUTION_MESH = 2,
		PARTICLE_EFFECT = 3,
		AI_CAMERA = 4,
		MODEL = 5,
		MORPH_MESH = 6,
		UNKNOWN = 7,
	};

	struct VobLighting
	{
		DirectX::XMVECTOR direction;// pre-inverted
		COLOR color;
		bool receiveLightSun;
	};

	struct Decal {
		VEC2 quad_size;
		UV uv_offset;
		bool two_sided;

		zenkit::AlphaFunction alpha;
		// TODO shouldn't this be part of TexInfo? Does it work identical to model materials?
		// Seems very likely that a texture is always used with the same alphafunc, so just pass it to shader when setting texture?
	};

	struct StaticInstance {
		VisualType type;
		std::string visual_name;
		DirectX::XMMATRIX transform;
		std::array<DirectX::XMVECTOR, 2> bbox;// pos_min, pos_max
		VobLighting lighting;
		std::optional<Decal> decal = std::nullopt;
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
		MatToChunksToVertsBasic worldMesh;
		MatToChunksToVertsBasic staticMeshes;
		//VERTEX_DATA_BY_MAT dynamicMeshes;
		MatToChunksToVertsBlend staticMeshesBlend;
		std::vector<FileData> worldMeshLightmaps;
	};
}