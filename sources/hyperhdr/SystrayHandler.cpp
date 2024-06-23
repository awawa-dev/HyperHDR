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
	#include <QSettings>
	#include <list>
#endif

#ifndef _WIN32
	#include <unistd.h>
#else
	#include <windows.h>
#endif

#ifdef __APPLE__
	#include <ApplicationServices/ApplicationServices.h>
#endif

#include <QBuffer>
#include <QFile>
#include <QCoreApplication>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QStringList>

#include <HyperhdrConfig.h>

#include <image/ColorRgb.h>
#include <effects/EffectDefinition.h>
#include <webserver/WebServer.h>
#include <utils/Logger.h>
#include <systray/Systray.h>
#include <utils-image/utils-image.h>

#include "HyperHdrDaemon.h"
#include "SystrayHandler.h"

#ifdef __linux__
#define SYSTRAY_WIDGET_LIB "libsystray-widget.so"
#include <stdlib.h>
#include <dlfcn.h>
namespace
{
	void* _library = nullptr;
	SystrayInitializeFun SystrayInitialize = nullptr;
	SystrayLoopFun SystrayLoop = nullptr;
	SystrayUpdateFun SystrayUpdate = nullptr;
	SystrayCloseFun SystrayClose = nullptr;
}
#endif

SystrayHandler::SystrayHandler(HyperHdrDaemon* hyperhdrDaemon, quint16 webPort, QString rootFolder)
	: QObject(),
	_menu(nullptr),
	_haveSystray(false),
	_webPort(webPort),
	_rootFolder(rootFolder),
	_selectedInstance(-1)
{
	Q_INIT_RESOURCE(resources);

	std::shared_ptr<HyperHdrManager> instanceManager;
	hyperhdrDaemon->getInstanceManager(instanceManager);
	_instanceManager = instanceManager;
	connect(instanceManager.get(), &HyperHdrManager::SignalInstanceStateChanged, this, &SystrayHandler::signalInstanceStateChangedHandler);
	connect(instanceManager.get(), &HyperHdrManager::SignalSettingsChanged, this, &SystrayHandler::signalSettingsChangedHandler);

#ifdef __linux__
	// Load library
	_library = dlopen(SYSTRAY_WIDGET_LIB, RTLD_NOW);

	if (_library)
	{
		SystrayInitialize = (SystrayInitializeFun)dlsym(_library, "SystrayInitialize");
		SystrayLoop = (SystrayLoopFun)dlsym(_library, "SystrayLoop");
		SystrayUpdate = (SystrayUpdateFun)dlsym(_library, "SystrayUpdate");
		SystrayClose = (SystrayCloseFun)dlsym(_library, "SystrayClose");

		if (SystrayInitialize == nullptr || SystrayLoop == nullptr || SystrayUpdate == nullptr || SystrayClose == nullptr)
		{
			printf("Could not resolve libSystrayWidget.so functions\n");
			dlclose(_library);
			_library = nullptr;
		}
	}
	else
		printf("Could not load libSystrayWidget.so library\n");

	if (_library != nullptr)
		_haveSystray = SystrayInitialize(nullptr);
#else
	_haveSystray = SystrayInitialize(nullptr);
#endif	
}

SystrayHandler::~SystrayHandler()
{
	printf("Releasing SysTray\n");
	if (_haveSystray)
		SystrayClose();

#ifdef __linux__
	if (_library != nullptr)
	{
		dlclose(_library);
		_library = nullptr;
	}
#endif
}

bool SystrayHandler::isInitialized()
{
	return _haveSystray;
}

void SystrayHandler::loop()
{
	if (_haveSystray)
		SystrayLoop();
}

void SystrayHandler::close()
{
	if (_haveSystray)
	{
		SystrayClose();
	}
}

static QString preloadSvg(const QString& filename)
{
	if (filename.indexOf(":/") == 0)
	{
		QFile stream(filename);
		if (!stream.open(QIODevice::ReadOnly))
			return filename;
		QByteArray ar = stream.readAll();
		stream.close();

		return QString(ar);
	}
	return filename;
}

