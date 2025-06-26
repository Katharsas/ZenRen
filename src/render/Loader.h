#pragma once

#include "Util.h"
#include "render/Common.h"

#include "zenkit/Mmap.hh" 
#include "zenkit/Material.hh"

namespace assets
{
	struct LoadDebugFlags {
		bool loadVobs = true;
		bool validateMeshData = true;

		bool disableVobToLightVisibilityRayChecks = false;
		bool disableVertexIndices = false;

		bool vobsTint = false;
		bool vobsTintUnlit = false;
		bool staticLights = false;
		bool staticLightRays = false;
		bool staticLightTintUnreached = false;
	};

	namespace FormatsSource
	{
		const ::util::FileExt __3DS = ::util::FileExt::create(".3DS");
		const ::util::FileExt ASC = ::util::FileExt::create(".ASC");
		const ::util::FileExt MDS = ::util::FileExt::create(".MDS");
		const ::util::FileExt PFX = ::util::FileExt::create(".PFX");
		const ::util::FileExt MMS = ::util::FileExt::create(".MMS");
		const ::util::FileExt TGA = ::util::FileExt::create(".TGA");
		const ::util::FileExt PNG = ::util::FileExt::create(".PNG");

		const std::initializer_list ALL = { __3DS, ASC, MDS, PFX, MMS, TGA, PNG };
	}

	// see https://github.com/Try/OpenGothic/wiki/Gothic-file-formats
	// MRM = single mesh (multiresolution)
	// MDM = single mesh
	// MDH = model hierarchy
	// MDL = model hierarchy including all used single meshes
	namespace FormatsCompiled
	{
		const ::util::FileExt ZEN = ::util::FileExt::create(".ZEN");
		const ::util::FileExt TEX = ::util::FileExt::create(".TEX");
		const ::util::FileExt MRM = ::util::FileExt::create(".MRM");
		const ::util::FileExt MDM = ::util::FileExt::create(".MDM");
		const ::util::FileExt MDH = ::util::FileExt::create(".MDH");
		const ::util::FileExt MDL = ::util::FileExt::create(".MDL");
		const ::util::FileExt MAN = ::util::FileExt::create(".MAN");

		const std::initializer_list ALL = { ZEN, TEX, MRM, MDM, MDH, MDL, MAN };
	}
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
		Color color;
		bool receiveLightSun;
	};

	struct Decal {
		Vec2 quad_size;
		Uv uv_offset;
		bool two_sided;

		zenkit::AlphaFunction alpha;
		// TODO shouldn't this be part of TexInfo? Does it work identical to model materials?
		// Seems very likely that a texture is always used with the same alphafunc, so just pass it to shader when setting texture?
	};

	struct StaticInstance {
		VisualType type;
		std::string visual_name;
		DirectX::XMMATRIX transform;
		std::array<DirectX::XMVECTOR, 2> bbox;// pos_min, pos_max, TODO rename to bboxGlobal to make clear it is already transformed to world space
		VobLighting lighting;
		std::optional<Decal> decal = std::nullopt;
	};

	struct Light {
		// TODO falloff and type surely play a role
		Vec3 pos;
		bool isStatic;
		Color color;
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
		bool isG2 = false;
		bool isOutdoorLevel = false;
		grid::Grid chunkGrid;
		MatToChunksToVertsBasic worldMesh;
		MatToChunksToVertsBasic staticMeshes;
		std::vector<FileData> worldMeshLightmaps;
	};
}