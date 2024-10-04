// global vars (read and write in window object)
window.webPrio = 1;
window.webOrigin = "Web Configuration";
window.showOptHelp = true;
window.gitHubReleaseApiUrl = "https://api.github.com/repos/awawa-dev/HyperHDR/releases";
window.currentChannel = null;
window.currentVersion = null;
window.latestVersion = null;
window.gitHubVersionList = null;
window.serverInfo = {};
window.serverSchema = {};
window.serverConfig = {};
window.schema = {};
window.sysInfo = {};
window.jsonPort = 8090;
window.websocket = null;
window.hyperhdr = {};
window.wsTan = 1;
window.ledStreamActive = false;
window.imageStreamActive = false;
window.loggingStreamActive = false;
window.watchdog = 0;
window.debugMessagesActive = true;
window.wSess = [];
window.comps = [];
window.defaultPasswordIsSet = null;
tokenList = {};

var lastSelectedInstanceCache = getStorage('lastSelectedInstance', false);
window.currentHyperHdrInstance = 0;
window.currentHyperHdrInstanceName = getInstanceNameByIndex(window.currentHyperHdrInstance);

function storageComp()
{
	if (typeof (Storage) !== "undefined")
		return true;
	return false;
}

function getStorage(item, session)
{
	if (storageComp())
	{
		if (session === true)
			return sessionStorage.getItem(item);
		else
			return localStorage.getItem(item);
	}
	return null;
}

function setStorage(item, value, session)
{
	if (storageComp())
	{
		if (session === true)
			sessionStorage.setItem(item, value);
		else
			localStorage.setItem(item, value);
	}
}

function removeStorage(item, session)
{
	if (storageComp())
	{
		if (session === true)
			sessionStorage.removeItem(item);
		else
			localStorage.removeItem(item);
	}
}

function getInstanceNameByIndex(index)
{
	var instData = window.serverInfo.instance;
	for (var key in instData)
	{
		if (instData[key].instance == index)
			return instData[key].friendly_name;
	}
	return "unknown";
}

function connectionLostDetection(type)
{
	if (window.watchdog > 2)
	{
		var interval_id = window.setInterval(function () { clearInterval(interval_id); }, 9999); // Get a reference to the last
		for (var i = 1; i < interval_id; i++)
			window.clearInterval(i);
		if (type == 'restart')
		{
			$("body").html($("#container_restart").html());
			// setTimeout delay for probably slower systems, some browser don't execute THIS action
			setTimeout(restartAction, 250);
		}
		else
		{
			var targetImg = document.getElementById("no_connection_img");
			var sourceImg = document.getElementById("left_top_hyperhdr_logo");
			if (targetImg != null && sourceImg != null &&
				sourceImg.naturalWidth > 0 && sourceImg.naturalHeight > 0)
			{
				var canvas = document.createElement("canvas");
				canvas.width = sourceImg.naturalWidth;
				canvas.height = sourceImg.naturalHeight;
				targetImg.width = sourceImg.naturalWidth;
				targetImg.height = sourceImg.naturalHeight;
				var ctx = canvas.getContext("2d");
				ctx.drawImage(sourceImg, 0, 0);
				targetImg.src = canvas.toDataURL("image/png");
			}

			$("body").html($("#container_connection_lost").html());
			connectionLostAction();
		}
	}
	else
	{
		$.get("/hyperhdr_heart_beat", function () { window.watchdog = 0 }).fail(function () { window.watchdog++; });
	}
}

setInterval(connectionLostDetection, 3000);

// init websocket to hyperhdr and bind socket events to jquery events of $(hyperhdr) object

