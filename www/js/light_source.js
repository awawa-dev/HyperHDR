var ledsCustomCfgInitialized = false;
var finalLedArray = [];
var conf_editor = null;
var aceEdt = null;
var ledStarter = false;

function round(number)
{
	var factor = Math.pow(10, 4);
	var tempNumber = number * factor;
	var roundedTempNumber = Math.round(tempNumber);
	return roundedTempNumber / factor;
};

var _lastLeds = [];
var _lastOrigin = "";
var _resizeObserver = null;

if (typeof ResizeObserver === "function" && _resizeObserver === null)
{
	_resizeObserver = new ResizeObserver(entries => {		
		if ( _lastOrigin != "" && _lastLeds.length > 0 && $('#previewledcount').length)
		{
			createLedPreview(_lastLeds, _lastOrigin);			
		}
	});	
}

function createLedPreview(leds, origin)
{
	_lastLeds = leds;
	_lastOrigin = origin;
	
	if (!ledStarter)
		$('#collapse1').collapse('toggle');
	
	if (origin == "classic")
	{		
		$('#leds_preview').css("padding-top", "56.25%").css("position","relative");
	}
	else if (origin == "text")
	{
		$('#leds_preview').css("padding-top", "56.25%").css("position","relative");
	}
	else if (origin == "matrix")
	{		
		$('#leds_preview').css("padding-top", "100%").css("position","relative");
	}
	else if (origin == "zoom")
	{		
		$('#leds_preview').css("padding-top", "44%").css("position","relative");
	}
	
	ledStarter = true;

	$('#previewledcount').html(`${$.i18n('conf_leds_layout_preview_totalleds', leds.length)} (${$.i18n('conf_leds_layout_preview_ledpower', ((leds.length * 0.06) * 1.1).toFixed(1))})`);

	$('.st_helper').css("border", "8px solid grey");

	var canvas_height = $('#leds_preview').innerHeight();
	var canvas_width = $('#leds_preview').innerWidth();

	var leds_html = "";
	var hashTable = {};
	var groups = false;
	var disabledLed = false;
	
	for (var idx = 0; idx < leds.length; idx++)
	{		
		var led = leds[idx];
		var led_id = 'ledc_' + [idx];
		var bgcolor = "hsla(" + (idx * 360 / leds.length) + ",100%,50%,0.75)";
		var optGroup = "";

		if (led.disabled === true)
		{
			bgcolor = "rgba(255, 255, 255, 0.75)";
			disabledLed = true;
		}
		else if (led.group !== undefined && led.group != 0)
		{
			if (led.group in hashTable)			
				bgcolor = hashTable[led.group];			
			else
				hashTable[led.group] = bgcolor;
			
			optGroup = ", group: " + led.group;
			
			groups = true;
		}
		
		var pos = "left:" + Math.round(led.hmin * canvas_width) + "px;" +
			"top:" + Math.round(led.vmin * canvas_height) + "px;" +
			"width:" + Math.min((led.hmax - led.hmin) * canvas_width + 1, (canvas_width - 1)) + "px;" +
			"height:" + Math.min((led.vmax - led.vmin) * canvas_height + 1, (canvas_height - 1)) + "px;";
		leds_html += '<div id="' + led_id + '" group="'+led.group+'" class="led" style="background-color: ' + bgcolor + ';' + pos + '" title="' + idx + optGroup + '"><span id="' + led_id + '_num" class="led_prev_num">' + ((led.name) ? led.name : idx) + '</span></div>';

	}

	if ($._data(document.getElementById('visualCreatorPanel'), "events") != null)
		leds_html += '<div data-i18n="conf_leds_layout_frame" style="position: absolute; text-align: center; left: ' + (0.2 * canvas_width) + 'px; top: ' + (0.35 * canvas_height) + 'px; width: ' + (0.6 * canvas_width) + 'px;">'+$.i18n('led_editor_context_moving')+'</div>';
	if (groups)
		leds_html += '<div data-i18n="conf_leds_layout_frame" style="position: absolute; text-align: center; left: ' + (0.2 * canvas_width) + 'px; top: ' + (0.45 * canvas_height) + 'px; width: ' + (0.6 * canvas_width) + 'px;">'+$.i18n('conf_leds_grouping_notification')+'</div>';
	if (disabledLed)
		leds_html += '<div data-i18n="conf_leds_layout_frame" style="position: absolute; text-align: center; left: ' + (0.2 * canvas_width) + 'px; top: ' + (0.55 * canvas_height) + 'px; width: ' + (0.6 * canvas_width) + 'px;">'+$.i18n('conf_leds_disabled_notification')+'</div>';
	
	$('#leds_preview').html(leds_html);

	//first LEDs
	var colors = ["rgba(0,0,0,0.8)", "rgba(128,128,128,0.8)", "rgba(169,169,169,0.8)"];
	for (var i = 0; i < 3 && i < leds.length; i++)		
	{		
		var led = leds[i];
		var angle = Math.round(90 - Math.atan((led.vmax - led.vmin) * canvas_height / (Math.max(led.hmax - led.hmin, 0.0000001) * canvas_width)) * (180 / Math.PI));
		var hColor = `-webkit-linear-gradient(${angle}deg, rgba(255,255,255,0.0) 50%, ${colors[i]} 50%)`;
		var zIndex = 12-i;

		if (led.group !== undefined && led.group != 0 && !(leds[i].disabled === true))
			$('#ledc_'+i).css({ "background-image": hColor, "z-index": zIndex });
		else
			$('#ledc_'+i).css({ "background-color": colors[i], "z-index": zIndex });
	}

	// disabled LEDs
	for (var i = 0; i < leds.length; i++)
		if (leds[i].disabled === true)
		{		
			$('#ledc_'+i).addClass((i>=3) ? "crosslineDark" : "crosslineWhite");			
		}

	if ($('#leds_prev_toggle_num').hasClass('btn-success'))
		$('.led_prev_num').css("display", "inline");

	// update ace Editor content
	aceEdt.set(finalLedArray);
}

