//****************************
// Wizard AtmoOrb
//****************************
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
