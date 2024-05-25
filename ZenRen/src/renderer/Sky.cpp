#include "stdafx.h"
#include "Sky.h"

#include "RendererCommon.h"

namespace renderer
{
	using std::array;

	namespace timekey
	{
		const float midday = .0f;

		const float evening = .3f;
		const float evening_early = evening - .05f;
		const float evening_late = evening + .05f;

		const float midnight = .5f;

		const float morning = .7f;
		const float morning_early = morning - .05f;
		const float morning_late = morning + .05f;
	}

	// interpolates from black to sky light color
	const float skyLightIntensityOutdoor = 200 / 256.0f;
	const float skyLightIntensityIndoor = 120 / 256.0f;

	struct SkyState {
		float timeKey;
		D3DXCOLOR lightColor;// same as polyColor
		D3DXCOLOR skyColor;// same as fogColor
	};

	D3DXCOLOR fromSRGB(uint8_t r, uint8_t g, uint8_t b) {
		return D3DXCOLOR(
			fromSRGB(r / 256.f),
			fromSRGB(g / 256.f),
			fromSRGB(b / 256.f),
			1.f);
	}

	const D3DXCOLOR nightCloudColor = fromSRGB(55, 55, 155);

	// We assume that all state arrays have same length and use same timeKeys in same order as defined here.
	// In theory, we don't need to put time keys into every state struct, but this makes state definitions more readable.
	const array timeKeys = {
		timekey::midday,
		timekey::evening_early,
		timekey::evening,
		timekey::evening_late,
		timekey::midnight,
		timekey::morning_early,
		timekey::morning,
		timekey::morning_late,
	};

	const array skyStates = {
		SkyState {
			timekey::midday,
			fromSRGB(255, 250, 235),
			fromSRGB(120, 140, 180),
		},
		SkyState {
			timekey::evening_early,
			fromSRGB(255, 250, 235),
			fromSRGB(120, 140, 180),
		},
		SkyState {
			timekey::evening,
			fromSRGB(255, 185, 170),
			fromSRGB(180, 75, 60),
		},
		SkyState {
			timekey::evening_late,
			fromSRGB(105, 105, 195),
			fromSRGB(20, 20, 60),
		},
		SkyState {
			timekey::midnight,
			fromSRGB(40, 60, 210),
			fromSRGB(5, 5, 20),
		},
		SkyState {
			timekey::morning_early,
			fromSRGB(40, 60, 210),
			fromSRGB(5, 5, 20),
		},
		SkyState {
			timekey::morning,
			fromSRGB(190, 160, 255),
			fromSRGB(80, 60, 105),
		},
		SkyState {
			timekey::morning_late,
			fromSRGB(255, 250, 235),
			fromSRGB(120, 140, 180),
		},
	};

	float fromAlpha(uint8_t a) {
		return a / 255.f;
	}

	// each previous SkyTexType is defined to last until next timekey
	const array skyLayerBase = {
		SkyTexState { timekey::midday,        SkyTexType::DAY,   fromAlpha(215) },
		SkyTexState { timekey::evening_early, SkyTexType::NIGHT, fromAlpha(  0) },
		SkyTexState { timekey::evening,       SkyTexType::NIGHT, fromAlpha(128) },
		SkyTexState { timekey::evening_late,  SkyTexType::NIGHT, fromAlpha(255) },
		SkyTexState { timekey::midnight,      SkyTexType::NIGHT, fromAlpha(255) },
		SkyTexState { timekey::morning_early, SkyTexType::NIGHT, fromAlpha(255) },
		SkyTexState { timekey::morning,       SkyTexType::NIGHT, fromAlpha(128) },
		SkyTexState { timekey::morning_late,  SkyTexType::DAY,   fromAlpha(215) },
	};
	const array skyLayerOverlay = {
		SkyTexState { timekey::midday,        SkyTexType::DAY,   fromAlpha(128) },// TODO original alpha = 255, but ingame the overlay does not fully block base layer, so something is wrong!
		SkyTexState { timekey::evening_early, SkyTexType::DAY,   fromAlpha(255) },// TODO check ingame
		SkyTexState { timekey::evening,       SkyTexType::DAY,   fromAlpha(128) },
		SkyTexState { timekey::evening_late,  SkyTexType::NIGHT, fromAlpha(  0), nightCloudColor },
		SkyTexState { timekey::midnight,      SkyTexType::NIGHT, fromAlpha(215), nightCloudColor },
		SkyTexState { timekey::morning_early, SkyTexType::DAY,   fromAlpha(  0), nightCloudColor },
		SkyTexState { timekey::morning,       SkyTexType::DAY,   fromAlpha(128) },
		SkyTexState { timekey::morning_late,  SkyTexType::DAY,   fromAlpha(255) },
	};

	struct CurrentTimeKeys {
		float timeOfDay;// current time
		int32_t lastTimeKeyIndex;// most recently elapsed
		int32_t nextTimeKeyIndex;// upcoming
		float delta;// how much time has passed from last to next, between 0 and 1
	};

	float interpolate(float last, float next, float delta) {
		return last + (next - last) * delta;
	}
	D3DXCOLOR interpolate(D3DXCOLOR last, D3DXCOLOR next, float delta) {
		return last + (next - last) * delta;
	}

	CurrentTimeKeys getTimeKeysInterpolated(float timeKey) {
		for (int32_t i = 0; i < timeKeys.size(); i++) {
			float last = timeKeys[i];
			int32_t nextIndex = (i + 1) % timeKeys.size();
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
		assert(false);
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

	SkyTexState getSkyLayerInterpolated(CurrentTimeKeys timeKeys, bool isBaseLayer) {
		const auto& layer = isBaseLayer ? skyLayerBase : skyLayerOverlay;
		SkyTexState result;
		result.timeKey = timeKeys.timeOfDay;
		SkyTexState last = layer[timeKeys.lastTimeKeyIndex];
		SkyTexState next = layer[timeKeys.nextTimeKeyIndex];

		result.type = last.type;
		result.alpha = interpolate(last.alpha, next.alpha, timeKeys.delta);
		result.texlightColor = interpolate(last.texlightColor, next.texlightColor, timeKeys.delta);
		return result;
	}

	// cache last-used interpolated values so we don't have to re-interpolate on each API call
	CurrentTimeKeys currentTimeKeys = getTimeKeysInterpolated(defaultTime);
	SkyState currentSkyState = getSkyStateInterpolated(currentTimeKeys);
	array<SkyTexState, 2> currentLayers = {
		getSkyLayerInterpolated(currentTimeKeys, true),
		getSkyLayerInterpolated(currentTimeKeys, false),
	};

	SkyState getSkyState(float timeOfDay) {
		if (timeOfDay != currentSkyState.timeKey) {
			if (timeOfDay != currentTimeKeys.timeOfDay) {
				currentTimeKeys = getTimeKeysInterpolated(timeOfDay);
			}
			currentSkyState = getSkyStateInterpolated(currentTimeKeys);
		}
		return currentSkyState;
	}

	array<SkyTexState, 2> getSkyLayers(float timeOfDay) {
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

	D3DXCOLOR getSkyLightFromIntensity(float intensity, float currentTime)
	{
		SkyState sky = getSkyState(currentTime);
		return multiplyColor(sky.lightColor, intensity);
	}

	D3DXCOLOR getSkyColor(float currentTime)
	{
		return getSkyState(currentTime).skyColor;
	}
}