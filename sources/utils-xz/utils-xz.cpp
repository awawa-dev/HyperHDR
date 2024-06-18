#include <iostream>
#include <fstream>
#include <vector>
#include <lzma.h>
#include <utils-xz/utils-xz.h>

_XZ_SHARED_API const char* DecompressXZ(size_t downloadedDataSize, const uint8_t* downloadedData, const char* fileName)
{
	size_t outSize = 67174456;
	std::vector<uint8_t> outBuffer;
	const char* error = nullptr;

	std::ofstream file;
	file.open(fileName, std::ios::out | std::ios::trunc | std::ios::binary);

	if (!file.is_open())
	{
		return "Could not open file for writing";
	}

	try
	{
		outBuffer.resize(outSize);
	}
	catch(...)	
	{
		error = "Could not allocate buffer";
	}

	if (error == nullptr)
	{
		const uint32_t flags = LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED;
		lzma_stream strm = LZMA_STREAM_INIT;
		strm.next_in = downloadedData;
		strm.avail_in = downloadedDataSize;
		lzma_ret lzmaRet = lzma_stream_decoder(&strm, outSize, flags);
		if (lzmaRet == LZMA_OK)
		{
			do {
				strm.next_out = outBuffer.data();
				strm.avail_out = outSize;
				lzmaRet = lzma_code(&strm, LZMA_FINISH);
				if (lzmaRet == LZMA_MEMLIMIT_ERROR)
				{
					outSize = lzma_memusage(&strm);
					try
					{
						outBuffer.resize(outSize);
					}
					catch (...)
					{
						error = "Could not increase buffer size";
						break;
					}
					lzma_memlimit_set(&strm, outSize);
					strm.avail_out = 0;
				}
				else if (lzmaRet != LZMA_OK && lzmaRet != LZMA_STREAM_END)
				{
					error = "LZMA decoder returned error";
					break;
				}
				else
				{
					std::streamsize toWrite = static_cast<std::streamsize>(outSize - strm.avail_out);
					file.write(reinterpret_cast<char*>(outBuffer.data()), toWrite);
				}
			} while (strm.avail_out == 0 && lzmaRet != LZMA_STREAM_END);
			file.flush();
		}
		else
		{
			error = "Could not initialize LZMA decoder";
		}

		lzma_end(&strm);
	}

	file.close();

	if (error != nullptr)
		std::remove(fileName);

	return error;
}
