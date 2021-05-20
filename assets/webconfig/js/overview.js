$(document).ready( function() {
	performTranslation();

	putInstanceName(document.getElementById("compent_status_header"));
	
	function updateGrabber()
	{		
		$("#dash_current_video_device").html(window.serverInfo.grabberstate.device);
		$("#dash_current_video_mode").html(window.serverInfo.grabberstate.videoMode);
	}
	
	function updateComponents()
	{
		var components = window.comps;
		var components_html = "";
		for (var idx=0; idx<components.length;idx++)
		{
			if(components[idx].name != "ALL" && components[idx].name != "GRABBER")
				components_html += '<tr><td>'+$.i18n('general_comp_'+components[idx].name)+'</td><td style="padding-left: 1.5em;"><i class="fa fa-circle component-'+(components[idx].enabled?"on":"off")+'"></i></td></tr>';
		}
		$("#tab_components").html(components_html);

		//info
		var hyperhdr_enabled = true;

		components.forEach( function(obj) {
			if (obj.name == "ALL")
			{
				hyperhdr_enabled = obj.enabled
			}
		});

		var instancename = window.currentHyperHdrInstanceName;

		$('#dash_statush').html(hyperhdr_enabled ? '<span style="color:green">'+$.i18n('general_btn_on')+'</span>' : '<span style="color:red">'+$.i18n('general_btn_off')+'</span>');
		$('#btn_hsc').html(hyperhdr_enabled ? '<button class="btn btn-sm btn-danger" onClick="requestSetComponentState(\'ALL\',false)">'+$.i18n('dashboard_infobox_label_disableh', instancename)+'</button>' : '<button class="btn btn-sm btn-success" onClick="requestSetComponentState(\'ALL\',true)">'+$.i18n('dashboard_infobox_label_enableh', instancename)+'</button>');
	}

	// add more info
	$('#dash_leddevice').html(window.serverConfig.device.type);
	$('#dash_currv').html(window.currentVersion);
	$('#dash_instance').html(window.currentHyperHdrInstanceName);

	getReleases(function(callback){
		if(callback)
		{
			$('#dash_latev').html(window.latestVersion.tag_name);

			if (semverLite.gt(window.latestVersion.tag_name, window.currentVersion))
				$('#versioninforesult').html('<div class="callout callout-warning" style="margin:0px"><a target="_blank" href="' + window.latestVersion.html_url + '">'+$.i18n('dashboard_infobox_message_updatewarning', window.latestVersion.tag_name) + '</a></div>');
			else
				$('#versioninforesult').html('<div class="callout callout-success" style="margin:0px">'+$.i18n('dashboard_infobox_message_updatesuccess')+'</div>');

			}
	});



	//determine platform
	var html = "";
	
	if (window.serverInfo.grabbers != null && window.serverInfo.grabbers != undefined &&
	    window.serverInfo.grabbers.active != null && window.serverInfo.grabbers.active != undefined)
	{
		var grabbers = window.serverInfo.grabbers.active;
		if (grabbers.indexOf('Media Foundation') > -1)
			html += 'Windows (Microsoft Media Foundation)';
		else if (grabbers.indexOf('macOS AVF') > -1)
			html += 'macOS (AVF)';
		else if (grabbers.indexOf('V4L2') > -1)
			html += 'Linux (V4L2)';		
		else
			html += 'Unknown';
	}
	else
	{
		html = 'None';
	}

	$('#dash_platform').html(html);


	//interval update
	updateComponents();
	updateGrabber();
	
	$(window.hyperhdr).on("components-updated",updateComponents);
	$(window.hyperhdr).on("grabberstate-update",updateGrabber);

	removeOverlay();
});
