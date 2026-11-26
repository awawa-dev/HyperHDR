$(document).ready( function() {
	performTranslation();
	var oldEffects = [];
	var effects_editor = null;	
	var foregroundEffect_editor = null;
	var backgroundEffect_editor = null;
	var soundEffect_editor = null;
	var isSound = (window.serverInfo.sound != null);
	
	{
		//foreground effect
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/effects_startup_effect.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_fge_heading_title"), 'editor_container_foregroundEffect', 'btn_submit_foregroundEffect'));
		$('#conf_cont').append(createHelpTable(window.schema.foregroundEffect.properties, $.i18n("edt_conf_fge_heading_title")));

		//background effect
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/effects_background_effect.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_bge_heading_title"), 'editor_container_backgroundEffect', 'btn_submit_backgroundEffect'));
		$('#conf_cont').append(createHelpTable(window.schema.backgroundEffect.properties, $.i18n("edt_conf_bge_heading_title")));		
		
		if (isSound)
		{
			//sound effect
			$('#conf_cont').append(createOptPanel('<svg data-src="svg/effects_sound_device.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_sound_heading_title"), 'editor_container_soundEffect', 'btn_submit_soundEffect'));
			$('#conf_cont').append(createHelpTable(window.schema.soundEffect.properties, $.i18n("edt_conf_sound_heading_title")));
		}
	}
	
	var newEffects = window.serverInfo.effects.sort(function(a, b) {
		if (a.name.indexOf('Music:') >= 0 && b.name.indexOf('Music:') < 0)
			return 1;
		if (a.name.indexOf('Music:') < 0 && b.name.indexOf('Music:') >= 0)
			return -1;
		return (a.name > b.name);
	});
	
	var _foregroundEffect = window.schema.foregroundEffect;
	var _backgroundEffect = window.schema.backgroundEffect;

	
	_foregroundEffect.properties.effect.enum = [];
	_backgroundEffect.properties.effect.enum = [];
	_foregroundEffect.properties.effect.options.enum_titles = [];
	_backgroundEffect.properties.effect.options.enum_titles = [];

	
	for(var i = 0; i < newEffects.length; i++)
	{
		var effectName = newEffects[i].name.toString();
		_foregroundEffect.properties.effect.enum.push(effectName);
		_backgroundEffect.properties.effect.enum.push(effectName);
		_foregroundEffect.properties.effect.options.enum_titles.push(effectName);
		_backgroundEffect.properties.effect.options.enum_titles.push(effectName);		
	}
	
	foregroundEffect_editor = createJsonEditor('editor_container_foregroundEffect', {
		foregroundEffect   : _foregroundEffect
	}, true, true, undefined, true);

	backgroundEffect_editor = createJsonEditor('editor_container_backgroundEffect', {
		backgroundEffect   : _backgroundEffect
	}, true, true, undefined, true);

	if (isSound)
	{
		var devices =window.serverInfo.sound.sound_available;
		var _soundEffect = window.schema.soundEffect;
		
		_soundEffect.properties.device.enum = [];
		_soundEffect.properties.device.options = {};
		_soundEffect.properties.device.options.enum_titles = [];
		for(var i = 0; i < devices.length; i++)
		{
			_soundEffect.properties.device.enum.push(devices[i].toString());			
			_soundEffect.properties.device.options.enum_titles.push(devices[i].toString());			
		}
	
		soundEffect_editor = createJsonEditor('editor_container_soundEffect', {
			soundEffect   : _soundEffect
		}, true, true);
	};


	foregroundEffect_editor.on('ready',function() {
	});

	foregroundEffect_editor.on('change',function() {
		foregroundEffect_editor.validate().length ? $('#btn_submit_foregroundEffect').attr('disabled', true) : $('#btn_submit_foregroundEffect').attr('disabled', false);
	});

	backgroundEffect_editor.on('change',function() {
		backgroundEffect_editor.validate().length ? $('#btn_submit_backgroundEffect').attr('disabled', true) : $('#btn_submit_backgroundEffect').attr('disabled', false);
	});
	
	if (isSound)
	{
		soundEffect_editor.on('change',function() {
			soundEffect_editor.validate().length ? $('#btn_submit_soundEffect').attr('disabled', true) : $('#btn_submit_soundEffect').attr('disabled', false);
		});
	};

	$('#btn_submit_foregroundEffect').off().on('click',function() {
		var value = foregroundEffect_editor.getValue();
		if(typeof value.foregroundEffect.effect == 'undefined' && window.serverConfig.foregroundEffect != null)
			value.foregroundEffect.effect = window.serverConfig.foregroundEffect.effect;
		requestWriteConfig(value);
	});

	$('#btn_submit_backgroundEffect').off().on('click',function() {
		var value = backgroundEffect_editor.getValue();
		if(typeof value.backgroundEffect.effect == 'undefined' && window.serverConfig.backgroundEffect != null)
			value.backgroundEffect.effect = window.serverConfig.backgroundEffect.effect;
		requestWriteConfig(value);
	});

	if (isSound)
	{
		$('#btn_submit_soundEffect').off().on('click',function() {
			var value = soundEffect_editor.getValue();
			if(typeof value.soundEffect.device == 'undefined' && window.serverConfig.soundEffect != null)
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
	
	removeOverlay();
});
