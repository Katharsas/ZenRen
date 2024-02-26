#include "stdafx.h"
#include "ObjLoader.h"

#include <filesystem>

#include "MeshUtil.h"
#include "../../Util.h"
#include "../RenderUtil.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

namespace renderer::loader {

	using namespace DirectX;
	using ::util::asciiToLower;
	using ::util::getOrCreate;

	std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV> loadObj(const std::string& inputFile) {

		tinyobj::ObjReaderConfig reader_config;
		reader_config.triangulate = true;

		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile(inputFile, reader_config)) {
			if (!reader.Error().empty()) {
				LOG(FATAL) << "TinyObjReader: " << reader.Error();
			}
			else {
				LOG(FATAL) << "TinyObjReader: Unknown Error!";
			}
		}

		if (!reader.Warning().empty()) {
			LOG(WARNING) << "TinyObjReader: " << reader.Warning();
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		auto& materials = reader.GetMaterials();

		int textureCount = 0;
		for (size_t matIndex = 0; matIndex < materials.size(); matIndex++) {
			auto& texname = materials[matIndex].diffuse_texname;
			if (!texname.empty()) {
				textureCount++;
			}
			auto path = std::filesystem::path(texname);
			auto filename = ::util::toString(path.filename());
			//LOG(DEBUG) << "  Texture: " << filename;
		}

		LOG(INFO) << "Number of materials: " << materials.size();
		LOG(INFO) << "Number of textures: " << textureCount;
		LOG(INFO) << "Number of shapes: " << shapes.size();

		const float scale = 1.0f;

		std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV> matsToVertices;
		int32_t faceCount = 0;
		int32_t faceSkippedCount = 0;

		for (size_t shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {

			size_t indexOffset = 0;
			for (size_t faceIndex = 0; faceIndex < shapes[shapeIndex].mesh.num_face_vertices.size(); faceIndex++) {
				size_t vertexCount = size_t(shapes[shapeIndex].mesh.num_face_vertices[faceIndex]);
				if (vertexCount != 3) {
					LOG(FATAL) << "Only exactly 3 vertices per face are allowed!";
				}
				std::array<POS, 3> facePos;
				std::array<NORMAL_UV_LUV, 3> faceOther;
				bool hasNormals = false;
				for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {

					tinyobj::index_t idx = shapes[shapeIndex].mesh.indices[indexOffset + vertexIndex];
					POS pos;
					NORMAL_UV_LUV other;

					tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
					pos = { vx * scale, vy * scale, vz * -scale };// FLIPPED Z -> flip axis

					if (idx.normal_index >= 0) {
						tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
						other.normal = { nx, ny, -nz };// FLIPPED Z -> flip axis
						hasNormals = true;
					}

					if (idx.texcoord_index >= 0) {
						tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
						other.uvDiffuse = { tx, ty };
					}
					else {
						other.uvDiffuse = { 0, 0 };
					}

					//vertex.colorLightmap = { 1, 1, 1, 1 };
					other.uvLightmap = { 0, 0 };

					// Wavefront .obj verts are stored counter-clockwise, so we switch to clockwise
					facePos.at(2 - vertexIndex) = pos;
					faceOther.at(2 - vertexIndex) = other;
				}
				if (!hasNormals) {
					// calculate face normals
					std::array posXm = { toXM4Pos(facePos[0]), toXM4Pos(facePos[1]), toXM4Pos(facePos[2]) };
					XMVECTOR normal = XMVector3Normalize(XMVector3Cross(posXm[1] - posXm[0], posXm[2] - posXm[0]));
					VEC3 normalVec3 = toVec3(normal);
					for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
						faceOther.at(vertexIndex).normal = normalVec3;
					}
				}

				// per-face material
				const uint32_t materialIndex = shapes[shapeIndex].mesh.material_ids[faceIndex];

				auto& texture = materials[materialIndex].diffuse_texname;
				if (!texture.empty()) {
					const auto texFilepath = std::filesystem::path(texture);
					const auto texFilename = ::util::toString(texFilepath.filename());

					Material material = { ::util::asciiToLower(texFilename) };
					insert(matsToVertices, material, facePos, faceOther);
					faceCount++;
				}
				else {
					faceSkippedCount++;
				}
				indexOffset += vertexCount;
			}
		}

		LOG(DEBUG) << "World triangle count: " << faceCount;
		LOG(DEBUG) << "World triangle skipped count (no texture assigned): " << faceSkippedCount;

		return matsToVertices;
	}

}
