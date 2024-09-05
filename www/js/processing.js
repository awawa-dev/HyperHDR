$(document).ready( function() {

	performTranslation();

	var editor_color = null;
	var editor_smoothing = null;
	var editor_blackborder = null;

	function watchForRate()
	{
		var newFps = editor_smoothing.getEditor('root.smoothing.updateFrequency').getValue();
		if (!isNaN(newFps) && newFps > 0 && Math.round(1000.0/newFps) * newFps != 1000)
		{
			newFps = Math.round(1000.0/newFps);
			newFps = 1000.0/newFps;
			$('#realFps').text(newFps.toFixed(2));
		}
		else
			$('#realFps').text("");
	};

	$(window.hyperhdr).off("cmd-hasLedClock-update").on("cmd-hasLedClock-update", function(event)
	{
		if (event.response.success)
		{
			var interval = event.response.info.hasLedClock;
			if ( !isNaN(interval) && interval > 0)
			{
				var label = $("div[data-schemapath='root.smoothing.updateFrequency'] label.required");
				if (label != null)
					label.append( `<br/><span class="text-danger"><small>${$.i18n("controlled_by_led_driver")}</small></span>` );
				editor_smoothing.getEditor('root.smoothing.updateFrequency').setValue(1000/interval);
				editor_smoothing.getEditor('root.smoothing.updateFrequency').disable();
			}
		}		
	});

	requestHasLedClock();
		
	{
		//color		
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/image_processing_color_calibration.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_color_heading_title"), 'editor_container_color', 'btn_submit_color'));
		$('#conf_cont').append(createHelpTable(window.schema.color.properties, $.i18n("edt_conf_color_heading_title")));
		
		//smoothing		
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/image_processing_smoothing.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_smooth_heading_title"), 'editor_container_smoothing', 'btn_submit_smoothing'));
		$('#conf_cont').append(createHelpTable(window.schema.smoothing.properties, $.i18n("edt_conf_smooth_heading_title")));
		
		//blackborder
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/image_processing_blackborder.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_bb_heading_title"), 'editor_container_blackborder', 'btn_submit_blackborder'));
		$('#conf_cont').append(createHelpTable(window.schema.blackborderdetector.properties, $.i18n("edt_conf_bb_heading_title")));
	}	
	
	//color
	editor_color = createJsonEditor('editor_container_color', {
		color              : window.schema.color
	}, true, true, undefined, true);

	editor_color.on('change',function() {
		editor_color.validate().length ? $('#btn_submit_color').attr('disabled', true) : $('#btn_submit_color').attr('disabled', false);
	});
	
	$('#btn_submit_color').off().on('click',function() {
		requestWriteConfig(editor_color.getValue());
	});
	
	//smoothing
	editor_smoothing = createJsonEditor('editor_container_smoothing', {
		smoothing          : window.schema.smoothing
	}, true, true, undefined, true);

	editor_smoothing.on('change',function() {
		editor_smoothing.validate().length ? $('#btn_submit_smoothing').attr('disabled', true) : $('#btn_submit_smoothing').attr('disabled', false);
		
	});
	
	$('#btn_submit_smoothing').off().on('click',function() {
		requestWriteConfig(editor_smoothing.getValue());
	});

	var appendFps = $("div[data-schemapath='root.smoothing.updateFrequency'] div.input-group-text");
	if (appendFps != null)
	{
		let realRefreshRate = document.createElement("span");
		realRefreshRate.setAttribute("id", "realFps");		
		appendFps.get(0).prepend(realRefreshRate);
	};

	editor_smoothing.watch('root.smoothing.updateFrequency',() => {
		watchForRate();
	});
	watchForRate();

	//blackborder
	editor_blackborder = createJsonEditor('editor_container_blackborder', {
		blackborderdetector: window.schema.blackborderdetector
	}, true, true, undefined, true);

	editor_blackborder.on('change',function() {
		editor_blackborder.validate().length ? $('#btn_submit_blackborder').attr('disabled', true) : $('#btn_submit_blackborder').attr('disabled', false);
	});
	
	$('#btn_submit_blackborder').off().on('click',function() {
		requestWriteConfig(editor_blackborder.getValue());
	});
	
	//create introduction
	if(window.showOptHelp)
	{
		createHint("intro", $.i18n('conf_colors_color_intro'), "editor_container_color");
		createHint("intro", $.i18n('conf_colors_smoothing_intro'), "editor_container_smoothing");
		createHint("intro", $.i18n('conf_colors_blackborder_intro'), "editor_container_blackborder");
	}
	
	removeOverlay();
});
