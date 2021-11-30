#include <QStringBuilder>
#include <QUrlQuery>
#include <QFile>
#include <QByteArray>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QProcess>

#include "CgiHandler.h"
#include "QtHttpHeader.h"
#include <utils/FileUtils.h>

CgiHandler::CgiHandler(QObject* parent)
	: QObject(parent)
	, _reply(nullptr)
	, _request(nullptr)
	, _args(QStringList())
	, _baseUrl()
	, _log(Logger::getInstance("WEBSERVER"))
{
}

void CgiHandler::setBaseUrl(const QString& url)
{
	_baseUrl = url;
}

void CgiHandler::exec(const QStringList& args, QtHttpRequest* request, QtHttpReply* reply)
{
	_args = args;
	_request = request;
	_reply = reply;

	if (_args.at(0) == "cfg_jsonserver")
	{
		cmd_cfg_jsonserver();
	}	
	else
	{
		throw std::runtime_error("CGI command not found");
	}
}

void CgiHandler::cmd_cfg_jsonserver()
{
	quint16 jsonPort = 19444;
	// send result as reply
	_reply->addHeader("Content-Type", "text/plain");
	_reply->appendRawData(QByteArrayLiteral(":") % QString::number(jsonPort).toUtf8());
}