function createClassicLedLayoutSimple(ledstop, ledsleft, ledsright, ledsbottom, position, groupX, groupY, reverse)
{

	let params = {
		ledstop: 0,
		ledsleft: 0,
		ledsright: 0,
		ledsbottom: 0,
		ledsglength: 0,
		ledsgpos: 0,
		position: 0,
		groupX: 0,
		groupY: 0,
		reverse: false,
		ledsHDepth: 0.08,
		ledsVDepth: 0.05,
		overlap: 0,
		edgeVGap: 0,
		ptblh: 0,
		ptblv: 1,
		ptbrh: 1,
		ptbrv: 1,
		pttlh: 0,
		pttlv: 0,
		pttrh: 1,
		pttrv: 0		
	};

	params.ledstop = ledstop;
	params.ledsleft = ledsleft;
	params.ledsright = ledsright;
	params.ledsbottom = ledsbottom;
	params.position = position;
	params.groupX = groupX;
	params.groupY = groupY;
	params.reverse = reverse;

	return createClassicLedLayout(params);
}

function createClassicLedLayout(params)
{
	//helper
	var edgeHGap = params.edgeVGap / (16 / 9);
	var ledArray = [];

	function createFinalArray(array)
	{
		var finalLedArray = [];
		for (var i = 0; i < array.length; i++)
		{
			var hmin = array[i].hmin;
			var hmax = array[i].hmax;
			var vmin = array[i].vmin;
			var vmax = array[i].vmax;
			var group = array[i].group;
			finalLedArray[i] = {
				"hmax": hmax,
				"hmin": hmin,
				"vmax": vmax,
				"vmin": vmin,
				"group": group
			}
		}
		return finalLedArray;
	}

	function rotateArray(array, times)
	{
		if (times > 0)
		{
			while (times--)
			{
				array.push(array.shift())
			}
			return array;
		}
		else
		{
			while (times++)
			{
				array.unshift(array.pop())
			}
			return array;
		}
	}

	function valScan(val)
	{
		if (val > 1)
			return 1;
		if (val < 0)
			return 0;
		return val;
	}

	function ovl(scan, val)
	{
		if (scan == "+")
			return valScan(val += params.overlap);
		else
			return valScan(val -= params.overlap);
	}

	function createLedArray(hmin, hmax, vmin, vmax, group)
	{
		hmin = round(hmin);
		hmax = round(hmax);
		vmin = round(vmin);
		vmax = round(vmax);
		ledArray.push(
		{
			"hmin": hmin,
			"hmax": hmax,
			"vmin": vmin,
			"vmax": vmax,
			"group": group
		});
	}

	function createTopLeds(groupStart)
	{
		var steph = (params.pttrh - params.pttlh - (2 * edgeHGap)) / params.ledstop;
		var stepv = (params.pttrv - params.pttlv) / params.ledstop;
		var maxGroup = 0;
		
		for (var i = 0; i < params.ledstop; i++)
		{
			var hmin = ovl("-", params.pttlh + (steph * Number([i])) + edgeHGap);
			var hmax = ovl("+", params.pttlh + (steph * Number([i + 1])) + edgeHGap);
			var vmin = params.pttlv + (stepv * Number([i]));
			var vmax = vmin + params.ledsHDepth;
						
			if (params.groupX > 0 && (params.groupX * 2 <= params.ledstop))
			{
				var group = Math.floor(((i * params.groupX) / params.ledstop))+1;
				maxGroup = Math.max(maxGroup, group);
				createLedArray(hmin, hmax, vmin, vmax, group + groupStart);
			}
			else
				createLedArray(hmin, hmax, vmin, vmax, 0);
		}
		
		return maxGroup + groupStart;
	}

	function createRightLeds(groupStart)
	{
		var steph = (params.ptbrh - params.pttrh) / params.ledsright;
		var stepv = (params.ptbrv - params.pttrv - (2 * params.edgeVGap)) / params.ledsright;
		var maxGroup = 0;
		
		for (var i = 0; i < params.ledsright; i++)
		{
			var hmax = params.pttrh + (steph * Number([i + 1]));
			var hmin = hmax - params.ledsVDepth;
			var vmin = ovl("-", params.pttrv + (stepv * Number([i])) + params.edgeVGap);
			var vmax = ovl("+", params.pttrv + (stepv * Number([i + 1])) + params.edgeVGap);

			if (params.groupY > 0 && (params.groupY * 2 <= params.ledsright))
			{
				var group = Math.floor(((i * params.groupY) / params.ledsright))+1;
				maxGroup = Math.max(maxGroup, group);
				createLedArray(hmin, hmax, vmin, vmax, group + groupStart);
			}			
			else
				createLedArray(hmin, hmax, vmin, vmax, 0);
		}
		
		return maxGroup + groupStart;
	}

	function createBottomLeds(groupStart)
	{
		var steph = (params.ptbrh - params.ptblh - (2 * edgeHGap)) / params.ledsbottom;
		var stepv = (params.ptbrv - params.ptblv) / params.ledsbottom;
		var maxGroup = 0;
		
		for (var i = params.ledsbottom - 1; i > -1; i--)
		{
			var hmin = ovl("-", params.ptblh + (steph * Number([i])) + edgeHGap);
			var hmax = ovl("+", params.ptblh + (steph * Number([i + 1])) + edgeHGap);
			var vmax = params.ptblv + (stepv * Number([i]));
			var vmin = vmax - params.ledsHDepth;
			
			if (params.groupX > 0 && (params.groupX * 2 <= params.ledsbottom))
			{
				var group = Math.floor(((i * params.groupX) / params.ledsbottom))+1;
				maxGroup = Math.max(maxGroup, group);
				createLedArray(hmin, hmax, vmin, vmax, group + groupStart);
			}
			else
				createLedArray(hmin, hmax, vmin, vmax, 0);
		}
		
		return maxGroup + groupStart;
	}

	function createLeftLeds(groupStart)
	{
		var steph = (params.ptblh - params.pttlh) / params.ledsleft;
		var stepv = (params.ptblv - params.pttlv - (2 * params.edgeVGap)) / params.ledsleft;
		var maxGroup = 0;

		for (var i = params.ledsleft - 1; i > -1; i--)
		{
			var hmin = params.pttlh + (steph * Number([i]));
			var hmax = hmin + params.ledsVDepth;
			var vmin = ovl("-", params.pttlv + (stepv * Number([i])) + params.edgeVGap);
			var vmax = ovl("+", params.pttlv + (stepv * Number([i + 1])) + params.edgeVGap);

			if (params.groupY > 0 && (params.groupY * 2 <= params.ledsleft))
			{
				var group = Math.floor(((i * params.groupY) / params.ledsleft))+1;
				maxGroup = Math.max(maxGroup, group);
				createLedArray(hmin, hmax, vmin, vmax, group + groupStart);
			}
			else
				createLedArray(hmin, hmax, vmin, vmax, 0);
		}
		
		return maxGroup + groupStart;
	}

	//rectangle
	var groupIndex = 0;
	groupIndex = createTopLeds(groupIndex);
	groupIndex = createRightLeds(groupIndex);
	groupIndex = createBottomLeds(groupIndex);
	groupIndex = createLeftLeds(groupIndex);

	//check led gap pos
	if (params.ledsgpos + params.ledsglength > ledArray.length)
	{
		var mpos = Math.max(0, ledArray.length - params.ledsglength);
		//$('#ip_cl_ledsgpos').val(mpos);
		ledsgpos = mpos;
	}

	//check led gap length
	if (params.ledsglength >= ledArray.length)
	{
		//$('#ip_cl_ledsglength').val(ledArray.length-1);
		params.ledsglength = ledArray.length - params.ledsglength - 1;
	}

	if (params.ledsglength != 0)
	{
		ledArray.splice(params.ledsgpos, params.ledsglength);
	}

	if (params.position != 0)
	{
		rotateArray(ledArray, params.position);
	}

	if (params.reverse)
		ledArray.reverse();

	return createFinalArray(ledArray);
}

