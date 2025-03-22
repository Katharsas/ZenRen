#include "stdafx.h"
#include "Logger.h"

#include "g3log/logworker.hpp"

namespace logger
{
	std::unique_ptr<g3::LogWorker> worker;
	std::unique_ptr<g3::SinkHandle<DebugSink>> debugSinkHandle;
	std::unique_ptr<g3::SinkHandle<FileSink>> fileSinkHandle;

	bool isEnabled(LEVELS level)
	{
		return level.value >= MIN_LOG_LEVEL.value;
	}

	std::string formatLogEntry(LEVELS level, g3::LogMessageMover& logEntry)
	{
		const std::string levelPre = level == WARNING ? "! " : "  ";
		const std::string levelPost = std::string((7 - level.text.length()), ' ');
		return levelPre + "LOG__" + level.text + levelPost + " " + logEntry.get()._message + "\n";
	}

	void init()
	{
		// Configure Logger (make sure log dir exists, TODO: create dir if mising)

		// https://github.com/KjellKod/g3sinks/blob/master/snippets/ColorCoutSink.hpp
		worker = std::move(g3::LogWorker::createLogWorker());
		debugSinkHandle = std::move(worker->addSink(std::make_unique<DebugSink>(), &DebugSink::ReceiveLogMessage));
		fileSinkHandle = std::move(worker->addSink(std::make_unique<FileSink>(), &FileSink::ReceiveLogMessage));

		g3::initializeLogging(worker.get());
	}

	void initFileSink(bool isFileSinkEnabled, const std::string& logFile)
	{
		if (isFileSinkEnabled) {
			// allow sink to write its buffer to file
			auto pSink = fileSinkHandle.get()->sink().lock();
			if (pSink) {
				FileSink* filesink = pSink.get()->_real_sink.get();
				filesink->onReady(logFile);
			}
		}
		else {
			// remove sink and throw away buffer
			worker->removeSink(std::move(fileSinkHandle));
		}
	}

	void DebugSink::ReceiveLogMessage(g3::LogMessageMover logEntry)
	{
		const LEVELS level = logEntry.get()._level;
		if (isEnabled(level)) {
			const std::string logEntryString = formatLogEntry(level, logEntry);
			const std::wstring logEntryW = util::utf8ToWide(logEntryString);
			OutputDebugStringW(logEntryW.c_str());
		}
	}

	void BufferUntilReadySink::ReceiveLogMessage(g3::LogMessageMover logEntry)
	{
		const LEVELS level = logEntry.get()._level;
		if (isEnabled(level)) {
			const std::string logEntryString = formatLogEntry(level, logEntry);
			if (outputReady) {
				if (beforeOutputReadyBuffer != "") {
					write(beforeOutputReadyBuffer);
					beforeOutputReadyBuffer = "";
				}
				write(logEntryString);
			}
			else {
				beforeOutputReadyBuffer += logEntryString;
			}
		}
	}

	void FileSink::ReceiveLogMessage(g3::LogMessageMover logEntry)
	{
		bufferdSink.ReceiveLogMessage(logEntry);
	}

	void FileSink::onReady(const std::string& logfile)
	{
		ofs.open(logfile, std::ofstream::out | std::ofstream::trunc);
		bufferdSink.write = [this](const std::string& string) -> void {
			ofs << string;
			ofs << std::flush;
		};
		bufferdSink.outputReady = true;
	}

	FileSink::~FileSink()
	{
		bufferdSink.beforeOutputReadyBuffer = "";
		ofs << "Logger exiting.";
		ofs.close();
	}
}