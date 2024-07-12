//clear priority and other tasks if people reload the page or lost connection while a wizard was active
$(window.hyperhdr).one("ready", function (event)
{
	if (getStorage("wizardactive") === 'true')
	{
		requestPriorityClear();
		setStorage("wizardactive", false);
		if (getStorage("kodiAddress") != null)
		{
			kodiAddress = getStorage("kodiAddress");
			sendToKodi("stop");
		}
	}
});

function resetWizard(reload)
{
	$("#wizard_modal").modal('hide');
	clearInterval(wIntveralId);
	requestPriorityClear();
	setStorage("wizardactive", false);
	$('#wizp1').toggle(true);
	$('#wizp2').toggle(false);
	$('#wizp3').toggle(false);
	//cc
	if (withKodi)
		sendToKodi("stop");
	step = 0;
	if (!reload) location.reload();
}

//rgb byte order wizard
var wIntveralId;
var new_rgb_order;

function changeColor()
{
	var color = $("#wiz_canv_color").css('background-color');

	if (color == 'rgb(255, 0, 0)')
	{
		$("#wiz_canv_color").css('background-color', 'rgb(0, 255, 0)');
		requestSetColor('0', '255', '0');
	}
	else
	{
		$("#wiz_canv_color").css('background-color', 'rgb(255, 0, 0)');
		requestSetColor('255', '0', '0');
	}
}

function startWizardRGB()
{
	//create html
	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('wiz_rgb_title'));
	$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n('wiz_rgb_title') + '</h4><p>' + $.i18n('wiz_rgb_intro1') + '</p><p style="font-weight:bold;">' + $.i18n('wiz_rgb_intro2') + '</p>');
	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
	$('#wizp2_body').html('<p style="font-weight:bold">' + $.i18n('wiz_rgb_expl') + '</p>');
	$('#wizp2_body').append('<div class="form-group"><label>' + $.i18n('wiz_rgb_switchevery') + '</label><div class="input-group" style="width:100px"><select id="wiz_switchtime_select" class="form-select"></select><div class="input-group-addon">' + $.i18n('edt_append_s') + '</div></div></div>');
	$('#wizp2_body').append('<canvas id="wiz_canv_color" width="100" height="100" style="border-radius:60px;background-color:red; display:block; margin: 10px 0;border:4px solid grey;"></canvas><label>' + $.i18n('wiz_rgb_q') + '</label>');
	$('#wizp2_body').append('<table class="table borderless" style="width:200px"><tbody><tr><td class="ltd"><label>' + $.i18n('wiz_rgb_qrend') + '</label></td><td class="itd"><select id="wiz_r_select" class="form-select wselect"></select></td></tr><tr><td class="ltd"><label>' + $.i18n('wiz_rgb_qgend') + '</label></td><td class="itd"><select id="wiz_g_select" class="form-select wselect"></select></td></tr></tbody></table>');
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_save"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_save') + '</button><button type="button" class="btn btn-primary" id="btn_wiz_checkok" style="display:none" data-bs-dismiss="modal"><svg data-src="svg/button_success.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_ok') + '</button><button type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');

	//open modal  
	var ta5 = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	ta5.show();

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		beginWizardRGB();
		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});
}

function beginWizardRGB()
{
	$("#wiz_switchtime_select").off().on('change', function ()
	{
		clearInterval(wIntveralId);
		var time = $("#wiz_switchtime_select").val();
		wIntveralId = setInterval(function () { changeColor(); }, time * 1000);
	});

	$('.wselect').change(function ()
	{
		var rgb_order = window.serverConfig.device.colorOrder.split("");
		var redS = $("#wiz_r_select").val();
		var greenS = $("#wiz_g_select").val();
		var blueS = rgb_order.toString().replace(/,/g, "").replace(redS, "").replace(greenS, "");

		for (var i = 0; i < rgb_order.length; i++)
		{
			if (redS == rgb_order[i])
				$('#wiz_g_select option[value=' + rgb_order[i] + ']').attr('disabled', true);
			else
				$('#wiz_g_select option[value=' + rgb_order[i] + ']').attr('disabled', false);
			if (greenS == rgb_order[i])
				$('#wiz_r_select option[value=' + rgb_order[i] + ']').attr('disabled', true);
			else
				$('#wiz_r_select option[value=' + rgb_order[i] + ']').attr('disabled', false);
		}

		if (redS != 'null' && greenS != 'null')
		{
			$('#btn_wiz_save').attr('disabled', false);

			var final_rgb_order = "rgb";
			new_rgb_order = "";
			for (var i = 0; i < final_rgb_order.length; i++)
			{
				if (final_rgb_order[i] == "r")
					new_rgb_order = new_rgb_order + redS;
				else if (final_rgb_order[i] == "g")
					new_rgb_order = new_rgb_order + greenS;
				else
					new_rgb_order = new_rgb_order + blueS;
			}

			$('#btn_wiz_save').toggle(true);
			$('#btn_wiz_save').attr('disabled', false);

			$('#btn_wiz_checkok').toggle(false);
		}
		else
			$('#btn_wiz_save').attr('disabled', true);
	});

	$("#wiz_switchtime_select").append(createSelOpt('5', '5'), createSelOpt('10', '10'), createSelOpt('15', '15'), createSelOpt('30', '30'));
	$("#wiz_switchtime_select").trigger('change');

	$("#wiz_r_select").append(createSelOpt("null", ""), createSelOpt('r', $.i18n('general_col_red')), createSelOpt('g', $.i18n('general_col_green')), createSelOpt('b', $.i18n('general_col_blue')));
	$("#wiz_g_select").html($("#wiz_r_select").html());
	$("#wiz_r_select").trigger('change');

	requestSetColor('255', '0', '0');
	setTimeout(requestSetSource, 100, 'auto');
	setStorage("wizardactive", true);

	$('#btn_wiz_abort').off().on('click', function () { resetWizard(true); });

	$('#btn_wiz_checkok').off().on('click', function ()
	{
		showInfoDialog('success', "", $.i18n('infoDialog_wizrgb_text'));
		resetWizard();
	});

	$('#btn_wiz_save').off().on('click', function ()
	{
		resetWizard();
		window.serverConfig.device.colorOrder = new_rgb_order;
		requestWriteConfig({ "device": window.serverConfig.device });
	});
}

$('#btn_wizard_byteorder').off().on('click', startWizardRGB);

//color calibration wizard

var kodiHost = document.location.hostname;
var kodiPort = 9090;
var kodiAddress = kodiHost;

var wiz_editor;
var colorLength;
var cobj;
var step = 0;
var withKodi = false;
var profile = 0;
var websAddress;
var imgAddress;
var vidAddress = "https://sourceforge.net/projects/hyperion-project/files/resources/vid/";
var picnr = 0;
var availVideos = ["Sweet_Cocoon", "Caminandes_2_GranDillama", "Caminandes_3_Llamigos"];

if (getStorage("kodiAddress") != null)
{

	kodiAddress = getStorage("kodiAddress");
	[kodiHost, kodiPort] = kodiAddress.split(":", 2);

	// Ensure that Kodi's default REST-API port is not used, as now the Web-Socket port is used
	if (kodiPort === "8080")
	{
		kodiAddress = kodiHost;
		kodiPort = undefined;
		setStorage("kodiAddress", kodiAddress);
	}
}

function switchPicture(pictures)
{
	if (typeof pictures[picnr] === 'undefined')
		picnr = 0;

	sendToKodi('playP', pictures[picnr]);
	picnr++;
}

function sendToKodi(type, content, cb)
{
	var command;

	switch (type)
	{
		case "msg":
			command = { "jsonrpc": "2.0", "method": "GUI.ShowNotification", "params": { "title": $.i18n('wiz_cc_title'), "message": content, "image": "info", "displaytime": 5000 }, "id": "1" };
			break;
		case "stop":
			command = { "jsonrpc": "2.0", "method": "Player.Stop", "params": { "playerid": 2 }, "id": "1" };
			break;
		case "playP":
			content = imgAddress + content + '.png';
			command = { "jsonrpc": "2.0", "method": "Player.Open", "params": { "item": { "file": content } }, "id": "1" };
			break;
		case "playV":
			content = vidAddress + content;
			command = { "jsonrpc": "2.0", "method": "Player.Open", "params": { "item": { "file": content } }, "id": "1" };
			break;
		case "rotate":
			command = { "jsonrpc": "2.0", "method": "Player.Rotate", "params": { "playerid": 2 }, "id": "1" };
			break;
		default:
			if (cb != undefined)
			{
				cb("error");
			}
	}

	if ("WebSocket" in window)
	{
		//Add kodi default web-socket port, in case port has been explicitly provided
		if (kodiPort == undefined)
		{
			kodiPort = 9090;
		}

		var ws = new WebSocket("ws://" + kodiHost + ":" + kodiPort + "/jsonrpc/websocket");

		ws.onopen = function ()
		{
			ws.send(JSON.stringify(command));
		};

		ws.onmessage = function (evt)
		{
			var response = JSON.parse(evt.data);

			if (cb != undefined)
			{
				if (response.result != undefined)
				{
					if (response.result === "OK")
					{
						cb("success");
					} else
					{
						cb("error");
					}
				}
			}
		};

		ws.onerror = function (evt)
		{
			if (cb != undefined)
			{
				cb("error");
			}
		};
	}
	else
	{
		console.log("Kodi Access: WebSocket NOT supported by this browser");
		cb("error");
	}
}

