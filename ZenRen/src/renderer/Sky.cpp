#include "stdafx.h"
#include "Sky.h"

#include "RendererCommon.h"

namespace renderer
{
	namespace timekeys
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

	const std::array skyStates = {
		SkyState {
			timekeys::midday,
			fromSRGB(255, 250, 235),
			fromSRGB(120, 140, 180),
		},
		SkyState {
			timekeys::evening_early,
			fromSRGB(255, 250, 235),
			fromSRGB(120, 140, 180),
		},
		SkyState {
			timekeys::evening,
			fromSRGB(255, 185, 170),
			fromSRGB(180, 75, 60),
		},
		SkyState {
			timekeys::evening_late,
			fromSRGB(105, 105, 195),
			fromSRGB(20, 20, 60),
		},
		SkyState {
			timekeys::midnight,
			fromSRGB(40, 60, 210),
			fromSRGB(5, 5, 20),
		},
		SkyState {
			timekeys::morning_early,
			fromSRGB(40, 60, 210),
			fromSRGB(5, 5, 20),
		},
		SkyState {
			timekeys::morning,
			fromSRGB(190, 160, 255),
			fromSRGB(80, 60, 105),
		},
		SkyState {
			timekeys::morning_late,
			fromSRGB(255, 250, 235),
			fromSRGB(120, 140, 180),
		},
	};

	SkyState getSkyStateInterpolated(float timeKey)
	{
		SkyState result;
		result.timeKey = timeKey;

		for (int32_t i = 0; i < skyStates.size(); i++) {
			SkyState current = skyStates[i];
			SkyState next = skyStates[(i+1) % skyStates.size()];
			if (next.timeKey < current.timeKey) {
				next.timeKey += 1.f;// wrap over
			}
			if (current.timeKey <= timeKey && next.timeKey >= timeKey) {
				float currentDiff = timeKey - current.timeKey;
				float nextDiff = next.timeKey - timeKey;
				float totalDiff = currentDiff + nextDiff;

				float currentFactor = 1.f - (currentDiff / totalDiff);
				float nextFactor = 1.f - (nextDiff / totalDiff);
				result.lightColor = current.lightColor * currentFactor + next.lightColor * nextFactor;
				result.skyColor = current.skyColor * currentFactor + next.skyColor * nextFactor;
				return result;
			}
		}

		assert(false);
	}

	SkyState currentSkyState = getSkyStateInterpolated(0.f);

	SkyState getSkyState(float timeKey) {
		if (timeKey != currentSkyState.timeKey) {
			currentSkyState = getSkyStateInterpolated(timeKey);
		}
		return currentSkyState;
	}

	D3DXCOLOR getSkyLightFromIntensity(float intensity, float currentTime)
	{
		SkyState sky = getSkyState(currentTime);
		return multiplyColor(sky.lightColor, intensity);
	}

	void drawSky(D3d d3d, ID3D11RenderTargetView* targetRtv, float currentTime) 
	{
		// poor man's sky
		SkyState sky = getSkyState(currentTime);
		d3d.deviceContext->ClearRenderTargetView(targetRtv, sky.skyColor);
	}
}