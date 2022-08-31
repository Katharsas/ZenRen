#include "stdafx.h"
#include "PipelineWorld.h"

#include <filesystem>

#include "Camera.h"
#include "Texture.h"
#include "../Util.h"
#include "TextureLoader.h";

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

using namespace DirectX;

namespace renderer::world {

	typedef POS_NORMAL_UV VERTEX;

	// Note: smallest type for constant buffer values is 32 bit; cannot use bool or uint_16 without packing

	enum ShaderOutputDirect : uint32_t {
		Solid = 0,
		Diffuse = 1,
		Normal = 2,
	};

	__declspec(align(16))
	struct CbGlobalSettings {
		float ambientLight;
		int32_t outputDirectEnabled;
		ShaderOutputDirect outputDirectType;
	};

	__declspec(align(16))
	struct CbPerObject {
		XMMATRIX worldViewMatrix;
		XMMATRIX worldViewMatrixInverseTranposed;
		XMMATRIX projectionMatrix;
	};

	struct World {
		std::vector<Mesh> meshes;
		XMMATRIX transform;
	};

	ID3D11Buffer* cbGlobalSettingsBuffer;
	ID3D11Buffer* cbPerObjectBuffer;

	float rot = 0.1f;
	float scale = 1.0f;
	float scaleDir = 1.0f;

	World world;

	Texture* texture;
	ID3D11SamplerState* linearSamplerState = nullptr;

	void loadTestObj(D3d d3d) {
		std::string inputfile = "world.obj";
		//std::string inputfile = "cube.obj";
		tinyobj::ObjReaderConfig reader_config;
		reader_config.triangulate = true;

		tinyobj::ObjReader reader;

		if (!reader.ParseFromFile(inputfile, reader_config)) {
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
			auto& path = std::filesystem::path(texname);
			auto& filename = path.filename().u8string();
			//LOG(DEBUG) << "  Texture: " << filename;
		}

		LOG(INFO) << "Number of materials: " << materials.size();
		LOG(INFO) << "Number of textures: " << textureCount;
		LOG(INFO) << "Number of shapes: " << shapes.size();
		
		const float scale = 1.0f;

		std::unordered_map<std::string, std::vector<VERTEX>> matsToVertices;
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
					vertex.pos = { vx * scale, vy * scale, vz *  - scale };// FLIPPED Z -> flip axis

					if (idx.normal_index >= 0) {
						tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
						vertex.normal = { nx, ny, - nz };// FLIPPED Z -> flip axis
						hasNormals = true;
					}

					if (idx.texcoord_index >= 0) {
						tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
						vertex.uv = { tx, ty };
					}
					else {
						vertex.uv = { 0, 0 };
					}

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
					auto& texFilepath = std::filesystem::path(texname);
					auto& texFilename = texFilepath.filename().u8string();
					util::asciiToLowercase(texFilename);

					auto& matVertices = util::getOrCreate(matsToVertices, texFilename);
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

		std::filesystem::path userDir = util::getUserFolderPath();
		std::filesystem::path texDir = userDir / "CLOUD/Eigene Projekte/Gothic Reloaded Mod/Textures";
		//std::filesystem::path texDir = "Level";
		loader::scanDirForTextures(texDir);

		int32_t meshCount = 0;
		int32_t maxMeshCount = 50;

		for (const auto& it : matsToVertices) {
			if (meshCount >= maxMeshCount) {
				break;
			}
			auto& filename = it.first;
			auto& vertices = it.second;
			if (!vertices.empty()) {
				auto& actualPath = loader::getTexturePathOrDefault(filename);
				LOG(DEBUG) << "  Texture: " << filename;

				Mesh mesh;
				{
					// vertex data
					mesh.vertexCount = vertices.size();
					D3D11_BUFFER_DESC bufferDesc;
					ZeroMemory(&bufferDesc, sizeof(bufferDesc));

					bufferDesc.Usage = D3D11_USAGE_DEFAULT;
					bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
					bufferDesc.ByteWidth = sizeof(VERTEX) * vertices.size();

					D3D11_SUBRESOURCE_DATA initialData;
					initialData.pSysMem = vertices.data();
					d3d.device->CreateBuffer(&bufferDesc, &initialData, &(mesh.vertexBuffer));
				}

				Texture* texture = new Texture(d3d, actualPath.u8string());

				mesh.baseColor = texture;
				world.meshes.push_back(mesh);

				meshCount++;
			}
		}

		LOG(DEBUG) << "World material/texture count: " << meshCount;
	}