function performAction()
{
	var h;

	if (step == 1)
	{
		$('#wiz_cc_desc').html($.i18n('wiz_cc_chooseid'));
		updateWEditor(["id"]);
		$('#btn_wiz_back').attr("disabled", true);
	}
	else
		$('#btn_wiz_back').attr("disabled", false);

	if (step == 2)
	{
		updateWEditor(["white"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_white_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_white_title'));
			sendToKodi('playP', "white");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_white_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 3)
	{
		updateWEditor(["gammaRed", "gammaGreen", "gammaBlue"]);
		h = '<p>' + $.i18n('wiz_cc_adjustgamma') + '</p>';
		if (withKodi)
		{
			sendToKodi('playP', "HGradient");
			h += '<button id="wiz_cc_btn_sp" class="btn btn-primary">' + $.i18n('wiz_cc_btn_switchpic') + '</button>';
		}
		else
			h += '<p>' + $.i18n('wiz_cc_lettvshowm', "grey_1, grey_2, grey_3, HGradient, VGradient") + '</p>';
		$('#wiz_cc_desc').html(h);
		$('#wiz_cc_btn_sp').off().on('click', function ()
		{
			switchPicture(["VGradient", "grey_1", "grey_2", "grey_3", "HGradient"]);
		});
	}
	if (step == 4)
	{
		updateWEditor(["red"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_red_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_red_title'));
			sendToKodi('playP', "red");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_red_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 5)
	{
		updateWEditor(["green"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_green_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_green_title'));
			sendToKodi('playP', "green");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_green_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 6)
	{
		updateWEditor(["blue"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_blue_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_blue_title'));
			sendToKodi('playP', "blue");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_blue_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 7)
	{
		updateWEditor(["cyan"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_cyan_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_cyan_title'));
			sendToKodi('playP', "cyan");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_cyan_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 8)
	{
		updateWEditor(["magenta"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_magenta_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_magenta_title'));
			sendToKodi('playP', "magenta");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_magenta_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 9)
	{
		updateWEditor(["yellow"]);
		h = $.i18n('wiz_cc_adjustit', $.i18n('edt_conf_color_yellow_title'));
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_yellow_title'));
			sendToKodi('playP', "yellow");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_yellow_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 10)
	{
		updateWEditor(["backlightThreshold", "backlightColored"]);
		h = $.i18n('wiz_cc_backlight');
		if (withKodi)
		{
			h += '<br/>' + $.i18n('wiz_cc_kodishould', $.i18n('edt_conf_color_black_title'));
			sendToKodi('playP', "black");
		}
		else
			h += '<br/>' + $.i18n('wiz_cc_lettvshow', $.i18n('edt_conf_color_black_title'));
		$('#wiz_cc_desc').html(h);
	}
	if (step == 11)
	{
		updateWEditor([""], true);
		h = '<p>' + $.i18n('wiz_cc_testintro') + '</p>';
		if (withKodi)
		{
			h += '<p>' + $.i18n('wiz_cc_testintrok') + '</p>';
			sendToKodi('stop');
			for (var i = 0; i < availVideos.length; i++)
			{
				var txt = availVideos[i].replace(/_/g, " ");
				h += '<div><button id="' + availVideos[i] + '" class="btn btn-sm btn-primary videobtn"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg> ' + txt + '</button></div>';
			}
			h += '<div><button id="stop" class="btn btn-sm btn-danger videobtn" style="margin-bottom:15px"><svg data-src="svg/button_stop.svg" fill="currentColor" class="svg4hyperhdr"></svg> ' + $.i18n('wiz_cc_btn_stop') + '</button></div>';
		}
		else
			h += '<p>' + $.i18n('wiz_cc_testintrowok') + ' <a href="https://sourceforge.net/projects/hyperion-project/files/resources/vid/" target="_blank">' + $.i18n('wiz_cc_link') + '</a></p>';
		h += '<p>' + $.i18n('wiz_cc_summary') + '</p>';
		$('#wiz_cc_desc').html(h);

		$('.videobtn').off().on('click', function (e)
		{
			if (e.target.id == "stop")
				sendToKodi("stop");
			else
				sendToKodi("playV", e.target.id + '.mp4');

			$(this).attr("disabled", true);
			setTimeout(function () { $('.videobtn').attr("disabled", false) }, 10000);
		});

		$('#btn_wiz_next').attr("disabled", true);
		$('#btn_wiz_save').toggle(true);
		$('#btn_wiz_save').attr('disabled', false);
	}
	else
	{
		$('#btn_wiz_next').attr("disabled", false);
		$('#btn_wiz_save').toggle(false);
	}
}

function updateWEditor(el, all)
{
	for (var key in cobj)
	{
		if (all === true || el[0] == key || el[1] == key || el[2] == key)
			$('#editor_container_wiz [data-schemapath*=".' + profile + '.' + key + '"]').toggle(true);
		else
			$('#editor_container_wiz [data-schemapath*=".' + profile + '.' + key + '"]').toggle(false);
	}
}

function startWizardCC()
{

	// Ensure that Kodi's default REST-API port is not used, as now the Web-Socket port is used
	[kodiHost, kodiPort] = kodiAddress.split(":", 2);
	if (kodiPort === "8080")
	{
		kodiAddress = kodiHost;
		kodiPort = undefined;
	}
	//create html
	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('wiz_cc_title'));
	$('#wizp1_body').html(`<h4 style="font-weight:bold;text-transform:uppercase;">${$.i18n('wiz_cc_title')}</h4><p>${$.i18n('wiz_cc_intro1')}</p><label>${$.i18n('wiz_cc_kwebs')}</label><input class="form-control" style="width:170px;margin:auto" id="wiz_cc_kodiip" type="text" placeholder="${kodiAddress}" value="${kodiAddress}" /><span id="kodi_status"></span><span id="multi_cali"></span>`);
	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont" disabled="disabled"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
	$('#wizp2_body').html('<div id="wiz_cc_desc" style="font-weight:bold"></div><div id="editor_container_wiz"></div>');
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_back"><svg data-src="svg/button_back.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_back') + '</button><button type="button" class="btn btn-primary" id="btn_wiz_next">' + $.i18n('general_btn_next') + '<svg data-src="svg/button_next.svg" fill="currentColor" class="svg4hyperhdr"></svg></button><button type="button" class="btn btn-warning" id="btn_wiz_save" style="display:none"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_save') + '</button><button type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');

	//open modal
	var ta6 = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	ta6.show();

	$('#wiz_cc_kodiip').off().on('change', function ()
	{

		kodiAddress = encodeURI($(this).val().trim());
		$('#wizp1_body').find("kodiAddress").val(kodiAddress);

		$('#kodi_status').html('');

		// Remove Kodi's default Web-Socket port (9090) from display and ensure Kodi's default REST-API port (8080) is mapped to web-socket port to ease migration
		if (kodiAddress !== "")
		{
			[kodiHost, kodiPort] = kodiAddress.split(":", 2);
			if (kodiPort === "9090" || kodiPort === "8080")
			{
				kodiAddress = kodiHost;
				kodiPort = undefined;
			}
			sendToKodi("msg", $.i18n('wiz_cc_kodimsg_start'), function (cb)
			{
				if (cb == "error")
				{
					$('#kodi_status').html('<p style="color:red;font-weight:bold;margin-top:5px">' + $.i18n('wiz_cc_kodidiscon') + '</p>');
					withKodi = false;
				}
				else
				{
					setStorage("kodiAddress", kodiAddress);

					$('#kodi_status').html('<p style="color:green;font-weight:bold;margin-top:5px">' + $.i18n('wiz_cc_kodicon') + '</p>');
					withKodi = true;
				}

				$('#btn_wiz_cont').attr('disabled', false);
			});
		}
	});

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		beginWizardCC();
		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});

	$('#wiz_cc_kodiip').trigger("change");
	colorLength = window.serverConfig.color.channelAdjustment;
	cobj = window.schema.color.properties.channelAdjustment.items.properties;
	websAddress = document.location.hostname + ':' + window.serverConfig.webConfig.port;
	imgAddress = 'http://' + websAddress + '/img/cc/';
	setStorage("wizardactive", true);

	//check profile count
	if (colorLength.length > 1)
	{
		$('#multi_cali').html('<p style="font-weight:bold;">' + $.i18n('wiz_cc_morethanone') + '</p><select id="wiz_select" class="form-select" style="width:200px;margin:auto"></select>');
		for (var i = 0; i < colorLength.length; i++)
			$('#wiz_select').append(createSelOpt(i, i + 1 + ' (' + colorLength[i].id + ')'));

		$('#wiz_select').off().on('change', function ()
		{
			profile = $(this).val();
		});
	}

	//prepare editor
	wiz_editor = createJsonEditor('editor_container_wiz', {
		color: window.schema.color
	}, true, true);

	$('#editor_container_wiz h4').toggle(false);
	$('#editor_container_wiz .btn-group').toggle(false);
	$('#editor_container_wiz [data-schemapath="root.color.imageToLedMappingType"]').toggle(false);
	$('#editor_container_wiz [data-schemapath="root.color.sparse_processing"]').toggle(false);
	for (var i = 0; i < colorLength.length; i++)
		$('#editor_container_wiz [data-schemapath*="root.color.channelAdjustment.' + i + '."]').toggle(false);
}

function beginWizardCC()
{
	$('#btn_wiz_next').off().on('click', function ()
	{
		step++;
		performAction();
	});

	$('#btn_wiz_back').off().on('click', function ()
	{
		step--;
		performAction();
	});

	$('#btn_wiz_abort').off().on('click', resetWizard);

	$('#btn_wiz_save').off().on('click', function ()
	{
		requestWriteConfig(wiz_editor.getValue());
		resetWizard();
	});

	wiz_editor.on("change", function (e)
	{
		var val = wiz_editor.getEditor('root.color.channelAdjustment.' + profile + '').getValue();
		var temp = JSON.parse(JSON.stringify(val));
		delete temp.leds
		requestAdjustment(JSON.stringify(temp), "", true);
	});

	step++
	performAction();
}

$('#btn_wizard_colorcalibration').off().on('click', startWizardCC);

// Layout positions
var lightPosTop = { hmin: 0.15, hmax: 0.85, vmin: 0, vmax: 0.2, group: 0 };
var lightPosTopLeft = { hmin: 0, hmax: 0.15, vmin: 0, vmax: 0.15, group: 0 };
var lightPosTopRight = { hmin: 0.85, hmax: 1.0, vmin: 0, vmax: 0.15, group: 0 };
var lightPosBottom = { hmin: 0.15, hmax: 0.85, vmin: 0.8, vmax: 1.0, group: 0 };
var lightPosBottomLeft = { hmin: 0, hmax: 0.15, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosBottomRight = { hmin: 0.85, hmax: 1.0, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosLeft = { hmin: 0, hmax: 0.15, vmin: 0.15, vmax: 0.85, group: 0 };
var lightPosLeftTop = { hmin: 0, hmax: 0.15, vmin: 0, vmax: 0.5, group: 0 };
var lightPosLeftMiddle = { hmin: 0, hmax: 0.15, vmin: 0.25, vmax: 0.75, group: 0 };
var lightPosLeftBottom = { hmin: 0, hmax: 0.15, vmin: 0.5, vmax: 1.0, group: 0 };
var lightPosRight = { hmin: 0.85, hmax: 1.0, vmin: 0.15, vmax: 0.85, group: 0 };
var lightPosRightTop = { hmin: 0.85, hmax: 1.0, vmin: 0, vmax: 0.5, group: 0 };
var lightPosRightMiddle = { hmin: 0.85, hmax: 1.0, vmin: 0.25, vmax: 0.75, group: 0 };
var lightPosRightBottom = { hmin: 0.85, hmax: 1.0, vmin: 0.5, vmax: 1.0, group: 0 };
var lightPosEntire = { hmin: 0.0, hmax: 1.0, vmin: 0.0, vmax: 1.0, group: 0 };

var lightPosBottomLeft14 = { hmin: 0, hmax: 0.25, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosBottomLeft12 = { hmin: 0.25, hmax: 0.5, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosBottomLeft34 = { hmin: 0.5, hmax: 0.75, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosBottomLeft11 = { hmin: 0.75, hmax: 1, vmin: 0.85, vmax: 1.0, group: 0 };

var lightPosBottomLeft112 = { hmin: 0, hmax: 0.5, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosBottomLeft121 = { hmin: 0.5, hmax: 1, vmin: 0.85, vmax: 1.0, group: 0 };
var lightPosBottomLeftNewMid = { hmin: 0.25, hmax: 0.75, vmin: 0.85, vmax: 1.0, group: 0 };

var lightPosTopLeft112 = { hmin: 0, hmax: 0.5, vmin: 0, vmax: 0.15, group: 0 };
var lightPosTopLeft121 = { hmin: 0.5, hmax: 1, vmin: 0, vmax: 0.15, group: 0 };
var lightPosTopLeftNewMid = { hmin: 0.25, hmax: 0.75, vmin: 0, vmax: 0.15, group: 0 };

function assignLightPos(id, pos, name)
{
	var i = null;

	if (pos === "top")
		i = lightPosTop;
	else if (pos === "topleft")
		i = lightPosTopLeft;
	else if (pos === "topright")
		i = lightPosTopRight;
	else if (pos === "bottom")
		i = lightPosBottom;
	else if (pos === "bottomleft")
		i = lightPosBottomLeft;
	else if (pos === "bottomright")
		i = lightPosBottomRight;
	else if (pos === "left")
		i = lightPosLeft;
	else if (pos === "lefttop")
		i = lightPosLeftTop;
	else if (pos === "leftmiddle")
		i = lightPosLeftMiddle;
	else if (pos === "leftbottom")
		i = lightPosLeftBottom;
	else if (pos === "right")
		i = lightPosRight;
	else if (pos === "righttop")
		i = lightPosRightTop;
	else if (pos === "rightmiddle")
		i = lightPosRightMiddle;
	else if (pos === "rightbottom")
		i = lightPosRightBottom;
	else if (pos === "lightPosBottomLeft14")
		i = lightPosBottomLeft14;
	else if (pos === "lightPosBottomLeft12")
		i = lightPosBottomLeft12;
	else if (pos === "lightPosBottomLeft34")
		i = lightPosBottomLeft34;
	else if (pos === "lightPosBottomLeft11")
		i = lightPosBottomLeft11;
	else if (pos === "lightPosBottomLeft112")
		i = lightPosBottomLeft112;
	else if (pos === "lightPosBottomLeft121")
		i = lightPosBottomLeft121;
	else if (pos === "lightPosBottomLeftNewMid")
		i = lightPosBottomLeftNewMid;
	else if (pos === "lightPosTopLeft112")
		i = lightPosTopLeft112;
	else if (pos === "lightPosTopLeft121")
		i = lightPosTopLeft121;
	else if (pos === "lightPosTopLeftNewMid")
		i = lightPosTopLeftNewMid;
	else
		i = lightPosEntire;

	i.name = name;
	return i;
}

function getHostInLights(hostname)
{
	return lights.filter(
		function (lights)
		{
			return lights.host === hostname
		}
	);
}

function getIpInLights(ip)
{
	return lights.filter(
		function (lights)
		{
			return lights.ip === ip
		}
	);
}

function getIdInLights(id)
{
	return lights.filter(
		function (lights)
		{
			return lights.id === id
		}
	);
}

//****************************
// Wizard Philips Hue
//****************************

var hueIPs = [];
var hueIPsinc = 0;
var lightIDs = null;
var groupIDs = null;
var lightLocation = [];
var groupLights = [];
// CHannels for v2 api
var channels = {};
var entertainmentResources = null;
var deviceResources = null;
var groupLightsLocations = [];
var hueType = "philipshue";
var useV2Api=false;
var useV2ApiConfig=false;

function startWizardPhilipsHue(e)
{
	if (typeof e.data.type != "undefined") hueType = e.data.type;
	useV2ApiConfig=(e.data.useApiV2||false)&&(hueType == 'philipshueentertainment');

	//create html

	var hue_title = 'wiz_hue_title';
	var hue_intro1 = 'wiz_hue_intro1';
	var hue_intro2 = 'wiz_hue_intro2';
	var hue_desc1 = 'wiz_hue_desc1';
	var hue_create_user = 'wiz_hue_create_user';
	if (hueType == 'philipshueentertainment')
	{
		hue_title = 'wiz_hue_e_title';
		hue_intro1 = 'wiz_hue_e_intro1';
		hue_desc1 = 'wiz_hue_e_desc1';
		hue_create_user = 'wiz_hue_e_create_user';
	}
	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n(hue_title));
	$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n(hue_title) + '</h4><p>' + $.i18n(hue_intro1) + '</p>' + $.i18n(hue_intro2));
	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
	$('#wizp2_body').html('<div id="wh_topcontainer"></div>');
	$('#wh_topcontainer').append('<p style="font-weight:bold">' + $.i18n(hue_desc1) + '</p><div class="form-group"><label>' + $.i18n('wiz_hue_ip') + '</label><div class="input-group" style="width:250px"><input type="text" class="form-control" id="ip"><div class="input-group-append"><span class="input-group-text" id="retry_bridge" style="height: 100%;display: inline-block;cursor:pointer"><svg data-src="svg/button_reload.svg" fill="currentColor" class="svg4hyperhdr"></svg></span></div></div></div><span style="font-weight:bold;color:red" id="wiz_hue_ipstate"></span><span style="font-weight:bold;color:green;" class="" id="wiz_hue_discovered"></span>');
	$('#wh_topcontainer').append();
	$('#wh_topcontainer').append('<div class="form-group" id="usrcont" style="display:none"></div>');
	if (hueType == 'philipshue')
	{
		$('#usrcont').append('<label>' + $.i18n('wiz_hue_username') + '</label><div class="form-group input-group" style="width:250px"><input type="text" class="form-control" id="user"><div class="input-group-append"><span class="input-group-text" id="retry_usr" style="cursor:pointer"><svg data-src="svg/button_reload.svg" fill="currentColor" class="svg4hyperhdr" style="width:1.4em;"></svg></span></div></div>');
	}
	if (hueType == 'philipshueentertainment')
	{
		$('#usrcont').append('<label>' + $.i18n('wiz_hue_username') + '</label><div class="form-group input-group" style="width:250px"><input type="text" class="form-control" id="user"></div><label>' + $.i18n('wiz_hue_clientkey') + '</label><div class="input-group" style="width:250px"><input type="text" class="form-control" id="clientkey"><div class="input-group-append"><span class="input-group-text" id="retry_usr" style="height: 100%;display: inline-block;cursor:pointer"><svg data-src="svg/button_reload.svg" fill="currentColor" class="svg4hyperhdr" style="width:1.4em;"></svg></span></div></div><input type="hidden" id="groupId"><input type="hidden" id="entertainmentConfigurationId">');
	}
	$('#usrcont').append('<span style="font-weight:bold;color:red" id="wiz_hue_usrstate"></span><br><button type="button" class="btn btn-primary" style="display:none" id="wiz_hue_create_user"> <svg data-src="svg/button_add.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n(hue_create_user) + '</button>');
	if (hueType == 'philipshueentertainment')
	{
		$('#wizp2_body').append('<div id="hue_grp_ids_t" style="display:none"><p style="font-weight:bold">' + $.i18n('wiz_hue_e_desc2') + '</p></div>');
		createTableFlex("gidsh", "gidsb", "hue_grp_ids_t");
		$('.gidsh').append(createTableRowFlex([$.i18n('edt_dev_spec_groupId_title'), $.i18n('wiz_hue_e_use_group')], true));
		$('#wizp2_body').append('<div id="hue_ids_t" style="display:none"><p style="font-weight:bold" id="hue_id_headline">' + $.i18n('wiz_hue_e_desc3') + '</p></div>');
	}
	else
	{
		$('#wizp2_body').append('<div id="hue_ids_t" style="display:none"><p style="font-weight:bold" id="hue_id_headline">' + $.i18n('wiz_hue_desc2') + '</p></div>');
	}
	createTableFlex("lidsh", "lidsb", "hue_ids_t");
	$('.lidsh').append(createTableRowFlex([$.i18n('edt_dev_spec_lightid_title'), $.i18n('wiz_pos'), $.i18n('wiz_identify')], true));
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_save" style="display:none"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_save') + '</button><button type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
	$('#wizp3_body').html('<span>' + $.i18n('wiz_hue_press_link') + '</span> <br /><br /><center><span id="connectionTime"></span><br /><svg data-src="svg/spinner_large.svg" fill="currentColor" class="svg4hyperhdr mt-3"></svg></center>');

	//open modal
	var phiWiz = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	phiWiz.show();

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		beginWizardHue();
		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});
}

function checkUserResult(reply, usr)
{
	if (reply)
	{
		$('#user').val(usr);
		if (hueType == 'philipshueentertainment' && $('#clientkey').val() == "")
		{
			$('#usrcont').toggle(true);
			$('#wiz_hue_usrstate').html($.i18n('wiz_hue_e_clientkey_needed'));
			$('#wiz_hue_create_user').toggle(true);
		} else
		{
			$('#wiz_hue_usrstate').html("");
			$('#wiz_hue_create_user').toggle(false);
			if (hueType == 'philipshue')
			{
				get_hue_lights();
			}
			if (hueType == 'philipshueentertainment')
			{
				get_hue_groups();
			}
		}
	}
	else
	{
		$('#wiz_hue_usrstate').html($.i18n('wiz_hue_failure_user'));
		$('#wiz_hue_create_user').toggle(true);
	}
};

function checkBridgeResult(reply, usr)
{
	if (reply)
	{
		//abort checking, first reachable result is used
		$('#wiz_hue_ipstate').html("");
		$('#ip').val(hueIPs[hueIPsinc].internalipaddress)

		//now check hue user on this bridge
		$('#usrcont').toggle(true);
		checkHueBridge(checkUserResult, $('#user').val() ? $('#user').val() : "newdeveloper");
	}
	else
	{
		//increment and check again
		if (hueIPs.length - 1 > hueIPsinc)
		{
			hueIPsinc++;
			checkHueBridge(checkBridgeResult);
		}
		else
		{
			$('#usrcont').toggle(false);
			$('#wiz_hue_ipstate').html($.i18n('wiz_hue_failure_ip'));
		}
	}
};

function checkHueBridge(cb, hueUser)
{
	var usr = (typeof hueUser != "undefined") ? hueUser : 'config';
	if (usr == 'config') $('#wiz_hue_discovered').html("");

	if (hueIPs.length == 0)
		return;

	tunnel_hue_get(hueIPs[hueIPsinc].internalipaddress, '/api/' + usr).then( (json) =>
		{
			if (json != null)
			{
				if (json.config)
				{
					useV2Api=parseInt(json.config.swversion)>1948086000&&useV2ApiConfig
					cb(true, usr);
				}
				else if (json.name && json.bridgeid && json.modelid)
				{
					useV2Api=parseInt(json.swversion)>1948086000&&useV2ApiConfig
					conf_editor.getEditor("root.specificOptions.output").setValue(hueIPs[hueIPsinc].internalipaddress);
					$('#wiz_hue_discovered').html("Bridge: " + json.name + ", Modelid: " + json.modelid + ", API-Version: " + json.apiversion);
					cb(true);
				}
				else
				{
					cb(false);
				}
			}
			else
			{
				cb(false);
			}
		});
}

function useGroupId(id)
{
	if(useV2Api){
		$('#entertainmentConfigurationId').val(id);
		channels={};
		for (const channel of groupIDs[id].channels) {
			groupLights.push(channel.channel_id+"")
			// Fetch the device IDs for this channel (used for identify)
			channels[channel.channel_id+""]=channel;
			for (var member of channel.members) {
				var service = member.service;
				if (service.rtype == "entertainment") {
					for (const entertainmentResource of entertainmentResources) {
						if(service.rid==entertainmentResource.id){
							if(entertainmentResource.owner?.rtype=="device"){
								if(channels[channel.channel_id+""].deviceIds){
									channels[channel.channel_id+""].deviceIds.push(entertainmentResource.owner?.rid)
								}else{
									channels[channel.channel_id+""].deviceIds=[entertainmentResource.owner?.rid]
								}
							}
						}
					}
				}
			}
			groupLightsLocations[channel.channel_id+""] = [channel.position.x,channel.position.y,channel.position.z];
		}
	}else{
		$('#groupId').val(id);
		groupLights = groupIDs[id].lights;
		groupLightsLocations = groupIDs[id].locations;
	}
	get_hue_lights();
}

function identHueId(id, off, oState)
{
	if (off !== true)
	{
		setTimeout(identHueId, 1500, id, true, oState);
		var put_data = '{"on":true,"bri":254,"hue":47000,"sat":254}';
	}
	else
	{
		var put_data = '{"on":' + oState.on + ',"bri":' + oState.bri + ',"hue":' + oState.hue + ',"sat":' + oState.sat + '}';
	}

	tunnel_hue_put($('#ip').val(), '/api/' + $('#user').val() + '/lights/' + id + '/state', put_data);
}

async function discover_hue_bridges()
{
	const res = await requestLedDeviceDiscovery('philipshue');

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info;

		// Process devices returned by discovery
		console.log(r);

		if (r.devices.length == 0)
			$('#wiz_hue_ipstate').html($.i18n('wiz_hue_failure_ip'));
		else
		{
			for (const device of r.devices)
			{
				console.log("Device:", device);

				var ip = device.ip + ":" + device.port;
				console.log("Host:", ip);

				hueIPs.push({ internalipaddress: ip });
			}
			var usr = $('#user').val();
			if (usr != "")
			{
				checkHueBridge(checkUserResult, usr);
			} else
			{
				checkHueBridge(checkBridgeResult);
			}
		}
	}
}

function identify_hue_device(hostAddress, username, id)
{
	if(useV2Api)
	{
		// 	flash the channel
		let params = {
			host: hostAddress,
			clientkey: $('#clientkey').val()||conf_editor.getEditor("root.specificOptions.clientkey")?.getValue(),
			user: username,
			entertainmentConfigurationId: $('#entertainmentConfigurationId').val(),
			channelId: id
		};
		requestLedDeviceIdentification("philipshuev2", params);
	}
	else
	{
		let params = { host: hostAddress, user: username, lightId: id };
		requestLedDeviceIdentification("philipshue", params);
	}
}

async function getProperties_hue_bridge(hostAddress, username, resourceFilter)
{
	let params = { host: hostAddress, user: username, filter: resourceFilter };

	const res = await requestLedDeviceProperties('philipshue', params);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process properties returned
		console.log(r);
	}
}

function SaveHueConfig(saveLamps = true)
{
	var hueLedConfig = [];
	var finalLightIds = [];

	//create hue led config
	for (var key in lightIDs)
	{
		if (hueType == 'philipshueentertainment')
		{
			if (groupLights.indexOf(key) == -1) continue;
		}
		if ($('#hue_' + key).val() != "disabled")
		{
			finalLightIds.push(key);
			var idx_content = assignLightPos(key, $('#hue_' + key).val(), lightIDs[key].name);
			hueLedConfig.push(JSON.parse(JSON.stringify(idx_content)));
		}
	}

	var sc = window.serverConfig;

	if (!saveLamps && Array.isArray(sc.leds) && sc.leds.length ==  hueLedConfig.length)
	{
		hueLedConfig = sc.leds;
	}
	
	sc.leds = hueLedConfig;

	//Adjust gamma, brightness and compensation
	var c = sc.color.channelAdjustment[0];
	c.gammaBlue = 1.0;
	c.gammaRed = 1.0;
	c.gammaGreen = 1.0;
	c.brightness = 100;
	c.brightnessCompensation = 0;

	//device config

	//Start with a clean configuration
	var d = {};

	d.output = $('#ip').val();
	d.username = $('#user').val();
	d.type = 'philipshue';
	d.colorOrder = 'rgb';
	d.lightIds = finalLightIds;
	d.transitiontime = parseInt(eV("transitiontime", 1));
	d.restoreOriginalState = (eV("restoreOriginalState", false) == true);
	d.switchOffOnBlack = (eV("switchOffOnBlack", false) == true);

	d.blackLevel = parseFloat(eV("blackLevel", 0.009));
	d.onBlackTimeToPowerOff = parseInt(eV("onBlackTimeToPowerOff", 600));
	d.onBlackTimeToPowerOn = parseInt(eV("onBlackTimeToPowerOn", 300));
	d.brightnessFactor = parseFloat(eV("brightnessFactor", 1));

	d.clientkey = $('#clientkey').val();
	d.groupId = parseInt($('#groupId').val());
	d.entertainmentConfigurationId = $('#entertainmentConfigurationId').val();
	d.blackLightsTimeout = parseInt(eV("blackLightsTimeout", 5000));
	d.brightnessMin = parseFloat(eV("brightnessMin", 0));
	d.brightnessMax = parseFloat(eV("brightnessMax", 1));
	d.brightnessThreshold = parseFloat(eV("brightnessThreshold", 0.0001));
	d.handshakeTimeoutMin = parseInt(eV("handshakeTimeoutMin", 300));
	d.handshakeTimeoutMax = parseInt(eV("handshakeTimeoutMax", 1000));
	d.verbose = (eV("verbose") == true);

	if (hueType == 'philipshue')
	{
		d.useEntertainmentAPI = false;
		d.hardwareLedCount = finalLightIds.length;
		d.refreshTime = 0;
		d.verbose = false;
		//smoothing off
		sc.smoothing.enable = false;
	}

	if (hueType == 'philipshueentertainment')
	{
		d.useEntertainmentAPI = true;
		// using 'useV2Api' instead of 'useV2ApiConfig' because it includes the bridge check for api v2
		d.useEntertainmentAPIV2 = useV2Api;
		d.hardwareLedCount = groupLights.length;
		d.refreshTime = 20;
		//smoothing on
		sc.smoothing.enable = true;
	}

	window.serverConfig.device = d;
	requestWriteConfig(sc, true);
}

//return editor Value
function eV(vn, defaultVal = "")
{
	var editor = (vn) ? conf_editor.getEditor("root.specificOptions." + vn) : null;
	return (editor == null) ? defaultVal : ((defaultVal != "" && !isNaN(defaultVal) && isNaN(editor.getValue())) ? defaultVal : editor.getValue());
}

function beginWizardHue()
{
	var usr = eV("username");
	if (usr != "")
	{
		$('#user').val(usr);
	}

	if (hueType == 'philipshueentertainment')
	{
		var clkey = eV("clientkey");
		if (clkey != "")
		{
			$('#clientkey').val(clkey);
		}
	}
	//check if ip is empty/reachable/search for bridge
	if (eV("output") == "")
	{
		//getHueIPs();
		discover_hue_bridges();
	}
	else
	{
		var ip = eV("output");
		$('#ip').val(ip);
		hueIPs.unshift({ internalipaddress: ip });
		if (usr != "")
		{
			checkHueBridge(checkUserResult, usr);
		} else
		{
			checkHueBridge(checkBridgeResult);
		}
	}

	$('#retry_bridge').off().on('click', function ()
	{
		if ($('#ip').val() != "")
		{
			hueIPs.unshift({ internalipaddress: $('#ip').val() })
			hueIPsinc = 0;
		}
		else discover_hue_bridges();

		var usr = $('#user').val();
		if (usr != "")
		{
			checkHueBridge(checkUserResult, usr);
		} else
		{
			checkHueBridge(checkBridgeResult);
		}
	});

	$('#retry_usr').off().on('click', function ()
	{
		checkHueBridge(checkUserResult, $('#user').val() ? $('#user').val() : "newdeveloper");
	});

	$('#wiz_hue_create_user').off().on('click', function ()
	{
		if ($('#ip').val() != "") hueIPs.unshift({ internalipaddress: $('#ip').val() });
		createHueUser();
	});

	$('#btn_wiz_save').off().on("click", function ()
	{
		SaveHueConfig();		
		resetWizard();
	});

	$('#btn_wiz_abort').off().on('click', resetWizard);
}

function createHueUser()
{
	var connectionRetries = 30;
	var data = { "devicetype": "hyperhdr#" + Date.now() }
	if (hueType == 'philipshueentertainment')
	{
		data = { "devicetype": "hyperhdr#" + Date.now(), "generateclientkey": true }
	}
	var UserInterval = setInterval(function ()
	{
		tunnel_hue_post($("#ip").val(), '/api', JSON.stringify(data)).then( (r) =>
			{
				if (r != null)	
				{
					if (Object.keys(r).length === 0)
					{
						r=[{error : true}];
					}

					$('#wizp1').toggle(false);
					$('#wizp2').toggle(false);
					$('#wizp3').toggle(true);

					connectionRetries--;
					$("#connectionTime").html(connectionRetries);
					if (connectionRetries == 0)
					{
						abortConnection(UserInterval);
					}
					else
					{
						if (typeof r[0].error != 'undefined')
						{
							console.log(connectionRetries + ": link not pressed");
						}
						if (typeof r[0].success != 'undefined')
						{
							$('#wizp1').toggle(false);
							$('#wizp2').toggle(true);
							$('#wizp3').toggle(false);
							if (r[0].success.username != 'undefined')
							{
								$('#user').val(r[0].success.username);
								conf_editor.getEditor("root.specificOptions.username").setValue(r[0].success.username);
							}
							if (hueType == 'philipshueentertainment')
							{
								if (r[0].success.clientkey != 'undefined')
								{
									$('#clientkey').val(r[0].success.clientkey);
									conf_editor.getEditor("root.specificOptions.clientkey").setValue(r[0].success.clientkey);
								}
							}
							checkHueBridge(checkUserResult, r[0].success.username);
							clearInterval(UserInterval);
						}
					}
				}
				else
				{
						$('#wizp1').toggle(false);
						$('#wizp2').toggle(true);
						$('#wizp3').toggle(false);
						clearInterval(UserInterval);
				}
			}
		)
	}, 1000);
}

function get_hue_groups()
{
	if(useV2Api){
		// api v2 uses entertainment configurations not groups
		tunnel_hue_get($("#ip").val(), '/clip/v2/resource/entertainment_configuration',{'hue-application-key':$("#user").val() }).then( (r) =>
			{
				if (r != null)
				{
					// Also get all entertainment resources and devices. this will be used later to identify a channel
					tunnel_hue_get($("#ip").val(), '/clip/v2/resource/entertainment',{'hue-application-key':$("#user").val() }).then(value => {
						entertainmentResources=value?.data
					})
					tunnel_hue_get($("#ip").val(), '/clip/v2/resource/device',{'hue-application-key':$("#user").val() }).then(value => {
						deviceResources=value?.data
					})
					if (r.data.length>0)
					{
						$('#wh_topcontainer').toggle(false);
						$('#hue_grp_ids_t').toggle(true);

						var gC = 0;
						groupIDs={};
						for (const group of r.data) {
							groupIDs[group.id] = group;
							$('.gidsb').append(createTableRowFlex([group.name + '<br> (' + group.id + ')', '<button class="btn btn-sm btn-primary" onClick=useGroupId("' + group.id + '")>' + $.i18n(useV2Api?'wiz_hue_e_use_entertainmentconfigurationid':'wiz_hue_e_use_groupid', group.id) + '</button>']));
							gC++;
						}
						if (gC == 0)
						{
							noAPISupport('wiz_hue_e_noegrpids');
						}
					}
					else
					{
						noAPISupport('wiz_hue_e_nogrpids');
					}
				}
			}
		)
	}else{
		tunnel_hue_get($("#ip").val(), '/api/' + $("#user").val() + '/groups').then( (r) =>
			{
				if (r != null)
				{
					if (Object.keys(r).length > 0)
					{
						$('#wh_topcontainer').toggle(false);
						$('#hue_grp_ids_t').toggle(true);

						groupIDs = r;

						var gC = 0;
						for (var groupid in r)
						{
							if (r[groupid].type == 'Entertainment')
							{
								$('.gidsb').append(createTableRowFlex([groupid + ' (' + r[groupid].name + ')', '<button class="btn btn-sm btn-primary" onClick=useGroupId(' + groupid + ')>' + $.i18n('wiz_hue_e_use_groupid', groupid) + '</button>']));
								gC++;
							}
						}
						if (gC == 0)
						{
							noAPISupport('wiz_hue_e_noegrpids');
						}
					}
					else
					{
						noAPISupport('wiz_hue_e_nogrpids');
					}
				}
			}
		)
	}
}

function noAPISupport(txt)
{
	showNotification('danger', $.i18n('wiz_hue_e_title'), $.i18n('wiz_hue_e_noapisupport_hint'));
	conf_editor.getEditor("root.specificOptions.useEntertainmentAPI").setValue(false);
	$("input[name='root[specificOptions][useEntertainmentAPI]']").trigger("change");
	$('#btn_wiz_holder').append('<div class="callout callout-danger" style="margin-top:0px">' + $.i18n('wiz_hue_e_noapisupport_hint') + '</div>');
	$('#hue_grp_ids_t').toggle(false);
	var txt = (txt) ? $.i18n(txt) : $.i18n('wiz_hue_e_nogrpids');
	$('<p style="font-weight:bold;color:red;">' + txt + '<br />' + $.i18n('wiz_hue_e_noapisupport') + '</p>').insertBefore('#wizp2_body #hue_ids_t');
	$('#hue_id_headline').html($.i18n('wiz_hue_desc2'));
	hueType = 'philipshue';
	get_hue_lights();
}

function get_light_state(id)
{
	tunnel_hue_get($("#ip").val(), '/api/' + $("#user").val() + '/lights/' + id).then( (r) =>
		{
			if (r != null)
			{
				if (Object.keys(r).length > 0)
				{
					identHueId(id, false, r['state']);
				}
			}
		});
}

function get_hue_lights()
{
	(useV2Api?Promise.resolve(channels):tunnel_hue_get($("#ip").val(), '/api/' + $("#user").val() + '/lights')).then( (r) =>
		{
			if (r != null && Object.keys(r).length > 0)
			{
				if (hueType == 'philipshue')
				{
					$('#wh_topcontainer').toggle(false);
				}
				$('#hue_ids_t, #btn_wiz_save').toggle(true);
				lightIDs = r;
				var lightOptions = [
					"top", "topleft", "topright",
					"bottom", "bottomleft", "bottomright",
					"left", "lefttop", "leftmiddle", "leftbottom",
					"right", "righttop", "rightmiddle", "rightbottom",
					"entire",
					"lightPosBottomLeft14", "lightPosBottomLeft12", "lightPosBottomLeft34", "lightPosBottomLeft11",
					"lightPosBottomLeft112", "lightPosBottomLeftNewMid", "lightPosBottomLeft121",
					"lightPosTopLeft112", "lightPosTopLeftNewMid", "lightPosTopLeft121"
				];

				if (hueType == 'philipshue')
				{
					lightOptions.unshift("disabled");
				}

				$('.lidsb').html("");
				var pos = "";
				for (var lightid in r)
				{
					if (hueType == 'philipshueentertainment')
					{
						if (groupLights.indexOf(lightid) == -1) continue;

						if (groupLightsLocations.hasOwnProperty(lightid))
						{
							lightLocation = groupLightsLocations[lightid];
							var x = lightLocation[0];
							var y = lightLocation[1];
							var z = lightLocation[2];
							var xval = (x < 0) ? "left" : "right";
							if (z != 1 && x >= -0.25 && x <= 0.25) xval = "";
							switch (z)
							{
								case 1: // top / Ceiling height
									pos = "top" + xval;
									break;
								case 0: // middle / TV height
									pos = (xval == "" && y >= 0.75) ? "bottom" : xval + "middle";
									break;
								case -1: // bottom / Ground height
									pos = xval + "bottom";
									break;
							}
						}
					}
					var options = "";
					for (var opt in lightOptions)
					{
						var val = lightOptions[opt];
						var txt = (val != 'entire' && val != 'disabled') ? 'conf_leds_layout_cl_' : 'wiz_ids_';
						options += '<option value="' + val + '"';
						if (pos == val) options += ' selected="selected"';
						options += '>' + $.i18n(txt + val) + '</option>';
					}
					let descLightVal = (useV2Api) ? `Channel ${lightid}` : r[lightid].name;
					let selectLightControl = `<select id="hue_${lightid}" class="hue_sel_watch form-select">${options}</select>`;
					let ipVal = encodeURI($("#ip").val());
					let userVal = encodeURI($("#user").val());					
					let buttonLightLink = `<button class="btn btn-sm btn-primary" onClick=identify_hue_device("${ipVal}","${userVal}",${lightid})>***${$.i18n(useV2Api?'wiz_hue_identify':'wiz_hue_blinkblue', lightid)}</button>`;					
					$('.lidsb').append(createTableRowFlex([`${lightid} (${descLightVal})`, selectLightControl, buttonLightLink]));
				}

				if (hueType != 'philipshueentertainment')
				{
					$('.hue_sel_watch').bind("change", function ()
					{
						var cC = 0;
						for (var key in lightIDs)
						{
							if ($('#hue_' + key).val() != "disabled")
							{
								cC++;
							}
						}

						(cC == 0) ? $('#btn_wiz_save').attr("disabled", true) : $('#btn_wiz_save').attr("disabled", false);

					});
				}
				$('.hue_sel_watch').trigger('change');

				if (useV2Api)
				{
					SaveHueConfig(false);
				}
			}
			else
			{
				var txt = '<p style="font-weight:bold;color:red;">' + $.i18n('wiz_hue_noids') + '</p>';
				$('#wizp2_body').append(txt);
			}
		});
}

function abortConnection(UserInterval)
{
	clearInterval(UserInterval);
	$('#wizp1').toggle(false);
	$('#wizp2').toggle(true);
	$('#wizp3').toggle(false);
	$("#wiz_hue_usrstate").html($.i18n('wiz_hue_failure_connection'));
}

//****************************
// Wizard WLED
//****************************
var lights = null;
function startWizardWLED(e)
{
	//create html

	var wled_title = 'wiz_wled_title';
	var wled_intro1 = 'wiz_wled_intro1';

	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n(wled_title));
	$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n(wled_title) + '</h4><p>' + $.i18n(wled_intro1) + '</p>');
	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');

	/*$('#wizp2_body').html('<div id="wh_topcontainer"></div>');
  
	$('#wh_topcontainer').append('<div class="form-group" id="usrcont" style="display:none"></div>');
  
	$('#wizp2_body').append('<div id="hue_ids_t" style="display:none"><p style="font-weight:bold" id="hue_id_headline">'+$.i18n('wiz_wled_desc2')+'</p></div>');
  
	createTable("lidsh", "lidsb", "hue_ids_t");
	$('.lidsh').append(createTableRow([$.i18n('edt_dev_spec_lights_title'),$.i18n('wiz_pos'),$.i18n('wiz_identify')], true));
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_save" style="display:none"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_save')+'</button><button type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_cancel')+'</button>');
  */
	//open modal  

	var ta1 = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	ta1.show();

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		/* For testing only
	
		  discover_wled();
	
		  var hostAddress = conf_editor.getEditor("root.specificOptions.host").getValue();
		  if(hostAddress != "")
		  {
			getProperties_wled(hostAddress,"info");
			identify_wled(hostAddress)
		  }
	
		 For testing only */
	});
}

async function discover_wled()
{
	const res = await requestLedDeviceDiscovery('wled');

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process devices returned by discovery
		console.log(r);

		if (r.devices.length == 0)
			$('#wiz_hue_ipstate').html($.i18n('wiz_hue_failure_ip'));
		else
		{
			for (const device of r.devices)
			{
				console.log("Device:", device);

				var ip = device.hostname + ":" + device.port;
				console.log("Host:", ip);

				//wledIPs.push({internalipaddress : ip});
			}
		}
	}
}

async function getProperties_wled(hostAddress, resourceFilter)
{
	let params = { host: hostAddress, filter: resourceFilter };

	const res = await requestLedDeviceProperties('wled', params);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process properties returned
		console.log(r);
	}
}

function identify_wled(hostAddress)
{
	let params = { host: hostAddress };
	requestLedDeviceIdentification("wled", params);
}

//****************************
// Wizard Yeelight
//****************************
var lights = null;
function startWizardYeelight(e)
{
	//create html

	var yeelight_title = 'wiz_yeelight_title';
	var yeelight_intro1 = 'wiz_yeelight_intro1';

	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n(yeelight_title));
	$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n(yeelight_title) + '</h4><p>' + $.i18n(yeelight_intro1) + '</p>');

	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_cancel') + '</button>');

	$('#wizp2_body').html('<div id="wh_topcontainer"></div>');

	$('#wh_topcontainer').append('<div class="form-group" id="usrcont" style="display:none"></div>');

	$('#wizp2_body').append('<div id="yee_ids_t" style="display:none"><p style="font-weight:bold" id="yee_id_headline">' + $.i18n('wiz_yeelight_desc2') + '</p></div>');

	createTableFlex("lidsh", "lidsb", "yee_ids_t");
	$('.lidsh').append(createTableRowFlex(([$.i18n('edt_dev_spec_lights_title'), $.i18n('wiz_pos'), $.i18n('wiz_identify')], true)));
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_save" style="display:none"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_save') + '</button><buttowindow.serverConfig.device = d;n type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_cancel') + '</button>');

	//open modal
	var ta2 = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	ta2.show();

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		beginWizardYeelight();
		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});
}

