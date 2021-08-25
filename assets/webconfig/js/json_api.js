function GetAddress()
{
	return window.location.protocol + "//" + window.location.hostname + (window.location.port ? ':' + window.location.port: '')+'/json-rpc?request=';
}

function CopyToClipboard(par)
{
	var textarea = document.createElement("textarea");
	textarea.textContent = par;
	textarea.style.position = "fixed";
	document.body.appendChild(textarea);
	textarea.select();
	try {
		document.execCommand("copy");
	}
	catch (ex) {
		console.warn("Copy to clipboard failed.", ex);		
	}
	finally {
		document.body.removeChild(textarea);
	}
}

function CopyJson(sender)
{	
	CopyToClipboard(sender.html());
}

function CopyLink(sender)
{	
	var rpc = GetAddress();
	var text = sender.html().replace(/[\r\n]/g,'').replace(/\s+/g, '').replace(/\t/g, '');
	var urlenc=encodeURI(rpc+text);
	
	CopyToClipboard(urlenc);
}

function CopyLinkSafe(sender)
{	
	var rpc = GetAddress();
	var text = sender.html().replace(/[\r\n]/g,'').replace(/\t/g, '');
	var urlenc=encodeURI(rpc+text);
	
	CopyToClipboard(urlenc);
}

function RunIt(sender)
{
	var rpc = GetAddress();
	var text = sender.html().replace(/[\r\n]/g,'').replace(/\s+/g, '').replace(/\t/g, '');
	var urlenc=encodeURI(rpc+text);
		
	window.open(urlenc); 
}
function RunItSafe(sender)
{
	var rpc = GetAddress();
	var text = sender.html().replace(/[\r\n]/g,'').replace(/\t/g, '');
	var urlenc=encodeURI(rpc+text);
		
	window.open(urlenc); 
}

