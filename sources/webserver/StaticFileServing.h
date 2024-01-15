#pragma once

#include "QtHttpReply.h"

class CgiHandler;
class QtHttpRequest;


class StaticFileServing : public QObject
{
	Q_OBJECT

public:
	explicit StaticFileServing(QObject* parent = nullptr);
	~StaticFileServing() override;

	void setBaseUrl(const QString& url);
	void setSsdpXmlDesc(const QString& desc);

public slots:
	void onRequestNeedsReply(QtHttpRequest* request, QtHttpReply* reply);

private:
	QString         _baseUrl;
	QHash<QString, QString> _mimeDb;
	CgiHandler*     _cgi;
	Logger*         _log;
	QString         _ssdpXmlDesc;

	void printErrorToReply(QtHttpReply* reply, QtHttpReply::StatusCode code, QString errorMessage);
	QString getMimeName(QString filename);
};
