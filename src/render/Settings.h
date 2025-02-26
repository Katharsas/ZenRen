#pragma once

namespace render
{
	enum ShaderMode {
		Default,
		Solid,
		Diffuse,
		Normals,
		Light_Sun,
		Light_Static
	};

	struct ShaderSettings {
		ShaderMode mode;
	};

	struct RenderSettings {
		float viewDistance = 1000;

		bool wireframe = false;
		bool reverseZ = true;

		bool anisotropicFilter = true;
		uint32_t anisotropicLevel = 16;

		ShaderSettings shader = {
			ShaderMode::Default,
		};

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

		float debugFloat1 = 1.f;
		float debugFloat2 = 1.f;
	};
}