static void loadSvg(std::unique_ptr<SystrayMenu>& menu, QString filename, QString rootFolder, QString destFilename = "")
{

#ifdef __linux__
			int iconDim = 16;
	#else
		#ifdef __APPLE__
			int iconDim = 18;
		#else
			int iconDim = 22;

			if (filename == ":/hyperhdr-tray-icon.svg")
				iconDim = 32;
	#endif
#endif

#ifdef __linux__
	if (destFilename.isEmpty())
	{
		destFilename = filename;
		if (destFilename.indexOf(":/") == 0)
			destFilename = destFilename.right(destFilename.size() - 2);
		destFilename.replace(".svg", ".png");
	}

	QString fullPath = rootFolder + "/icons/" + destFilename;
	QFileInfo iconFile(fullPath);

	if (!iconFile.exists())
	{
		QDir().mkpath(iconFile.absolutePath());

		std::vector<uint8_t> ar;
		utils_image::svg2png(preloadSvg(filename).toStdString(), iconDim, iconDim, ar);

		QFile newIcon(fullPath);
		newIcon.open(QIODevice::WriteOnly);
		newIcon.write(reinterpret_cast<char*>(ar.data()), ar.size());
		newIcon.close();
	}

	menu->tooltip = fullPath.toStdString();
#else
	std::vector<uint8_t> ar;
	utils_image::svg2png(preloadSvg(filename).toStdString(), iconDim, iconDim, ar);

	menu->icon.resize(ar.size());
	memcpy(menu->icon.data(), ar.data(), ar.size());
#endif
}

