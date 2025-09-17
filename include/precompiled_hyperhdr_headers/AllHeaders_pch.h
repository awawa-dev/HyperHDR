#ifndef PCH_ENABLED
	#define PCH_ENABLED

	#include <QByteArray>
	#include <QBuffer>
	#include <QDateTime>
	#include <QDir>
	#include <QFile>
	#include <QFileInfo>
	#include <QHash>
	#include <QHostAddress>
	#include <QJsonArray>
	#include <QJsonDocument>
	#include <QJsonObject>
	#include <QJsonValue>
	#include <QList>
	#include <QLocalSocket>
	#include <QMap>
	#include <QMetaType>
	#include <QMultiMap>
	#include <QMutex>
	#include <QMutexLocker>
	#include <QNetworkReply>
	#include <QPair>	
	#include <QRectF>
	#include <QRegularExpression>
	#include <QSemaphore>	
	#include <QSet>
	#include <QSize>
	#include <QSocketNotifier>
	#include <QStack>
	#include <QString>
	#include <QStringList>
	#include <QTcpSocket>
	#include <QThread>
	#include <QThreadStorage>
	#include <QTimer>
	#include <QUdpSocket>
	#include <QUrl>
	#include <QUrlQuery>
	#include <QVariant>
	#include <QVector>
	
	#include <algorithm>
	#include <atomic>
	#include <cassert>
	#include <cfloat>
	#include <chrono>
	#include <cmath>
	#include <cstdio>
	#include <cstdarg>
	#include <cstdlib>
	#include <cstdint>
	#include <cstring>
	#include <ctime>
	#include <iostream>
	#include <functional>
	#include <limits>
	#include <list>
	#include <map>
	#include <memory>
	#include <mutex>
	#include <set>
	#include <sstream>
	#include <stdexcept>
	#include <utility>
	#include <vector>
	#include <iomanip>
	#include <optional>
	#include <type_traits>

	#if __has_include(<format>)
		#include <format>
		#if !(defined(__cpp_lib_format) && ( !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) || (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 130300) ))
			#if defined(__APPLE__)
				#warning "********** Warning: <format> found, but C++20 features incomplete/disabled. Falling back to basic logging. **********"
			#else
				#pragma message("********** Warning: <format> found, but C++20 features incomplete/disabled. Falling back to basic logging. **********")
			#endif
		#endif
	#else
		#if defined(__APPLE__)
			#warning "********** Warning: <format> header not found. Falling back to basic logging. **********"
		#else
			#pragma message("********** Warning: <format> header not found. Falling back to basic logging. **********")
		#endif
	#endif

#endif // PCH_ENABLED
