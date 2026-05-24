#pragma once

#include <stdint.h>
#include <array>
#include <future>
#include <fstream>
#include <optional>
#include <filesystem>

#include "zlib.h"

#include "ThreadPool.h"

namespace blitter
{
	constexpr uint32_t k_magic = 0x4E4A4354;

#pragma pack(push, 1)
	struct Header
	{
		uint32_t magic = k_magic; // TCJN magic number
		uint32_t headerCRC = 0u; // JamCRC for header, excluding first 8 bytes
		uint32_t payloadCRC = 0u; // JamCRC for compressed payload
		uint32_t previousPayloadSize = 0u; // Total size for previous payload parts (excluding current payload) in bytes
		uint32_t unknown1 = 0u; // Currently unknown, but very likely high part for previousPayloadSize in order to have 64bit size (aka image over 4 GB)
		uint32_t payloadSize = 0u; // Payload size in bytes
		uint8_t partIdx = 0u; // Current part index, 0 based
		bool isLastPart = false; // Flag to detect last part (aka final disc)
		uint32_t unknown2 = 0u; // Currently unknown
		uint32_t timestamp = 0u; // 32bit timestamp (time32_t)

		std::array<uint8_t, 0x1DE> pad{}; // Unknown or unused
	};
#pragma pack(pop)

	struct ImageInfo
	{
		uint32_t version = 0u;
		uint32_t timestamp = 0u;
		std::vector<std::wstring> modelLocks;
	};

	struct BiosLock
	{
		uint32_t version = 0u; //1313489748, 1599820901, 1617716598
	};

	struct StatusInfo
	{
		bool requireNextDecompress = false;
		bool requireNextRemoveLocks = false;
		uint8_t nextPartIdx = 0u;
		std::wstring errorMessage;
		std::filesystem::path inputPath;
		std::ofstream outputStream;
	};

	class BlitterUtils
	{
	public:
		explicit BlitterUtils(size_t i_threadCount, const std::function<void(StatusInfo&)>& i_statusChanged, const std::function<void(uint8_t)>& i_progressCallback);
		bool PrepareForDecompression();
		void UncompressAsync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream);
		void RemoveLocksAsync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream);

		std::optional<ImageInfo> GetImageInfo(const std::filesystem::path& i_inputPath) const;

	private:
		void UncompressSync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream);
		void RemoveLocksSync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream);
		void PatchLocks(std::vector<Bytef>& io_buffer) const;
		bool IsValidHeader(const Header& i_header) const;

		ThreadPool m_threadPool;
		z_stream m_inputStream{};
		z_stream m_outputStream{};
		uint32_t m_previousPayloadSize = 0u;
		bool m_isCompressed = false;
		std::function<void(StatusInfo&)> m_onStatusChanged;
		std::function<void(uint8_t)> m_onProgressChanged;
	};
}