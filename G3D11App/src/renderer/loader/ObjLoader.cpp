#include "stdafx.h"
#include "ObjLoader.h"

#include <filesystem>

#include "../../Util.h"
#include "../RenderUtil.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

namespace renderer::loader {

	using namespace DirectX;
	using ::util::asciiToLowercase;
	using ::util::getOrCreate;

	typedef WORLD_VERTEX VERTEX;

	std::unordered_map<Material, std::vector<VERTEX>> loadObj(const std::string& inputFile) {

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
			auto filename = path.filename().u8string();
			//LOG(DEBUG) << "  Texture: " << filename;
		}

		LOG(INFO) << "Number of materials: " << materials.size();
		LOG(INFO) << "Number of textures: " << textureCount;
		LOG(INFO) << "Number of shapes: " << shapes.size();

		const float scale = 1.0f;

		std::unordered_map<Material, std::vector<WORLD_VERTEX>> matsToVertices;
		int32_t faceCount = 0;
		int32_t faceSkippedCount = 0;

		for (size_t shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {

			size_t indexOffset = 0;
			for (size_t faceIndex = 0; faceIndex < shapes[shapeIndex].mesh.num_face_vertices.size(); faceIndex++) {
				size_t vertexCount = size_t(shapes[shapeIndex].mesh.num_face_vertices[faceIndex]);
				if (vertexCount != 3) {
					LOG(FATAL) << "Only exactly 3 vertices per face are allowed!";
				}
				std::array<VERTEX, 3> vertices;
				bool hasNormals = false;
				for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {

					tinyobj::index_t idx = shapes[shapeIndex].mesh.indices[indexOffset + vertexIndex];
					VERTEX vertex;

					tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
					vertex.pos = { vx * scale, vy * scale, vz * -scale };// FLIPPED Z -> flip axis

					if (idx.normal_index >= 0) {
						tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
						vertex.normal = { nx, ny, -nz };// FLIPPED Z -> flip axis
						hasNormals = true;
					}

					if (idx.texcoord_index >= 0) {
						tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
						vertex.uvDiffuse = { tx, ty };
					}
					else {
						vertex.uvDiffuse = { 0, 0 };
					}

					vertex.colorLightmap = { 1, 1, 1, 1 };
					vertex.uvLightmap = { 0, 0 };

					vertices.at(2 - vertexIndex) = vertex;// FLIPPED Z -> flip faces
				}
				if (!hasNormals) {
					// calculate face normals
					XMVECTOR v1 = XMVectorSet(vertices[0].pos.x, vertices[0].pos.y, vertices[0].pos.z, 0);
					XMVECTOR v2 = XMVectorSet(vertices[1].pos.x, vertices[1].pos.y, vertices[1].pos.z, 0);
					XMVECTOR v3 = XMVectorSet(vertices[2].pos.x, vertices[2].pos.y, vertices[2].pos.z, 0);
					XMVECTOR normal = XMVector3Normalize(XMVector3Cross(v2 - v1, v3 - v1));
					VEC3 normalVec3 = { XMVectorGetX(normal), XMVectorGetY(normal), XMVectorGetZ(normal) };
					for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
						vertices.at(vertexIndex).normal = normalVec3;
					}
				}

				// per-face material
				const uint32_t materialIndex = shapes[shapeIndex].mesh.material_ids[faceIndex];

				auto& texname = materials[materialIndex].diffuse_texname;
				if (!texname.empty()) {
					auto texFilepath = std::filesystem::path(texname);
					auto texFilename = texFilepath.filename().u8string();
					asciiToLowercase(texFilename);

					auto& matVertices = getOrCreate(matsToVertices, { texFilename });
					matVertices.insert(matVertices.end(), vertices.begin(), vertices.end());

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
