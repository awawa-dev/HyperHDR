#pragma once

#ifndef PCH_ENABLED
	#include <QObject>
	#include <QString>
	#include <QStringList>
#endif

class QtHttpReply;
class QtHttpRequest;
class Logger;


class CgiHandler : public QObject
{
	Q_OBJECT

public:
	CgiHandler(QObject* parent = nullptr);

	void setBaseUrl(const QString& url);
	void exec(const QStringList& args, QtHttpRequest* request, QtHttpReply* reply);

private:
	void cmd_cfg_jsonserver();

	QtHttpReply*		_reply;
	QtHttpRequest*		_request;
	QStringList         _args;
	QString             _baseUrl;
	Logger*				_log;
};