function beginWizardYeelight()
{
	lights = [];
	configuredLights = conf_editor.getEditor("root.specificOptions.lights").getValue();

	discover_yeelight_lights();

	$('#btn_wiz_save').off().on("click", function ()
	{
		var yeelightLedConfig = [];
		var finalLights = [];

		//create yeelight led config
		for (var key in lights)
		{
			if ($('#yee_' + key).val() !== "disabled")
			{
				//delete lights[key].model;

				// Set Name to layout-position, if empty
				if (lights[key].name === "")
				{
					lights[key].name = $.i18n('conf_leds_layout_cl_' + $('#yee_' + key).val());
				}

				finalLights.push(lights[key]);

				var name = lights[key].host;
				if (lights[key].name !== "")
					name += '_' + lights[key].name;

				var idx_content = assignLightPos(key, $('#yee_' + key).val(), name);
				yeelightLedConfig.push(JSON.parse(JSON.stringify(idx_content)));
			}
		}

		//LED layout
		window.serverConfig.leds = yeelightLedConfig;

		//LED device config
		//Start with a clean configuration
		var d = {};

		d.type = 'yeelight';
		d.hardwareLedCount = finalLights.length;
		d.colorOrder = conf_editor.getEditor("root.generalOptions.colorOrder").getValue();
		d.colorModel = parseInt(conf_editor.getEditor("root.specificOptions.colorModel").getValue());

		d.transEffect = parseInt(conf_editor.getEditor("root.specificOptions.transEffect").getValue());
		d.transTime = parseInt(conf_editor.getEditor("root.specificOptions.transTime").getValue());
		d.extraTimeDarkness = parseInt(conf_editor.getEditor("root.specificOptions.extraTimeDarkness").getValue());

		d.brightnessMin = parseInt(conf_editor.getEditor("root.specificOptions.brightnessMin").getValue());
		d.brightnessSwitchOffOnMinimum = JSON.parse(conf_editor.getEditor("root.specificOptions.brightnessSwitchOffOnMinimum").getValue());
		d.brightnessMax = parseInt(conf_editor.getEditor("root.specificOptions.brightnessMax").getValue());
		d.brightnessFactor = parseFloat(conf_editor.getEditor("root.specificOptions.brightnessFactor").getValue());

		d.debugLevel = parseInt(conf_editor.getEditor("root.specificOptions.debugLevel").getValue());

		d.lights = finalLights;

		window.serverConfig.device = d;

		//smoothing off
		if (!(window.serverConfig.smoothing == null))
			window.serverConfig.smoothing.enable = false;

		requestWriteConfig(window.serverConfig, true);
		resetWizard();
	});

	$('#btn_wiz_abort').off().on('click', resetWizard);
}