void SystrayHandler::createSystray()
{
	if (!_haveSystray)
		return;

	std::unique_ptr<SystrayMenu> mainMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);

	loadSvg(mainMenu, ":/hyperhdr-tray-icon.svg", _rootFolder);

	// settings menu
	std::unique_ptr<SystrayMenu> settingsMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(settingsMenu, ":/settings.svg", _rootFolder);
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

	// instances
	std::unique_ptr<SystrayMenu> instances = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	instances->label = "Instances";
	loadSvg(instances, ":/instance.svg", _rootFolder);

	std::shared_ptr<HyperHdrManager> instanceManager = _instanceManager.lock();
	if (instanceManager != nullptr)
	{
		std::vector<QString> instancesList;
		SAFE_CALL_0_RET(instanceManager.get(), getInstances, std::vector<QString>, instancesList);
		for (int i = instancesList.size() - 2; i >= 0; i -= 2)
		{
			bool ok = false;
			int key = instancesList[i].toInt(&ok);
			if (ok)
			{
				std::unique_ptr<SystrayMenu> instanceItem = std::unique_ptr<SystrayMenu>(new SystrayMenu);
				instanceItem->label = instancesList[i + 1].toStdString();
				instanceItem->checkGroup = key;
				instanceItem->isChecked = (key == _selectedInstance);
				instanceItem->context = this;
				instanceItem->callback = [](SystrayMenu* m) {
					SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
					sh->_selectedInstance = m->checkGroup;
					if (sh != nullptr)
						QUEUE_CALL_0(sh, selectInstance);
				};

				std::swap(instances->submenu, instanceItem->next);
				std::swap(instances->submenu, instanceItem);
			}
		}

		std::unique_ptr<SystrayMenu> separatorInstance = std::unique_ptr<SystrayMenu>(new SystrayMenu);
		separatorInstance->label = "-";

		std::swap(instances->submenu, separatorInstance->next);
		std::swap(instances->submenu, separatorInstance);

		std::unique_ptr<SystrayMenu> instancesAll = std::unique_ptr<SystrayMenu>(new SystrayMenu);
		instancesAll->label = "All";
		instancesAll->isChecked = (_selectedInstance == -1);
		instancesAll->context = this;
		instancesAll->callback = [](SystrayMenu* m) {
			SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
			sh->_selectedInstance = -1;
			if (sh != nullptr)
				QUEUE_CALL_0(sh, selectInstance);
		};

		std::swap(instances->submenu, instancesAll->next);
		std::swap(instances->submenu, instancesAll);
	}
	
	// color menu
	std::unique_ptr<SystrayMenu> colorMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(colorMenu, ":/color.svg", _rootFolder);
	colorMenu->label = "&Color";
	colorMenu->context = this;

	std::list<std::string> colors{ "white", "red", "green", "blue", "yellow", "magenta", "cyan" };
	colors.reverse();
	for (const std::string& color : colors)
	{
		QString svgTemplate = "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"32\" height=\"32\" version=\"1.1\" viewBox=\"0 0 32 32\">"
			"<rect width=\"26\" height=\"26\" x=\"3\" y=\"3\" fill=\"%1\" stroke-width=\"2\" stroke=\"gray\" />"
			"</svg>";
		QString svg = QString(svgTemplate).arg(QString::fromStdString(color));
		
		std::unique_ptr<SystrayMenu> colorItem = std::unique_ptr<SystrayMenu>(new SystrayMenu);
		loadSvg(colorItem, svg, _rootFolder, QString("%1.png").arg(QString::fromStdString(color)));
		colorItem->label = color;
		colorItem->context = this;
		colorItem->callback = [](SystrayMenu* m) {
			SystrayHandler* sh = qobject_cast<SystrayHandler*>(m->context);
			QString colorName = QString::fromStdString(m->label);
			ColorRgb color = utils_image::colorRgbfromString(colorName.toStdString());
			if (sh != nullptr)
				QUEUE_CALL_1(sh, setColor, ColorRgb, color);
		};

		std::swap(colorMenu->submenu, colorItem->next);
		std::swap(colorMenu->submenu, colorItem);
	}

	// effects
	std::list<EffectDefinition> efxs, efxsSorted;
	if (instanceManager != nullptr)
		efxs = instanceManager->getEffects();

	std::unique_ptr<SystrayMenu> effectsMenu = std::unique_ptr<SystrayMenu>(new SystrayMenu);
	loadSvg(effectsMenu, ":/effects.svg", _rootFolder);
	effectsMenu->label = "&Effects";


	efxs.sort([](const EffectDefinition& a, const EffectDefinition& b) {
		return a.name > b.name;
	});

	std::copy_if(efxs.begin(), efxs.end(),
		std::back_inserter(efxsSorted),
		[](const EffectDefinition& a) { return a.name.find("Music:") != std::string::npos; });

	std::copy_if(efxs.begin(), efxs.end(),
		std::back_inserter(efxsSorted),
		[](const EffectDefinition& a) { return !(a.name.find("Music:") != std::string::npos); });


	for (const EffectDefinition& efx : efxsSorted)
	{
		QString effectName = QString::fromStdString(efx.name);
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
	loadSvg(clearMenu, ":/clear.svg", _rootFolder);
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
	loadSvg(quitMenu, ":/quit.svg", _rootFolder);
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
	loadSvg(autostartMenu, ":/autorun.svg", _rootFolder);
	autostartMenu->label = (getCurrentAutorunState()) ? "Disable autostart" : "Enable autostart";
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
	std::swap(instances->next, colorMenu);
	std::swap(separator3->next, instances);
	std::swap(autostartMenu->next, separator3);
	std::swap(separator1->next, autostartMenu);
#else
	std::swap(instances->next, colorMenu);
	std::swap(separator1->next, instances);
#endif

	std::swap(settingsMenu->next, separator1);	
	std::swap(mainMenu->submenu, settingsMenu);

	SystrayUpdate(mainMenu.get());
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

void SystrayHandler::selectInstance()
{
	createSystray();
}

void SystrayHandler::setColor(ColorRgb color)
{
	std::shared_ptr<HyperHdrManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		QUEUE_CALL_4(instanceManager.get(), setInstanceColor, int, _selectedInstance, int, 1, ColorRgb, color, int, 0);
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

#ifdef __linux__
	QString command = QString("xdg-open %1").arg(link);
	if (system(QSTRING_CSTR(command)) == -1)
	{
		printf("xdg-open <http_link> failed. xdg-utils package is required.\n");
	}
#endif
	
#ifdef __APPLE__
	std::string slink = link.toStdString();
	CFURLRef url = CFURLCreateWithBytes(
		NULL,                       
		(UInt8*)slink.c_str(),
		slink.length(),
		kCFStringEncodingASCII,
		NULL
	);
	LSOpenCFURLRef(url, 0);
	CFRelease(url);
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
	std::shared_ptr<HyperHdrManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		QUEUE_CALL_3(instanceManager.get(), setInstanceEffect, int, _selectedInstance, QString, effect, int, 1);
}

void SystrayHandler::clearEfxColor()
{
	std::shared_ptr<HyperHdrManager> instanceManager = _instanceManager.lock();
	if (instanceManager)
		QUEUE_CALL_2(instanceManager.get(), clearInstancePriority, int, _selectedInstance, int, 1);
}

void SystrayHandler::signalInstanceStateChangedHandler(InstanceState state, quint8 instance, const QString& name)
{
	if (instance == _selectedInstance && state == InstanceState::STOP)
		_selectedInstance = -1;

	createSystray();
}

void SystrayHandler::signalSettingsChangedHandler(settings::type type, const QJsonDocument& data)
{
	if (type == settings::type::WEBSERVER)
	{
		_webPort = data.object()["port"].toInt();
	}
}

