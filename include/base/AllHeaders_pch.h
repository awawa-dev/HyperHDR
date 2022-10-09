#ifndef ALLHEADERS_PCH_H_
#define ALLHEADERS_PCH_H_

#include <QAtomicInteger>
#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QExplicitlySharedDataPointer>
#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>
#include <QPainter>
#include <QRectF>
#include <QRegularExpression>
#include <QResource>
#include <QSet>
#include <QSize>
#include <QSocketNotifier>
#include <QStack>
#include <QString>
#include <QStringList>
#include <QSemaphore>
#include <QSharedData>
#include <QQueue>
#include <QTcpSocket>
#include <QTimer>
#include <QTextStream>
#include <QThread>
#include <QThreadStorage>
#include <QUdpSocket>
#include <QUuid>
#include <QVariant>
#include <QVector>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QStringView>
#else
#include <QStringRef>
#endif

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <functional>
#include <iterator>
#include <inttypes.h>
#include <iostream>
#include <limits>
#include <list>
#include <stdexcept>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sstream>
#include <map>
#include <math.h>
#include <memory>
#include <type_traits>
#include <vector>


#endif // ALLHEADERS_PCH_H_