async function discover_yeelight_lights()
{
	var light = {};
	// Get discovered lights
	const res = await requestLedDeviceDiscovery('yeelight');

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info;

		// Process devices returned by discovery
		for (const device of r.devices)
		{
			//console.log("Device:", device);

			if (device.hostname !== "")
			{
				if (getHostInLights(device.hostname).length === 0)
				{
					var light = {};
					light.host = device.hostname;
					light.port = device.port;
					light.name = device.other.name;
					light.model = device.other.model;
					lights.push(light);
				}
			}
		}

		// Add additional items from configuration
		for (var keyConfig in configuredLights)
		{
			var [host, port] = configuredLights[keyConfig].host.split(":", 2);

			//In case port has been explicitly provided, overwrite port given as part of hostname
			if (configuredLights[keyConfig].port !== 0)
				port = configuredLights[keyConfig].port;

			if (host !== "")
				if (getHostInLights(host).length === 0)
				{
					var light = {};
					light.host = host;
					light.port = port;
					light.name = configuredLights[keyConfig].name;
					light.model = "color4";
					lights.push(light);
				}
		}

		assign_yeelight_lights();
	}
}

function assign_yeelight_lights()
{
	var models = ['color', 'color1', 'YLDP02YL', 'YLDP02YL', 'color2', 'YLDP06YL', 'color4', 'YLDP13YL', 'color6', 'YLDP13AYL', 'colorc', "YLDP004-A", 'stripe', 'YLDD04YL', 'strip1', 'YLDD01YL', 'YLDD02YL', 'strip4', 'YLDD05YL', 'strip6', 'YLDD05YL'];

	// If records are left for configuration
	if (Object.keys(lights).length > 0)
	{
		$('#wh_topcontainer').toggle(false);
		$('#yee_ids_t, #btn_wiz_save').toggle(true);

		var lightOptions = [
			"top", "topleft", "topright",
			"bottom", "bottomleft", "bottomright",
			"left", "lefttop", "leftmiddle", "leftbottom",
			"right", "righttop", "rightmiddle", "rightbottom",
			"entire"
		];

		lightOptions.unshift("disabled");

		$('.lidsb').html("");
		var pos = "";

		for (var lightid in lights)
		{
			var lightHostname = lights[lightid].host;
			var lightPort = lights[lightid].port;
			var lightName = lights[lightid].name;

			if (lightName === "")
				lightName = $.i18n('edt_dev_spec_lights_itemtitle');

			var options = "";
			for (var opt in lightOptions)
			{
				var val = lightOptions[opt];
				var txt = (val !== 'entire' && val !== 'disabled') ? 'conf_leds_layout_cl_' : 'wiz_ids_';
				options += '<option value="' + val + '"';
				if (pos === val) options += ' selected="selected"';
				options += '>' + $.i18n(txt + val) + '</option>';
			}

			var enabled = 'enabled';
			if (!models.includes(lights[lightid].model))
			{
				var enabled = 'disabled';
				options = '<option value=disabled>' + $.i18n('wiz_yeelight_unsupported') + '</option>';
			}

			$('.lidsb').append(createTableRowFlex(([(parseInt(lightid, 10) + 1) + '. ' + lightName + '<br>(' + lightHostname + ')', '<select id="yee_' + lightid + '" ' + enabled + ' class="yee_sel_watch form-select">'
				+ options
				+ '</select>', '<button class="btn btn-sm btn-primary" onClick=identify_yeelight_device("' + lightHostname + '",' + lightPort + ')>'
				+ $.i18n('wiz_identify_light', lightName) + '</button>'])));
		}

		$('.yee_sel_watch').bind("change", function ()
		{
			var cC = 0;
			for (var key in lights)
			{
				if ($('#yee_' + key).val() !== "disabled")
				{
					cC++;
				}
			}

			if (cC === 0)
				$('#btn_wiz_save').attr("disabled", true);
			else
				$('#btn_wiz_save').attr("disabled", false);
		});
		$('.yee_sel_watch').trigger('change');
	}
	else
	{
		var noLightsTxt = '<p style="font-weight:bold;color:red;">' + $.i18n('wiz_noLights', 'Yeelights') + '</p>';
		$('#wizp2_body').append(noLightsTxt);
	}
}

