#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <zstd.h>
#include <cstring>
#include <utils-zstd/utils-zstd.h>

_ZSTD_SHARED_API const char* DecompressZSTD(size_t downloadedDataSize, const uint8_t* downloadedData, const char* fileName)
{
	const size_t SINGLE_LUT_SIZE = (static_cast<size_t>(256 * 256 * 256) * 3);

	size_t outSize = SINGLE_LUT_SIZE / 2;
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
		ZSTD_DCtx* const dctx = ZSTD_createDCtx();

		if (dctx == NULL)
		{
			error = "ZSTD_createDCtx() failed!";
		}
		else
		{
			int total = SINGLE_LUT_SIZE * 3;
			ZSTD_inBuffer input = { downloadedData, downloadedDataSize, 0 };
			while (input.pos < input.size)
			{
				ZSTD_outBuffer outputSeek = { outBuffer.data(), outBuffer.size(), 0 };
				size_t const ret = ZSTD_decompressStream(dctx, &outputSeek, &input);
				if (ZSTD_isError(ret) || outputSeek.pos != outBuffer.size() || total < 0)
				{
					error = "Error during decompression";
					break;
				}
				else
				{
					file.write(reinterpret_cast<char*>(outBuffer.data()), outBuffer.size());
				}
				total -= outBuffer.size();
			}
			ZSTD_freeDCtx(dctx);
		}
	}

	file.close();

	if (error != nullptr)
		std::remove(fileName);
		
	return error;
}

_ZSTD_SHARED_API const char* DecompressZSTD(size_t downloadedDataSize, const uint8_t* downloadedData, uint8_t* dest, int destSeek, int destSize)
{
	const char* error = nullptr;

	ZSTD_DCtx* const dctx = ZSTD_createDCtx();

	if (dctx == NULL)
	{
		error = "ZSTD_createDCtx() failed!";
	}
	else
	{
		int totalOutput = 0;
		ZSTD_inBuffer input = { downloadedData, downloadedDataSize, 0 };
		while (totalOutput <= destSeek && input.pos < input.size)
		{
			ZSTD_outBuffer outputSeek = { dest, static_cast<size_t>(destSize), static_cast<size_t>(0) };
			size_t const ret = ZSTD_decompressStream(dctx, &outputSeek, &input);
			if (ZSTD_isError(ret) || outputSeek.pos != destSize)
			{
				error = "Error during decompression";
				break;
			}
			totalOutput += outputSeek.pos;
		}
		ZSTD_freeDCtx(dctx);
	}
	
	return error;
}
