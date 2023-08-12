var instNameInit = false

function systemLink(e)
{
	loadContent(e);
	window.scrollTo(0, 0);
	if (isSmallScreen())
		$('[data-widget="pushmenu"]').PushMenu('collapse');		
}

$(document).ready(function () {

	var darkModeOverwrite = getStorage("darkModeOverwrite", true);

	if(darkModeOverwrite == "false" || darkModeOverwrite == null)
	{
		if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
			handleDarkMode();
		}
	
		if (window.matchMedia && window.matchMedia('(prefers-color-scheme: light)').matches) {
			setStorage("darkMode", "off", false);
		}
	}
	
	if(getStorage("darkMode", false) == "on")
	{
		handleDarkMode();
	}

	loadContentTo("#container_connection_lost", "connection_lost");
	loadContentTo("#container_restart", "restart");
	initWebSocket();

	$(window.hyperhdr).on("cmd-serverinfo", function (event) {
		if (event.response.info == null)
			return;

		window.serverInfo = event.response.info;
		
		window.readOnlyMode = window.sysInfo.hyperhdr.readOnlyMode;
	
		// comps
		window.comps = event.response.info.components;

		window.serverInfo.grabberstate = event.response.info.grabbers.current;

		$(window.hyperhdr).trigger("ready");

		window.comps.forEach(function (obj) {
			if (obj.name == "ALL") {
				if (obj.enabled)
					$("#hyperhdr_disabled_notify").fadeOut("fast");
				else
					$("#hyperhdr_disabled_notify").fadeIn("fast");
			}
		});
			
		// update listing at button
		updateHyperhdrInstanceListing();

		if (!instNameInit) {
			window.currentHyperHdrInstanceName = getInstanceNameByIndex(0);
			instNameInit = true;
		}

		if (window.suppressMissingLutWarningPerSession !== true &&
			window.serverInfo.hasOwnProperty('grabbers') && window.serverInfo.grabbers.hasOwnProperty('active') &&
			window.serverInfo.grabbers.active.length > 0 &&
			!(window.serverInfo.grabbers.lut_for_hdr_exists === 1) &&
			getStorage("suppressMissingLutWarning") !== "true" )
		{
			window.suppressMissingLutWarningPerSession = true;
			setTimeout(function(){
				var suppressMissingLutWarningCheckbox = '<div class="text-end text-muted mt-1 me-3 mb-2">'+$.i18n('dashboard_message_do_not_show_again') + 
					': <input id="chk_suppressMissingLutWarning" class="form-check-input" type="checkbox" onChange="suppressMissingLutWarning()"> </div>';
				showNotification('warning', $.i18n('warning_lut_not_found_1'), $.i18n('warning_lut_not_found_t'),
					'<a href="#lut_downloader" class="ms-1 callout-link" style="cursor:pointer;" onClick="systemLink(event);">' +
					$.i18n('warning_lut_not_found_3') + '</a><br/><br/>' + $.i18n('warning_lut_not_found_2') + 
					'<a href="#grabber_calibration" class="ms-1 me-2 callout-link" style="cursor:pointer;" onClick="systemLink(event);">' +
					$.i18n('main_menu_grabber_calibration_token') + '</a>' +
					$.i18n('dashboard_newsbox_readmore') + ":" +
					'<a href="https://www.hyperhdr.eu/2022/04/usb-grabbers-hdr-to-sdr-quality-test.html" class="ms-1 callout-link" style="cursor:pointer;">' +
					$.i18n('dashboard_newsbox_visitblog') + '</a>' +					
					suppressMissingLutWarningCheckbox);
			}, 500);			
		}

		if (window.suppressErrorDetectedWarningPerSession !== true &&
			window.serverInfo.lastError.length > 0 &&
			getStorage("suppressErrorDetectedWarning") !== "true" )
		{
			window.suppressErrorDetectedWarningPerSession = true;
			setTimeout(function(){
				var suppressErrorDetectedWarningCheckbox = '<div class="text-end text-muted mt-1 me-3 mb-2">'+$.i18n('dashboard_message_do_not_show_again') + 
					': <input id="chk_suppressErrorDetectedWarning" class="form-check-input" type="checkbox" onChange="suppressErrorDetectedWarningCheckbox()"> </div>';
				showNotification('warning', $.i18n('warning_errors_detected_1'), $.i18n('warning_errors_detected_t'),
					`<div class="text-danger">${window.serverInfo.lastError}</div>`+
					$.i18n('warning_errors_detected_2') +
					'<a href="#logs" class="ms-1 callout-link" style="cursor:pointer;" onClick="systemLink(event);">' +
					$.i18n('main_menu_logging_token') + '</a>' +		
					suppressErrorDetectedWarningCheckbox);
			}, 700);			
		}
	}); // end cmd-serverinfo

	//End language selection
	
	$(window.hyperhdr).on("cmd-sessions-update", function (event) {
		let incoming = event.response.data;		
		let newArray = window.serverInfo.sessions.filter(old => old.name != incoming.service).concat(incoming.items);		
		window.serverInfo.sessions = newArray.sort((a, b) => a.name > b. name);

		// actions
		updateHyperhdrInstanceListing();
		$(window.hyperhdr).trigger("cmd-sessions-received");
	});

	$(window.hyperhdr).on("cmd-authorize-tokenRequest cmd-authorize-getPendingTokenRequests", function (event) {
		var val = event.response.info;
		if (Array.isArray(event.response.info)) {
			if (event.response.info.length == 0) {
				return
			}
			val = event.response.info[0]
			if (val.comment == '')
				$('#modal_dialog').modal('hide');
		}

		showInfoDialog("grantToken", $.i18n('conf_network_tok_grantT'), $.i18n('conf_network_tok_grantMsg') + '<br><span style="font-weight:bold">App: ' + val.comment + '</span><br><span style="font-weight:bold">Code: ' + val.id + '</span>')
		$("#tok_grant_acc").off().on('click', function () {
			tokenList.push(val)
			// forward event, in case we need to rebuild the list now
			$(window.hyperhdr).trigger({ type: "build-token-list" });
			requestHandleTokenRequest(val.id, true)
		});
		$("#tok_deny_acc").off().on('click', function () {
			requestHandleTokenRequest(val.id, false)
		});
	});

	$(window.hyperhdr).one("cmd-authorize-getTokenList", function (event) {
		tokenList = event.response.info;
	});

	$(window.hyperhdr).on("cmd-sysinfo", function (event) {
		requestServerInfo();
		window.sysInfo = event.response.info;

		window.currentVersion = window.sysInfo.hyperhdr.version;
		window.currentChannel = window.sysInfo.hyperhdr.channel;
	});

	$(window.hyperhdr).one("cmd-config-getschema", function (event) {
		window.serverSchema = event.response.info;
		requestServerConfig();
		requestTokenInfo();
		requestGetPendingTokenRequests();

		window.schema = window.serverSchema.properties;
	});

	$(window.hyperhdr).on("cmd-config-getconfig", function (event) {
		window.serverConfig = event.response.info;
		requestSysInfo();

		window.showOptHelp = window.serverConfig.general.showOptHelp;
	});

	$(window.hyperhdr).on("cmd-config-setconfig cmd-instance-saveName cmd-instance-deleteInstance cmd-instance-createInstance", function (event) {
		if (event.response.success === true)
		{
			var timeout = (event.type == "cmd-config-setconfig") ? 0 : 500;
			toaster('success', $.i18n("dashboard_alert_message_confsave_success_t"), $.i18n("dashboard_alert_message_confsave_success"), 0);
		}
	});

	$(window.hyperhdr).on("cmd-authorize-login", function (event) {
		// show menu
		var tar = document.getElementById("main-window-wrappper");	
		if (tar != null)
		{
			tar.classList.remove("content-wrapper-nonactive");
			tar.classList.add("content-wrapper");
		};
		
		var aside = document.getElementById("main-window-aside");
		if (aside != null)
		{
			aside.classList.remove("d-none");
		}
		
		var abutton = document.getElementById("main-window-button-menu");
		if (abutton != null)
		{
			abutton.classList.remove("d-none");
		}
	
		if (window.defaultPasswordIsSet === true && getStorage("suppressDefaultPwWarning") !== "true" )
		{
			$("#btn_lock_ui").addClass('disabled');
			setTimeout(function(){
				var supprPwWarnCheckbox = '<div class="text-end text-muted mt-1 me-3 mb-2">'+$.i18n('dashboard_message_do_not_show_again') + 
					': <input id="chk_suppressDefaultPw" type="checkbox" class="form-check-input" onChange="suppressDefaultPwWarning()"> </div>';
				showNotification('warning', $.i18n('dashboard_message_default_password'), $.i18n('dashboard_message_default_password_t'), '<a class="ms-1 callout-link" style="cursor:pointer;" onClick="changePassword()">' +
					$.i18n('InfoDialog_changePassword_title') + '</a>' + supprPwWarnCheckbox);
			}, 500);			
		}
		else
		{
			$("#btn_lock_ui").removeAttr('style');
		}


		if (event.response.hasOwnProperty('info'))
		{
			setStorage("loginToken", event.response.info.token, true);
		}

		requestServerConfigSchema();
		
		resizeMainWindow();
	});

	$(window.hyperhdr).on("cmd-authorize-newPassword", function (event) {
		if (event.response.success === true) {
			toaster('success', $.i18n("infoDialog_general_success_title"), $.i18n("InfoDialog_changePassword_success"), 500);			
			// not necessarily true, but better than nothing
			window.defaultPasswordIsSet = false;
			$("#btn_lock_ui").css('color','inherit');
			$("#btn_lock_ui").removeClass('disabled');
		}
	});

	$(window.hyperhdr).on("cmd-authorize-newPasswordRequired", function (event) {
		var loginToken = getStorage("loginToken", true)

		if (event.response.info.newPasswordRequired == true) {
			window.defaultPasswordIsSet = true;

			if (loginToken)
				requestTokenAuthorization(loginToken)
			else
				requestAuthorization('hyperhdr');
		}
		else {
			// hide menu
			var tar = document.getElementById("main-window-wrappper");	
			
			if (tar != null)
			{
				tar.classList.add("content-wrapper-nonactive");
				tar.classList.remove("content-wrapper");
			};
			
			var aside = document.getElementById("main-window-aside");
			if (aside != null)
				aside.classList.add("d-none");
			
			var abutton = document.getElementById("main-window-button-menu");
			if (abutton != null)
				abutton.classList.add("d-none");

			// login
			if (loginToken)
				requestTokenAuthorization(loginToken)
			else
				loadContentTo("#page-content", "login")
		}
	});

	$(window.hyperhdr).on("cmd-authorize-adminRequired", function (event) {
		//Check if a admin login is required.
		//If yes: check if default pw is set. If no: go ahead to get server config and render page
		if (event.response.info.adminRequired === true)
			requestRequiresDefaultPasswortChange();
		else
			requestServerConfigSchema();
	});

	$(window.hyperhdr).on("error", function (event) {
		//If we are getting an error "No Authorization" back with a set loginToken we will forward to new Login (Token is expired.
		//e.g.: hyperhdrd was started new in the meantime)
		if (event.reason == "No Authorization" && getStorage("loginToken", true)) {
			removeStorage("loginToken", true);
			requestRequiresAdminAuth();
		}
		else {
			showInfoDialog("error", "Error", event.reason);
		}
	});

	$(window.hyperhdr).on("open", function (event) {
		requestRequiresAdminAuth();
	});

	$(window.hyperhdr).one("ready", function (event) {
		loadContent();
	});

	$(window.hyperhdr).on("cmd-adjustment-update", function (event) {
		window.serverInfo.adjustment = event.response.data
	});

	$(window.hyperhdr).on("cmd-videomode-update", function (event) {
		window.serverInfo.videomode = event.response.data.videomode
	});

	$(window.hyperhdr).on("cmd-videomodehdr-update", function (event) {
		window.serverInfo.videomodehdr = event.response.data.videomodehdr
	});	

	$(window.hyperhdr).on("cmd-grabberstate-update", function (event) {
		window.serverInfo.grabberstate = event.response.data;
		$(window.hyperhdr).trigger("grabberstate-update", event.response.data);
	});	

	$(window.hyperhdr).on("cmd-components-update", function (event) {
		let obj = event.response.data

		// notfication in index
		if (obj.name == "ALL") {
			if (obj.enabled)
				$("#hyperhdr_disabled_notify").fadeOut("fast");
			else
				$("#hyperhdr_disabled_notify").fadeIn("fast");
		}

		window.comps.forEach((entry, index) => {
			if (entry.name === obj.name) {
				window.comps[index] = obj;
			}
		});
		// notify the update
		$(window.hyperhdr).trigger("components-updated", event.response.data);
	});

	$(window.hyperhdr).on("cmd-instance-update", function (event) {
		window.serverInfo.instance = event.response.data
		var avail = event.response.data;
		// notify the update
		$(window.hyperhdr).trigger("instance-updated");

		// if our current instance is no longer available we are at instance 0 again.
		var isInData = false;
		for (var key in avail) {
			if (avail[key].instance == currentHyperHdrInstance && avail[key].running) {
				isInData = true;
			}
		}

		if (!isInData) {
			//Delete Storage information about the last used but now stopped instance
			if (getStorage('lastSelectedInstance', false))
				removeStorage('lastSelectedInstance', false)

			currentHyperHdrInstance = 0;
			currentHyperHdrInstanceName = getInstanceNameByIndex(0);
			requestServerConfig();
			setTimeout(requestServerInfo, 100)
			setTimeout(requestTokenInfo, 200)
			setTimeout(loadContent, 300, undefined, true)
		}

		// update listing for button
		updateHyperhdrInstanceListing();
	});

	$(window.hyperhdr).on("cmd-instance-switchTo", function (event) {
		requestServerConfig();
		setTimeout(requestServerInfo, 200)
		setTimeout(requestTokenInfo, 400)
		setTimeout(loadContent, 400, undefined, true)
	});

	$(".nav-link.system-link").bind('click', systemLink);
		
	$(document).on('collapsed.lte.pushmenu', function(){ 
		document.getElementById('showMenuIcon').style.opacity = 0;
		document.getElementById('hideMenuIcon').style.opacity = 1;
	});
	
	$(document).on('shown.lte.pushmenu', function(){
		document.getElementById('showMenuIcon').style.opacity = 1;
		document.getElementById('hideMenuIcon').style.opacity = 0;
	});
});

