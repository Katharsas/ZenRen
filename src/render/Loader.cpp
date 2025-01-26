#include "stdafx.h"
#include "Loader.h"

// Needed because Mmap must be complete type for default destructor to compile
//#include "zenkit/Mmap.hh" 

namespace render
{
	FileData::FileData(FileData&&) noexcept = default;
	FileData::~FileData() noexcept = default;

	FileData::FileData(const std::string& name, const std::byte * data, uint64_t size)
		: name(name), data(data), size(size) {}

	FileData::FileData(const std::string& name, const std::byte * data, uint64_t size, zenkit::Mmap&& mmap)
		: name(name), data(data), size(size), mmap(std::move(mmap)) {}

	FileData::FileData(const std::string& name, const std::byte * data, uint64_t size, std::unique_ptr<std::vector<std::uint8_t>>&& buffer)
		: name(name), data(data), size(size), buffer(std::move(buffer)) {}

	FileData::FileData(const std::string& name, const std::byte* data, uint64_t size, const std::shared_ptr<std::vector<std::uint8_t>>& buffer)
		: name(name), data(data), size(size), bufferShared(buffer) {}
}