function initWebSocket()
{
	if ("WebSocket" in window)
	{
		if (window.websocket == null)
		{
			window.jsonPort = '';
			if (document.location.port == '' && document.location.protocol == "http:")
				window.jsonPort = '80';
			else if (document.location.port == '' && document.location.protocol == "https:")
				window.jsonPort = '443';
			else
				window.jsonPort = document.location.port;

			try
			{
				window.websocket = (document.location.protocol == "https:") ? new WebSocket('wss://' + document.location.hostname + ":" + window.jsonPort) : new WebSocket('ws://' + document.location.hostname + ":" + window.jsonPort);
			}
			catch (error)
			{
				alert("Connection to websocket failed. Please open this page in a new page/tab in your browser or use secure port 8092 (for example https://localhost:8092).");
			}

			window.websocket.onopen = function (event)
			{
				$(window.hyperhdr).trigger({ type: "open" });

				$(window.hyperhdr).on("cmd-serverinfo", function (event)
				{
					window.watchdog = 0;
				});
			};

			window.websocket.onclose = function (event)
			{
				// See http://tools.ietf.org/html/rfc6455#section-7.4.1
				var reason;
				switch (event.code)
				{
					case 1000: reason = "Normal closure, meaning that the purpose for which the connection was established has been fulfilled."; break;
					case 1001: reason = "An endpoint is \"going away\", such as a server going down or a browser having navigated away from a page."; break;
					case 1002: reason = "An endpoint is terminating the connection due to a protocol error"; break;
					case 1003: reason = "An endpoint is terminating the connection because it has received a type of data it cannot accept (e.g., an endpoint that understands only text data MAY send this if it receives a binary message)."; break;
					case 1004: reason = "Reserved. The specific meaning might be defined in the future."; break;
					case 1005: reason = "No status code was actually present."; break;
					case 1006: reason = "The connection was closed abnormally, e.g., without sending or receiving a Close control frame"; break;
					case 1007: reason = "An endpoint is terminating the connection because it has received data within a message that was not consistent with the type of the message (e.g., non-UTF-8 [http://tools.ietf.org/html/rfc3629] data within a text message)."; break;
					case 1008: reason = "An endpoint is terminating the connection because it has received a message that \"violates its policy\". This reason is given either if there is no other sutible reason, or if there is a need to hide specific details about the policy."; break;
					case 1009: reason = "An endpoint is terminating the connection because it has received a message that is too big for it to process."; break;
					case 1010: reason = "An endpoint (client) is terminating the connection because it has expected the server to negotiate one or more extension, but the server didn't return them in the response message of the WebSocket handshake. <br /> Specifically, the extensions that are needed are: " + event.reason; break;
					case 1011: reason = "A server is terminating the connection because it encountered an unexpected condition that prevented it from fulfilling the request."; break;
					case 1015: reason = "The connection was closed due to a failure to perform a TLS handshake (e.g., the server certificate can't be verified)."; break;
					default: reason = "Unknown reason";
				}
				$(window.hyperhdr).trigger({ type: "close", reason: reason });
				window.watchdog = 10;
				connectionLostDetection();
			};

			window.websocket.onmessage = function (event)
			{
				try
				{

					if (event.data instanceof Blob) {
						var blob = new Blob([event.data], { type: "image/jpeg" });
						$(window.hyperhdr).trigger({ type: "cmd-image-stream-frame", response: blob });
						return;
					}
					var response = JSON.parse(event.data);
					var success = response.success;
					var cmd = response.command;
					var tan = response.tan;
					if (success || typeof (success) == "undefined")
					{
						$(window.hyperhdr).trigger({ type: "cmd-" + cmd, response: response });
					}
					else
					{
						// skip tan -1 error handling
						if (tan != -1)
						{
							var error = response.hasOwnProperty("error") ? response.error : "unknown";

							if (error == "Not ready")
								window.location.reload();
							else							
								$(window.hyperhdr).trigger({ type: "error", reason: error });

							console.log("[window.websocket::onmessage] ", error);							
						}
					}
				}
				catch (exception_error)
				{
					$(window.hyperhdr).trigger({ type: "error", reason: exception_error });
					console.log("[window.websocket::onmessage] ", exception_error);
				}
			};

			window.websocket.onerror = function (error)
			{
				$(window.hyperhdr).trigger({ type: "error", reason: error });
				console.log("[window.websocket::onerror] ", error);
			};
		}
	}
	else
	{
		$(window.hyperhdr).trigger("error");
		alert("Websocket is not supported by your browser");
		return;
	}
}

