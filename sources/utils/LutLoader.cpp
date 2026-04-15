/* LutLoader.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/
#include <HyperhdrConfig.h>
#include <utils/LutLoader.h>
#include <image/ColorRgb.h>
#include <utils/FrameDecoder.h>
#include <utils/InternalClock.h>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#ifdef __linux__
	#include <unistd.h>
#endif

#ifdef ENABLE_ZSTD
	#include <utils-zstd/utils-zstd.h>
#endif


namespace {
	const int LUT_FILE_SIZE = 256 * 256 * 256 *3;
	const int LUT_MEMORY_ALIGN = 64;

	// Reserve at least this much system RAM for OS + HyperHDR + headroom.
	const qint64 RESERVED_SYSTEM_BYTES = static_cast<qint64>(512) * 1024 * 1024;  // 512 MB
	// Never use more than this fraction of total physical RAM for the LUT cache.
	const double  MAX_RAM_FRACTION = 0.25;
	// Absolute floor: always allow at least 2 cached entries.
	const int     MIN_CACHE_ENTRIES = 2;

	struct LutCacheEntry
	{
		QByteArray data;
		QString loadedFile;
		bool compressed = false;
	};

	QHash<QString, LutCacheEntry>& lutMemoryCache()
	{
		static QHash<QString, LutCacheEntry> cache;
		return cache;
	}

	// Insertion-order list for LRU eviction (oldest at front).
	QList<QString>& lutCacheInsertionOrder()
	{
		static QList<QString> order;
		return order;
	}

	QMutex& lutMemoryCacheMutex()
	{
		static QMutex mutex;
		return mutex;
	}

	qint64 getTotalPhysicalMemory()
	{
#ifdef __linux__
		long pages = sysconf(_SC_PHYS_PAGES);
		long pageSize = sysconf(_SC_PAGE_SIZE);
		if (pages > 0 && pageSize > 0)
			return static_cast<qint64>(pages) * static_cast<qint64>(pageSize);
#endif
		// Fallback: assume 1 GB so the cap is still conservative.
		return static_cast<qint64>(1024) * 1024 * 1024;
	}

	int computeMaxCacheEntries()
	{
		const qint64 totalRam = getTotalPhysicalMemory();
		const qint64 budgetByFraction = static_cast<qint64>(totalRam * MAX_RAM_FRACTION);
		const qint64 budgetByReserve  = totalRam - RESERVED_SYSTEM_BYTES;
		const qint64 budget = qMin(budgetByFraction, qMax(budgetByReserve, static_cast<qint64>(0)));
		int maxEntries = static_cast<int>(budget / LUT_FILE_SIZE);
		return qMax(maxEntries, MIN_CACHE_ENTRIES);
	}
}

void LutLoader::loadLutFile(const LoggerName& _log, PixelFormat color, const QList<QString>& files, bool useMemoryCache)
{
	bool is_yuv = (color == PixelFormat::YUYV);

	_lutBufferInit = false;
	_loadedLutFile.clear();
	_loadedLutCompressed = false;

	if (color != PixelFormat::RGB24 && color != PixelFormat::YUYV)
	{
		if (_log.size()) Error(_log, "Unsupported mode for loading LUT table: {:s}", (pixelFormatToString(color)));
		return;
	}

	if (_hdrToneMappingEnabled || is_yuv)
	{
		// Pre-compute the LUT index once — it depends only on format+mode, not the file.
		int index = 0;
		if (is_yuv && _hdrToneMappingEnabled)
			index = LUT_FILE_SIZE;
		else if (is_yuv)
			index = LUT_FILE_SIZE * 2;

		// Fast path: check the memory cache BEFORE any file I/O so that
		// previously-loaded DV / HDR bucket LUTs are served entirely
		// from RAM without touching the SD card or network filesystem.
		if (useMemoryCache)
		{
			for (const QString& fileName3d : files)
			{
				for (const QString& variant : { QStringLiteral("raw"), QStringLiteral("zst") })
				{
					const QString cacheKey = QStringLiteral("%1|%2|%3")
						.arg(fileName3d)
						.arg(variant)
						.arg(index);
					const bool isZst = (variant == QStringLiteral("zst"));
					if (loadCachedLut(cacheKey, isZst ? (fileName3d + ".zst") : fileName3d, isZst))
					{
						if (_log.size())
							Info(_log, "Loaded LUT from memory cache (no disk I/O): '{:s}' [{:s}]", (fileName3d), (variant));
						return;
					}
				}
			}
		}

		for (QString fileName3d : files)
		{
			QFile file(fileName3d);
			bool compressed = false;

			if (file.open(QIODevice::ReadOnly)
				#ifdef ENABLE_ZSTD
					|| [&]() {file.setFileName(fileName3d + ".zst"); compressed = true; return file.open(QIODevice::ReadOnly); } ()
				#endif
				)
			{
				int length;

				if (_log.size())
				{
					Debug(_log, "LUT file found: {:s} ({:s})", (file.fileName()), (compressed) ? "compressed" : "uncompressed");
				}

				length = file.size();

				if ((length == LUT_FILE_SIZE * 3) || compressed)
				{
					if (is_yuv && _hdrToneMappingEnabled)
					{
						if (_log.size()) Debug(_log, "Index 1 for HDR YUV");
					}
					else if (is_yuv)
					{
						if (_log.size()) Debug(_log, "Index 2 for YUV");
					}
					else
					{
						if (_log.size()) Debug(_log, "Index 0 for HDR RGB");
					}

					const QString cacheKey = QStringLiteral("%1|%2|%3")
						.arg(fileName3d)
						.arg(compressed ? QStringLiteral("zst") : QStringLiteral("raw"))
						.arg(index);

					_lut.resize(LUT_FILE_SIZE + LUT_MEMORY_ALIGN);

					if (!compressed)
					{
						file.seek(index);

						if (file.read((char*)_lut.data(), LUT_FILE_SIZE) != LUT_FILE_SIZE)
						{
							if (_log.size()) Error(_log, "Error reading LUT file {:s}", (fileName3d));
						}
						else
						{
							_lutBufferInit = true;
							_loadedHdrToneMappingMode = _hdrToneMappingEnabled;
							_loadedLutFile = file.fileName();
							_loadedLutCompressed = false;
							if (useMemoryCache)
								storeCachedLut(cacheKey);
							if (_log.size()) Info(_log, "Found and loaded LUT: '{:s}'", (fileName3d));
						}
					}
					else
					{
						_lutBufferInit = decompressLut(_log, file, index);
						if (_lutBufferInit)
						{
							_loadedHdrToneMappingMode = _hdrToneMappingEnabled;
							_loadedLutFile = file.fileName();
							_loadedLutCompressed = true;
							if (useMemoryCache)
								storeCachedLut(cacheKey);
						}
						if (_log.size())
						{
							if (_lutBufferInit) Info(_log, "Found and loaded LUT: '{:s}'", (fileName3d));
							else Error(_log, "Error reading LUT file {:s}", (fileName3d));
						}
					}

					// hasher(index / LUT_FILE_SIZE, _log);
				}
				else
				{
					if (_log.size()) Error(_log, "LUT file has invalid length: {:d} vs {:d} => {:s}", length, (LUT_FILE_SIZE * 3), (fileName3d));
				}

				file.close();

				return;
			}
			else
			{
				if (_log.size()) Warning(_log, "LUT file is not found here: {:s}", (fileName3d));
			}
		}

		if (_log.size()) Error(_log, "Could not find any required LUT file");
	}
}

int LutLoader::getMemoryCacheEntryCount()
{
	QMutexLocker locker(&lutMemoryCacheMutex());
	return lutMemoryCache().size();
}

void LutLoader::clearLutMemoryCache()
{
	QMutexLocker locker(&lutMemoryCacheMutex());
	lutMemoryCache().clear();
	lutCacheInsertionOrder().clear();
}

int LutLoader::getMemoryCacheMaxEntries()
{
	static int maxEntries = computeMaxCacheEntries();
	return maxEntries;
}

bool LutLoader::loadCachedLut(const QString& cacheKey, const QString& loadedFile, bool compressed)
{
	QMutexLocker locker(&lutMemoryCacheMutex());
	const auto it = lutMemoryCache().constFind(cacheKey);
	if (it == lutMemoryCache().constEnd())
		return false;

	const LutCacheEntry& entry = it.value();
	if (entry.data.size() != LUT_FILE_SIZE)
		return false;

	_lut.resize(LUT_FILE_SIZE + LUT_MEMORY_ALIGN);
	memcpy(_lut.data(), entry.data.constData(), static_cast<size_t>(LUT_FILE_SIZE));
	_lutBufferInit = true;
	_loadedHdrToneMappingMode = _hdrToneMappingEnabled;
	_loadedLutFile = entry.loadedFile.isEmpty() ? loadedFile : entry.loadedFile;
	_loadedLutCompressed = entry.compressed || compressed;
	return true;
}

void LutLoader::storeCachedLut(const QString& cacheKey)
{
	if (!_lutBufferInit || _lut.data() == nullptr)
		return;

	LutCacheEntry entry;
	entry.data = QByteArray(reinterpret_cast<const char*>(_lut.data()), LUT_FILE_SIZE);
	entry.loadedFile = _loadedLutFile;
	entry.compressed = _loadedLutCompressed;

	QMutexLocker locker(&lutMemoryCacheMutex());

	// If this key is already cached, remove it from the order list
	// so it will be re-appended as most-recently-used.
	if (lutMemoryCache().contains(cacheKey))
		lutCacheInsertionOrder().removeOne(cacheKey);

	// Evict oldest entries until we are within the memory-scaled cap.
	const int maxEntries = getMemoryCacheMaxEntries();
	while (lutMemoryCache().size() >= maxEntries && !lutCacheInsertionOrder().isEmpty())
	{
		const QString oldest = lutCacheInsertionOrder().takeFirst();
		lutMemoryCache().remove(oldest);
	}

	lutMemoryCache().insert(cacheKey, entry);
	lutCacheInsertionOrder().append(cacheKey);
}

bool LutLoader::decompressLut(const LoggerName& _log, QFile& file, int index)
{
	auto now = InternalClock::nowPrecise();
	[[maybe_unused]] const char* retVal = "HyperHDR was built without a support for ZSTD decoder";
	QByteArray compressedFile = file.readAll();
	#ifdef ENABLE_ZSTD
		retVal = DecompressZSTD(compressedFile.size(), reinterpret_cast<uint8_t*>(compressedFile.data()), _lut.data(), index, LUT_FILE_SIZE);
	#endif

	if (retVal != nullptr && _log.size())
	{
		Error(_log, "Error while decompressing LUT: {:s}", retVal);
	}

	if (_log.size()) Info(_log, "Decompression took {:f} seconds", (InternalClock::nowPrecise() - now) / 1000.0);

	return retVal == nullptr;
}

void LutLoader::hasher(int index, const LoggerName& _log)
{
	if (_log.size())
	{
		auto start = _lut.data();
		auto end = start + _lut.size();
		uint8_t position = 0;
		uint16_t fletcher1 = 0;
		uint16_t fletcher2 = 0;
		uint16_t fletcherExt = 0;
		while (start < end)
		{
			fletcher1 = (fletcher1 + (uint16_t)(*(start))) % 255;
			fletcher2 = (fletcher2 + fletcher1) % 255;
			fletcherExt = (fletcherExt + (*(start++) ^ (position++))) % 255;
		}
		Info(_log, "CRC for {:d} segment: 0x{:02X} 0x{:02X} 0x{:02X}", index, fletcher1, fletcher2, fletcherExt);
	}
}
