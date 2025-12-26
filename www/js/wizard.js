//clear priority and other tasks if people reload the page or lost connection while a wizard was active
$(window.hyperhdr).one("ready", function (event)
{
    if (getStorage("wizardactive") === 'true')
    {
        requestPriorityClear();
        setStorage("wizardactive", false);
    }
});

function resetWizard(reload)
{
    $("#wizard_modal").modal('hide');
    requestPriorityClear();
    setStorage("wizardactive", false);
    $('#wizp1').toggle(true);
    $('#wizp2').toggle(false);
    $('#wizp3').toggle(false);
    if (!reload) location.reload();
}

var wIntveralId = null;
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

	$('#btn_wiz_abort').off().on('click', function () { clearInterval(wIntveralId); resetWizard(true); });

	$('#btn_wiz_checkok').off().on('click', function ()
	{
		showInfoDialog('success', "", $.i18n('infoDialog_wizrgb_text'));
        clearInterval(wIntveralId);
		resetWizard();
	});

	$('#btn_wiz_save').off().on('click', function ()
	{
        clearInterval(wIntveralId);
		resetWizard();
		window.serverConfig.device.colorOrder = new_rgb_order;
		requestWriteConfig({ "device": window.serverConfig.device });
	});
}
