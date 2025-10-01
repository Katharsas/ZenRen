#pragma once

namespace render::pass::world
{
	// windows things :(
#undef NEAR
#undef FAR
	enum LodMode {
		FULL,
		NEAR,
		FAR,
	};

	struct WorldSettings {
		float timeOfDay = 0.f;// range 0 to 1 (midday to midday)
		float timeOfDayChangeSpeed = 0.f;// good showcase value: 0.03

		bool drawWorld = true;
		bool drawStaticObjects = true;
		bool drawSky = true;

		bool chunkedRendering = true;

		bool renderCloseFirst = true;
		float renderCloseRadius = 70;

		LodMode lodDisplayMode = LodMode::FULL;
		bool enableLod = true;
		float lodRadius = 110;
		bool enablePerPixelLod = true;
		bool enableLodDithering = true;
		float lodRadiusDitherWidth = 20;

		bool enableFrustumCulling = true;
		bool updateFrustumCulling = true;

		bool debugWorldShaderEnabled = false;
		//float debugDrawVertAmount = 1.f;

		bool debugSingleDrawEnabled = false;
		int32_t debugSingleDrawIndex = 0;

		bool chunkFilterXEnabled = false;
		bool chunkFilterYEnabled = false;
		int16_t chunkFilterX = 0;
		int16_t chunkFilterY = 0;
	};
}