async function getProperties_yeelight(hostname, port)
{
	let params = { hostname: hostname, port: port };

	const res = await requestLedDeviceProperties('yeelight', params);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process properties returned
		console.log(r);
	}
}

function identify_yeelight_device(hostname, port)
{
	let params = { hostname: hostname, port: port };
	requestLedDeviceIdentification("yeelight", params);
}

//****************************
// Wizard AtmoOrb
//****************************
var lights = null;
function startWizardAtmoorb(e)
{
	//create html

	var atmoorb_title = 'wiz_atmoorb_title';
	var atmoorb_intro1 = 'wiz_atmoorb_intro1';

	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n(atmoorb_title));
	$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n(atmoorb_title) + '</h4><p>' + $.i18n(atmoorb_intro1) + '</p>');

	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_cancel') + '</button>');

	$('#wizp2_body').html('<div id="wh_topcontainer"></div>');

	$('#wh_topcontainer').append('<div class="form-group" id="usrcont" style="display:none"></div>');

	$('#wizp2_body').append('<div id="orb_ids_t" style="display:none"><p style="font-weight:bold" id="orb_id_headline">' + $.i18n('wiz_atmoorb_desc2') + '</p></div>');

	createTableFlex("lidsh", "lidsb", "orb_ids_t");
	$('.lidsh').append(createTableRowFlex(([$.i18n('edt_dev_spec_lights_title'), $.i18n('wiz_pos'), $.i18n('wiz_identify')], true)));
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_save" style="display:none"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_save') + '</button><buttowindow.serverConfig.device = d;n type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_cancel') + '</button>');

	//open modal
	var ta3 = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	ta3.show();

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		beginWizardAtmoOrb();
		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});
}

