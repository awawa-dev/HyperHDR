$(document).ready( function() {
	performTranslation();
	var editor_color = null;
	var editor_smoothing = null;
	var editor_blackborder = null;
		
	{
		//color		
		$('#conf_cont').append(createOptPanel('fa-photo', $.i18n("edt_conf_color_heading_title"), 'editor_container_color', 'btn_submit_color'));
		$('#conf_cont').append(createHelpTable(window.schema.color.properties, $.i18n("edt_conf_color_heading_title")));
		
		//smoothing		
		$('#conf_cont').append(createOptPanel('fa-photo', $.i18n("edt_conf_smooth_heading_title"), 'editor_container_smoothing', 'btn_submit_smoothing'));
		$('#conf_cont').append(createHelpTable(window.schema.smoothing.properties, $.i18n("edt_conf_smooth_heading_title")));
		
		//blackborder
		$('#conf_cont').append(createOptPanel('fa-photo', $.i18n("edt_conf_bb_heading_title"), 'editor_container_blackborder', 'btn_submit_blackborder'));
		$('#conf_cont').append(createHelpTable(window.schema.blackborderdetector.properties, $.i18n("edt_conf_bb_heading_title")));
	}	
	
	//color
	editor_color = createJsonEditor('editor_container_color', {
		color              : window.schema.color
	}, true, true, undefined, true);

	editor_color.on('change',function() {
		editor_color.validate().length || window.readOnlyMode ? $('#btn_submit_color').attr('disabled', true) : $('#btn_submit_color').attr('disabled', false);
	});
	
	$('#btn_submit_color').off().on('click',function() {
		requestWriteConfig(editor_color.getValue());
	});
	
	//smoothing
	editor_smoothing = createJsonEditor('editor_container_smoothing', {
		smoothing          : window.schema.smoothing
	}, true, true, undefined, true);

	editor_smoothing.on('change',function() {
		editor_smoothing.validate().length || window.readOnlyMode ? $('#btn_submit_smoothing').attr('disabled', true) : $('#btn_submit_smoothing').attr('disabled', false);
		
	});
	
	$('#btn_submit_smoothing').off().on('click',function() {
		requestWriteConfig(editor_smoothing.getValue());
	});

	//blackborder
	editor_blackborder = createJsonEditor('editor_container_blackborder', {
		blackborderdetector: window.schema.blackborderdetector
	}, true, true, undefined, true);

	editor_blackborder.on('change',function() {
		editor_blackborder.validate().length || window.readOnlyMode ? $('#btn_submit_blackborder').attr('disabled', true) : $('#btn_submit_blackborder').attr('disabled', false);
	});
	
	$('#btn_submit_blackborder').off().on('click',function() {
		requestWriteConfig(editor_blackborder.getValue());
	});
	
	//wiki links
	//$('#editor_container_blackborder').append(buildWL("user/moretopics/bbmode","edt_conf_bb_mode_title",true));
	
	//create introduction
	if(window.showOptHelp)
	{
		createHint("intro", $.i18n('conf_colors_color_intro'), "editor_container_color");
		createHint("intro", $.i18n('conf_colors_smoothing_intro'), "editor_container_smoothing");
		createHint("intro", $.i18n('conf_colors_blackborder_intro'), "editor_container_blackborder");
	}
	
	removeOverlay();
});
