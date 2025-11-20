//****************************
// Wizard Cololight
//****************************

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
            + $.i18n('wiz_identify') + '</button'])));

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
