var instNameInit = false

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

		// determine button visibility
		var running = window.serverInfo.instance.filter(entry => entry.running);
		if (running.length > 1)
		{			
			$('#btn_hypinstanceswitch').removeClass('disabled');
		}
		else
			$('#btn_hypinstanceswitch').addClass('disabled');
			
		// update listing at button
		updateHyperhdrInstanceListing()
		if (!instNameInit) {
			window.currentHyperHdrInstanceName = getInstanceNameByIndex(0);
			instNameInit = true;
		}

		updateSessions();
	}); // end cmd-serverinfo

	//End language selection
	
	$(window.hyperhdr).on("cmd-sessions-update", function (event) {
		window.serverInfo.sessions = event.response.data;
		updateSessions();
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
		requestServerInfo();
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

	$(window.hyperhdr).on("cmd-config-setconfig", function (event) {
		if (event.response.success === true) {
			showNotification('success', $.i18n('dashboard_alert_message_confsave_success'), $.i18n('dashboard_alert_message_confsave_success_t'))
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
			setTimeout(function(){
				var supprPwWarnCheckbox = '<div class="text-right">'+$.i18n('dashboard_message_do_not_show_again') +
					' <input id="chk_suppressDefaultPw" type="checkbox" onChange="suppressDefaultPwWarning()"> </div>';
				showNotification('warning', $.i18n('dashboard_message_default_password'), $.i18n('dashboard_message_default_password_t'), '<a class="ps-2 callout-link" style="cursor:pointer;" onClick="changePassword()">' +
					$.i18n('InfoDialog_changePassword_title') + '</a><br/><br/>' + supprPwWarnCheckbox);
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
			showInfoDialog("success", $.i18n('InfoDialog_changePassword_success'));
			// not necessarily true, but better than nothing
			window.defaultPasswordIsSet = false;
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

		// determine button visibility
		var running = serverInfo.instance.filter(entry => entry.running);
		if (running.length > 1)
		{			
			$('#btn_hypinstanceswitch').removeClass('disabled');
		}
		else
			$('#btn_hypinstanceswitch').addClass('disabled');

		// update listing for button
		updateHyperhdrInstanceListing()
	});

	$(window.hyperhdr).on("cmd-instance-switchTo", function (event) {
		requestServerConfig();
		setTimeout(requestServerInfo, 200)
		setTimeout(requestTokenInfo, 400)
		setTimeout(loadContent, 400, undefined, true)
	});

	$(window.hyperhdr).on("cmd-effects-update", function (event) {
		window.serverInfo.effects = event.response.data.effects
	});

	$(".nav-link.system-link").bind('click', function (e) {
		loadContent(e);
		window.scrollTo(0, 0);
		if (isSmallScreen())
			$('[data-widget="pushmenu"]').PushMenu('collapse');		
	});
		
	$(document).on('collapsed.lte.pushmenu', function(){
		document.getElementById('showMenuIcon').style.opacity = 0;
		document.getElementById('hideMenuIcon').style.opacity = 1;
	});
	
	$(document).on('shown.lte.pushmenu', function(){
		document.getElementById('showMenuIcon').style.opacity = 1;
		document.getElementById('hideMenuIcon').style.opacity = 0;
	});
});

function suppressDefaultPwWarning(){

  if (document.getElementById('chk_suppressDefaultPw').checked)
	setStorage("suppressDefaultPwWarning", "true");
  else
	setStorage("suppressDefaultPwWarning", "false");
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


