/* FileServer.cpp
*
*  MIT License
*
*  Copyright (c) 2020-2024 awawa-dev
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

#include <QTcpSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QUrlQuery>
#include <QMimeDatabase>
#include <QStringBuilder>
#include <QUrlQuery>
#include <QList>
#include <QPair>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <exception>
#include <QHash>
#include <QDir>
#include <QCoreApplication>


#include <utils/Logger.h>

#include "QtHttpRequest.h"
#include "QtHttpReply.h"
#include "QtHttpHeader.h"

#include "FileServer.h"
#include "HyperhdrConfig.h"

QString         FileServer::_baseUrl;
QString         FileServer::_ssdpXmlDesc;

FileServer::FileServer():
	_log(Logger::getInstance("WEBSERVER"))
{
#if defined(USE_EMBEDDED_WEB_RESOURCES)
	Q_INIT_RESOURCE(web_resources);
#else
	_resourcePath = QDir(qApp->applicationDirPath()).filePath("../lib/web_resources.rcc");
	if (!QResource::registerResource(_resourcePath))
	{		
		Error(_log, "Could not initialize web server resources: %s", QSTRING_CSTR(_resourcePath));
		_resourcePath = "";
	}
	else
	{
		Info(_log, "Web server resources initialized: %s", QSTRING_CSTR(_resourcePath));
	}
#endif

	_mimeDb["js"] = "application/javascript";
	_mimeDb["html"] = "text/html";
	_mimeDb["json"] = "application/json";
	_mimeDb["css"] = "text/css";
	_mimeDb["png"] = "image/png";
	_mimeDb["woff"] = "font/woff";
	_mimeDb["svg"] = "image/svg+xml";
	_mimeDb["jpg"] = "image/jpeg";
	_mimeDb["jpeg"] = "image/jpeg";
	_mimeDb["woff2"] = "font/woff2";	
}

FileServer::~FileServer()
{
	#if !defined(USE_EMBEDDED_WEB_RESOURCES)
	if (!_resourcePath.isEmpty())
	{
		Info(_log, "Web server resources released: %s", QSTRING_CSTR(_resourcePath));
		QResource::unregisterResource(_resourcePath);
	}
	#endif
}

void FileServer::setBaseUrl(const QString& url)
{
	_baseUrl = url;
}

void FileServer::setSsdpXmlDesc(const QString& desc)
{
	if (desc.isEmpty())
	{
		_ssdpXmlDesc.clear();
	}
	else
	{
		_ssdpXmlDesc = desc;
	}
}

void FileServer::printErrorToReply(QtHttpReply* reply, QtHttpReply::StatusCode code, QString errorMessage)
{
	reply->setStatusCode(code);
	reply->addHeader("Content-Type", QByteArrayLiteral("text/html"));	
	reply->appendRawData(QString(QString::number(code) + " - " + errorMessage).toLocal8Bit());	
}

QString FileServer::getMimeName(QString filename)
{
	QString extension = QFileInfo(filename).suffix();

	if (extension.isEmpty() || !_mimeDb.contains(extension))
		return QMimeDatabase().mimeTypeForFile(filename).name();
		
	return _mimeDb[extension];
}

void FileServer::onRequestNeedsReply(QtHttpRequest* request, QtHttpReply* reply)
{
	QString command = request->getCommand();
	if (command == QStringLiteral("GET"))
	{
		QString path = request->getUrl().path();
		QStringList uri_parts = path.split('/', Qt::SkipEmptyParts);
		// special uri handling for server commands
		if (!uri_parts.empty())
		{
			if (uri_parts.at(0) == "hyperhdr_heart_beat")
			{
				reply->addHeader("Content-Type", "text/plain");
				reply->appendRawData("alive");
				return;
			}
			else if (uri_parts.at(0) == "description.xml" && !_ssdpXmlDesc.isEmpty())
			{
				reply->addHeader("Content-Type", "text/xml");
				reply->appendRawData(_ssdpXmlDesc.toLocal8Bit());
				return;
			}
		}

		QFileInfo info(_baseUrl % "/" % path);
		if (path == "/" || path.isEmpty())
		{
			path = "index.html";
		}
		else if (info.isDir() && path.endsWith("/"))
		{
			path += "index.html";
		}
		else if (info.isDir() && !path.endsWith("/"))
		{
			path += "/index.html";
		}

		// get static files
		QFile file(_baseUrl % "/" % path);
		if (file.exists())
		{
			QString mimeName = getMimeName(file.fileName());
			if (file.open(QFile::ReadOnly)) {
				QByteArray data = file.readAll();
				reply->addHeader("Content-Type", mimeName.toLocal8Bit());
				reply->addHeader(QtHttpHeader::AccessControlAllow, "*");
				reply->appendRawData(data);
				file.close();
			}
			else
			{
				printErrorToReply(reply, QtHttpReply::Forbidden, "Requested file: " % path);
			}
		}
		else
		{
			printErrorToReply(reply, QtHttpReply::NotFound, "Requested file: " % path);
		}
	}
	else
	{
		printErrorToReply(reply, QtHttpReply::MethodNotAllowed, "Unhandled HTTP/1.1 method " % command);
	}
}
