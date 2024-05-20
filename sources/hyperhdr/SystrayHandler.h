/* SystrayHandler.h
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

#pragma once

#ifndef PCH_ENABLED		
	#include <memory>
	#include <vector>
#endif

#include <base/HyperHdrInstance.h>
#include <base/HyperHdrManager.h>
#include <systray/Systray.h>
class HyperHdrDaemon;


class SystrayHandler : public QObject
{
	Q_OBJECT

public:
	SystrayHandler(HyperHdrDaemon* hyperhdrDaemon, quint16 webPort, QString rootFolder);
	~SystrayHandler();
	bool isInitialized();

public slots:
	void setColor(ColorRgb color);
	void settings();
	void setEffect(QString effect);
	void clearEfxColor();	
	void loop();
	void close();
	void setAutorunState();
	void selectInstance();

private slots:
	void signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name);
	void signalSettingsChangedHandler(settings::type type, const QJsonDocument& data);
	void createSystray();

private:

	#ifdef _WIN32
		bool getCurrentAutorunState();
	#endif
	std::unique_ptr<SystrayMenu>	_menu;
	bool							_haveSystray;
	std::weak_ptr<HyperHdrManager>	_instanceManager;
	quint16							_webPort;
	QString							_rootFolder;
	int								_selectedInstance;
};