function sendToHyperhdr(command, subcommand, msg)
{
	if (typeof subcommand != 'undefined' && subcommand.length > 0)
		subcommand = ',"subcommand":"' + subcommand + '"';
	else
		subcommand = "";

	if (typeof msg != 'undefined' && msg.length > 0)
		msg = "," + msg;
	else
		msg = "";

	window.websocket.send('{"command":"' + command + '", "tan":' + window.wsTan + subcommand + msg + '}');
}

// Send a json message to Hyperhdr and wait for a matching response
// A response matches, when command(+subcommand) of request and response is the same
// command:    The string command
// subcommand: The optional string subcommand
// data:       The json data as Object
// tan:        The optional tan, default 1. If the tan is -1, we skip global response error handling
// Returns data of response or false if timeout
async function sendAsyncToHyperhdr(command, subcommand, data, tan = 1)
{
	let obj = { command, tan };

	if (subcommand) 
	{
		Object.assign(obj, { subcommand });
	}

	if (data)
	{
		Object.assign(obj, data);
	}

	//if (process.env.DEV || sstore.getters['common/getDebugState']) console.log('SENDAS', obj)
	return __sendAsync(obj);
}

// Send a json message to Hyperhdr and wait for a matching response
// A response matches, when command(+subcommand) of request and response is the same
// Returns data of response or false if timeout
async function __sendAsync(data)
{
	return new Promise((resolve, reject) =>
	{
		let cmd = data.command;
		let subc = data.subcommand;
		let tan = data.tan;

		if (subc)
			cmd = `${cmd}-${subc}`;

		let func = (e) =>
		{
			let rdata;

			try
			{
				rdata = JSON.parse(e.data);
			}
			catch (error)
			{
				console.error("[window.websocket::onmessage] ", error);
				resolve(false);
			}

			if (rdata.command == cmd && rdata.tan == tan)
			{
				window.websocket.removeEventListener('message', func);
				resolve(rdata);
			}
		};

		// after 7 sec we resolve false
		setTimeout(() => { window.websocket.removeEventListener('message', func); resolve(false); }, 7000);

		window.websocket.addEventListener('message', func);
		window.websocket.send(JSON.stringify(data) + '\n');
	})
}

// -----------------------------------------------------------
// wrapped server commands

// Test if admin requires authentication
function requestRequiresAdminAuth()
{
	sendToHyperhdr("authorize", "adminRequired");
}
// Test if the default password needs to be changed
function requestRequiresDefaultPasswortChange()
{
	sendToHyperhdr("authorize", "newPasswordRequired");
}
// Change password
function requestChangePassword(oldPw, newPw)
{
	sendToHyperhdr("authorize", "newPassword", '"password": "' + oldPw + '", "newPassword":"' + newPw + '"');
}

function requestAuthorization(password)
{
	sendToHyperhdr("authorize", "login", '"password": "' + password + '"');
}

function requestLogout()
{
	sendToHyperhdr("authorize", "logout", '');
}

function requestTokenAuthorization(token)
{
	sendToHyperhdr("authorize", "login", '"token": "' + token + '"');
}

function requestToken(comment)
{
	sendToHyperhdr("authorize", "createToken", '"comment": "' + comment + '"');
}

function requestTokenInfo()
{
	sendToHyperhdr("authorize", "getTokenList", "");
}

function requestGetPendingTokenRequests(id, state)
{
	sendToHyperhdr("authorize", "getPendingTokenRequests", "");
}

function requestHandleTokenRequest(id, state)
{
	sendToHyperhdr("authorize", "answerRequest", '"id":"' + id + '", "accept":' + state);
}

function requestTokenDelete(id)
{
	sendToHyperhdr("authorize", "deleteToken", '"id":"' + id + '"');
}

function requestInstanceRename(inst, name)
{
	sendToHyperhdr("instance", "saveName", '"instance": ' + inst + ', "name": "' + name + '"');
}

function requestInstanceStartStop(inst, start)
{
	if (start)
		sendToHyperhdr("instance", "startInstance", '"instance": ' + inst);
	else
		sendToHyperhdr("instance", "stopInstance", '"instance": ' + inst);
}

