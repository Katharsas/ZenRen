#include "stdafx.h"

#include "GameLoop.h"

#include "Input.h"
#include "Actions.h"
#include "TimerPrecision.h"
#include "PerfStats.h"
#include "renderer/Gui.h"
#include "renderer/Renderer.h"
#include "renderer/Camera.h"
#include "renderer/loader/AssetFinder.h"
#include "imgui/imgui.h"


namespace game
{
	struct Settings {
		// frame limiter
		bool frameLimiterEnabled = true;
		int32_t frameLimit = 60;
	};

	Settings settings;
	stats::FrameSample frameTime;


	void init(HWND hWnd, Arguments args)
	{
		enablePreciseTimerResolution();
		initMicrosleep();

		renderer::addSettings("Graphics", {
			[]() -> void {
				ImGui::Checkbox("Framelimiter", &settings.frameLimiterEnabled);
				ImGui::PushItemWidth(40);
				ImGui::InputInt("Frame Limit", &settings.frameLimit, 0);
				if (settings.frameLimit < 10) {
					settings.frameLimit = 10;
				}
				ImGui::PopItemWidth();
			}
		});

		initActions();

		if (args.vdfFilesRoot.has_value()) {
			renderer::loader::initVdfAssetSourceDir(args.vdfFilesRoot.value());
		}
		if (args.assetFilesRoot.has_value()) {
			renderer::loader::initFileAssetSourceDir(args.assetFilesRoot.value());
		}
		
		renderer::initD3D(hWnd);

		if (args.level.has_value()) {
			renderer::loadLevel(args.level.value());
		}

		LOG(DEBUG) << "Statistics in microseconds";
	}

	void renderAndSleep()
	{
		// START RENDER
		int32_t deltaTimeMicros = frameTime.updateStart();
		float deltaTime = (float)deltaTimeMicros / 1000000;
		//LOG(DEBUG) << "DeltaTime:" << deltaTime;

		processUserInput(deltaTime);
		renderer::update();

		frameTime.updateEndRenderStart();

		renderer::renderFrame();

		frameTime.renderEndSleepStart();
		int32_t frameTimeTarget = 0;
		
		if (settings.frameLimiterEnabled)
		{
			frameTimeTarget = 1000000 / settings.frameLimit;
			const int32_t sleepTarget = frameTimeTarget - frameTime.renderTimeMicros;
			if (sleepTarget > 0)
			{
				// sleep_for can wake up about once every 1,4ms
				//std::this_thread::sleep_for(std::chrono::microseconds(100));

				// microsleep can wake up about once every 0,5ms
				microsleep(sleepTarget + 470); // make sure we never get too short frametimes
			}
		}

		frameTime.sleepEnd();
		stats::addFrameSample(frameTime, frameTimeTarget);
	}

	void onWindowResized(uint32_t width, uint32_t height)
	{
		LOG(DEBUG) << "Resizing game window! Width: " << width << ", Height: " << height;
		renderer::onWindowResize(width, height);
	}

	void execute()
	{
		renderAndSleep();
	}

	void cleanup()
	{
		renderer::cleanD3D();
		cleanupMicrosleep();
		disablePreciseTimerResolution();
	}
}