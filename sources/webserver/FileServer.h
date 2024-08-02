#pragma once

#include "QtHttpReply.h"

class CgiHandler;
class QtHttpRequest;

class FileServer
{
public:
	FileServer();
	virtual ~FileServer();

	static void setBaseUrl(const QString& url);
	static void setSsdpXmlDesc(const QString& desc);
	void onRequestNeedsReply(QtHttpRequest* request, QtHttpReply* reply);

private:
	static QString			_ssdpXmlDesc;
	static QString			_baseUrl;
	QHash<QString, QString> _mimeDb;
	Logger*					_log;	
	QString					_resourcePath;

	void printErrorToReply(QtHttpReply* reply, QtHttpReply::StatusCode code, QString errorMessage);
	QString getMimeName(QString filename);
};