function requestInstanceDelete(inst)
{
	sendToHyperhdr("instance", "deleteInstance", '"instance": ' + inst);
}

function requestInstanceCreate(name)
{
	sendToHyperhdr("instance", "createInstance", '"name": "' + name + '"');
}

function requestInstanceSwitch(inst)
{
	sendToHyperhdr("instance", "switchTo", '"instance": ' + inst);
}

function requestServerInfo()
{
	sendToHyperhdr("serverinfo", "", '"subscribe":["components-update","sessions-update","priorities-update", "imageToLedMapping-update", "adjustment-update", "videomode-update", "videomodehdr-update", "settings-update", "instance-update", "grabberstate-update", "benchmark-update"]');
}

function requestSysInfo()
{
	sendToHyperhdr("sysinfo");
}

function requestServerConfigSchema()
{
	sendToHyperhdr("config", "getschema");
}

function requestServerConfig()
{
	sendToHyperhdr("config", "getconfig");
}

function requestLedColorsStart()
{
	window.ledStreamActive = true;
	sendToHyperhdr("ledcolors", "ledstream-start");
}

function requestLedColorsStop()
{
	window.ledStreamActive = false;
	sendToHyperhdr("ledcolors", "ledstream-stop");
}

function requestLedImageStart()
{
	window.imageStreamActive = true;
	sendToHyperhdr("ledcolors", "imagestream-start");
}

function requestLedImageStop()
{
	window.imageStreamActive = false;
	sendToHyperhdr("ledcolors", "imagestream-stop");
}

function requestPriorityClear(prio)
{
	if (typeof prio !== 'number')
		prio = window.webPrio;

	sendToHyperhdr("clear", "", '"priority":' + prio + '');
}

function requestClearAll()
{
	requestPriorityClear(-1)
}

function requestPlayEffect(effectName, duration)
{
	sendToHyperhdr("effect", "", '"effect":{"name":"' + effectName + '"},"priority":' + window.webPrio + ',"duration":' + validateDuration(duration) + ',"origin":"' + window.webOrigin + '"');
}

function requestSetColor(r, g, b, duration)
{
	sendToHyperhdr("color", "", '"color":[' + r + ',' + g + ',' + b + '], "priority":' + window.webPrio + ',"duration":' + validateDuration(duration) + ',"origin":"' + window.webOrigin + '"');
}



function requestSetImage(data, SX, SY, duration, name)
{
	sendToHyperhdr("image", "",  `"priority": ${window.webPrio},`+
		`"duration": ${validateDuration(duration)} ,"imagedata":"${data}", "format":"auto", `+
		`"imagewidth": ${SX}, "imageheight": ${SY},`+
		`"origin": "${window.webOrigin}", "name": "${name}"`);
}

function requestSetComponentState(comp, state)
{
	var state_str = state ? "true" : "false";
	sendToHyperhdr("componentstate", "", '"componentstate":{"component":"' + comp + '","state":' + state_str + '}');
}

function requestSetSource(prio)
{
	if (prio == "auto")
		sendToHyperhdr("sourceselect", "", '"auto":true');
	else
		sendToHyperhdr("sourceselect", "", '"priority":' + prio);
}

function requestWriteConfig(config, full)
{
	if (full === true)
		window.serverConfig = config;
	else
	{
		jQuery.each(config, function (i, val)
		{
			window.serverConfig[i] = val;
		});
	}

	sendToHyperhdr("config", "setconfig", '"config":' + JSON.stringify(window.serverConfig));
}

function requestTestEffect(effectName, effectPy, effectArgs, data)
{
	sendToHyperhdr("effect", "", '"effect":{"name":"' + effectName + '", "args":' + effectArgs + '}, "priority":' + window.webPrio + ', "origin":"' + window.webOrigin + '", "pythonScript":"' + effectPy + '", "imageData":"' + data + '"');
}

function requestLoggingStart()
{
	window.loggingStreamActive = true;
	sendToHyperhdr("logging", "start");
}

function requestLoggingStop()
{
	window.loggingStreamActive = false;
	sendToHyperhdr("logging", "stop");
}

