#include "stdafx.h"
#include "PipelineWorld.h"

#include "Camera.h"
#include "Texture.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

namespace renderer::world {

	bool RENDER_FLAT = false;

	struct CbPerObject
	{
		XMMATRIX worldViewProjection;
		FLOAT textureColorFactor;
	};

	struct World {
		Mesh mesh;
		XMMATRIX transform;
	};

	ID3D11Buffer* cbPerObjectBuffer;

	World world;

	Texture* texture;
	ID3D11SamplerState* linearSamplerState = nullptr;

	void loadTestObj(D3d d3d) {
		std::string inputfile = "cube.obj";
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
		
		const float scale = 1.0f;

		std::vector<POS_UV> meshData;
		meshData.reserve(attrib.vertices.size());

		for (size_t shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {
			
			size_t indexOffset = 0;
			for (size_t faceIndex = 0; faceIndex < shapes[shapeIndex].mesh.num_face_vertices.size(); faceIndex++) {
				size_t vertexCount = size_t(shapes[shapeIndex].mesh.num_face_vertices[faceIndex]);

				for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {

					tinyobj::index_t idx = shapes[shapeIndex].mesh.indices[indexOffset + vertexIndex];
					POS_UV pos_uv;

					tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

					pos_uv.pos = { vx * scale, vy * scale, vz * scale };

					if (idx.normal_index >= 0) {
						tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
					}

					if (idx.texcoord_index >= 0) {
						tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

						pos_uv.uv = { tx, ty };
					}
					else {
						pos_uv.uv = { 0, 0 };
					}

					meshData.push_back(pos_uv);
				}
				indexOffset += vertexCount;
			}
		}

		LOG(DEBUG) << "World vertex count: " << meshData.size();

		{
			world.mesh.vertexCount = meshData.size();
			D3D11_BUFFER_DESC bufferDesc;
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));

			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(POS_UV) * meshData.size();

			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = meshData.data();
			d3d.device->CreateBuffer(&bufferDesc, &initialData, &(world.mesh.vertexBuffer));
		}
	}

	void updateObjects()
	{
		world.transform = XMMatrixIdentity();
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
		D3D11_BUFFER_DESC cbbd;
		ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

		cbbd.Usage = D3D11_USAGE_DEFAULT;// TODO this should probably be dynamic, see https://www.gamedev.net/forums/topic/673486-difference-between-d3d11-usage-default-and-d3d11-usage-dynamic/
		cbbd.ByteWidth = sizeof(CbPerObject);
		cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbbd.CPUAccessFlags = 0;
		cbbd.MiscFlags = 0;

		d3d.device->CreateBuffer(&cbbd, nullptr, &cbPerObjectBuffer);
	}

	void draw(D3d d3d, ShaderManager* shaders)
	{
		// set the shader objects avtive
		Shader* shader = shaders->getShader("flatBasicColorTexShader");
		d3d.deviceContext->VSSetShader(shader->getVertexShader(), 0, 0);
		d3d.deviceContext->IASetInputLayout(shader->getVertexLayout());
		d3d.deviceContext->PSSetShader(shader->getPixelShader(), 0, 0);

		auto* resourceView = texture->GetResourceView();
		d3d.deviceContext->PSSetShaderResources(0, 1, &resourceView);
		d3d.deviceContext->PSSetSamplers(0, 1, &linearSamplerState);

		UINT stride = sizeof(POS_UV);
		UINT offset = 0;
		d3d.deviceContext->IASetVertexBuffers(0, 1, &(world.mesh.vertexBuffer), &stride, &offset);

		CbPerObject cbPerObject = { camera::calculateWorldViewProjection(world.transform), RENDER_FLAT ? 0 : 1.0f };
		d3d.deviceContext->UpdateSubresource(cbPerObjectBuffer, 0, nullptr, &cbPerObject, 0, 0);
		d3d.deviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
		d3d.deviceContext->PSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

		d3d.deviceContext->Draw(world.mesh.vertexCount, 0);
	}

	void clean()
	{
		cbPerObjectBuffer->Release();
		linearSamplerState->Release();
		delete texture;

		world.mesh.release();
	}
}