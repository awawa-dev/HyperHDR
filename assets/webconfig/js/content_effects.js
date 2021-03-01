$(document).ready( function() {
	performTranslation();
	var oldEffects = [];
	var effects_editor = null;
	var confFgEff = window.serverConfig.foregroundEffect.effect;
	var confBgEff = window.serverConfig.backgroundEffect.effect;
	var soundDevice = window.serverConfig.soundEffect.device;
	var foregroundEffect_editor = null;
	var backgroundEffect_editor = null;
	var soundEffect_editor = null;
	var isSound = (window.serverInfo.sound != null);
	if(window.showOptHelp)
	{
		//foreground effect
		$('#conf_cont').append(createRow('conf_cont_fge'));
		$('#conf_cont_fge').append(createOptPanel('fa-spinner', $.i18n("edt_conf_fge_heading_title"), 'editor_container_foregroundEffect', 'btn_submit_foregroundEffect'));
		$('#conf_cont_fge').append(createHelpTable(window.schema.foregroundEffect.properties, $.i18n("edt_conf_fge_heading_title")));

		//background effect
		$('#conf_cont').append(createRow('conf_cont_bge'));
		$('#conf_cont_bge').append(createOptPanel('fa-spinner', $.i18n("edt_conf_bge_heading_title"), 'editor_container_backgroundEffect', 'btn_submit_backgroundEffect'));
		$('#conf_cont_bge').append(createHelpTable(window.schema.backgroundEffect.properties, $.i18n("edt_conf_bge_heading_title")));		
		
		if (isSound)
		{
			//sound effect
			$('#conf_cont').append(createRow('conf_cont_sound'));
			$('#conf_cont_sound').append(createOptPanel('fa-spinner', $.i18n("edt_conf_sound_heading_title"), 'editor_container_soundEffect', 'btn_submit_soundEffect'));
			$('#conf_cont_sound').append(createHelpTable(window.schema.soundEffect.properties, $.i18n("edt_conf_sound_heading_title")));
		}
	}
	else
	{
		$('#conf_cont').addClass('row');
		$('#conf_cont').append(createOptPanel('fa-spinner', $.i18n("edt_conf_fge_heading_title"), 'editor_container_foregroundEffect', 'btn_submit_foregroundEffect'));
		$('#conf_cont').append(createOptPanel('fa-spinner', $.i18n("edt_conf_bge_heading_title"), 'editor_container_backgroundEffect', 'btn_submit_backgroundEffect'));		
		
		if (isSound)
			$('#conf_cont').append(createOptPanel('fa-spinner', $.i18n("edt_conf_sound_heading_title"), 'editor_container_soundEffect', 'btn_submit_soundEffect'));		
	}	

	foregroundEffect_editor = createJsonEditor('editor_container_foregroundEffect', {
		foregroundEffect   : window.schema.foregroundEffect
	}, true, true);

	backgroundEffect_editor = createJsonEditor('editor_container_backgroundEffect', {
		backgroundEffect   : window.schema.backgroundEffect
	}, true, true);

	if (isSound)
	{
		soundEffect_editor = createJsonEditor('editor_container_soundEffect', {
			soundEffect   : window.schema.soundEffect
		}, true, true);
	};


	foregroundEffect_editor.on('ready',function() {
		updateEffectlist();
	});

	foregroundEffect_editor.on('change',function() {
		foregroundEffect_editor.validate().length || window.readOnlyMode ? $('#btn_submit_foregroundEffect').attr('disabled', true) : $('#btn_submit_foregroundEffect').attr('disabled', false);
	});

	backgroundEffect_editor.on('change',function() {
		backgroundEffect_editor.validate().length || window.readOnlyMode ? $('#btn_submit_backgroundEffect').attr('disabled', true) : $('#btn_submit_backgroundEffect').attr('disabled', false);
	});
	
	if (isSound)
	{
		soundEffect_editor.on('change',function() {
			soundEffect_editor.validate().length || window.readOnlyMode ? $('#btn_submit_soundEffect').attr('disabled', true) : $('#btn_submit_soundEffect').attr('disabled', false);
		});
	};

	$('#btn_submit_foregroundEffect').off().on('click',function() {
		var value = foregroundEffect_editor.getValue();
		if(typeof value.foregroundEffect.effect == 'undefined')
			value.foregroundEffect.effect = window.serverConfig.foregroundEffect.effect;
		requestWriteConfig(value);
	});

	$('#btn_submit_backgroundEffect').off().on('click',function() {
		var value = backgroundEffect_editor.getValue();
		if(typeof value.backgroundEffect.effect == 'undefined')
			value.backgroundEffect.effect = window.serverConfig.backgroundEffect.effect;
		requestWriteConfig(value);
	});

	if (isSound)
	{
		$('#btn_submit_soundEffect').off().on('click',function() {
			var value = soundEffect_editor.getValue();
			if(typeof value.soundEffect.device == 'undefined')
				value.soundEffect.device = window.serverConfig.soundEffect.device;
			requestWriteConfig(value);
		});
	};

	//create introduction
	if(window.showOptHelp)
	{
		createHint("intro", $.i18n('conf_effect_fgeff_intro'), "editor_container_foregroundEffect");
		createHint("intro", $.i18n('conf_effect_bgeff_intro'), "editor_container_backgroundEffect");
		
		if (isSound)	
			createHint("intro", $.i18n('conf_effect_sndeff_intro'), "editor_container_soundEffect");
	}


	if (isSound)
	{
		var devices = window.serverInfo.sound.sound_available;

		if (devices.length > 0)
		{
			$('#root_soundEffect_device').html('');			
			var sysSndArr = [];

			for(var i = 0; i < devices.length; i++)
			{
				sysSndArr.push(devices[i]);
			}
			
			$('#root_soundEffect_device').append(createSel(sysSndArr, $.i18n('available_sound_devices')));
			$('#root_soundEffect_device').val(soundDevice);			
		}
	}
	
	function updateEffectlist(){
		var newEffects = window.serverInfo.effects;
		if (newEffects.length != oldEffects.length)
		{
			$('#root_foregroundEffect_effect').html('');
			var sysEffArr = [];

			for(var i = 0; i < newEffects.length; i++)
			{
				var effectName = newEffects[i].name;
				sysEffArr.push(effectName);
			}
			$('#root_foregroundEffect_effect').append(createSel(sysEffArr, $.i18n('remote_optgroup_syseffets')));
			$('#root_backgroundEffect_effect').html($('#root_foregroundEffect_effect').html());
			oldEffects = newEffects;

			$('#root_foregroundEffect_effect').val(confFgEff);
			$('#root_backgroundEffect_effect').val(confBgEff);
		}
	}

	//interval update
	$(window.hyperion).on("cmd-effects-update", function(event){
		window.serverInfo.effects = event.response.data.effects
		updateEffectlist();
	});

	removeOverlay();
});
