#pragma once

#include <string>
#include "WinDx.h"
#include "Shader.h"

namespace render
{
	class ShaderManager
	{
	public:
		ShaderManager(D3d d3d);
		~ShaderManager();
		void reloadShaders(D3d d3d);
		Shader* getShader(const std::string& shaderName);
	private:
		std::unordered_map<std::string, Shader*> shaders;

		void insert(const std::string& shaderName, Shader* shader);

		void clearAll();

		const std::string folder = std::string("shaders");
		const std::string extension = std::string(".hlsl");

		const std::string filePath(const std::string shaderName);
	};

}