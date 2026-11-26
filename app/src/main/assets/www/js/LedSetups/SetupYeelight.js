//****************************
// Wizard Yeelight
//****************************
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
