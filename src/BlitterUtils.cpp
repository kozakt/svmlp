#include "BlitterUtils.h"

#include "Epilogue.h"

namespace blitter
{
    BlitterUtils::BlitterUtils(size_t i_threadCount, const std::function<void(StatusInfo&)>& i_statusChanged, const std::function<void(uint8_t)>& i_progressCallback)
        : m_threadPool(i_threadCount)
        , m_onStatusChanged(i_statusChanged)
        , m_onProgressChanged(i_progressCallback)
    {
    }

    bool BlitterUtils::PrepareForDecompression()
    {
        m_outputStream = {};

        m_outputStream.zalloc = Z_NULL;
        m_outputStream.zfree = Z_NULL;
        m_outputStream.opaque = Z_NULL;
        m_outputStream.avail_in = 0u;
        m_outputStream.next_in = Z_NULL;

        m_inputStream = {};

        m_inputStream.zalloc = Z_NULL;
        m_inputStream.zfree = Z_NULL;
        m_inputStream.opaque = Z_NULL;
        m_inputStream.avail_in = 0u;
        m_inputStream.next_in = Z_NULL;

        m_previousPayloadSize = 0u;
        m_isCompressed = false;

        return inflateInit(&m_inputStream) == Z_OK;
    }

    void BlitterUtils::UncompressAsync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream)
    {
        m_threadPool.enqueue([this, i_inputPath, outputStreamShared = std::make_shared<std::ofstream>(std::move(o_outputStream))]()
            {
                UncompressSync(i_inputPath, std::move(*outputStreamShared));
            });
    }

    void BlitterUtils::RemoveLocksAsync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream)
    {
        m_threadPool.enqueue([this, i_inputPath, outputStreamShared = std::make_shared<std::ofstream>(std::move(o_outputStream))]()
            {
                RemoveLocksSync(i_inputPath, std::move(*outputStreamShared));
            });
    }

    std::optional<ImageInfo> BlitterUtils::GetImageInfo(const std::filesystem::path& i_inputPath) const
    {
        std::ifstream inputFile(i_inputPath, std::ios::in | std::ios::binary);

        if (!inputFile.is_open())
        {
            return std::nullopt;
        }

        inputFile.seekg(0u, std::ios::beg);

        Header header{};

        inputFile.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (!IsValidHeader(header) || header.partIdx != 0u)
        {
            return std::nullopt;
        }

        uint32_t format = 0u;
        inputFile.read(reinterpret_cast<char*>(&format), sizeof(format));
        
        constexpr size_t bufferSize = 0x1000; // 4 KB should be enough decompress header

        std::vector<Bytef> inBuffer(bufferSize);
        std::vector<Bytef> outBuffer(bufferSize * 10u);

        if (format == 0x42494C5A) // ZLIB
        {
            inputFile.read(reinterpret_cast<char*>(inBuffer.data()), inBuffer.size());
            const std::streamsize bytesRead = inputFile.gcount();

            z_stream stream = {};

            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;
            stream.avail_in = static_cast<uInt>(bytesRead);
            stream.next_in = inBuffer.data();

            int ret = inflateInit(&stream);

            stream.avail_out = static_cast<uInt>(outBuffer.size());
            stream.next_out = outBuffer.data();
            ret = inflate(&stream, Z_NO_FLUSH);

            inflateEnd(&stream);

            if (ret != Z_OK && ret != Z_STREAM_END)
            {
                return std::nullopt;
            }
            
            if (stream.avail_out == 0u)
            {
                // Output buffer too small
                return std::nullopt;
            }
        }
        else // Uncompressed
        {
            inputFile.seekg(0x200, std::ios::beg);

            inputFile.read(reinterpret_cast<char*>(outBuffer.data()), outBuffer.size());
        }

        ImageInfo info{};

        memcpy(&info.version, &outBuffer[0], sizeof(info.version));

        info.timestamp = header.timestamp;

        if (info.version == 0x5F5B5465)
        {
            for (uint32_t i = 0; i < 4u; i++)
            {
                const uint32_t offset = 0x0C + (i * 0x80);

                // Make sure strings are null terminated
                outBuffer[offset + 0x3F] = 0u;
                outBuffer[offset + 0x40 + 0x3F] = 0u;

                std::wstring manufacturer(reinterpret_cast<const wchar_t*>(&outBuffer[offset]));
                std::wstring model(reinterpret_cast<const wchar_t*>(&outBuffer[offset + 0x40]));

                if (!manufacturer.empty() || !model.empty())
                {
                    info.modelLocks.emplace_back(std::format(L"<{}>:<{}>", manufacturer, model));
                }
            }
        }
        else if (info.version == 0x606C6576)
        {
            for (uint32_t i = 0; i < 4u; i++)
            {
                const uint32_t offset = 0x0C + (i * 0x80);

                // Make sure strings are null terminated
                outBuffer[offset + 0x3F] = 0u;
                outBuffer[offset + 0x40 + 0x3F] = 0u;

                std::wstring manufacturer(reinterpret_cast<const wchar_t*>(&outBuffer[offset]));
                std::wstring model(reinterpret_cast<const wchar_t*>(&outBuffer[offset + 0x40]));

                if (!manufacturer.empty() || !model.empty())
                {
                    info.modelLocks.emplace_back(std::format(L"<{}>:<{}>", manufacturer, model));
                }
            }

            constexpr uint32_t conunterOffset = 0x0C + 0x200;
            uint8_t count = 0u;
            memcpy(&count, &outBuffer[conunterOffset], sizeof(count));
            for (uint32_t i = 0; i < count; i++)
            {
                const uint32_t offset = 0x0C + 0x200 + 0x01 + (i * 0x20);
                
                // Make sure string is null terminated
                outBuffer[offset + 0x1F] = 0u;

                std::wstring model(reinterpret_cast<const wchar_t*>(&outBuffer[offset]));

                if (!model.empty())
                {
                    info.modelLocks.emplace_back(std::format(L"<{}>", model));
                }
            }
        }
        else if (info.version == 0x4E4A4354)
        {
            const uint32_t offset = 0x0C;

            // Make sure strings are null terminated
            outBuffer[offset + 0x1F] = 0u;
            outBuffer[offset + 0x20 + 0x1F] = 0u;

            std::string manufacturer(reinterpret_cast<const char*>(&outBuffer[offset]));
            std::string model(reinterpret_cast<const char*>(&outBuffer[offset + 0x20]));

            if (!manufacturer.empty() || !model.empty())
            {
                std::size_t size = std::mbstowcs(nullptr, manufacturer.c_str(), 0u);
                std::wstring wmanufacturer(size, L'\0');
                std::mbstowcs(&wmanufacturer[0], manufacturer.c_str(), size);
                
                size = std::mbstowcs(nullptr, model.c_str(), 0u);
                std::wstring wmodel(size, L'\0');
                std::mbstowcs(&wmodel[0], model.c_str(), size);

                info.modelLocks.emplace_back(std::format(L"<{}>:<{}>", wmanufacturer, wmodel));
            }
        }
        else
        {
            // Unsupported lock version
            info.modelLocks.emplace_back(L"Unsupported lock version");
        }

        return info;
    }

    void BlitterUtils::UncompressSync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream)
    {
        StatusInfo info{};
        info.inputPath = i_inputPath;

        Epilogue epilogue([this, &info]()
            {
                if (m_onStatusChanged)
                {
                    m_onStatusChanged(info);
                }
            });

        std::ifstream inputFile(i_inputPath, std::ios::in | std::ios::binary);
        std::ofstream outputFile = std::move(o_outputStream);

        if (!inputFile.is_open() || !outputFile.is_open())
        {
            return;
        }

        inputFile.seekg(0u, std::ios::end);
        std::streampos fileSize = inputFile.tellg();
        inputFile.seekg(0u, std::ios::beg);

        Header header{};

        inputFile.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (!IsValidHeader(header))
        {
            return;
        }
        
        info.requireNextDecompress = !header.isLastPart;
        info.nextPartIdx = header.isLastPart ? 0u : header.partIdx + 1u;

        constexpr size_t bufferSize = 16384u;

        std::vector<Bytef> inBuffer(bufferSize);
        std::vector<Bytef> outBuffer(bufferSize);

        size_t streamSize = static_cast<size_t>(fileSize - inputFile.tellg());

        uint32_t payloadSize = 0u;

        uint32_t format = 0u;
        
        if (header.partIdx == 0u)
        {
            inputFile.read(reinterpret_cast<char*>(&format), sizeof(format));
            payloadSize += 4u;
            
            m_isCompressed = format == 0x42494C5A;// ZLIB value
        }

        if (!m_isCompressed)
        {
            outputFile.write(reinterpret_cast<char*>(&format), sizeof(format));
            while (!inputFile.eof())
            {
                inputFile.read(reinterpret_cast<char*>(outBuffer.data()), outBuffer.size());
                const std::streamsize bytesRead = inputFile.gcount();
                outputFile.write(reinterpret_cast<char*>(outBuffer.data()), bytesRead);
            }

            info.outputStream = std::move(outputFile);
            return;
        }

        int ret = Z_OK;

        uint8_t completedPercentage = 0u;
        
        do
        {
            inputFile.read(reinterpret_cast<char*>(inBuffer.data()), inBuffer.size());

            const std::streamsize bytesRead = inputFile.gcount();

            payloadSize += static_cast<uint32_t>(bytesRead);

            m_inputStream.avail_in = static_cast<uInt>(bytesRead);
            m_inputStream.next_in = inBuffer.data();

            do
            {
                m_inputStream.avail_out = static_cast<uInt>(outBuffer.size());
                m_inputStream.next_out = outBuffer.data();
                ret = inflate(&m_inputStream, Z_SYNC_FLUSH);

                switch (ret)
                {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;
                    [[fallthrough]];
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&m_inputStream);
                    return;
                }

                const size_t bytesDecompressed = outBuffer.size() - m_inputStream.avail_out;

                outputFile.write(reinterpret_cast<char*>(outBuffer.data()), bytesDecompressed);

                std::this_thread::yield();

            } while (m_inputStream.avail_out == 0u);

            const uint8_t percentage = static_cast<uint8_t>(static_cast<float>(payloadSize) * 100.0f / static_cast<float>(streamSize));

            if (m_onProgressChanged && completedPercentage != percentage)
            {
                completedPercentage = percentage;
                
                m_onProgressChanged(percentage);
            }

        } while (ret != Z_STREAM_END && !inputFile.eof());

        m_previousPayloadSize += payloadSize;

        if (ret == Z_STREAM_END && header.isLastPart)
        {
            inflateEnd(&m_inputStream);
        }
        
        outputFile.flush();

        info.outputStream = std::move(outputFile);
    }

    void BlitterUtils::RemoveLocksSync(const std::filesystem::path& i_inputPath, std::ofstream&& o_outputStream)
    {
        StatusInfo info{};
        info.inputPath = i_inputPath;

        Epilogue epilogue([this, &info]()
            {
                if (m_onStatusChanged)
                {
                    m_onStatusChanged(info);
                }
            });

        std::ifstream inputFile(i_inputPath, std::ios::in | std::ios::binary);
        std::ofstream outputFile = std::move(o_outputStream);

        if (!inputFile.is_open())
        {
            info.errorMessage = std::format(L"Can't open file: '{}'", i_inputPath.c_str());
            return;
        }

        if (!outputFile.is_open())
        {
            info.errorMessage = std::format(L"Can't write to resulting file");
            return;
        }

        inputFile.seekg(0u, std::ios::end);
        std::streampos fileSize = inputFile.tellg();
        inputFile.seekg(0u, std::ios::beg);

        Header header{};

        inputFile.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (!IsValidHeader(header))
        {
            info.errorMessage = std::format(L"Invalid or unknown image header in file: '{}'", i_inputPath.c_str());
            return;
        }

        constexpr size_t bufferSize = 16384u;

        std::vector<Bytef> inBuffer(bufferSize);
        std::vector<Bytef> decompressedBuffer(bufferSize);
        std::vector<Bytef> outBuffer(bufferSize);

        size_t streamSize = static_cast<size_t>(fileSize - inputFile.tellg());

        uint32_t payloadCrc = 0u;
        uint32_t payloadSize = 0;
        uint32_t readSize = 0;
        int ret_inf = Z_OK;
        int ret_def = Z_OK;
        
        outputFile.write(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.partIdx == 0u)
        {
            uint32_t format = 0u;
            inputFile.read(reinterpret_cast<char*>(&format), sizeof(format));
            m_isCompressed = format == 0x42494C5A;// ZLIB value

            if (m_isCompressed)
            {
                readSize += 4u;

                outputFile.write(reinterpret_cast<char*>(&format), sizeof(format));
                payloadSize += 4u;

                payloadCrc = crc32(payloadCrc, reinterpret_cast<const Bytef*>(&format), sizeof(format));

                ret_def = deflateInit2(&m_outputStream, Z_BEST_SPEED, Z_DEFLATED, 14, 7, Z_DEFAULT_STRATEGY);
            }
            else
            {
                inputFile.seekg(0x200, std::ios::beg);
            }
        }

        info.requireNextRemoveLocks = !header.isLastPart;
        info.nextPartIdx = header.isLastPart ? 0u : header.partIdx + 1u;
        
        bool pendingLocksProcess = header.partIdx == 0u;

        if (!m_isCompressed)
        {
            inputFile.read(reinterpret_cast<char*>(outBuffer.data()), outBuffer.size());

            if (pendingLocksProcess)
            {
                PatchLocks(outBuffer);
                pendingLocksProcess = false;
            }

            std::streamsize bytesRead = inputFile.gcount();
            payloadCrc = crc32(payloadCrc, outBuffer.data(), static_cast<uInt>(bytesRead));
            payloadSize += static_cast<uint32_t>(bytesRead);
            outputFile.write(reinterpret_cast<char*>(outBuffer.data()), bytesRead);

            while (!inputFile.eof())
            {
                inputFile.read(reinterpret_cast<char*>(outBuffer.data()), outBuffer.size());
                bytesRead = inputFile.gcount();
                payloadCrc = crc32(payloadCrc, outBuffer.data(), static_cast<uInt>(bytesRead));
                payloadSize += static_cast<uint32_t>(bytesRead);
                outputFile.write(reinterpret_cast<char*>(outBuffer.data()), bytesRead);
            }

            header.payloadCRC = ~payloadCrc;
            header.payloadSize = payloadSize;
            header.previousPayloadSize = m_previousPayloadSize;

            constexpr uint8_t headerDataOffset = 0x08;
            const uint32_t headerCrc = crc32(0u, reinterpret_cast<const Bytef*>(&header) + headerDataOffset, sizeof(header) - headerDataOffset);
            header.headerCRC = ~headerCrc;

            m_previousPayloadSize += payloadSize;

            outputFile.seekp(0u, std::ios::beg);
            outputFile.write(reinterpret_cast<char*>(&header), sizeof(header));

            outputFile.flush();

            info.outputStream = std::move(outputFile);
            return;
        }

        uint8_t completedPercentage = 0u;

        do
        {
            inputFile.read(reinterpret_cast<char*>(inBuffer.data()), inBuffer.size());

            const std::streamsize bytesRead = inputFile.gcount();

            readSize += static_cast<uint32_t>(bytesRead);

            m_inputStream.avail_in = static_cast<uInt>(bytesRead);
            m_inputStream.next_in = inBuffer.data();

            do
            {
                m_inputStream.avail_out = static_cast<uInt>(decompressedBuffer.size());
                m_inputStream.next_out = decompressedBuffer.data();
                ret_inf = inflate(&m_inputStream, Z_SYNC_FLUSH);

                switch (ret_inf)
                {
                case Z_NEED_DICT:
                    ret_inf = Z_DATA_ERROR;
                    [[fallthrough]];
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&m_inputStream);
                    info.errorMessage = std::format(L"Error during file '{}' decompression.", i_inputPath.c_str());
                    return;
                }

                const uInt bytesDecompressed = static_cast<uInt>(decompressedBuffer.size()) - m_inputStream.avail_out;

                if (pendingLocksProcess)
                {
                    PatchLocks(decompressedBuffer);
                    pendingLocksProcess = false;
                }

                m_outputStream.next_in = decompressedBuffer.data();
                m_outputStream.avail_in = bytesDecompressed;

                do
                {
                    m_outputStream.next_out = outBuffer.data();
                    m_outputStream.avail_out = static_cast<uInt>(outBuffer.size());

                    ret_def = deflate(&m_outputStream, Z_NO_FLUSH);

                    const uInt bytesCompressed = static_cast<uInt>(outBuffer.size()) - m_outputStream.avail_out;
                    payloadSize += bytesCompressed;

                    payloadCrc = crc32(payloadCrc, outBuffer.data(), bytesCompressed);

                    outputFile.write(reinterpret_cast<char*>(outBuffer.data()), bytesCompressed);

                } while (m_outputStream.avail_out == 0u);
            } while (m_inputStream.avail_out == 0u);

            const uint8_t percentage = static_cast<uint8_t>(static_cast<float>(readSize) * 100.0f / static_cast<float>(streamSize));

            if (m_onProgressChanged && completedPercentage != percentage)
            {
                completedPercentage = percentage;

                m_onProgressChanged(percentage);
            }

        } while (ret_inf != Z_STREAM_END && (!inputFile.eof() || m_inputStream.avail_in > 0));

        m_outputStream.next_out = outBuffer.data();
        m_outputStream.avail_out = static_cast<uInt>(outBuffer.size());

        int flush = header.isLastPart ? Z_FINISH : Z_SYNC_FLUSH;

        ret_def = deflate(&m_outputStream, flush);

        const uInt bytesCompressed = static_cast<uInt>(outBuffer.size()) - m_outputStream.avail_out;
        payloadSize += bytesCompressed;

        payloadCrc = crc32(payloadCrc, outBuffer.data(), bytesCompressed);
        outputFile.write(reinterpret_cast<char*>(outBuffer.data()), bytesCompressed);

        header.payloadCRC = ~payloadCrc;
        header.payloadSize = payloadSize;
        header.previousPayloadSize = m_previousPayloadSize;

        constexpr uint8_t headerDataOffset = 0x08;
        const uint32_t headerCrc = crc32(0u, reinterpret_cast<const Bytef*>(&header) + headerDataOffset, sizeof(header) - headerDataOffset);
        header.headerCRC = ~headerCrc;
        
        m_previousPayloadSize += payloadSize;

        outputFile.seekp(0u, std::ios::beg);
        outputFile.write(reinterpret_cast<char*>(&header), sizeof(header));

        if (ret_inf == Z_STREAM_END && header.isLastPart)
        {
            inflateEnd(&m_inputStream);
            deflateEnd(&m_outputStream);
        }

        outputFile.flush();

        info.outputStream = std::move(outputFile);
    }

    void BlitterUtils::PatchLocks(std::vector<Bytef>& io_buffer) const
    {
        uint32_t version = 0u;
        memcpy(&version, io_buffer.data(), sizeof(version));

        if (version == 0x5F5B5465)
        {
            const uint32_t offset = 0x0C;
            const uint32_t size = 0x200;
            memset(io_buffer.data() + offset, 0x00, size);
        }
        else if (version == 0x606C6576)
        {
            const uint32_t offset = 0x0C;
            const uint32_t size = 0x200;
            memset(io_buffer.data() + offset, 0x00, size);

            constexpr uint32_t conunterOffset = 0x0C + 0x200;
            uint8_t count = 0u;
            memcpy(&count, &io_buffer[conunterOffset], sizeof(count));
            for (uint32_t i = 0; i < count; i++)
            {
                const uint32_t off = 0x0C + 0x200 + 0x01 + (i * 0x20);

                memset(io_buffer.data() + off, 0x00, 0x20);
            }
        }
        else if (version == 0x4E4A4354)
        {
            const uint32_t offset = 0x0C;
            const uint32_t size = 0x40;
            memset(io_buffer.data() + offset, 0x00, size);
        }
        else
        {
            // Unsupported lock version
        }
    }
    
    bool BlitterUtils::IsValidHeader(const Header& i_header) const
    {
        if (i_header.magic != k_magic)
        {
            return false;
        }

        constexpr size_t crcOffset = offsetof(Header, payloadCRC);

        const uint32_t headerCrc = ~crc32(0u, reinterpret_cast<const Bytef*>(&i_header) + crcOffset, sizeof(i_header) - crcOffset);

        if (i_header.headerCRC != headerCrc)
        {
            return false;
        }

        return true;
    }
}