function createClassicLeds()
{	
	//get values
	let params = {
		ledstop: parseInt($("#ip_cl_top").val()),
		ledsbottom: parseInt($("#ip_cl_bottom").val()),
		ledsleft: parseInt($("#ip_cl_left").val()),
		ledsright: parseInt($("#ip_cl_right").val()),
		ledsglength: parseInt($("#ip_cl_glength").val()),
		ledsgpos: parseInt($("#ip_cl_gpos").val()),
		position: parseInt($("#ip_cl_position").val()),
		groupX: parseInt($("#ip_cl_groupX").val()),
		groupY: parseInt($("#ip_cl_groupY").val()),
		reverse: $("#ip_cl_reverse").is(":checked"),

		//advanced values
		ledsVDepth: parseInt($("#ip_cl_vdepth").val()) / 100,
		ledsHDepth: parseInt($("#ip_cl_hdepth").val()) / 100,
		edgeVGap: parseInt($("#ip_cl_edgegap").val()) / 100 / 2,
		//cornerVGap : parseInt($("#ip_cl_cornergap").val())/100/2,
		overlap: $("#ip_cl_overlap").val() / 100,

		//trapezoid values % -> float
		ptblh: parseInt($("#ip_cl_pblh").val()) / 100,
		ptblv: parseInt($("#ip_cl_pblv").val()) / 100,
		ptbrh: parseInt($("#ip_cl_pbrh").val()) / 100,
		ptbrv: parseInt($("#ip_cl_pbrv").val()) / 100,
		pttlh: parseInt($("#ip_cl_ptlh").val()) / 100,
		pttlv: parseInt($("#ip_cl_ptlv").val()) / 100,
		pttrh: parseInt($("#ip_cl_ptrh").val()) / 100,
		pttrv: parseInt($("#ip_cl_ptrv").val()) / 100,
	}

	finalLedArray = createClassicLedLayout(params);

	//check led gap pos
	if (params.ledsgpos + params.ledsglength > finalLedArray.length)
	{
		var mpos = Math.max(0, finalLedArray.length - params.ledsglength);
		$('#ip_cl_ledsgpos').val(mpos);
	}
	//check led gap length
	if (params.ledsglength >= finalLedArray.length)
	{
		$('#ip_cl_ledsglength').val(finalLedArray.length - 1);
	}

	createLedPreview(finalLedArray, 'classic');
}

function createMatrixLayout(ledshoriz, ledsvert, cabling, start)
{
	// Big thank you to RanzQ (Juha Rantanen) from Github for this script
	// https://raw.githubusercontent.com/RanzQ/hyperion-audio-effects/master/matrix-config.js

	var parallel = false
	var leds = []
	var hblock = 1.0 / ledshoriz
	var vblock = 1.0 / ledsvert

	if (cabling == "parallel")
	{
		parallel = true
	}

	/**
	 * Adds led to the hyperhdr config led array
	 * @param {Number} x     Horizontal position in matrix
	 * @param {Number} y     Vertical position in matrix
	 */
	function addLed(x, y)
	{
		var hscanMin = x * hblock
		var hscanMax = (x + 1) * hblock
		var vscanMin = y * vblock
		var vscanMax = (y + 1) * vblock

		hscanMin = round(hscanMin);
		hscanMax = round(hscanMax);
		vscanMin = round(vscanMin);
		vscanMax = round(vscanMax);

		leds.push(
		{
			hmin: hscanMin,
			hmax: hscanMax,
			vmin: vscanMin,
			vmax: vscanMax,
			group: 0
		})
	}

	var startYX = start.split('-')
	var startX = startYX[1] === 'right' ? ledshoriz - 1 : 0
	var startY = startYX[0] === 'bottom' ? ledsvert - 1 : 0
	var endX = startX === 0 ? ledshoriz - 1 : 0
	var endY = startY === 0 ? ledsvert - 1 : 0
	var forward = startX < endX

	var downward = startY < endY

	var x, y

	for (y = startY; downward && y <= endY || !downward && y >= endY; y += downward ? 1 : -1)
	{
		for (x = startX; forward && x <= endX || !forward && x >= endX; x += forward ? 1 : -1)
		{
			addLed(x, y)
		}
		if (!parallel)
		{
			forward = !forward
			var tmp = startX
			startX = endX
			endX = tmp
		}
	}

	return leds;
}