function requestMappingType(type)
{
	sendToHyperhdr("processing", "", '"mappingType": "' + type + '"');
}

function requestVideoMode(newMode)
{
	sendToHyperhdr("videomode", "", '"videoMode": "' + newMode + '"');
}

function requestPerformanceCounter(allCounter)
{
	if (allCounter)
		sendToHyperhdr("performance-counters", "all", "");
	else
		sendToHyperhdr("performance-counters", "resources", "");
}

function requestCalibrationStart()
{
	sendToHyperhdr("signal-calibration", "start", "");
}

function requestCalibrationStop()
{
	sendToHyperhdr("signal-calibration", "stop", "");
}

async function requestCalibrationData()
{
	return sendAsyncToHyperhdr("signal-calibration", "get-info", "", Math.floor(Math.random() * 1000));
}

function requestVideoModeHdr(newMode)
{
	sendToHyperhdr("videomodehdr", "", '"HDR": ' + newMode + '');
}


function requestAdjustment(type, value, complete)
{
	if (complete === true)
		sendToHyperhdr("adjustment", "", '"adjustment": ' + type + '');
	else
		sendToHyperhdr("adjustment", "", '"adjustment": {"' + type + '": ' + value + '}');
}

async function requestLedDeviceDiscovery(type, params)
{
	let data = { ledDeviceType: type, params: params };

	return sendAsyncToHyperhdr("leddevice", "discover", data, Math.floor(Math.random() * 1000));
}

async function requestLedDeviceProperties(type, params)
{
	let data = { ledDeviceType: type, params: params };

	return sendAsyncToHyperhdr("leddevice", "getProperties", data, Math.floor(Math.random() * 1000));
}

async function tunnel_hue_get(_ip, _path,header={})
{
	let data = { service: "hue", ip: _ip, path: _path, data: "",header };
	let r = await sendAsyncToHyperhdr("tunnel", "get", data, Math.floor(Math.random() * 1000));
	if (r["success"] != true || r["isTunnelOk"] != true)
		return null;
	else
		return r["info"];
}

async function tunnel_hue_post(_ip, _path, _data)
{
	let data = { service: "hue", ip: _ip, path: _path, data: _data };
	let r = await sendAsyncToHyperhdr("tunnel", "post", data, Math.floor(Math.random() * 1000));
	if (r["success"] != true || r["isTunnelOk"] != true)
		return null;
	else
		return r["info"];
}

async function tunnel_hue_put(_ip, _path, _data)
{
	let data = { service: "hue", ip: _ip, path: _path, data: _data };
	let r = await sendAsyncToHyperhdr("tunnel", "put", data, Math.floor(Math.random() * 1000));
	if (r["success"] != true || r["isTunnelOk"] != true)
		return null;
	else
		return r["info"];
}

function requestLedDeviceIdentification(type, params)
{
	sendToHyperhdr("leddevice", "identify", '"ledDeviceType": "' + type + '","params": ' + JSON.stringify(params) + '');
}

async function requestGetDB()
{
	return sendAsyncToHyperhdr("save-db", "", "", Math.floor(Math.random() * 1000));
}

function requestRestoreDB(backupData)
{
	sendToHyperhdr("load-db", "", '"config":' + JSON.stringify(backupData));
}

function requestBenchmark(mode, status)
{
	sendToHyperhdr("benchmark", mode, '"status": ' + status);
}

function requestLutInstall(address, hardware_brightness, hardware_contrast, hardware_saturation, now)
{
	sendToHyperhdr("lut-install", address, `"hardware_brightness":${hardware_brightness},
											"hardware_contrast":${hardware_contrast},
											"hardware_saturation":${hardware_saturation},
											"now":${now}`);
}

async function requestLutCalibration(mode, debug, postprocessing)
{
	sendToHyperhdr("lut-calibration", mode, `"debug":${debug}, "postprocessing":${postprocessing}`);
}

async function requestHasLedClock()
{
	sendToHyperhdr("leddevice", "hasLedClock", `"ledDeviceType": "", "params": {}`, Math.floor(Math.random() * 1000));
}
