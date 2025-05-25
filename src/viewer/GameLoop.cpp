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
		render::stats::TimeSampler render;
		render::stats::TimeSampler present;
		render::stats::TimeSampler wait;
	}
	frameTimes;


	bool validateIsDir(const std::filesystem::path& path, bool isFatal = false)
	{
		if (std::filesystem::is_directory(path)) {
			return true;
		}
		else {
			auto& level = isFatal ? FATAL : WARNING;
			LOG(level) << "Path is not a valid directory: " << path;
			return false;
		}
	}

	void init(HWND hWnd, Arguments args, uint16_t width, uint16_t height)
	{
		LOG(INFO);
		LOG(INFO) << "#############################################";
		LOG(INFO) << "Initializing ZenRen";
		LOG(INFO) << "#############################################";

		auto sampler = render::stats::TimeSampler();
		sampler.start();

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
				uint32_t renderTime = render::stats::getTimeSamplerStats(frameTimes.render).average;
				uint32_t presentTime = render::stats::getTimeSamplerStats(frameTimes.present).average;
				
				const int32_t fpsReal = fullTime == 0 ? 0 : (1000000 / fullTime);

				std::stringstream buffer;
				buffer << "FPS:  " << util::leftPad(std::to_string(fpsReal), 4) << '\n';
				buffer << "  Total: " + util::leftPad(std::to_string(fullTime), 4) << " us";
				if (settings.frameLimiterEnabled) {
					buffer << " [LIMITED]";
				}
				buffer << '\n';
				buffer << "    Render  (CPU): " + util::leftPad(std::to_string(renderTime), 4) << " us\n";
				buffer << "    Present (GPU): " + util::leftPad(std::to_string(presentTime), 4) << " us\n";

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

		assets::initAssetsIntern();
		if (args.vdfFilesRoot.has_value()) {
			auto& vdfFilesRoot = args.vdfFilesRoot.value();
			if (validateIsDir(vdfFilesRoot, true)) {
				assets::initVdfAssetSourceDir(vdfFilesRoot);
			}
		}
		if (args.assetFilesRoot.has_value()) {
			auto& assetFilesRoot = args.assetFilesRoot.value();
			if (validateIsDir(assetFilesRoot, true)) {
				assets::initFileAssetSourceDir(assetFilesRoot);
			}
		}
		
		render::initD3D(hWnd, { width, height });

		if (!args.level.has_value()) {
			LOG(WARNING) << "No level file argument specified! Loading empty level!";
		}
		// disable sky by default if there are no assets to take sky textures from
		bool defaultSky = args.vdfFilesRoot.has_value() || args.assetFilesRoot.has_value();
		bool levelLoaded = render::loadLevel(args.level, defaultSky);
		if (!levelLoaded) {
			LOG(WARNING) << "Available level files:";
			assets::printFoundZens();
		}

		LOG(INFO);
		LOG(INFO) << "#############################################";
		sampler.logMillisAndRestart("ZenRen initialized total");
		LOG(INFO) << "#############################################";

		assets::cleanAssetSources();

		frameTimes.full.start();
	}

	void renderAndSleep()
	{
		// This render loop's latency is optimal as long as full frame time fits inside frame limit (limiter is adding sleep time).
		// Otherwise however overlapping CPU and GPU time could halve frametime in best case, but also increases latency to over one frame.
		// See: https://gamedev.stackexchange.com/a/109400
		// TODO: Visualize tradeoff. Maybe have both as an option (or select automatically if FPS increase makes up for increased latency)

		// START RENDER
		float deltaTime = (float) frameTimes.full.sampleRestart() / 1000000;
		frameTimes.render.start(frameTimes.full.last);

		processUserInput(deltaTime);
		render::update(deltaTime);
		render::renderFrame();

		render::stats::sampleAndStart(frameTimes.render, frameTimes.present);

		render::presentFrameBlocking();

		render::stats::sampleAndStart(frameTimes.present, frameTimes.wait);

		int32_t frameTimeTarget = 0;
		
		if (settings.frameLimiterEnabled)
		{
			frameTimeTarget = 1000000 / settings.frameLimit;
			const int32_t sleepTarget = frameTimeTarget - (frameTimes.render.lastTimeMicros + frameTimes.present.lastTimeMicros);
			if (sleepTarget > 0)
			{
				// microsleep can wake up about once every 0,5ms
				microsleep(sleepTarget + 470); // make sure we never get too short frametimes
			}
		}

		frameTimes.wait.sample();
	}

	void onWindowResized(uint16_t width, uint16_t height)
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