function createMatrixLeds()
{
	// Big thank you to RanzQ (Juha Rantanen) from Github for this script
	// https://raw.githubusercontent.com/RanzQ/hyperion-audio-effects/master/matrix-config.js

	//get values
	var ledshoriz = parseInt($("#ip_ma_ledshoriz").val());
	var ledsvert = parseInt($("#ip_ma_ledsvert").val());
	var cabling = $("#ip_ma_cabling").val();
	var start = $("#ip_ma_start").val();

	finalLedArray = createMatrixLayout(ledshoriz, ledsvert, cabling, start);
	createLedPreview(finalLedArray, 'matrix');
}

function isEmpty(obj)
{
	for (var key in obj)
	{
		if (obj.hasOwnProperty(key))
			return false;
	}
	return true;
}

$(document).ready(function()
{
	// translate
	performTranslation();

	//add intros
	if (window.showOptHelp)
	{
		createHintH("callout-info", $.i18n('conf_leds_device_intro'), "leddevice_intro");
		createHintH("callout-info", $.i18n('conf_leds_layout_intro'), "layout_intro");
		$('#led_vis_help').html('<div><div class="led_ex" style="background-color:black;margin-right:5px;margin-top:3px"></div><div style="display:inline-block;vertical-align:top">' + $.i18n('conf_leds_layout_preview_l1') + '</div></div><div class="led_ex" style="background-color:grey;margin-top:3px;margin-right:2px"></div><div class="led_ex" style="background-color: rgb(169, 169, 169);margin-right:5px;margin-top:3px;"></div><div style="display:inline-block;vertical-align:top">' + $.i18n('conf_leds_layout_preview_l2') + '</div>');
	}

	var slConfig = window.serverConfig.ledConfig;	

	//restore ledConfig - Classic
	for (var key in slConfig.classic)
	{
		if (typeof(slConfig.classic[key]) === "boolean")
			$('#ip_cl_' + key).prop('checked', slConfig.classic[key]);
		else
			$('#ip_cl_' + key).val(slConfig.classic[key]);
	}

	//restore ledConfig - Matrix
	for (var key in slConfig.matrix)
	{
		if (typeof(slConfig.matrix[key]) === "boolean")
			$('#ip_ma_' + key).prop('checked', slConfig.matrix[key]);
		else
			$('#ip_ma_' + key).val(slConfig.matrix[key]);
	}

	async function deviceListRefresh(ledTypeTarget, discoveryResult, targetDiscoveryEditor, targetDiscoveryFirstLabel, targetDiscoverySecondLabel = 'main_menu_update_token')
	{
		let receiver = $("#deviceListInstances");
		receiver.off();
		receiver.empty();

		$("<option />", {value: "", text: $.i18n(targetDiscoveryFirstLabel)}).appendTo(receiver);

		if (discoveryResult.info != null && discoveryResult.info.devices != null)
		{					
			(discoveryResult.info.devices).forEach(function (val) {
				$("<option />", {value: val.value, text: val.name}).appendTo(receiver);
			});
		}
		
		$("<option />", {value: -1, text: $.i18n(targetDiscoverySecondLabel)}).appendTo(receiver);

		receiver.on('change', function ()
		{
			let selVal = $("#deviceListInstances").val();
			if (selVal == -1)			
				requestLedDeviceDiscovery(ledTypeTarget).then( (newResult) => deviceListRefresh(ledTypeTarget, newResult, targetDiscoveryEditor, targetDiscoveryFirstLabel, targetDiscoverySecondLabel));
			else if (selVal != "")
				conf_editor.getEditor(targetDiscoveryEditor).setValue(selVal);
		});			
	}

	function saveValues()
	{
		var ledConfig = {
			classic:
			{},
			matrix:
			{}
		};

		for (var key in slConfig.classic)
		{
			if (typeof(slConfig.classic[key]) === "boolean")
				ledConfig.classic[key] = $('#ip_cl_' + key).is(':checked');
			else if (Number.isInteger(slConfig.classic[key]))
				ledConfig.classic[key] = parseInt($('#ip_cl_' + key).val());
			else
				ledConfig.classic[key] = $('#ip_cl_' + key).val();
		}

		for (var key in slConfig.matrix)
		{
			if (typeof(slConfig.matrix[key]) === "boolean")
				ledConfig.matrix[key] = $('#ip_ma_' + key).is(':checked');
			else if (Number.isInteger(slConfig.matrix[key]))
				ledConfig.matrix[key] = parseInt($('#ip_ma_' + key).val());
			else
				ledConfig.matrix[key] = $('#ip_ma_' + key).val();
		}
		requestWriteConfig(
		{
			ledConfig
		});
	}

	
	// bind change event to all inputs
	$('.ledCLconstr').bind("change", function()
	{
		valValue(this.id, this.value, this.min, this.max);
		createClassicLeds();
	});

	$('.ledMAconstr').bind("change", function()
	{
		valValue(this.id, this.value, this.min, this.max);
		createMatrixLeds();
	});

	// v4 of json schema with diff required assignment
	var ledschema = {
		"items":
		{
			"additionalProperties": false,
			"required": ["hmin", "hmax", "vmin", "vmax", "group"],
			"properties":
			{
				"name":
				{
					"type": "string"
				},
				"colorOrder":
				{
					"enum": ["rgb", "bgr", "rbg", "brg", "gbr", "grb"],
					"type": "string"
				},
				"hmin":
				{
					"maximum": 1,
					"minimum": 0,
					"type": "number"
				},
				"hmax":
				{
					"maximum": 1,
					"minimum": 0,
					"type": "number"
				},
				"vmin":
				{
					"maximum": 1,
					"minimum": 0,
					"type": "number"
				},
				"vmax":
				{
					"maximum": 1,
					"minimum": 0,
					"type": "number"
				},
				"group": {
					"type": "integer",
					"minimum": 0					
				},
				"disabled": {
					"type": "boolean",
					"default": false
				}
			},
			"type": "object"
		},
		"type": "array"
	};
	//create jsonace editor
	aceEdt = new JSONACEEditor(document.getElementById("aceedit"),
	{
		mode: 'code',
		schema: ledschema,
		onChange: function()
		{
			var success = true;
			try
			{
				aceEdt.get();
			}
			catch (err)
			{
				success = false;
			}

			if (success)
			{
				$('#leds_custom_updsim').attr("disabled", false);
				$('#leds_custom_save').attr("disabled", false);
			}
			else
			{
				$('#leds_custom_updsim').attr("disabled", true);
				$('#leds_custom_save').attr("disabled", true);
			}

		}
	}, window.serverConfig.leds);

	//TODO: HACK! No callback for schema validation - Add it!
	setInterval(function()
	{
		if ($('#aceedit table').hasClass('jsoneditor-text-errors'))
		{
			$('#leds_custom_updsim').attr("disabled", true);
			$('#leds_custom_save').attr("disabled", true);
		}
	}, 1000);

	$('.jsoneditor-menu').toggle();

	// leds to finalLedArray
	finalLedArray = window.serverConfig.leds;

	// create and update editor
	$("#leddevices").off().on("change", function()
	{
		var generalOptions = window.serverSchema.properties.device;

		// Modified schema entry "hardwareLedCount" in generalOptions to minimum LedCount
		var ledType = $(this).val();

		//philipshueentertainment backward fix
		if (ledType == "philipshueentertainment") ledType = "philipshue";

		var specificOptions = window.serverSchema.properties.alldevices[ledType];
		
		$("#editor_container").empty();
		
		conf_editor = createJsonEditor('editor_container',
		{
			generalOptions: generalOptions,
			specificOptions: specificOptions,
		});

		var values_general = {};
		var values_specific = {};
		var isCurrentDevice = (window.serverConfig.device.type == ledType);

		for (var key in window.serverConfig.device)
		{
			if (key != "type" && key in generalOptions.properties) values_general[key] = window.serverConfig.device[key];
		};
		conf_editor.getEditor("root.generalOptions").setValue(values_general);

		if (isCurrentDevice)
		{
			var specificOptions_val = conf_editor.getEditor("root.specificOptions").getValue();
			for (var key in specificOptions_val)
			{
				values_specific[key] = (key in window.serverConfig.device) ? window.serverConfig.device[key] : specificOptions_val[key];
			};
			conf_editor.getEditor("root.specificOptions").setValue(values_specific);
		}
		else
			conf_editor.getEditor('root.generalOptions.refreshTime').setValue(0);

		if (window.serverConfig.smoothing != null && window.serverConfig.smoothing.enable)
		{
			var label = $("div[data-schemapath='root.generalOptions.refreshTime'] label.required");
			if (label != null)
				label.append( `<br/><span class="text-danger"><small>${$.i18n("controlled_by_smoothing_or_led_driver")}</small></span>` );
			conf_editor.getEditor('root.generalOptions.refreshTime').disable();
			var fps = window.serverConfig.smoothing.updateFrequency;
			if (!isNaN(fps))
				conf_editor.getEditor('root.generalOptions.refreshTime').setValue(Math.round(1000.0/fps));
		}

		// change save button state based on validation result
		var firstValid = conf_editor.validate();
		if ((firstValid.length > 1 || (firstValid.length == 1 && firstValid[0].path != "root.generalOptions.type")) )
		{
			$('#btn_submit_controller').attr('disabled', true);
		}
		else
		{
			$('#btn_submit_controller').attr('disabled', false);
		}

		conf_editor.on('change', function()
		{
			$('#btn_submit').attr('disabled', false);
			$('#btn_submit').attr('disabled', false);
			$('#btn_submit').attr('disabled', false);
		});

		// led controller sepecific wizards
		$('#btn_wiz_holder').html("");
		$('#btn_led_device_wiz').off();
		
		
		var whiteChannelList = $("div[data-schemapath='root.specificOptions.white_channel_limit']");
		if (whiteChannelList.length)
		{
			let infoRGBW = `<div class="ms-1 me-1 alert alert-yellow row" role="alert"><div class="col-12">${$.i18n('calibration_channel_info')}</div></div>`
			var insertCalInfo = whiteChannelList.first();
			
			insertCalInfo.prepend(infoRGBW);
		}

		let selectedLedGroup = $(this.options[this.selectedIndex]).closest('optgroup').attr('data-group-id');

		if (ledType == "philipshue")
		{
			$("input[name='root[specificOptions][useEntertainmentAPI]'], input[name='root[specificOptions][useEntertainmentAPIV2]']").bind("change",function()
			{
				let useApiV1 = $("input[name='root[specificOptions][useEntertainmentAPI]']").is(':checked');
				let useApiV2 = $("input[name='root[specificOptions][useEntertainmentAPIV2]']").is(':checked');

				var ledWizardType = (useApiV1 || useApiV2) ? "philipshueentertainment" : ledType;
				var data = {
					type: ledWizardType,
					useApiV2
				};
				var hue_title = (useApiV1 || useApiV2) ? 'wiz_hue_e_title' : 'wiz_hue_title';
				changeWizard(data, hue_title, startWizardPhilipsHue);
								
				createHintH('callout-warning', $.i18n('philips_option_changed_bri'), 'btn_wiz_holder');

			});
			$("input[name='root[specificOptions][useEntertainmentAPI]']").trigger("change");
		}
		else if ([ "cololight", "yeelight", "atmoorb"].includes(ledType))
		{
			const data = {
				type: ledType
			};
			const wizardTitle = `wiz_${ledType}_title`;
			const wizardFn = 'startWizard' + ledType.charAt(0).toUpperCase() + ledType.slice(1);
			changeWizard(data, wizardTitle, window[wizardFn]);
		}
		else if (ledType == "wled" || selectedLedGroup == "leds_group_0_SPI" || selectedLedGroup == "leds_group_4_serial")
		{					
			let selectorControl = $("<select id=\"deviceListInstances\" />");
			let targetControl = 'output';

			selectorControl.addClass("form-select bg-warning").css('width', String(40)+'%');

			if (ledType == "wled")
			{
				requestLedDeviceDiscovery(ledType).then( (result) => deviceListRefresh(ledType, result, 'root.specificOptions.host','select_wled_intro','select_wled_rescan'));
				targetControl = 'host';
			}
			else if (selectedLedGroup == "leds_group_4_serial")
				requestLedDeviceDiscovery(ledType).then( (result) => deviceListRefresh(ledType, result, 'root.specificOptions.output','edt_dev_spec_outputPath_title'));
			else if (selectedLedGroup == "leds_group_0_SPI")
				requestLedDeviceDiscovery(ledType).then( (result) => deviceListRefresh(ledType, result, 'root.specificOptions.output', 'edt_dev_spec_spipath_title'));
				
			$(`input[name='root[specificOptions][${targetControl}]']`)[0].style.width = String(58) + "%";
			$(`input[name='root[specificOptions][${targetControl}]']`)[0].parentElement.appendChild(selectorControl[0]);			
		}

		if (ledType == "ws2812spi" || ledType == "ws281x" || ledType == "sk6812spi") {
			createHintH('callout-warning', $.i18n('edt_rpi_ws281x_driver'), 'btn_wiz_holder');
		}

		function changeWizard(data, hint, fn)
		{
			$('#btn_wiz_holder').html("")
			createHint("wizard", $.i18n(hint), "btn_wiz_holder", "btn_led_device_wiz");
			$('#btn_led_device_wiz').off().on('click', data, fn);
		}
	});

	//philipshueentertainment backward fix
	if (window.serverConfig.device.type == "philipshueentertainment") window.serverConfig.device.type = "philipshue";

	// create led device selection
	let ledDevices = window.serverInfo.ledDevices.available;
	let ledGroups = ledDevices.map(a => a.group).sort().filter(function(value, index, array) {
		return (index === 0) || (value !== array[index-1]);
	});

	ledGroups.forEach((group) => {
		let elemsInGroups = Array();
		
		ledDevices.forEach(a => {
			if (a.group == group)
				elemsInGroups.push(a.name);
		});

		$("#leddevices").append(createSelWithGroupId(elemsInGroups.sort(), $.i18n(group), group));
	});


	$("#leddevices").val(window.serverConfig.device.type);
	

	// validate textfield and update preview
	$("#leds_custom_updsim").off().on("click", function()
	{
		const backup = aceEdt.get();
		createLedPreview(backup, 'text');
		aceEdt.set(backup);
	});

	// save led config and saveValues - passing textfield
	$("#btn_ma_save, #btn_cl_save").off().on("click", function()
	{
		requestWriteConfig(
		{
			"leds": finalLedArray
		});
		saveValues();
	});

	// save led config from textfield
	$("#leds_custom_save").off().on("click", function()
	{
		requestWriteConfig(JSON.parse('{"leds" :' + aceEdt.getText() + '}'));
		saveValues();
	});

	// toggle led numbers
	$('#leds_prev_toggle_num').off().on("click", function()
	{
		$('.led_prev_num').toggle();
		toggleClass('#leds_prev_toggle_num', "btn-danger", "btn-success");
	});

	var _backupLastOrigin;

	$('#leds_prev_zoom').off().on("click", function()
	{
		if ($('#led_zoom_panel').hasClass("col-lg-6"))
		{
			$('#cmd_zoom_in_icon').addClass('d-none');
			$('#cmd_zoom_out_icon').removeClass('d-none');

			_backupLastOrigin = _lastOrigin;
			_lastOrigin = "zoom";
			 $('#ledPropertiesForm > .modal-dialog').addClass("modal-dialog-centered");
			$('#led_zoom_panel').removeClass("col-lg-6");
			$('#led_zoom_panel').addClass("col-lg-12");
			$('#led_zoom_panel').addClass("order-1");
			$('#hyper-subpage').children('h3').addClass("d-none");
			$('#leds_cfg_nav').addClass("d-none");
			$('#layout_intro').addClass("d-none");
			$('#previewledcount').addClass("d-none");
			$('#previewledpower').addClass("d-none");
			$('#led_vis_help').addClass("d-none");
			$('#led_main_panel').addClass("order-2");
			window.scrollTo(0, 0);
		}
		else
		{
			$('#cmd_zoom_in_icon').removeClass('d-none');
			$('#cmd_zoom_out_icon').addClass('d-none');

			_lastOrigin = _backupLastOrigin;
			$('#ledPropertiesForm > .modal-dialog').removeClass("modal-dialog-centered");
			$('#led_zoom_panel').addClass("col-lg-6");
			$('#led_zoom_panel').removeClass("col-lg-12");
			$('#led_zoom_panel').removeClass("order-1");
			$('#hyper-subpage').children('h3').removeClass("d-none");
			$('#leds_cfg_nav').removeClass("d-none");
			$('#layout_intro').removeClass("d-none");
			$('#previewledcount').removeClass("d-none");
			$('#previewledpower').removeClass("d-none");
			$('#led_vis_help').removeClass("d-none");
			$('#led_main_panel').removeClass("order-2");
		}
	});

	// open checklist
	$('#leds_prev_checklist').off().on("click", function()
	{
		var liList = [$.i18n('conf_leds_layout_checkp1'), $.i18n('conf_leds_layout_checkp3'), $.i18n('conf_leds_layout_checkp2'), $.i18n('conf_leds_layout_checkp4')];
		var ul = document.createElement("ul");
		ul.className = "checklist"

		for (var i = 0; i < liList.length; i++)
		{
			var li = document.createElement("li");
			li.innerHTML = liList[i];
			ul.appendChild(li);
		}
		showInfoDialog('checklist', "", ul);
	});

	// nav
	$('#leds_cfg_nav').on('shown.bs.tab', function(e)
	{
		var target = $(e.target).attr("href") // activated tab
		if (target == "#menu_gencfg" && !ledsCustomCfgInitialized)
		{
			$('#leds_custom_updsim').trigger('click');
			ledsCustomCfgInitialized = true;
			
			if (typeof _resizeObserver === "object" && !(_resizeObserver === null))
			{
				_resizeObserver.unobserve(document.getElementById("leds_preview"));
				_resizeObserver.observe(document.getElementById("leds_preview"));
			}
		}
	});

	// context LED editor
	let selectedObject = null;

	function cancelAllContext(){
		selectedObject = null;
		$("#leds_preview").off("mousemove");
		$("#leds_preview").off("click");
		$('#visualCreatorPanel').off("mousedown");
		finalLedArray.forEach(resLed => {
			if ("backup_hmin" in resLed && "backup_vmin" in resLed &&
				"backup_hmax" in resLed && "backup_vmax" in resLed)
			{
				resLed.hmin = resLed.backup_hmin;
				resLed.vmin = resLed.backup_vmin;
				resLed.hmax = resLed.backup_hmax;
				resLed.vmax = resLed.backup_vmax;
				delete resLed.backup_hmin;
				delete resLed.backup_vmin;
				delete resLed.backup_hmax;
				delete resLed.backup_vmax;				
			}
		});
		document.removeEventListener("keyup", cancelAllContext, false);	
		document.getElementById("creator-context-menu").style.visibility = "hidden";
		createLedPreview(finalLedArray, _lastOrigin);
	}	

	let ctrlKey = false;
	$(document).on('keyup keydown', function(e){ctrlKey = e.ctrlKey} );

	$('.st_helper').on('contextmenu', function(e) {
		const meTop = (e.clientY + $(window).scrollTop()) + "px";
		const meLeft = (e.clientX + $(window).scrollLeft()) + "px";		
		let el = document.elementFromPoint(e.clientX, e.clientY);

		if (ctrlKey === true && el != null && el.classList.contains("led"))
		{
			el.classList.add("d-none");
			const newEl = document.elementFromPoint(e.clientX, e.clientY);
			el.classList.remove("d-none");
			if (newEl != null  && newEl.classList.contains("led"))
				el = newEl;
		}

		if (el != null && el.classList.contains("led"))
		{
			selectedObject = el;

			const idTxtS = selectedObject.id.split("_");
			if (idTxtS.length == 2)
			{
				const parsedIndexS = parseInt(idTxtS[1]);
				if (finalLedArray[parsedIndexS].disabled === true)
				{
					$('#cmd_led_enable_icon').removeClass('d-none');
					$('#cmd_led_disable_icon').addClass('d-none');
					$('#cmd_dis_enable_text').html($.i18n('led_editor_context_enable'));
				}
				else
				{
					$('#cmd_led_enable_icon').addClass('d-none');
					$('#cmd_led_disable_icon').removeClass('d-none');
					$('#cmd_dis_enable_text').html($.i18n('led_editor_context_disable'));
				}
			}

			$("#creator-context-menu").css({
				display: "block",
				top: meTop,
				left: meLeft
			}).addClass("showContextMenu");

			$('.st_helper').on("click", cancelAllContext);

			document.getElementById("creator-context-menu").style.visibility = "visible";
		}
		return false;
	});

	$("#creator-context-menu a").on("click", function() {
		const idTxt = selectedObject.id.split("_");

		$('.st_helper').off("click");

		document.getElementById("creator-context-menu").style.visibility = "hidden";
		
		if (idTxt.length == 2)
		{
			const parsedIndex = parseInt(idTxt[1]);

			if (!isNaN(parsedIndex))
			{
				if (this.id == "CMD_ENABLE")
				{
					if ("disabled" in finalLedArray[parsedIndex])
						delete finalLedArray[parsedIndex].disabled;
					else
						finalLedArray[parsedIndex].disabled = true;

					requestWriteConfig({ "leds": finalLedArray });

					createLedPreview(finalLedArray, _lastOrigin);

					selectedObject = null;
				}
				else if (this.id == "CMD_IDENTIFY")
				{
					let params = { blinkIndex: parsedIndex };
					requestLedDeviceIdentification("blink", params);
					selectedObject = null;
				}
				else if (this.id == "CMD_DELETE")
				{
					finalLedArray[parsedIndex] = undefined;

					finalLedArray = finalLedArray.filter(function(item) {
						return item !== undefined;
					});

					requestWriteConfig({ "leds": finalLedArray });

					createLedPreview(finalLedArray, _lastOrigin);

					selectedObject = null;
				}
				else if (this.id == "CMD_PROPERTIES")
				{
					$('#ledPropertiesFormLabel').html($.i18n('edt_conf_color_leds_title') + ": " + parsedIndex);
					$('#ledPropertiesDialogLeft').val(finalLedArray[parsedIndex].hmin);
					$('#ledPropertiesDialogTop').val(finalLedArray[parsedIndex].vmin);
					$('#ledPropertiesDialogRight').val(finalLedArray[parsedIndex].hmax);
					$('#ledPropertiesDialogBottom').val(finalLedArray[parsedIndex].vmax);

					finalLedArray[parsedIndex].backup_hmin = finalLedArray[parsedIndex].hmin;
					finalLedArray[parsedIndex].backup_vmin = finalLedArray[parsedIndex].vmin;
					finalLedArray[parsedIndex].backup_hmax = finalLedArray[parsedIndex].hmax;
					finalLedArray[parsedIndex].backup_vmax = finalLedArray[parsedIndex].vmax;

					$('#ledPropertiesDialogLeft, #ledPropertiesDialogTop, #ledPropertiesDialogRight, #ledPropertiesDialogBottom').off("change").on("change", function() {
						const _hmin = Number($('#ledPropertiesDialogLeft').val());
						const _vmin = Number($('#ledPropertiesDialogTop').val());
						const _hmax = Number($('#ledPropertiesDialogRight').val());
						const _vmax = Number($('#ledPropertiesDialogBottom').val());

						if (!isNaN(_hmin) && !isNaN(_vmin) && !isNaN(_hmax) && !isNaN(_vmax))
							if (_hmin < _hmax && _vmin < _vmax && _hmin >= 0 && _vmin >= 0 && _hmax <= 1 && _vmax <= 1)
							{
								finalLedArray[parsedIndex].hmin = _hmin;
								finalLedArray[parsedIndex].vmin = _vmin;
								finalLedArray[parsedIndex].hmax = _hmax;
								finalLedArray[parsedIndex].vmax = _vmax;

								createLedPreview(finalLedArray, _lastOrigin);
							}
					});

					$("#ready_to_set_single_abort").off("click").on("click", function() {
						cancelAllContext();
						$('#ledPropertiesForm').modal('hide');
					});

					$("#ready_to_set_single_led").off("click").on("click", function() {
						const _hmin = Number($('#ledPropertiesDialogLeft').val());
						const _vmin = Number($('#ledPropertiesDialogTop').val());
						const _hmax = Number($('#ledPropertiesDialogRight').val());
						const _vmax = Number($('#ledPropertiesDialogBottom').val());

						if (!isNaN(_hmin) && !isNaN(_vmin) && !isNaN(_hmax) && !isNaN(_vmax))
							if (_hmin < _hmax && _vmin < _vmax && _hmin >= 0 && _vmin >= 0 && _hmax <= 1 && _vmax <= 1)
							{
								finalLedArray[parsedIndex].hmin = _hmin;
								finalLedArray[parsedIndex].vmin = _vmin;
								finalLedArray[parsedIndex].hmax = _hmax;
								finalLedArray[parsedIndex].vmax = _vmax;

								delete finalLedArray[parsedIndex].backup_hmin;
								delete finalLedArray[parsedIndex].backup_vmin;
								delete finalLedArray[parsedIndex].backup_hmax;
								delete finalLedArray[parsedIndex].backup_vmax;

								requestWriteConfig({ "leds": finalLedArray });

								cancelAllContext();

								$('#ledPropertiesForm').modal('hide');
							}
					});

					$('#ledPropertiesForm').modal('show');
				}
				else if (this.id == "CMD_MOVE")
				{					
					finalLedArray[parsedIndex].backup_hmin = finalLedArray[parsedIndex].hmin;
					finalLedArray[parsedIndex].backup_vmin = finalLedArray[parsedIndex].vmin;
					finalLedArray[parsedIndex].backup_hmax = finalLedArray[parsedIndex].hmax;
					finalLedArray[parsedIndex].backup_vmax = finalLedArray[parsedIndex].vmax;

					$("#leds_preview").off("mousemove").mousemove(function( event ) {
						const bounds = $("#leds_preview").get(0).getBoundingClientRect();
						let x = (event.clientX - bounds.left)/(bounds.right - bounds.left);
						let y = (event.clientY - bounds.top)/(bounds.bottom - bounds.top);
						const w = (finalLedArray[parsedIndex].hmax - finalLedArray[parsedIndex].hmin);
						const h = (finalLedArray[parsedIndex].vmax - finalLedArray[parsedIndex].vmin);
						const deltaZ = 0.004;

						if (x < deltaZ) x = 0;
						if (y < deltaZ) y = 0;
						if (x > (1- deltaZ)) x = 1;
						if (y > (1- deltaZ)) y = 1;
						if (x + w > (1- deltaZ)) x = 1 - w;
						if (y + h > (1- deltaZ)) y = 1 - h;

						finalLedArray[parsedIndex].hmin = x;
						finalLedArray[parsedIndex].vmin = y;
						finalLedArray[parsedIndex].hmax = x + w;
						finalLedArray[parsedIndex].vmax = y + h;

						createLedPreview(finalLedArray, _lastOrigin);
					});

					$('#visualCreatorPanel').off("mousedown").mousedown(function(event) {
						if (event.which != 1)
						{
							cancelAllContext();
							return;
						}

						delete finalLedArray[parsedIndex].backup_hmin;
						delete finalLedArray[parsedIndex].backup_vmin;
						delete finalLedArray[parsedIndex].backup_hmax;
						delete finalLedArray[parsedIndex].backup_vmax;

						requestWriteConfig({ "leds": finalLedArray });

						cancelAllContext();
					});

					document.addEventListener("keyup", cancelAllContext, false);		
				}
			}
		}
	});

	// save led device config
	$("#btn_submit_controller").off().on("click", function(event)
	{
		var ledDevice = $("#leddevices").val();
		var result = {
			device:
			{}
		};

		var general = conf_editor.getEditor("root.generalOptions").getValue();
		var specific = conf_editor.getEditor("root.specificOptions").getValue();
		for (var key in general)
		{
			result.device[key] = general[key];
		}

		for (var key in specific)
		{
			result.device[key] = specific[key];
		}
		result.device.type = ledDevice;
		requestWriteConfig(result)
	});
	
	$(".stepper-down").off().on("click", function(event)
	{	
		var target=this.parentElement.parentElement.firstElementChild;
		if (typeof target !== 'undefined' && target.type === "number")
		{
			target.stepDown();
			$(target).trigger('change');
		};
	});
	
	$(".stepper-up").off().on("click", function(event)
	{		
		var target=this.parentElement.parentElement.firstElementChild;
		if (typeof target !== 'undefined' && target.type === "number")
		{
			target.stepUp();
			$(target).trigger('change');
		};
	});
	
	removeOverlay();
	
	
	if (!isSmallScreen())
	{
		putInstanceName(document.getElementById('instTarget1'));
		document.getElementById('instTarget1').lastElementChild.style.marginLeft = "auto";
		document.getElementById('instTarget1').lastElementChild.classList.add("d-inline-flex", "align-items-center");
	}
	else
		putInstanceName(document.getElementById('instTarget3'));
	
	putInstanceName(document.getElementById('instTarget2'));

	$("#leddevices").trigger("change");
	
});
