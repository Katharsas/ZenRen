#pragma once

#include "Win.h"
#include "Util.h"

#include "g3log/g3log.hpp"
#include <fstream>

namespace logger
{
	const LEVELS MIN_LOG_LEVEL = DEBUG;

	bool isEnabled(LEVELS level);
	void init();
	void initFileSink(bool isFileSinkEnabled, const std::string& logFile);


	struct DebugSink {
		void ReceiveLogMessage(g3::LogMessageMover logEntry);
	};

	struct BufferUntilReadySink {
		bool outputReady = false;
		std::string beforeOutputReadyBuffer = "";
		std::function<void(const std::string&)> write = nullptr;

		void ReceiveLogMessage(g3::LogMessageMover logEntry);
	};

	class FileSink {
	public:
		void ReceiveLogMessage(g3::LogMessageMover logEntry);
		void onReady(const std::string& logfile);
		~FileSink();

	private:
		BufferUntilReadySink bufferdSink;
		std::ofstream ofs;
	};
}
