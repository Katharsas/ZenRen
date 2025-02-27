#include "stdafx.h"
#include "Sky.h"

namespace render
{
	using std::array;

	namespace timekey
	{
		const float day = .0f;

		const float day_end = .25f;
		const float evening = .3f;
		const float night_start = .35f;

		const float night = .5f;

		const float night_end = .65f;
		const float dawn = .7f;
		const float day_start = .75f;
	}

	// interpolates from black to sky light color
	const float skyLightIntensityOutdoor = 200 / 256.0f;
	const float skyLightIntensityIndoor = 120 / 256.0f;

	struct SkyState {
		float timeKey;
		COLOR lightColor;// same as polyColor
		COLOR skyColor;// same as fogColor
	};

	COLOR fromSRGB(uint8_t r, uint8_t g, uint8_t b) {
		return COLOR(
			fromSRGB(r / 255.f),
			fromSRGB(g / 255.f),
			fromSRGB(b / 255.f),
			1.f);
	}

	float fromAlpha(uint8_t a) {
		return a / 255.f;
	}

	// G1 defines an additional texture brightness color (domeColor1) in SkyState, however it is hardcoded to not be used by base night layer (star texture)
	// and it is effectively set to 255 for any time other than night time, which means it only ever affects the night overlay (night clouds texture).
	// G1 hardcodes re-scaling of domeColor1 to range 128 - 255.
	const COLOR nightCloudColor = fromSRGB((255 + 55) / 2, (255 + 55) / 2, (255 + 155) / 2);

	// We assume that all state arrays have same length and use same timeKeys in same order as defined here.
	// In theory, we don't need to put time keys into every state struct, but this makes state definitions more readable.
	const array timeKeys = {
		timekey::day,
		timekey::day_end,
		timekey::evening,
		timekey::night_start,
		timekey::night,
		timekey::night_end,
		timekey::dawn,
		timekey::day_start,
	};

	const COLOR daySkyColor = fromSRGB(116, 89, 75);// G1 defines 4 different variants of this inside INI, use first for now

	const array skyStates = {
		SkyState { timekey::day,         fromSRGB(255, 250, 235), daySkyColor },
		SkyState { timekey::day_end,     fromSRGB(255, 250, 235), daySkyColor },
		SkyState { timekey::evening,     fromSRGB(255, 185, 170), fromSRGB(180,  75,  60) },
		SkyState { timekey::night_start, fromSRGB(105, 105, 195), fromSRGB( 20,  20,  60) },
		SkyState { timekey::night,       fromSRGB( 40,  60, 210), fromSRGB(  5,   5,  20) },
		SkyState { timekey::night_end,   fromSRGB( 40,  60, 210), fromSRGB(  5,   5,  20) },
		SkyState { timekey::dawn,        fromSRGB(190, 160, 255), fromSRGB( 80,  60, 105) },
		SkyState { timekey::day_start,   fromSRGB(255, 250, 235), daySkyColor },
	};

	const SkyTex base_DAY =   { "SKYDAY_LAYER1" };
	const SkyTex base_NIGHT = { "SKYNIGHT_LAYER0", 20 * 4, true };
	const SkyTex over_DAY =   { "SKYDAY_LAYER0" };
	const SkyTex over_NIGHT = { "SKYNIGHT_LAYER1" };

	const UV speedNormal = { 0.9f, 1.1f };
	const UV speedSlow = { speedNormal.u * 0.2f, speedNormal.v * 0.2f };

	// each previous SkyTexType is defined to last until next timekey
	const array skyLayerBase = {
		SkyTexState { timekey::day,         base_DAY,   fromAlpha(215), speedNormal },
		SkyTexState { timekey::day_end,     base_NIGHT, fromAlpha(  0) },
		SkyTexState { timekey::evening,     base_NIGHT, fromAlpha(128) },
		SkyTexState { timekey::night_start, base_NIGHT, fromAlpha(255) },
		SkyTexState { timekey::night,       base_NIGHT, fromAlpha(255) },
		SkyTexState { timekey::night_end,   base_NIGHT, fromAlpha(255) },
		SkyTexState { timekey::dawn,        base_NIGHT, fromAlpha(128) },
		SkyTexState { timekey::day_start,   base_DAY,   fromAlpha(  0), speedNormal },
	};
	
	const array skyLayerOverlay = {
		SkyTexState { timekey::day,         over_DAY,   fromAlpha(255), speedSlow },
		SkyTexState { timekey::day_end,     over_DAY,   fromAlpha(255), speedSlow },
		SkyTexState { timekey::evening,     over_DAY,   fromAlpha(128), speedSlow },
		SkyTexState { timekey::night_start, over_NIGHT, fromAlpha(  0), speedNormal, nightCloudColor },
		SkyTexState { timekey::night,       over_NIGHT, fromAlpha(215), speedNormal, nightCloudColor },
		SkyTexState { timekey::night_end,   over_DAY,   fromAlpha(  0), speedNormal, nightCloudColor },
		SkyTexState { timekey::dawn,        over_DAY,   fromAlpha(128), mul(add(speedNormal, speedSlow), 0.5)}, // smoother than G1 (G1 switches from normal to slow instantly at dawn)
		SkyTexState { timekey::day_start,   over_DAY,   fromAlpha(255), speedSlow },
	};