$(document).ready( function(){	
$("textarea").attr("placeholder", $.i18n('json_api_ouput'));
performTranslation();
$("#json_api_components_expl_multi").html($.i18n("json_api_components_expl_multi"));

// components
var componentJson = 
`{
	"command":"componentstate",
	"componentstate":
	{
		"component":"{0}",
		"state": {1}
	}
}`;
 
$('input[name="compState"]').change(
	function(){
		if ($("input[name='compState_onoff']:checked").val())
		{
			BuildComponentJson();
		}
	});
$('input[name="compState_onoff"]').change(
	function(){
		if ($("input[name='compState']:checked").val())
		{
			BuildComponentJson();
		}
	});	
	
function BuildComponentJson()
{
	$('button[name="compStateButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
	
	var component = $('input[name="compState"]:checked').val();
	var state = $('input[name="compState_onoff"]:checked').val();
	var finJson = componentJson.replace("{0}", component).replace("{1}", state);
	$("#compState_json").html(finJson);
}
////////////////////////////////////////////////////////////////////////////////
var switchJson = 
`{
  "command" : "instance",
  "subcommand" : "switchTo",
  "instance" : {0}
}`;	

$('#instanceIndex').on('input',
	function(){
		var num = $("#instanceIndex").val();
		if ((!isNaN(num)) && !isNaN(parseFloat(num)))
		{
			BuildSwitchJson();
		}
		else
		{
			$('button[name="switchButtons"]').each(function(i, obj) {
				$(this).addClass('disabled');
			});
			$("#switch_json").html("");
		}
	});	
	
function BuildSwitchJson()
{
	$('button[name="switchButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
	
	var component = $('#instanceIndex').val();
	var finJson = switchJson.replace("{0}", component);
	$("#switch_json").html(finJson);
}

////////////////////////////////////////////////////////////////////////////////
var serverJson = 
`{
    "command":"serverinfo"
}`

$("#server_json").html(serverJson);

////////////////////////////////////////////////////////////////////////////////
var cropJson = 
`{
  "command" : "video-crop",
  "crop":
  {
	"left": {0},
	"right": {1},
	"top": {2},
	"bottom": {3}
  }
}`;

$('input[name="videoCrop"]').on('input',
	function(){
		var cropLeft = $("#cropLeft").val();
		var cropRight = $("#cropRight").val();
		var cropTop = $("#cropTop").val();
		var cropBottom = $("#cropBottom").val();
		
		if  (
		    ((!isNaN(cropLeft)) && !isNaN(parseFloat(cropLeft))) &&
			((!isNaN(cropRight)) && !isNaN(parseFloat(cropRight))) &&
			((!isNaN(cropTop)) && !isNaN(parseFloat(cropTop))) &&
			((!isNaN(cropBottom)) && !isNaN(parseFloat(cropBottom))) )
		{
			BuildCropJson();
		}
		else
		{
			$('button[name="videoCropButtons"]').each(function(i, obj) {
				$(this).addClass('disabled');
			});
			$("#video_crop_json").html("");
		}
	});	
	
function BuildCropJson()
{
	$('button[name="videoCropButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
	
	var cropLeft = $("#cropLeft").val();
	var cropRight = $("#cropRight").val();
	var cropTop = $("#cropTop").val();
	var cropBottom = $("#cropBottom").val();
	var finJson = cropJson.replace("{0}", cropLeft).replace("{1}", cropRight).replace("{2}", cropTop).replace("{3}", cropBottom);
	$("#video_crop_json").html(finJson);
}

/////////////////////////////////////////////////////////////////////////
var startStopJson = 
`{
  "command" : "instance",
  "subcommand" : "{0}",
  "instance" : {1}
}`;


function checkStartStop(){
	var num = $("#instanceStartStopIndex").val();
	if ((!isNaN(num)) && !isNaN(parseFloat(num)) && $("input[name='compInstanceStartStop']:checked").val())
	{
		BuildStartStoptJson();
	}
	else
	{
		$('button[name="instanceStartButtons"]').each(function(i, obj) {
			$(this).addClass('disabled');
		});
		$("#compInstanceStart_json").html("");
	}
};
 
$('#instanceStartStopIndex').on('input',
	function(){
		checkStartStop();
	});
	
$('input[name="compInstanceStartStop"]').change(
	function(){
		checkStartStop();
	});	
	
function BuildStartStoptJson()
{
	$('button[name="instanceStartButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
	
	var component = $("#instanceStartStopIndex").val();
	var state = $('input[name="compInstanceStartStop"]:checked').val();
	var finJson = startStopJson.replace("{1}", component).replace("{0}", state);
	$("#compInstanceStart_json").html(finJson);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
var adjustmentJson = 
`{
  "command" : "adjustment",
  "adjustment" : 
  {
     {2}"{0}" : {1}
  }
}`;

const editor_color = createJsonEditor('adjustmentEditor', {
		color              : window.schema.color.properties.channelAdjustment.items
	}, true, true);	
	
function requestAdjustmentJson(type, value)
{	
	var line = '"classic_config" : {0},\n     ';
	const editor = editor_color.getEditor('root.color.classic_config');
	var adj = adjustmentJson;
	
	
	if (type == "classic_config")
		line = '';
	else
		line = line.replace('{0}',  editor.value);
		
	adj = adj.replace("{0}", type).replace("{1}", value).replace("{2}", line);
	$("#adjustment_json").html(adj);
	
	$('button[name="adjustmentButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
}

for(var key in window.schema.color.properties.channelAdjustment.items.properties)
{
	const sourceKey = key;
	const sourcePath = 'root.color.'+key;
	const selectEditor = editor_color.getEditor(sourcePath);
	
	if ((selectEditor.path == "root.color.id") || (selectEditor.path == "root.color.leds"))
		selectEditor.container.hidden = true; 
	else
		editor_color.watch(sourcePath,() => {			
			const editor = editor_color.getEditor(sourcePath);
			
			if (editor.format === "colorpicker")
				requestAdjustmentJson(sourceKey, '['+editor.retVal[0]+','+editor.retVal[1]+','+editor.retVal[2]+']');
			else
				requestAdjustmentJson(sourceKey, editor.value);		
		});				
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
var effectJson = 
`{
  "command":"effect",
  "effect":{
    "name":"{0}"
  },
  "duration":{1},
  "priority":{2},
  "origin":"JSON API"
}`;

var colorJson =
`{
  "command":"color",
  "color":{0},
  "duration":{1},
  "priority":{2},
  "origin":"JSON API"
}`;

var rgb = {
	r: 255,
	g: 0,
	b: 0
};

var cpcolor = '#B500FF';

var pickerInit = false;

var duration = 0;

var lastTypeEffect = 0;
var effectPriority = 64;

function sendJsonColor()
{
	lastTypeEffect = 0;
	var colorA = "[" + rgb.r + "," + rgb.g + "," + rgb.b +"]";
	var sc = colorJson.replace("{0}", colorA).replace("{1}",validateDuration(duration)).replace("{2}", effectPriority);
	
	$("#effect_json").html(sc);
	
	$('button[name="effectsButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
}

function sendJsonEffect()
{
	lastTypeEffect = 1;
	var efx = $("#effect_select").val();
	if (efx != "__none__")
	{
		var sc = effectJson.replace("{0}", efx).replace("{1}",validateDuration(duration)).replace("{2}", effectPriority);
		
		$("#effect_json").html(sc);
		
		$('button[name="effectsButtons"]').each(function(i, obj) {
			$(this).removeClass('disabled');
		});
	}
}
	
function updateEffectlist()
{
	var newEffects = window.serverInfo.effects;
	
	$('#effect_select').html('<option value="__none__"></option>');
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
}

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

$("#effect-stepper-down").off().on("click", function(event) { StepUpDown(this, "down"); DurChange(); });	
$("#effect-stepper-up").off().on("click", function(event) { StepUpDown(this, "up"); DurChange(); });

updateEffectlist();

$("#effect_select").off().on("change", function(event)
{
	sendJsonEffect();
});

if (getStorage('rmcpcolor') != null)
{		
	cpcolor = getStorage('rmcpcolor');
	rgb = hexToRgb(cpcolor);
}

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
				sendJsonColor();			
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

if (getStorage('rmduration') != null)
{
	$("#remote_duration").val(getStorage('rmduration'));
	duration = getStorage('rmduration');
}

function DurChange(){
	var thisObj = document.getElementById('remote_duration');
	duration = valValue(thisObj.id, thisObj.value, thisObj.min, thisObj.max);
	if (lastTypeEffect == 0)
		sendJsonColor();
	else
		sendJsonEffect();
}

$('#remote_duration').off().on('input', function() {DurChange();} );

$('#effectPriority').off().on('input', function() {
	effectPriority = this.value;
	if (lastTypeEffect == 0)
		sendJsonColor();
	else
		sendJsonEffect();	
});

////////////////////////////////////////////////////////////////////////////////
var clearJson = 
`{
  "command" : "clear",
  "priority" : {0}
}`;	

$('#clearPriority').on('input',
	function(){
		var num = $("#clearPriority").val();
		if ((!isNaN(num)) && !isNaN(parseFloat(num)))
		{
			BuildClearJson();
		}
		else
		{
			$('button[name="clearPriorityButtons"]').each(function(i, obj) {
				$(this).addClass('disabled');
			});
			$("#clearPriority_json").html("");
		}
	});	
	
function BuildClearJson()
{
	$('button[name="clearPriorityButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
	
	var component = $('#clearPriority').val();
	var finJson = clearJson.replace("{0}", component);
	$("#clearPriority_json").html(finJson);
}
///////////////////////////////////////////////////////////////////////////////////////////////

var componentHdr = 
`{
	"command":"videomodehdr",
	"HDR":{0}
}`;
 
$('input[name="hdrModeState"]').change(
	function(){
		if ($("input[name='hdrModeState']:checked").val())
		{
			BuildHdrJson();
		}
	});

	
function BuildHdrJson()
{
	$('button[name="hdrModeButtons"]').each(function(i, obj) {
		$(this).removeClass('disabled');
	});
		
	var state = $('input[name="hdrModeState"]:checked').val();
	var finJson = componentHdr.replace("{0}", state);
	$("#hdrMode_json").html(finJson);
}
///////////////////////////////////////////////////////////////////////////////////////////////
});