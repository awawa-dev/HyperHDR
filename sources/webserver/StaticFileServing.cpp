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


#include <utils/Logger.h>

#include <utils/QStringUtils.h>
#include "QtHttpRequest.h"
#include "QtHttpReply.h"
#include "QtHttpHeader.h"
#include "CgiHandler.h"

#include "StaticFileServing.h"

StaticFileServing::StaticFileServing(QObject* parent)
	: QObject(parent)
	, _baseUrl()
	, _cgi(new CgiHandler(this))
	, _log(Logger::getInstance("WEBSERVER"))
{
	Q_INIT_RESOURCE(WebConfig);

	_mimeDb["js"] = "application/javascript";
	_mimeDb["html"] = "text/html";
	_mimeDb["json"] = "application/json";
	_mimeDb["css"] = "text/css";
	_mimeDb["png"] = "image/png";
	_mimeDb["woff"] = "font/woff";
	_mimeDb["svg"] = "image/svg+xml";
	_mimeDb["jpg"] = "image/jpeg";
	_mimeDb["jpeg"] = "image/jpeg";
}

StaticFileServing::~StaticFileServing()
{
	delete _cgi;
}

void StaticFileServing::setBaseUrl(const QString& url)
{
	_baseUrl = url;
	_cgi->setBaseUrl(url);
}

void StaticFileServing::setSsdpXmlDesc(const QString& desc)
{
	if (desc.isEmpty())
	{
		Warning(_log, "SSDP description is empty");
		_ssdpXmlDesc.clear();
	}
	else
	{
		Debug(_log, "SSDP description is set up");
		_ssdpXmlDesc = desc;
	}
}

void StaticFileServing::printErrorToReply(QtHttpReply* reply, QtHttpReply::StatusCode code, QString errorMessage)
{
	reply->setStatusCode(code);
	reply->addHeader("Content-Type", QByteArrayLiteral("text/html"));	
	reply->appendRawData(QString(QString::number(code) + " - " + errorMessage).toLocal8Bit());	
}

QString StaticFileServing::getMimeName(QString filename)
{
	QString extension = QFileInfo(filename).suffix();

	if (extension.isEmpty() || !_mimeDb.contains(extension))
		return QMimeDatabase().mimeTypeForFile(filename).name();
		
	return _mimeDb[extension];
}

void StaticFileServing::onRequestNeedsReply(QtHttpRequest* request, QtHttpReply* reply)
{
	QString command = request->getCommand();
	if (command == QStringLiteral("GET"))
	{
		QString path = request->getUrl().path();
		QStringList uri_parts = QStringUtils::SPLITTER(path, '/');
		// special uri handling for server commands
		if (!uri_parts.empty())
		{
			if (uri_parts.at(0) == "cgi")
			{
				uri_parts.removeAt(0);
				try
				{
					_cgi->exec(uri_parts, request, reply);
				}
				catch (std::exception& e)
				{
					Error(_log, "Exception while executing cgi %s :  %s", path.toStdString().c_str(), e.what());
					printErrorToReply(reply, QtHttpReply::InternalError, "script failed (" % path % ")");
				}
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