function beginWizardAtmoOrb()
{
	lights = [];
	configuredLights = [];

	var configruedOrbIds = conf_editor.getEditor("root.specificOptions.orbIds").getValue().trim();
	if (configruedOrbIds.length !== 0)
	{
		configuredLights = configruedOrbIds.split(",").map(Number);
	}

	var multiCastGroup = conf_editor.getEditor("root.specificOptions.output").getValue();
	var multiCastPort = parseInt(conf_editor.getEditor("root.specificOptions.port").getValue());

	discover_atmoorb_lights(multiCastGroup, multiCastPort);

	$('#btn_wiz_save').off().on("click", function ()
	{
		var atmoorbLedConfig = [];
		var finalLights = [];

		//create atmoorb led config
		for (var key in lights)
		{
			if ($('#orb_' + key).val() !== "disabled")
			{
				// Set Name to layout-position, if empty
				if (lights[key].name === "")
				{
					lights[key].name = $.i18n('conf_leds_layout_cl_' + $('#orb_' + key).val());
				}

				finalLights.push(lights[key].id);

				var name = lights[key].id;
				if (lights[key].host !== "")
					name += ':' + lights[key].host;

				var idx_content = assignLightPos(key, $('#orb_' + key).val(), name);
				atmoorbLedConfig.push(JSON.parse(JSON.stringify(idx_content)));
			}
		}

		//LED layout
		window.serverConfig.leds = atmoorbLedConfig;

		//LED device config
		//Start with a clean configuration
		var d = {};

		d.type = 'atmoorb';
		d.hardwareLedCount = finalLights.length;
		d.colorOrder = conf_editor.getEditor("root.generalOptions.colorOrder").getValue();

		d.orbIds = finalLights.toString();
		d.useOrbSmoothing = (eV("useOrbSmoothing") == true);

		d.output = conf_editor.getEditor("root.specificOptions.output").getValue();
		d.port = parseInt(conf_editor.getEditor("root.specificOptions.port").getValue());

		window.serverConfig.device = d;

		requestWriteConfig(window.serverConfig, true);
		resetWizard();
	});

	$('#btn_wiz_abort').off().on('click', resetWizard);
}

