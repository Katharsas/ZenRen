#include "stdafx.h"

#include "GameLoop.h"

#include "Input.h"
#include "Actions.h"
#include "TimerPrecision.h"
#include "PerfStats.h"
#include "render/Gui.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "assets/AssetFinder.h"
#include "imgui/imgui.h"


namespace viewer
{
	struct Settings {
		bool frameLimiterEnabled = true;
		int32_t frameLimit = 60;
	}
	settings;

	struct FrameTimes {
		stats::TimeSampler full;
		stats::TimeSampler update;
		stats::TimeSampler render;
		stats::TimeSampler wait;
	}
	frameTimes;

	void init(HWND hWnd, Arguments args, uint32_t width, uint32_t height)
	{
		enablePreciseTimerResolution();
		initMicrosleep();

		render::addInfo("", {
			[]() -> void {
				uint32_t fullTime = stats::getSampleStats(frameTimes.full).averageMicros;
				uint32_t updateTime = stats::getSampleStats(frameTimes.update).averageMicros;
				uint32_t renderTime = stats::getSampleStats(frameTimes.render).averageMicros;
				
				const int32_t fpsReal = 1000000 / fullTime;

				std::string microSymbol = util::fromU8(u8"�m");
				std::stringstream buffer;
				buffer << "Perf:  " << util::leftPad(std::to_string(fpsReal), 4) << " FPS" << std::endl;
				buffer << "  Render: " + util::leftPad(std::to_string(renderTime), 4) << " " << microSymbol << std::endl;
				buffer << "  Update: " + util::leftPad(std::to_string(updateTime), 4) << " " << microSymbol << std::endl;
				ImGui::Text(buffer.str().c_str());
			}
		});
		render::addSettings("FPS Limiter", {
			[]() -> void {
				ImGui::Checkbox("Enabled", &settings.frameLimiterEnabled);
				ImGui::PushItemWidth(render::INPUT_FLOAT_WIDTH);
				ImGui::InputInt("FPS Limit", &settings.frameLimit, 0);
				if (settings.frameLimit < 10) {
					settings.frameLimit = 10;
				}
				ImGui::PopItemWidth();
			}
		});

		initActions();

		if (args.vdfFilesRoot.has_value()) {
			assets::initVdfAssetSourceDir(args.vdfFilesRoot.value());
		}
		if (args.assetFilesRoot.has_value()) {
			assets::initFileAssetSourceDir(args.assetFilesRoot.value());
		}
		
		render::initD3D(hWnd, { width, height });

		if (args.level.has_value()) {
			render::loadLevel(args.level.value());
		}

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

		stats::sampleAndStart(frameTimes.update, frameTimes.render);

		render::renderFrame();

		stats::sampleAndStart(frameTimes.render, frameTimes.wait);

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
		LOG(DEBUG) << "Resizing game window! Width: " << width << ", Height: " << height;
		render::onWindowResize({ width, height });
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