var lights = null;
var selectedLightId = null;

function assignLightPos(id, pos, name)
{
    // Layout positions
    const lightPosTop = { hmin: 0.15, hmax: 0.85, vmin: 0, vmax: 0.2, group: 0 };
    const lightPosTopLeft = { hmin: 0, hmax: 0.15, vmin: 0, vmax: 0.15, group: 0 };
    const lightPosTopRight = { hmin: 0.85, hmax: 1.0, vmin: 0, vmax: 0.15, group: 0 };
    const lightPosBottom = { hmin: 0.15, hmax: 0.85, vmin: 0.8, vmax: 1.0, group: 0 };
    const lightPosBottomLeft = { hmin: 0, hmax: 0.15, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosBottomRight = { hmin: 0.85, hmax: 1.0, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosLeft = { hmin: 0, hmax: 0.15, vmin: 0.15, vmax: 0.85, group: 0 };
    const lightPosLeftTop = { hmin: 0, hmax: 0.15, vmin: 0, vmax: 0.5, group: 0 };
    const lightPosLeftMiddle = { hmin: 0, hmax: 0.15, vmin: 0.25, vmax: 0.75, group: 0 };
    const lightPosLeftBottom = { hmin: 0, hmax: 0.15, vmin: 0.5, vmax: 1.0, group: 0 };
    const lightPosRight = { hmin: 0.85, hmax: 1.0, vmin: 0.15, vmax: 0.85, group: 0 };
    const lightPosRightTop = { hmin: 0.85, hmax: 1.0, vmin: 0, vmax: 0.5, group: 0 };
    const lightPosRightMiddle = { hmin: 0.85, hmax: 1.0, vmin: 0.25, vmax: 0.75, group: 0 };
    const lightPosRightBottom = { hmin: 0.85, hmax: 1.0, vmin: 0.5, vmax: 1.0, group: 0 };
    const lightPosEntire = { hmin: 0.0, hmax: 1.0, vmin: 0.0, vmax: 1.0, group: 0 };

    const lightPosBottomLeft14 = { hmin: 0, hmax: 0.25, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosBottomLeft12 = { hmin: 0.25, hmax: 0.5, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosBottomLeft34 = { hmin: 0.5, hmax: 0.75, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosBottomLeft11 = { hmin: 0.75, hmax: 1, vmin: 0.85, vmax: 1.0, group: 0 };

    const lightPosBottomLeft112 = { hmin: 0, hmax: 0.5, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosBottomLeft121 = { hmin: 0.5, hmax: 1, vmin: 0.85, vmax: 1.0, group: 0 };
    const lightPosBottomLeftNewMid = { hmin: 0.25, hmax: 0.75, vmin: 0.85, vmax: 1.0, group: 0 };

    const lightPosTopLeft112 = { hmin: 0, hmax: 0.5, vmin: 0, vmax: 0.15, group: 0 };
    const lightPosTopLeft121 = { hmin: 0.5, hmax: 1, vmin: 0, vmax: 0.15, group: 0 };
    const lightPosTopLeftNewMid = { hmin: 0.25, hmax: 0.75, vmin: 0, vmax: 0.15, group: 0 };

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

// shared UI builder
function createLedConfig(haConfig)
{
    const lightOptions = [
        "disabled", "top", "topleft", "topright",
        "bottom", "bottomleft", "bottomright",
        "left", "lefttop", "leftmiddle", "leftbottom",
        "right", "righttop", "rightmiddle", "rightbottom",
        "entire",
        "lightPosBottomLeft14", "lightPosBottomLeft12", "lightPosBottomLeft34", "lightPosBottomLeft11",
        "lightPosBottomLeft112", "lightPosBottomLeftNewMid", "lightPosBottomLeft121",
        "lightPosTopLeft112", "lightPosTopLeftNewMid", "lightPosTopLeft121"
    ];

    $('#wizp2_body').html('<div id="wh_topcontainer"></div>');
    $('#wizp2_body').append(`<div id="ha_lamps_table_holder"><p style="font-weight:bold">${$.i18n('wiz_hue_e_desc3')}</p></div>`);
    $('#wizp2_footer').html(`<button type="button" class="btn btn-primary" id="btn_wiz_save"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>${$.i18n('general_btn_save')}</button><button type="button" class="btn btn-danger" id="btn_wiz_abort" disabled><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>${$.i18n('general_btn_cancel')}</button>`);

    if (window.serverConfig.leds != null && window.serverConfig.leds.length == haConfig.lamps.length)
    {
        $("#btn_wiz_abort").prop('disabled', false);
    }

    $('#btn_wiz_abort').off().on('click', function () { resetWizard(); });
    const _haConfig = haConfig;
    $('#btn_wiz_save').off().on('click', function () {
        let ledDef = [];
        for (var key in _haConfig.lamps)
        {
            var defaultPosition = $('#ha_lamp_pos_' + key).val();           
            var idx_content = assignLightPos(key, defaultPosition, _haConfig.lamps[key].name);

            _haConfig.lamps[key].defaultPosition = defaultPosition;
            if (defaultPosition != "disabled")
            {
                ledDef.push(JSON.parse(JSON.stringify(idx_content)));
            }
        }

        _haConfig.lamps = _haConfig.lamps.filter(item => item.defaultPosition != "disabled");

        window.serverConfig.leds = ledDef;
        requestWriteConfig({ "leds": window.serverConfig.leds });
        window.serverConfig.device = _haConfig;
        requestWriteConfig({ "device": window.serverConfig.device });
        resetWizard();
    });

    createTableFlex("ha_lamps_table_header", "ha_lamps_rows", "ha_lamps_table_holder");
    $('.ha_lamps_table_header').append(createTableRowFlex([$.i18n('edt_dev_spec_lightid_title'), $.i18n('wiz_pos'), $.i18n('wiz_identify')], true));
    $('.ha_lamps_rows').html("");

    var ledHaIndex = 0;
    haConfig.lamps.forEach((lamp) => {
        let options = "";
        for (var opt in lightOptions)
        {
            var val = lightOptions[opt];
            var txt = (val != 'entire' && val != 'disabled') ? 'conf_leds_layout_cl_' : 'wiz_ids_';
            options += `<option value="${val}" ${(lamp.defaultPosition === val) ? "selected" : "" } >${$.i18n(txt + val)}</option>`;
        }
        let selectLightControl = `<select id="ha_lamp_pos_${ledHaIndex}" class="hue_sel_watch form-select">${options}</select>`;
        if (haConfig.type == 'home_assistant')
        {       
            let ipVal = encodeURI(haConfig.homeAssistantHost);
            let tokenVal = haConfig.longLivedAccessToken;                   
            let buttonLightLink = `<button class="btn btn-sm btn-primary" onClick=identify_ha_device("${ipVal}","${tokenVal}","${lamp.name}")>${$.i18n('wiz_identify_light', ledHaIndex)}</button>`;
            $('.ha_lamps_rows').append(createTableRowFlex([lamp.name, selectLightControl, buttonLightLink]));
        }
        else if (haConfig.type == 'lifx')
        {           
            let buttonLightLink = `<button class="btn btn-sm btn-primary" onClick="const params = { name: '${lamp.name}', ipAddress: '${lamp.ipAddress}', macAddress: '${lamp.macAddress}' }; requestLedDeviceIdentification('${haConfig.type}', params);">${$.i18n('wiz_identify_light', ledHaIndex)}</button>`;
            $('.ha_lamps_rows').append(createTableRowFlex([lamp.name, selectLightControl, buttonLightLink]));
        }
        else /* ZIGBEE */
        {           
            let buttonLightLink = `<button class="btn btn-sm btn-primary" onClick="const params = { name: '${lamp.name}', type: '${lamp.colorModel}' }; requestLedDeviceIdentification('${haConfig.type}', params);">${$.i18n('wiz_identify_light', ledHaIndex)}</button>`;
            $('.ha_lamps_rows').append(createTableRowFlex([lamp.name, selectLightControl, buttonLightLink]));
        }
        ledHaIndex++;
    });

    $('#wizp1').toggle(false);
    $("#wizard_modal").addClass("modal-lg");
    $('#wizp2').toggle(true);
}