	void updateObjects()
	{
		rot += .015f;
		if (rot > 6.28f) rot = 0.0f;

		//scale += (scaleDir * 0.01);
		if (scale > 3.0f) scaleDir = -1.0f;
		if (scale < 0.6f) scaleDir = 1.0f;

		float halfPi = 1.57f;
		world.transform = XMMatrixIdentity();
		XMVECTOR rotAxis = XMVectorSet(1.0f, 0, 0, 0);
		XMMATRIX rotation = XMMatrixRotationAxis(rotAxis, rot);
		XMMATRIX scaleM = XMMatrixScaling(1, scale, 1);
		world.transform = scaleM;
		//world.transform = world.transform * rotation;
	}

	void updateShaderSettings(D3d d3d, RenderSettings settings)
	{
		CbGlobalSettings cbGlobalSettings;
		cbGlobalSettings.ambientLight = settings.shader.ambientLight;
		cbGlobalSettings.outputDirectEnabled = settings.shader.mode != ShaderMode::Default;
		
		if (settings.shader.mode == ShaderMode::Diffuse) {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Diffuse;
		}
		else if (settings.shader.mode == ShaderMode::Normals) {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Normal;
		}
		else {
			cbGlobalSettings.outputDirectType = ShaderOutputDirect::Solid;
		}

		d3d.deviceContext->UpdateSubresource(cbGlobalSettingsBuffer, 0, nullptr, &cbGlobalSettings, 0, 0);
	}

	void initLinearSampler(D3d d3d, RenderSettings settings)
	{
		if (linearSamplerState != nullptr) {
			linearSamplerState->Release();
		}
		D3D11_SAMPLER_DESC samplerDesc = CD3D11_SAMPLER_DESC();
		ZeroMemory(&samplerDesc, sizeof(samplerDesc));

		if (settings.anisotropicFilter) {
			samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		} else {
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = settings.anisotropicLevel;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		d3d.device->CreateSamplerState(&samplerDesc, &linearSamplerState);
	}

	void initVertexIndexBuffers(D3d d3d, bool reverseZ)
	{
		loadTestObj(d3d);

		texture = new Texture(d3d, "BARRIERE.png");

		// select which primtive type we are using
		d3d.deviceContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void initConstantBufferPerObject(D3d d3d)
	{
		{
			// TODO rename or move or something
			D3D11_BUFFER_DESC bufferDesc;
			ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
			bufferDesc.ByteWidth = sizeof(CbGlobalSettings);
			bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;

			d3d.device->CreateBuffer(&bufferDesc, nullptr, &cbGlobalSettingsBuffer);
		}
		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

		bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		bufferDesc.ByteWidth = sizeof(CbPerObject);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;

		d3d.device->CreateBuffer(&bufferDesc, nullptr, &cbPerObjectBuffer);
	}

	void draw(D3d d3d, ShaderManager* shaders)
	{
		// set the shader objects avtive
		Shader* shader = shaders->getShader("flatBasicColorTexShader");
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		// constant buffer (object)
		const auto objectMatrices = camera::getWorldViewMatrix(world.transform);
		CbPerObject cbPerObject = {
			objectMatrices.worldView,
			objectMatrices.worldViewNormal,
			camera::getProjectionMatrix(),
		};
		d3d.deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(1, 1, &cbPerObjectBuffer);
		d3d.deviceContext->PSSetConstantBuffers(1, 1, &cbPerObjectBuffer);

		// constant buffer (settings)
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbGlobalSettingsBuffer);
		d3d.deviceContext->PSSetConstantBuffers(0, 1, &cbGlobalSettingsBuffer);

		// vertex buffer(s)
		UINT stride = sizeof(VERTEX);
		UINT offset = 0;

		for (auto& mesh : world.meshes) {
			auto* resourceView = mesh.baseColor->GetResourceView();
			d3d.deviceContext->PSSetShaderResources(0, 1, &resourceView);
			d3d.deviceContext->IASetVertexBuffers(0, 1, &(mesh.vertexBuffer), &stride, &offset);

			d3d.deviceContext->Draw(mesh.vertexCount, 0);
		}
	}

	void clean()
	{
		cbPerObjectBuffer->Release();
		cbGlobalSettingsBuffer->Release();
		linearSamplerState->Release();
		delete texture;

		for (auto& mesh : world.meshes) {
			mesh.release();
		}
	}
}