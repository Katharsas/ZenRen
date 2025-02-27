#pragma once

namespace render
{
	enum ShaderMode {
		Full,
		Solid,
		Diffuse,
		Normals,
		Depth,
		Light_Sun,
		Light_Static
	};

	struct ShaderSettings {
		ShaderMode mode;
	};

	struct RenderSettings {
		float viewDistance = 1000;
		bool distanceFog = true;
		// fog settings about match 300% view distance in G1 
		float distanceFogStart = 120;
		float distanceFogEnd = 600;
		float distanceFogSkyFactor = 3;

		bool wireframe = false;
		bool reverseZ = true;

		bool anisotropicFilter = true;
		uint32_t anisotropicLevel = 16;

		ShaderSettings shader = {
			ShaderMode::Full,
		};
		bool outputAlpha = true;

		float resolutionScaling = 1.0f;
		bool resolutionUpscaleSmooth = true;
		bool downsampling = false;// does not work currently, TODO implement mipmap generation for linear BB

		uint32_t multisampleCount = 4;
		bool multisampleTransparency = true;
		bool distantAlphaDensityFix = true;

		bool depthPrepass = false;

		bool skyTexBlur = true;

		bool passesOpaque = true;
		bool passesBlend = true;

		float contrast = 1.0f;
		float brightness = 0.001f;
		float gamma = 0.92f;

		float debugFloat1 = 0.f;
		float debugFloat2 = 0.f;
	};
}