	struct CurrentTimeKeys {
		float timeOfDay;// current time
		uint32_t lastTimeKeyIndex;// most recently elapsed
		uint32_t nextTimeKeyIndex;// upcoming
		float delta;// how much time has passed from last to next, between 0 and 1
	};

	float interpolate(float last, float next, float delta) {
		return last + (next - last) * delta;
	}

	// TODO all vector structs should have component mapping function so we don't need to do this (same for add/mul in PipelineSky.cpp)
	UV interpolate(UV last, UV next, float delta) {
		
		return { interpolate(last.u, next.u, delta), interpolate(last.v, next.v, delta) };
	}

	COLOR interpolate(COLOR last, COLOR next, float delta) {
		return add(last, mul(sub(next, last), delta));
	}

	CurrentTimeKeys getTimeKeysInterpolated(float timeKey)
	{
		for (uint32_t i = 0; i < timeKeys.size(); i++) {
			float last = timeKeys[i];
			uint32_t nextIndex = (i + 1) % timeKeys.size();
			float next = timeKeys[nextIndex];
			if (next < last) {
				next += 1.f;// wrap over
			}
			if (last <= timeKey && next >= timeKey) {
				float interval = next - last;
				float delta = (timeKey - last) / interval;
				return { timeKey, i, nextIndex, delta };
			}
		}
		__assume(false);
	}

	SkyState getSkyStateInterpolated(CurrentTimeKeys timeKeys)
	{
		SkyState result;
		result.timeKey = timeKeys.timeOfDay;
		SkyState last = skyStates[timeKeys.lastTimeKeyIndex];
		SkyState next = skyStates[timeKeys.nextTimeKeyIndex];
		float lastFactor = 1.f - timeKeys.delta;
		float nextFactor = timeKeys.delta;

		result.lightColor = interpolate(last.lightColor, next.lightColor, timeKeys.delta);
		result.skyColor = interpolate(last.skyColor, next.skyColor, timeKeys.delta);
		return result;
	}

	SkyTexState getSkyLayerInterpolated(CurrentTimeKeys timeKeys, bool isBaseLayer)
	{
		const auto& layer = isBaseLayer ? skyLayerBase : skyLayerOverlay;
		SkyTexState result;
		result.timeKey = timeKeys.timeOfDay;
		SkyTexState last = layer[timeKeys.lastTimeKeyIndex];
		SkyTexState next = layer[timeKeys.nextTimeKeyIndex];

		result.tex = last.tex;
		result.alpha = interpolate(last.alpha, next.alpha, timeKeys.delta);
		result.texlightColor = interpolate(last.texlightColor, next.texlightColor, timeKeys.delta);

		// In original G1, uvSpeed is not interpolated at all which results in sudden speed jump at dawn.
		// We do, but we must not interpolate over texture changes.
		if (last.tex.name == next.tex.name) {
			result.uvSpeed = interpolate(last.uvSpeed, next.uvSpeed, timeKeys.delta);
		}
		else {
			result.uvSpeed = last.uvSpeed;
		}

		return result;
	}

	// cache last-used interpolated values so we don't have to re-interpolate on each API call
	CurrentTimeKeys currentTimeKeys = getTimeKeysInterpolated(defaultTime);
	SkyState currentSkyState = getSkyStateInterpolated(currentTimeKeys);
	array<SkyTexState, 2> currentLayers = {
		getSkyLayerInterpolated(currentTimeKeys, true),
		getSkyLayerInterpolated(currentTimeKeys, false),
	};

	SkyState getSkyState(float timeOfDay)
	{
		if (timeOfDay != currentSkyState.timeKey) {
			if (timeOfDay != currentTimeKeys.timeOfDay) {
				currentTimeKeys = getTimeKeysInterpolated(timeOfDay);
			}
			currentSkyState = getSkyStateInterpolated(currentTimeKeys);
		}
		return currentSkyState;
	}

	array<SkyTexState, 2> getSkyLayers(float timeOfDay)
	{
		if (timeOfDay != currentLayers[0].timeKey) {
			if (timeOfDay != currentTimeKeys.timeOfDay) {
				currentTimeKeys = getTimeKeysInterpolated(timeOfDay);
			}
			currentLayers = {
				getSkyLayerInterpolated(currentTimeKeys, true),
				getSkyLayerInterpolated(currentTimeKeys, false),
			};
		}
		return currentLayers;
	}

	bool getSwapLayers(float timeOfDay)
	{
		// TODO maybe move this bool into SkyState
		return timeOfDay >= timekey::day_start || timeOfDay <= timekey::day_end;
	}

	COLOR getSkyLightFromIntensity(float intensity, float currentTime)
	{
		SkyState sky = getSkyState(currentTime);
		return multiplyColor(sky.lightColor, intensity);
	}

	COLOR getSkyColor(float currentTime)
	{
		return getSkyState(currentTime).skyColor;
	}
}