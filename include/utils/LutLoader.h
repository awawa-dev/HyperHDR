#pragma once

#ifndef PCH_ENABLED
	#include <QString>
	#include <QList>
#endif

#include <utils/PixelFormat.h>
#include <image/MemoryBuffer.h>
#include <utils/Logger.h>

class LutLoader {
	public:
		int		_hdrToneMappingEnabled = 0;
		bool	_lutBufferInit = false;

		MemoryBuffer<uint8_t>	_lut;		

		void loadLutFile(Logger* _log, PixelFormat color, const QList<QString>& files);
};
