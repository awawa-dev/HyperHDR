$(document).ready(function()
{
	performTranslation();
	
	function createDivRemoteTable(hid, bid, cont)
	{
		var table = document.createElement('div');
		var thead = document.createElement('div');
		var tbody = document.createElement('div');

		table.className = "container p-0";
		table.style.marginBottom = "0px";
		thead.className = hid + " fw-bold container";
		tbody.className = bid + " container";
			
		table.appendChild(thead);	
		table.appendChild(tbody);

		$('#'+cont).append(table);
	}
	
	function createRemoteTableRow(list, head, align)
	{
		var row = document.createElement('div');
		
		row.className = 'row row-cols-4 mb-2 border-bottom';

		for(var i = 0; i < list.length; i++)
		{
			var el = document.createElement('div');
			
			if (i == 0)
				el.className = 'ps-1 pe-0 col-xl-2 col-sm-2 col-xs-2';
			else if (i == 2)
				el.className = 'ps-1 pe-0 col-xl-1 col-sm-2 col-xs-2';
			else if (i == 1)
				el.className = 'ps-1 pe-0 col-xl-5 col-sm-4 col-xs-4';
			else
				el.className = 'ps-1 pe-0 col-xl-4 col-sm-4 col-xs-4';
			
			if(head === true)
				el.style.fontWeight = "bold";
			
			if(align)
				el.style.verticalAlign = "middle";

			el.innerHTML = list[i];
			row.appendChild(el);
		}
		return row;
	}

	var oldEffects = [];
	var cpcolor = '#B500FF';
	var mappingList = window.serverSchema.properties.color.properties.imageToLedMappingType.enum;
	var duration = 0;
	var rgb = {
		r: 255,
		g: 0,
		b: 0
	};
	var lastImgData = "";
	var lastImgWidth = -1;
	var lastImgHeight = -1;
	var lastFileName = "";
	var colorAdjustmentEnabled = true;

	//create html
	createDivRemoteTable('ssthead', 'sstbody', 'sstcont');
	$('.ssthead').html(createRemoteTableRow([$.i18n('remote_input_origin'), $.i18n('remote_input_owner'), $.i18n('remote_input_priority'), $.i18n('remote_input_status')], true, true));
	createTable('crthead', 'crtbody', 'adjust_content', true);

	//create introduction
	if (window.showOptHelp)
	{
		createHint("intro", $.i18n('remote_color_intro', $.i18n('remote_losthint')), "color_intro");
		createHint("intro", $.i18n('remote_input_intro', $.i18n('remote_losthint')), "sstcont");
		createHint("intro", $.i18n('remote_adjustment_intro', $.i18n('remote_losthint')), "adjust_content");
		createHint("intro", $.i18n('remote_components_intro', $.i18n('remote_losthint')), "comp_intro");
		createHint("intro", $.i18n('remote_maptype_intro', $.i18n('remote_losthint')), "maptype_intro");
		createHint("intro", $.i18n('remote_videoMode_intro', $.i18n('remote_losthint')), "videomode_intro");
		createHint("intro", $.i18n('remote_videoModeHdr_intro', $.i18n('remote_losthint')), "videomodehdr_intro");
	}

	const editor_color = createJsonEditor('conf_cont', {
		color              : window.schema.color.properties.channelAdjustment.items
	}, true, true);	
	
	function BindColorCalibration()
	{
		for(var key in window.schema.color.properties.channelAdjustment.items.properties)
		{
			const sourceKey = key;
			const sourcePath = 'root.color.'+key;
			const selectEditor = editor_color.getEditor(sourcePath);
			
			if ((selectEditor.path == "root.color.id") || (selectEditor.path == "root.color.leds"))
				selectEditor.container.hidden = true; 
			else
				editor_color.watch(sourcePath,() => {
					if (colorAdjustmentEnabled)
					{
						const editor = editor_color.getEditor(sourcePath);
					
						if (editor.format === "colorpicker")
							requestAdjustment(sourceKey, '['+editor.retVal[0]+','+editor.retVal[1]+','+editor.retVal[2]+']');
						else
							requestAdjustment(sourceKey, editor.value);
					}
				});				
		}
	}
	
	function updateColorAdjustment()
	{	
		if (Array.isArray(window.serverInfo.adjustment) && window.serverInfo.adjustment.length > 0)
			editor_color.getEditor("root.color").setValue(window.serverInfo.adjustment[0]);
		else if (window.serverConfig['color'].hasOwnProperty('channelAdjustment') && window.serverConfig['color'].channelAdjustment.length > 0)
			editor_color.getEditor("root.color").setValue(window.serverConfig['color'].channelAdjustment[0]);

		BindColorCalibration();
	}	
	

	function sendEffect()
	{
		var efx = $("#effect_select").val();
		if (efx != "__none__" && efx != null)
		{
			requestPriorityClear();
			$(window.hyperhdr).one("cmd-clear", function(event)
			{
				setTimeout(function()
				{
					requestPlayEffect(efx, duration)
				}, 100);
			});
		}
	}

	function sendColor()
	{
		requestSetColor(rgb.r, rgb.g, rgb.b, duration);
	}

	function updateInputSelect()
	{
		$('.sstbody').html("");
		var prios = window.serverInfo.priorities;
		var clearAll = false;

		for (var i = 0; i < prios.length; i++)
		{
			var origin = prios[i].origin ? prios[i].origin : "System";
			origin = origin.split("@");
			var ip = origin[1];
			origin = origin[0];

			var owner = prios[i].owner;
			var active = prios[i].active;
			var visible = prios[i].visible;
			var priority = prios[i].priority;
			var compId = prios[i].componentId;
			var duration = prios[i].duration_ms / 1000;
			var value = "0,0,0";
			var btn_type = "default";
			var btn_text = $.i18n('remote_input_setsource_btn');
			var btn_state = "";

			if (compId == "GRABBER")
				continue;

			if (active)
				btn_type = "primary";

			if (priority > 254)
				continue;
			if (priority < 254 && (compId == "EFFECT" || compId == "COLOR" || compId == "IMAGE"))
				clearAll = true;

			if (visible)
			{
				btn_state = "disabled";
				btn_type = "success";
				btn_text = $.i18n('remote_input_sourceactiv_btn');
			}

			if (ip)
				origin += '<br/><span class="small text-secondary">' + $.i18n('remote_input_ip') + ' ' + ip + '</span>';

			if ("value" in prios[i])
				value = prios[i].value.RGB;

			switch (compId)
			{
				case "EFFECT":
					owner = $.i18n('remote_effects_label_effects') + ' ' + owner;
					break;
				case "COLOR":
					owner = $.i18n('remote_color_label_color') + '  ' + '<div style="width:18px; height:18px; border-radius:20px; margin-bottom:-4px; border:1px grey solid; background-color: rgb(' + value + '); display:inline-block" title="RGB: (' + value + ')"></div>';
					break;
				case "IMAGE":
					owner = $.i18n('remote_effects_label_picture') + ' ' + owner;
					break;
				case "SYSTEMGRABBER":
					owner = $.i18n('general_comp_GRABBER') + ': <span class="small text-secondary">' + owner + '<span/>';
					break;
				case "VIDEOGRABBER":
					owner = $.i18n('general_comp_VIDEOGRABBER') + ': <span class="small text-secondary">' + owner + '<span/>';
					break;
				case "FLATBUFSERVER":
					owner = $.i18n('general_comp_FLATBUFSERVER');
					break;
				case "PROTOSERVER":
					owner = $.i18n('general_comp_PROTOSERVER');
					break;
				case "RAWUDPSERVER":
					owner = $.i18n('general_comp_RAWUDPSERVER');
					break;					
			}

			if (duration && compId != "GRABBER" && compId != "FLATBUFSERVER" && compId != "PROTOSERVER")
				owner += '<br/><span style="font-size:80%; color:grey;">' + $.i18n('remote_input_duration') + ' ' + duration.toFixed(0) + $.i18n('edt_append_s') + '</span>';

			var btn = '<button id="srcBtn' + i + '" type="button" ' + btn_state + ' class="btn btn-' + btn_type + ' btn_input_selection mb-2" onclick="requestSetSource(' + priority + ');">' + btn_text + '</button>';

			if ((compId == "EFFECT" || compId == "COLOR" || compId == "IMAGE") && priority < 254)
				btn += '<button type="button" class="btn btn-danger ms-1 mb-2" onclick="requestPriorityClear(' + priority + ');"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr me-0"></svg></button>';

			if (btn_type != 'default')
				$('.sstbody').append(createRemoteTableRow([origin, owner, priority, btn], false, true));
		}
		var btn_auto_color = (window.serverInfo.priorities_autoselect ? "btn-success" : "btn-danger");
		var btn_auto_state = (window.serverInfo.priorities_autoselect ? "disabled" : "");
		var btn_auto_text = (window.serverInfo.priorities_autoselect ? $.i18n('general_btn_on') : $.i18n('general_btn_off'));
		var btn_call_state = (clearAll ? "" : "disabled");
		$('#auto_btn').html('<button id="srcBtn' + i + '" type="button" ' + btn_auto_state + ' class="mb-1 btn ' + btn_auto_color + '" style="margin-right:5px;display:inline-block;" onclick="requestSetSource(\'auto\');">' + $.i18n('remote_input_label_autoselect') + ' (' + btn_auto_text + ')</button>');
		$('#auto_btn').append('<button type="button" ' + btn_call_state + ' class="mb-1 btn btn-danger" style="display:inline-block;" onclick="requestClearAll();">' + $.i18n('remote_input_clearall') + '</button>');		
	}

	function updateLedMapping()
	{
		var mapping = window.serverInfo.imageToLedMappingType;

		$('#mappingsbutton').html("");
		for (var ix = 0; ix < mappingList.length; ix++)
		{
			var btn_style = (mapping == mappingList[ix]) ? 'btn-success' : 'btn-primary';
			var newRow = $('<div class="row">').append('<button type="button" id="lmBtn_' + mappingList[ix] + '" class="btn ' + btn_style + '" style="margin:3px;min-width:200px" onclick="requestMappingType(\'' + mappingList[ix] + '\');">' + $.i18n('remote_maptype_label_' + mappingList[ix]) + '</button>');
			
			$('#mappingsbutton').append(newRow);
		}
	}

	function initComponents()
	{
		var components = window.comps;		

		for (const comp of components)
		{
			if (comp.name === "ALL" || (comp.name === "FORWARDER" && window.serverInfo.currentInstance != 0))
				continue;					

			const comp_btn_id = "comp_btn_" + comp.name;

			if ($("#" + comp_btn_id).length === 0)
			{
				var d = '<div class="m-2 mb-3 d-flex align-items-center justify-content-start">' +
					`<input id="${comp_btn_id}" type="checkbox" class="mobileToggle"/><label for="${comp_btn_id}"></label>` +
					`<label class="ms-3">${$.i18n('general_comp_' + comp.name)}</label>` +
					'</div>';

				$('#componentsbutton').append(d);
				updateComponent(comp);
			}
		}
	}

	function updateComponent(component)
	{
		let compButton = $(`#comp_btn_${component.name}`);
		if (compButton.length)
		{		
			compButton.off('change');
				
			compButton.prop('checked', component.enabled);
			compButton.prop('disabled', false);
				
			compButton.change(e =>
			{
				e.currentTarget.setAttribute('disabled', 'disabled');
				requestSetComponentState(e.currentTarget.id.split('_').pop(), e.currentTarget.checked);
			});
		}
	}

	function updateEffectlist()
	{
		var newEffects = window.serverInfo.effects;
		if (newEffects.length != oldEffects.length)
		{
			$('#effect_select').html('<option hidden disabled selected value="__none__">&nbsp;</option>');
			var soundEffects = [];
			var classicEffects = [];

			for (var i = 0; i < newEffects.length; i++)
			{
				var effectName = newEffects[i].name;
				
				if (effectName.indexOf('Music:') >= 0)
					soundEffects.push(effectName);
				else
					classicEffects.push(effectName);

			}
			$('#effect_select').append(createSel(classicEffects, $.i18n('remote_optgroup_classicEffects')));
			$('#effect_select').append(createSel(soundEffects, $.i18n('remote_optgroup_musicEffects')));
			oldEffects = newEffects;
		}
	}

	function updateVideoModeHdr()
	{
		var videoModes = [$.i18n("general_btn_off").toUpperCase(), $.i18n("general_btn_on").toUpperCase()];
		var videoModesVal = [0, 1];
		var currVideoMode = window.serverInfo.videomodehdr;

		$('#videomodehdrbtns').html("");
		for (var ix = 0; ix < videoModes.length; ix++)
		{
			if (((currVideoMode) ? 1 : 0 ) == videoModesVal[ix])
				var btn_style = 'btn-success';
			else
				var btn_style = 'btn-primary';
			$('#videomodehdrbtns').append('<div class="row"><button type="button" id="vModeHdrBtn_' + videoModesVal[ix] + '" class="btn ' + btn_style + '" style="margin:3px;min-width:200px" onclick="requestVideoModeHdr(' + videoModesVal[ix] + ');">' + videoModes[ix] + '</button></div>');
		}
	}

	// colorpicker and effect
	if (getStorage('rmcpcolor') != null)
	{		      
		cpcolor = getStorage('rmcpcolor');
		
		if (!(/^#[0-9A-F]{6}$/i.test(cpcolor)))
		{
			cpcolor = "#000000";			
		}		
			
		rgb = hexToRgb(cpcolor);
	}

	if (getStorage('rmduration') != null)
	{
		$("#remote_duration").val(getStorage('rmduration'));
		duration = getStorage('rmduration');
	}

	var pickerInit = false;
	const colorParent = document.querySelector('#remote_color');	
	const picker = new Picker(
	{
		parent: colorParent,
		editor: true,
		alpha: false,
		color: cpcolor,
		editorFormat: 'rgb',
		popup: 'bottom',
		onChange: function(color)
		{			
			if (color != null && color.rgba != null && !isNaN(color.rgba[0]) && !isNaN(color.rgba[1]) && !isNaN(color.rgba[2]))
			{
				var myHex = (color.hex.length >= 7) ? color.hex.slice(0, 7) : color.hex;
				
				$('#remote_color_target').val(color.rgbString);
				document.querySelector('#remote_color_target_invoker').style.backgroundColor = color.rgbaString;
				
				if (!pickerInit)
					pickerInit = true;
				else
				{														
					rgb.r = color.rgba[0];
					rgb.g = color.rgba[1];
					rgb.b = color.rgba[2];
					sendColor();
					setStorage('rmcpcolor', myHex);
					updateInputSelect();					
				}
			}
		}
	});
	
	$("#remote_color_target_invoker").off().on("click", function()
	{
		picker.openHandler();
	});
	
	$("#remote_color_target").off().on("click", function()
	{
		picker.openHandler();
	});
	
	$("#reset_color").off().on("click", function()
	{
		requestPriorityClear();
		lastImgData = "";
		$("#effect_select").val("__none__");
		$("#remote_input_img").val("");
	});

	$("#remote_duration").off().on("change", function()
	{
		duration = valValue(this.id, this.value, this.min, this.max);
		setStorage('rmduration', duration);
	});

	$("#effect_select").off().on("change", function(event)
	{
		sendEffect();
	});

	$("#remote_input_reseff, #remote_input_rescol").off().on("click", function()
	{
		if (this.id == "remote_input_rescol")
		{
			sendColor();			
		}
		else
			sendEffect();
	});

	$("#remote_input_repimg").off().on("click", function()
	{
		if (lastImgData != "")
			requestSetImage(lastImgData, lastImgWidth, lastImgHeight, duration, lastFileName);
	});

	$("#remote_input_img").change(function()
	{
		readImg(this, function(src, fileName)
		{
			lastFileName = fileName;

			const imgLoader = new Image();
			imgLoader.onload = function () {
				const tempCanvas = document.createElement('canvas');
				tempCanvas.height = imgLoader.naturalHeight;
				tempCanvas.width = imgLoader.naturalWidth;
				const ctx = tempCanvas.getContext('2d');
				ctx.drawImage(imgLoader, 0, 0);				

				lastImgData = tempCanvas.toDataURL('image/png').split(",")[1];
				lastImgWidth = tempCanvas.width;
				lastImgHeight = tempCanvas.height
				requestSetImage(lastImgData, lastImgWidth, lastImgHeight, duration, lastFileName);
			}
			imgLoader.src = src;
		});
	});
	
	function StepUpDown(me, where)
	{
		var target = me.parentElement.parentElement.firstElementChild;
		if (typeof target !== 'undefined' && target.type === "number")
		{
			if (where == "down")
				target.stepDown();
			else
				target.stepUp();
			$(target).trigger('change');
		};
	};
	
	$("#effect-stepper-down").off().on("click", function(event) { StepUpDown(this, "down"); });	
	$("#effect-stepper-up").off().on("click", function(event) { StepUpDown(this, "up"); });

	//force first update
	initComponents();
	updateInputSelect();
	updateLedMapping();
	updateVideoModeHdr();
	updateEffectlist();
	updateColorAdjustment();
	
	
	var instHeaders = document.getElementsByClassName("card-header");
	Array.prototype.forEach.call(instHeaders, function(instHeader, index) {
		if (!instHeader.classList.contains('not-instance'))
			putInstanceName(instHeader);
	});

	// interval updates

	$(window.hyperhdr).on('components-updated', function(e, comp)
	{
		//console.log ("components-updated", e, comp);
		updateComponent(comp);
	});

	$(window.hyperhdr).on("priorities-update", function(event)
	{
		updateInputSelect();
	});

	$(window.hyperhdr).on("adjustment-update", function (event, colorData) {
		colorAdjustmentEnabled = false;
		for(var key in colorData)
		{
			const selectEditor = editor_color.getEditor('root.color.'+key);
			if (selectEditor != null && selectEditor.getValue() != colorData[key])
				selectEditor.setValue(colorData[key]);
		}
		colorAdjustmentEnabled = true;
	});
	
	$(window.hyperhdr).on("cmd-imageToLedMapping-update", function(event)
	{
		window.serverInfo.imageToLedMappingType = event.response.data.imageToLedMappingType;
		updateLedMapping();
	});

	$(window.hyperhdr).on("cmd-videomodehdr-update", function(event)
	{
		window.serverInfo.videomodehdr = event.response.data.videomodehdr;
		updateVideoModeHdr();
	});

	removeOverlay();
});