async function discover_atmoorb_lights(multiCastGroup, multiCastPort)
{
	var light = {};

	var params = {};
	if (multiCastGroup !== "")
	{
		params.multiCastGroup = multiCastGroup;
	}

	if (multiCastPort !== 0)
	{
		params.multiCastPort = multiCastPort;
	}

	// Get discovered lights
	const res = await requestLedDeviceDiscovery('atmoorb', params);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info;

		// Process devices returned by discovery
		for (const device of r.devices)
		{
			if (device.id !== "")
			{
				if (getIdInLights(device.id).length === 0)
				{
					var light = {};
					light.id = device.id;
					light.ip = device.ip;
					light.host = device.hostname;
					lights.push(light);
				}
			}
		}

		// Add additional items from configuration
		for (const keyConfig in configuredLights)
		{
			if (configuredLights[keyConfig] !== "" && !isNaN(configuredLights[keyConfig]))
			{
				if (getIdInLights(configuredLights[keyConfig]).length === 0)
				{
					var light = {};
					light.id = configuredLights[keyConfig];
					light.ip = "";
					light.host = "";
					lights.push(light);
				}
			}
		}

		lights.sort((a, b) => (a.id > b.id) ? 1 : -1);

		assign_atmoorb_lights();
	}
}

function assign_atmoorb_lights()
{
	// If records are left for configuration
	if (Object.keys(lights).length > 0)
	{
		$('#wh_topcontainer').toggle(false);
		$('#orb_ids_t, #btn_wiz_save').toggle(true);

		var lightOptions = [
			"top", "topleft", "topright",
			"bottom", "bottomleft", "bottomright",
			"left", "lefttop", "leftmiddle", "leftbottom",
			"right", "righttop", "rightmiddle", "rightbottom",
			"entire"
		];

		lightOptions.unshift("disabled");

		$('.lidsb').html("");
		var pos = "";

		for (var lightid in lights)
		{
			var orbId = lights[lightid].id;
			var orbIp = lights[lightid].ip;
			var orbHostname = lights[lightid].host;

			if (orbHostname === "")
				orbHostname = $.i18n('edt_dev_spec_lights_itemtitle');

			var options = "";
			for (var opt in lightOptions)
			{
				var val = lightOptions[opt];
				var txt = (val !== 'entire' && val !== 'disabled') ? 'conf_leds_layout_cl_' : 'wiz_ids_';
				options += '<option value="' + val + '"';
				if (pos === val) options += ' selected="selected"';
				options += '>' + $.i18n(txt + val) + '</option>';
			}

			var enabled = 'enabled';
			if (orbId < 1 || orbId > 255)
			{
				enabled = 'disabled';
				options = '<option value=disabled>' + $.i18n('wiz_atmoorb_unsupported') + '</option>';
			}

			var lightAnnotation = "";
			if (orbIp !== "")
			{
				lightAnnotation = ': ' + orbIp + '<br>(' + orbHostname + ')';
			}

			$('.lidsb').append(createTableRowFlex(([orbId + lightAnnotation, '<select id="orb_' + lightid + '" ' + enabled + ' class="orb_sel_watch form-select">'
				+ options
				+ '</select>', '<button class="btn btn-sm btn-primary" ' + enabled + ' onClick=identify_atmoorb_device(' + orbId + ')>'
				+ $.i18n('wiz_identify_light', orbId) + '</button>'])));
		}

		$('.orb_sel_watch').bind("change", function ()
		{
			var cC = 0;
			for (var key in lights)
			{
				if ($('#orb_' + key).val() !== "disabled")
				{
					cC++;
				}
			}
			if (cC === 0)
				$('#btn_wiz_save').attr("disabled", true);
			else
				$('#btn_wiz_save').attr("disabled", false);
		});
		$('.orb_sel_watch').trigger('change');
	}
	else
	{
		var noLightsTxt = '<p style="font-weight:bold;color:red;">' + $.i18n('wiz_noLights', 'AtmoOrbs') + '</p>';
		$('#wizp2_body').append(noLightsTxt);
	}
}

