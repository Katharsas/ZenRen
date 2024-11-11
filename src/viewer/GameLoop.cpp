#include "stdafx.h"

#include "GameLoop.h"

#include "Input.h"
#include "Actions.h"
#include "TimerPrecision.h"
#include "render/PerfStats.h"
#include "render/Gui.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "assets/AssetFinder.h"
#include <imgui.h>


namespace viewer
{
	struct Settings {
		bool frameLimiterEnabled = true;
		int32_t frameLimit = 120;
	}
	settings;

	struct FrameTimes {
		render::stats::TimeSampler full;
		render::stats::TimeSampler update;
		render::stats::TimeSampler render;
		render::stats::TimeSampler wait;
	}
	frameTimes;

	void init(HWND hWnd, Arguments args, uint32_t width, uint32_t height)
	{
		LOG(INFO) << "Current working dir: " << std::filesystem::current_path();

		enablePreciseTimerResolution();
		initMicrosleep();

		frameTimes = {
			render::stats::createTimeSampler(),
			render::stats::createTimeSampler(),
			render::stats::createTimeSampler(),
			render::stats::createTimeSampler()
		};

		render::gui::addInfo("", {
			[]() -> void {
				uint32_t fullTime = render::stats::getTimeSamplerStats(frameTimes.full).average;
				uint32_t updateTime = render::stats::getTimeSamplerStats(frameTimes.update).average;
				uint32_t renderTime = render::stats::getTimeSamplerStats(frameTimes.render).average;
				
				const int32_t fpsReal = fullTime == 0 ? 0 : (1000000 / fullTime);

				std::stringstream buffer;
				buffer << "FPS:  " << util::leftPad(std::to_string(fpsReal), 4) << std::endl;

				// when frame limiter is enabled GPU time is masked by sleep time
				if (!settings.frameLimiterEnabled) {
					buffer << "  Render: " + util::leftPad(std::to_string(renderTime), 4) << " us" << std::endl;
				}
				else {
					buffer << "  Render: (limited)"  << std::endl;
				}

				buffer << "  Update: " + util::leftPad(std::to_string(updateTime), 4) << " us" << std::endl;
				ImGui::Text(buffer.str().c_str());
			}
		});
		render::gui::addSettings("FPS Limiter", {
			[]() -> void {
				ImGui::Checkbox("Enabled", &settings.frameLimiterEnabled);
				ImGui::PushItemWidth(render::gui::constants().inputFloatWidth);
				ImGui::InputInt("FPS Limit", &settings.frameLimit, 0);
				if (settings.frameLimit < 10) {
					settings.frameLimit = 10;
				}
				ImGui::PopItemWidth();
			}
		});

		input::setRdpCompatMode(args.rdpCompatMode);
		initActions();

		if (args.vdfFilesRoot.has_value()) {
			assets::initVdfAssetSourceDir(args.vdfFilesRoot.value());
		}
		if (args.assetFilesRoot.has_value()) {
			assets::initFileAssetSourceDir(args.assetFilesRoot.value());
		}
		
		render::initD3D(hWnd, { width, height });

		if (!args.level.has_value()) {
			LOG(WARNING) << "No level file argument specified! Loading empty level!";
		}
		// disable sky by default if there are no assets to take sky textures from
		bool defaultSky = args.vdfFilesRoot.has_value() || args.assetFilesRoot.has_value();
		render::loadLevel(args.level, defaultSky);


		LOG(DEBUG) << "Statistics in microseconds";

		frameTimes.full.start();
	}

	void renderAndSleep()
	{
		// START RENDER
		float deltaTime = (float) frameTimes.full.sampleRestart() / 1000000;
		frameTimes.update.start(frameTimes.full.last);

		processUserInput(deltaTime);
		render::update(deltaTime);

		render::stats::sampleAndStart(frameTimes.update, frameTimes.render);

		render::renderFrame();

		render::stats::sampleAndStart(frameTimes.render, frameTimes.wait);

		int32_t frameTimeTarget = 0;
		
		if (settings.frameLimiterEnabled)
		{
			frameTimeTarget = 1000000 / settings.frameLimit;
			const int32_t sleepTarget = frameTimeTarget - (frameTimes.update.lastTimeMicros + frameTimes.render.lastTimeMicros);
			if (sleepTarget > 0)
			{
				// sleep_for can wake up about once every 1,4ms
				//std::this_thread::sleep_for(std::chrono::microseconds(100));

				// microsleep can wake up about once every 0,5ms
				microsleep(sleepTarget + 470); // make sure we never get too short frametimes
			}
		}

		frameTimes.wait.sample();
	}

	void onWindowResized(uint32_t width, uint32_t height)
	{
		LOG(DEBUG) << "Window size change! Width: " << width << ", Height: " << height;
		render::onWindowResize({ width, height });
	}

	void onWindowDpiChanged(float dpiScale) {
		LOG(DEBUG) << "Window DPI change! Scale: " << dpiScale;
		render::onWindowDpiChange(dpiScale);
	}

	void execute()
	{
		renderAndSleep();
	}

	void cleanup()
	{
		render::cleanD3D();
		cleanupMicrosleep();
		disablePreciseTimerResolution();
	}
}