function suppressDefaultPwWarning()
{
	if (document.getElementById('chk_suppressDefaultPw').checked) 
		setStorage("suppressDefaultPwWarning", "true");
	else 
		setStorage("suppressDefaultPwWarning", "false");
}

function suppressErrorDetectedWarningCheckbox()
{
	if (document.getElementById('chk_suppressErrorDetectedWarning').checked) 
		setStorage("suppressErrorDetectedWarning", "true");
	else 
		setStorage("suppressErrorDetectedWarning", "false");
}

function suppressMissingLutWarning()
{
	if (document.getElementById('chk_suppressMissingLutWarning').checked) 
		setStorage("suppressMissingLutWarning", "true");
	else 
		setStorage("suppressMissingLutWarning", "false");
}

//Dark Mode
$("#btn_darkmode").off().on("click",function(e){

	if(getStorage("darkMode", false) != "on")
	{
		handleDarkMode();
		setStorage("darkModeOverwrite", true, true);
	}
	else {
		handleLightMode();
		setStorage("darkMode", "off", false);
		setStorage("darkModeOverwrite", true, true);		
	}	
});

$(window).on('resize', function() {
	resizeMainWindow();
});

function resizeMainWindow()
{	
	//document.title = ($('#hyper-subpage').width());	
    if(parseInt($("#main-window-aside").css('margin-left')) < -50) 
	{
		$('#page-content').addClass('main-no-margin');
		$('#page-content').addClass('small-me');
    }
	else
	{
        $('#page-content').removeClass('main-no-margin');
		$('#page-content').removeClass('small-me');
    }
}


