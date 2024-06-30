#pragma once

namespace renderer
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

		float contrast = 1;
		float brightness = 0;
		float gamma = 0.87;
	};
}

