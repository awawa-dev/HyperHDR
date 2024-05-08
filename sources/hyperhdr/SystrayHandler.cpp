/* SystrayHandler.cpp
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

#ifndef PCH_ENABLED
	#include <QColor>
	#include <QSettings>
	#include <list>
#endif

#ifndef _WIN32
	#include <unistd.h>
#endif

#include <QBuffer>
#include <QFile>
#include <QIcon>
#include <QPixmap>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QCloseEvent>
#include <QSettings>

#include <HyperhdrConfig.h>

#include <utils/ColorRgb.h>
#include <effectengine/EffectDefinition.h>
#include <webserver/WebServer.h>
#include <utils/Logger.h>
#include <systray/Systray.h>

#include "HyperHdrDaemon.h"
#include "SystrayHandler.h"

SystrayHandler::SystrayHandler(HyperHdrDaemon* hyperhdrDaemon, quint16 webPort)
	: QObject(),
	_menu(nullptr),
	_systray(nullptr),
	_hyperhdrHandle(),
	_webPort(webPort)
{
	Q_INIT_RESOURCE(resources);

	std::shared_ptr<HyperHdrManager> instanceManager;
	hyperhdrDaemon->getInstanceManager(instanceManager);
	_instanceManager = instanceManager;
	connect(instanceManager.get(), &HyperHdrManager::SignalInstanceStateChanged, this, &SystrayHandler::signalInstanceStateChangedHandler);
	connect(instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, this, &SystrayHandler::signalSettingsChangedHandler);

	_systray = std::unique_ptr<Systray>(new Systray());
	if (!_systray->initialize(nullptr))
	{
		_systray = nullptr;
	}
}

SystrayHandler::~SystrayHandler()
{
	printf("Releasing SysTray\n");
	if (_systray != nullptr)
		_systray->close();	
}

bool SystrayHandler::isInitialized()
{
	return _systray != nullptr;
}

void SystrayHandler::loop()
{
	if (_systray != nullptr)
		_systray->loop();
}

void SystrayHandler::close()
{
	if (_systray != nullptr)
	{
		_systray->close();
		_systray = nullptr;
	}
}

static void loadPng(std::unique_ptr<SystrayMenu>& menu, QString filename)
{
	QFile stream = QFile(filename);
	stream.open(QIODevice::ReadOnly);
	QByteArray ar = stream.readAll();
	stream.close();
	menu->icon.resize(ar.size());
	memcpy(menu->icon.data(), ar.data(), ar.size());
}

static void loadSvg(std::unique_ptr<SystrayMenu>& menu, QString filename)
{
	QImage source= (filename.indexOf(":/") == 0) ? QImage(filename) : QImage::fromData(filename.toUtf8(), "svg");	
	QImage image = source.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	QByteArray ar;
	QBuffer buffer(&ar);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "PNG"); // writes image into ba in PNG format
	menu->icon.resize(ar.size());
	memcpy(menu->icon.data(), ar.data(), ar.size());
}

void SystrayHandler::createSystray()
{
	if (_systray == nullptr)
		return;

	std::unique_ptr<SystrayMenu> mainMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);

	// main icon
	loadPng(mainMenu, ":/hyperhdr-icon-32px.png");

	// settings menu
	std::unique_ptr<SystrayMenu> settingsMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(settingsMenu, ":/settings.svg");
	settingsMenu->label = "&Settings";
	settingsMenu->context = this;
	settingsMenu->callback = [](SystrayMenu* m) {
		SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
		if (sh != nullptr)
			QUEUE_CALL_0(sh, settings);
	};

	// separator 1
	std::unique_ptr<SystrayMenu> separator1 = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	separator1->label = "-";

	// color menu
	std::unique_ptr<SystrayMenu> colorMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(colorMenu, ":/color.svg");
	colorMenu->label = "&Color";
	colorMenu->context = this;
	colorMenu->callback = [](SystrayMenu* m) {
		SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
		if (sh != nullptr)
			QUEUE_CALL_0(sh, settings);
	};

	std::list<std::string> colors{ "white", "red", "green", "blue", "yellow", "magenta", "cyan" };
	colors.reverse();
	for (const std::string& color : colors)
	{
		QString svgTemplate = "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"32\" height=\"32\" version=\"1.1\" viewBox=\"0 0 32 32\">"
			"<rect width=\"22\" height=\"22\" x=\"1\" y=\"1\" fill=\"%1\" stroke-width=\"2\" stroke=\"gray\" />"
			"</svg>";
		QString svg = QString(svgTemplate).arg(QString::fromStdString(color));
		
		std::unique_ptr<SystrayMenu> colorItem = std::unique_ptr<SystrayMenu>(new SystrayMenu);
		loadSvg(colorItem, svg);
		colorItem->label = color;
		colorItem->context = this;
		colorItem->callback = [](SystrayMenu* m) {
			SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
			QString colorName = QString::fromStdString(m->label);
			QColor color = QColor::fromString(colorName);
			if (sh != nullptr)
				QUEUE_CALL_1(sh, setColor, QColor, color);
		};

		std::swap(colorMenu->submenu, colorItem->next);
		std::swap(colorMenu->submenu, colorItem);
	}

	// effects
	std::list<EffectDefinition> efxs, efxsSorted;
	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		SAFE_CALL_0_RET(_hyperhdr.get(), getEffects, std::list<EffectDefinition>, efxs);

	std::unique_ptr<SystrayMenu> effectsMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(effectsMenu, ":/effects.svg");
	effectsMenu->label = "&Effects";


	efxs.sort([](const EffectDefinition& a, const EffectDefinition& b) {
		return a.name > b.name;
	});

	std::copy_if(efxs.begin(), efxs.end(),
		std::back_inserter(efxsSorted),
		[](const EffectDefinition& a) { return a.name.contains("Music:"); });

	std::copy_if(efxs.begin(), efxs.end(),
		std::back_inserter(efxsSorted),
		[](const EffectDefinition& a) { return !a.name.contains("Music:"); });


	for (const EffectDefinition& efx : efxsSorted)
	{
		QString effectName = efx.name;
		std::unique_ptr<SystrayMenu> effectItem = std::unique_ptr<SystrayMenu>(new SystrayMenu);
		effectItem->label = effectName.toStdString();
		effectItem->context = this;
		effectItem->callback = [](SystrayMenu* m) {
			SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
			QString effectName = QString::fromStdString(m->label);
			if (sh != nullptr)
				QUEUE_CALL_1(sh, setEffect, QString, effectName);
		};

		std::swap(effectsMenu->submenu, effectItem->next);
		std::swap(effectsMenu->submenu, effectItem);
	}

	// clear menu
	std::unique_ptr<SystrayMenu> clearMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(clearMenu, ":/clear.svg");
	clearMenu->label = "&Clear";
	clearMenu->context = this;
	clearMenu->callback = [](SystrayMenu* m) {
		SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
		if (sh != nullptr)
			QUEUE_CALL_0(sh, clearEfxColor);
	};

	// separator 2
	std::unique_ptr<SystrayMenu> separator2 = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	separator2->label = "-";

	// quit menu
	std::unique_ptr<SystrayMenu> quitMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(quitMenu, ":/quit.svg");
	quitMenu->label = "&Quit";
	quitMenu->context = this;
	quitMenu->callback = [](SystrayMenu* m) {
		SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
		if (sh != nullptr)
		{
			auto instance = QCoreApplication::instance();
			QUEUE_CALL_0(instance, quit);
		}
	};


#ifdef _WIN32
	std::unique_ptr<SystrayMenu> separator3 = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	separator3->label = "-";

	std::unique_ptr<SystrayMenu> autostartMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(autostartMenu, ":/autorun.svg");
	autostartMenu->label = (getCurrentAutorunState()) ? "Enable autostart" : "Disable autostart";
	autostartMenu->context = this;
	autostartMenu->callback = [](SystrayMenu* m) {
		SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
		if (sh != nullptr)
		{
			QUEUE_CALL_0(sh, setAutorunState);
		}
	};
#endif

	std::swap(separator2->next, quitMenu);
	std::swap(clearMenu->next, separator2);
	std::swap(effectsMenu->next, clearMenu);
	std::swap(colorMenu->next, effectsMenu);

#ifdef _WIN32
	std::swap(separator3->next, colorMenu);
	std::swap(autostartMenu->next, separator3);
	std::swap(separator1->next, autostartMenu);
#else
	std::swap(separator1->next, colorMenu);
#endif

	std::swap(settingsMenu->next, separator1);	
	std::swap(mainMenu->submenu, settingsMenu);

	_systray->update(mainMenu.get());
	std::swap(_menu, mainMenu);
}

#ifdef _WIN32
bool SystrayHandler::getCurrentAutorunState()
{	
	QSettings reg("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
	return reg.value("Hyperhdr", 0).toString() == qApp->applicationFilePath().replace('/', '\\');
}
#endif

void SystrayHandler::setAutorunState()
{
	#ifdef _WIN32
		bool currentState = getCurrentAutorunState();
		QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
		(currentState)
			? reg.remove("Hyperhdr")
			: reg.setValue("Hyperhdr", qApp->applicationFilePath().replace('/', '\\'));
		QUEUE_CALL_0(this, createSystray);
	#endif
}


void SystrayHandler::setColor(const QColor& color)
{
	std::vector<ColorRgb> rgbColor{ ColorRgb{ (uint8_t)color.red(), (uint8_t)color.green(), (uint8_t)color.blue() } };

	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		QUEUE_CALL_3(_hyperhdr.get(), setColor, int, 1, std::vector<ColorRgb>, rgbColor, int, 0);
}


void SystrayHandler::settings()
{
#ifndef _WIN32
	// Hide error messages when opening webbrowser

	int out_pipe[2];
	int saved_stdout;
	int saved_stderr;

	// saving stdout and stderr file descriptor
	saved_stdout = ::dup(STDOUT_FILENO);
	saved_stderr = ::dup(STDERR_FILENO);

	if (::pipe(out_pipe) == 0)
	{
		// redirecting stdout to pipe
		::dup2(out_pipe[1], STDOUT_FILENO);
		::close(out_pipe[1]);
		// redirecting stderr to stdout
		::dup2(STDOUT_FILENO, STDERR_FILENO);
	}
#endif

	QString link = QString("http://localhost:%1/").arg(_webPort);

#ifdef _WIN32
	const wchar_t* array = (const wchar_t*)link.utf16();
	ShellExecute(0, 0, array, 0, 0, SW_SHOW);
#endif

#ifndef _WIN32
	// restoring stdout
	::dup2(saved_stdout, STDOUT_FILENO);
	// restoring stderr
	::dup2(saved_stderr, STDERR_FILENO);
#endif
}

void SystrayHandler::setEffect(QString effect)
{
	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		QUEUE_CALL_2(_hyperhdr.get(), setEffect, QString, effect, int, 1);
}

void SystrayHandler::clearEfxColor()
{
	auto _hyperhdr = _hyperhdrHandle.lock();
	if (_hyperhdr)
		QUEUE_CALL_1(_hyperhdr.get(), clear, int, 1);
}

void SystrayHandler::signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name)
{
	switch (state) {
		case InstanceState::START:
			if (instance == 0)
			{
				std::shared_ptr<HyperHdrManager> instanceManager = _instanceManager.lock();

				if (instanceManager == nullptr)
					return;

				std::shared_ptr<HyperHdrInstance> retVal;
				SAFE_CALL_1_RET(instanceManager.get(), getHyperHdrInstance, std::shared_ptr<HyperHdrInstance>, retVal, quint8, 0);

				_hyperhdrHandle = retVal;

				createSystray();		
			}

			break;
		default:
			break;
	}
}

void SystrayHandler::signalSettingsChangedHandler(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::WEBSERVER)
	{
		_webPort = data.object()["port"].toInt();
	}
}