function identify_atmoorb_device(orbId)
{
	let params = { id: orbId };
	requestLedDeviceIdentification("atmoorb", params);
}

//****************************
// Wizard/Routines Nanoleaf
//****************************
async function discover_nanoleaf()
{
	const res = await requestLedDeviceDiscovery('nanoleaf');

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process devices returned by discovery
		console.log(r);

		if (r.devices.length == 0)
			$('#wiz_hue_ipstate').html($.i18n('wiz_hue_failure_ip'));
		else
		{
			for (const device of r.devices)
			{
				console.log("Device:", device);

				var ip = device.hostname + ":" + device.port;
				console.log("Host:", ip);

				//nanoleafIPs.push({internalipaddress : ip});
			}
		}
	}
}

async function getProperties_nanoleaf(hostAddress, authToken, resourceFilter)
{
	let params = { host: hostAddress, token: authToken, filter: resourceFilter };

	const res = await requestLedDeviceProperties('nanoleaf', params);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process properties returned
		console.log(r);
	}
}

function identify_nanoleaf(hostAddress, authToken)
{
	let params = { host: hostAddress, token: authToken };
	requestLedDeviceIdentification("nanoleaf", params);
}

//****************************
// Wizard Cololight
//****************************
var lights = null;
var selectedLightId = null;

function startWizardCololight(e)
{
	//create html

	var cololight_title = 'wiz_cololight_title';
	var cololight_intro1 = 'wiz_cololight_intro1';

	$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n(cololight_title));
	$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n(cololight_title) + '</h4><p>' + $.i18n(cololight_intro1) + '</p>');
	$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');

	$('#wizp2_body').html('<div id="wh_topcontainer"></div>');

	$('#wh_topcontainer').append('<div class="form-group" id="usrcont" style="display:none"></div>');

	$('#wizp2_body').append('<div id="colo_ids_t" style="display:none"><p style="font-weight:bold" id="colo_id_headline">' + $.i18n('wiz_cololight_desc2') + '</p></div>');

	createTableFlex("lidsh", "lidsb", "colo_ids_t");
	$('.lidsh').append(createTableRowFlex(([$.i18n('edt_dev_spec_lights_title'), $.i18n('wiz_identify')], true)));
	$('#wizp2_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_save" style="display:none"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_save') + '</button><buttowindow.serverConfig.device = d;n type="button" class="btn btn-danger" id="btn_wiz_abort"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>'
		+ $.i18n('general_btn_cancel') + '</button>');

	//open modal  
	var ta4 = new bootstrap.Modal($("#wizard_modal"), {
		backdrop: "static",
		keyboard: false
	});

	ta4.show();

	//listen for continue
	$('#btn_wiz_cont').off().on('click', function ()
	{
		beginWizardCololight();
		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});
}

function beginWizardCololight()
{
	lights = [];

	discover_cololights();

	$('#btn_wiz_save').off().on("click", function ()
	{
		//LED device config
		//Start with a clean configuration
		var d = {};

		d.type = 'cololight';

		//Cololight does not resolve into stable hostnames (as devices named the same), therefore use IP
		if (!lights[selectedLightId].ip)
		{
			d.host = lights[selectedLightId].host;
		} else
		{
			d.host = lights[selectedLightId].ip;
		}

		var coloLightProperties = lights[selectedLightId].props;
		if (Object.keys(coloLightProperties).length === 0)
		{
			alert($.i18n('wiz_cololight_noprops'));
			d.hardwareLedCount = 1;
		} else
		{
			if (coloLightProperties.ledCount > 0)
			{
				d.hardwareLedCount = coloLightProperties.ledCount;
			} else if (coloLightProperties.modelType === "Strip")
				d.hardwareLedCount = 120;
		}

		d.colorOrder = conf_editor.getEditor("root.generalOptions.colorOrder").getValue();

		window.serverConfig.device = d;

		//LED layout - have initial layout prepared matching the LED-count

		var coloLightLedConfig = [];

		if (coloLightProperties.modelType === "Strip")
		{
			coloLightLedConfig = createClassicLedLayoutSimple(d.hardwareLedCount / 2, d.hardwareLedCount / 4, d.hardwareLedCount / 4, 0, d.hardwareLedCount / 4 * 3, 0, 0, false);
		} else
		{
			coloLightLedConfig = createClassicLedLayoutSimple(0, 0, 0, d.hardwareLedCount, 0, 0, 0, true);
		}

		window.serverConfig.leds = coloLightLedConfig;

		//smoothing off
		window.serverConfig.smoothing.enable = false;

		requestWriteConfig(window.serverConfig, true);

		resetWizard();
	});

	$('#btn_wiz_abort').off().on('click', resetWizard);
}

async function discover_cololights()
{
	const res = await requestLedDeviceDiscovery('cololight');

	if (res && !res.error)
	{
		const r = res.info;

		// Process devices returned by discovery
		for (const device of r.devices)
		{
			if (device.ip !== "")
			{
				if (getIpInLights(device.ip).length === 0)
				{
					var light = {};
					light.ip = device.ip;
					light.host = device.hostname;
					light.name = device.name;
					light.type = device.type;
					lights.push(light);
				}
			}
		}
		assign_cololight_lights();
	}
}

function assign_cololight_lights()
{
	// If records are left for configuration
	if (Object.keys(lights).length > 0)
	{
		$('#wh_topcontainer').toggle(false);
		$('#colo_ids_t, #btn_wiz_save').toggle(true);

		$('.lidsb').html("");

		var options = "";

		for (var lightid in lights)
		{
			lights[lightid].id = lightid;

			var lightHostname = lights[lightid].host;
			var lightIP = lights[lightid].ip;

			var val = lightHostname + " (" + lightIP + ")";
			options += '<option value="' + lightid + '">' + val + '</option>';
		}

		var enabled = 'enabled';

		$('.lidsb').append(createTableRowFlex((['<select id="colo_select_id" ' + enabled + ' class="colo_sel_watch form-select">'
			+ options
			+ '</select>', '<button id="wiz_identify_btn" class="btn btn-sm btn-primary">'
			+ $.i18n('wiz_identify') + '</button>'])));

		$('.colo_sel_watch').bind("change", function ()
		{
			selectedLightId = $('#colo_select_id').val();
			var lightIP = lights[selectedLightId].ip;

			$('#wiz_identify_btn').unbind().bind('click', function (event) { identify_cololight_device(lightIP); });

			if (!lights[selectedLightId].props)
			{
				getProperties_cololight(lightIP);
			}
		});
		$('.colo_sel_watch').trigger('change');
	}
	else
	{
		var noLightsTxt = '<p style="font-weight:bold;color:red;">' + $.i18n('wiz_noLights', 'Cololights') + '</p>';
		$('#wizp2_body').append(noLightsTxt);
	}
}

async function getProperties_cololight(ip)
{
	let params = { host: ip };

	const res = await requestLedDeviceProperties('cololight', params);

	if (res && !res.error)
	{
		var coloLightProperties = res.info;

		//Store properties along light with given IP-address
		var id = getIpInLights(ip)[0].id;
		lights[id].props = coloLightProperties;
	}
}

function identify_cololight_device(hostAddress)
{
	let params = { host: hostAddress };
	requestLedDeviceIdentification("cololight", params);
}

//****************************
// Wizard/Routines RS232-Devices
//****************************
async function discover_providerRs232(rs232Type)
{
	const res = await requestLedDeviceDiscovery(rs232Type);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process serialPorts returned by discover
		console.log(r);
	}
}

//****************************
// Wizard/Routines HID (USB)-Devices
//****************************
async function discover_providerHid(hidType)
{
	const res = await requestLedDeviceDiscovery(hidType);

	// TODO: error case unhandled
	// res can be: false (timeout) or res.error (not found)
	if (res && !res.error)
	{
		const r = res.info

		// Process HID returned by discover
		console.log(r);
	}
}
