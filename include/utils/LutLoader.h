#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QList>
	#include <QFile>
#endif

#include <utils/PixelFormat.h>
#include <image/MemoryBuffer.h>
#include <utils/Logger.h>

class LutLoader {
	public:
		int		_hdrToneMappingEnabled = 0;
		int		_loadedHdrToneMappingMode = -1;
		bool	_lutBufferInit = false;

		MemoryBuffer<uint8_t>	_lut;		
		QString _loadedLutFile;
		bool _loadedLutCompressed = false;

		void loadLutFile(const LoggerName& _log, PixelFormat color, const QList<QString>& files, bool useMemoryCache = false);
		QString getLoadedLutFile() const { return _loadedLutFile; }
		bool isLoadedLutCompressed() const { return _loadedLutCompressed; }
		static int getMemoryCacheEntryCount();
		static int getMemoryCacheMaxEntries();
		static void clearLutMemoryCache();
	private:
		bool loadCachedLut(const QString& cacheKey, const QString& loadedFile, bool compressed);
		void storeCachedLut(const QString& cacheKey);
		bool decompressLut(const LoggerName& _log, QFile& file, int index);
		void hasher(int index, const LoggerName& _log);
};
