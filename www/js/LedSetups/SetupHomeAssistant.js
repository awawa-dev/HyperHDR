//****************************
// Wizard Home-Assistant
//****************************


function startWizardHome_assistant(e)
{
    $('#wiz_header').html(`<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>${$.i18n('wiz_home_assistant_title')}`);
    $('#wizp1_body').html(`<h4 style="font-weight:bold;text-transform:uppercase;">${$.i18n('wiz_home_assistant_title')}</h4><p>${$.i18n('wiz_ha_intro')}</p><div class="form-group" id="ha_form"></div>`);
    $('#wizp1_footer').html(`<button type="button" class="btn btn-primary" id="btn_wiz_cont" disabled><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>${$.i18n('general_btn_continue')}</button><button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>${$.i18n('general_btn_cancel')}</button>`);

    $('#ha_form').append(`<label for="hostHA" class="form-label">${$.i18n('device_address')}</label><div><input type="text" id="hostHA" name="haInputParams1" class="form-control float-left d-inline" style="width:52%"/><select id="deviceListInstances" class="form-select bg-warning float-left d-inline" style="width:46%" /></div>`);
    $('#ha_form').append(`<label for="tokenHA" class="form-label mt-4">${$.i18n('edt_dev_auth_key_title')}</label><input type="text" id="tokenHA" name="haInputParams2" class="form-control">`);

    requestLedDeviceDiscovery('home_assistant').then( (result) => deviceListRefresh('home_assistant', result, $('#hostHA'),'select_ha_intro','main_menu_update_token'));

    let haForm = new bootstrap.Modal($("#wizard_modal"), {
        backdrop: "static",
        keyboard: false
    });

    $('input[name^=haInputParams]').on('change input',function(e){      
        if ($('#hostHA').val().length > 0 && $('#tokenHA').val().length > 0)
        {
            $("#btn_wiz_cont").prop('disabled', false);
        }
    });

    $('#hostHA').val(conf_editor.getEditor("root.specificOptions.homeAssistantHost").getValue());
    $('#tokenHA').val(conf_editor.getEditor("root.specificOptions.longLivedAccessToken").getValue()).change();

    haForm.show();

    $('#btn_wiz_cont').off().on('click', function ()
    {
        let haConfig = {};
                    
        haConfig.type = 'home_assistant';   
        haConfig.colorOrder = conf_editor.getEditor("root.generalOptions.colorOrder").getValue();
        haConfig.homeAssistantHost = $('#hostHA').val().trim();
        haConfig.longLivedAccessToken= escape($('#tokenHA').val().trim());
        haConfig.transition = conf_editor.getEditor("root.specificOptions.transition").getValue();
        haConfig.constantBrightness = conf_editor.getEditor("root.specificOptions.constantBrightness").getValue();
        haConfig.restoreOriginalState = conf_editor.getEditor("root.specificOptions.restoreOriginalState").getValue();
        haConfig.maxRetry = conf_editor.getEditor("root.specificOptions.maxRetry").getValue();
        haConfig.lamps = [];        

        tunnel_home_assistant_get(haConfig.homeAssistantHost, '/api/states',{'Authorization':`Bearer ${haConfig.longLivedAccessToken}` }).then( (r) =>
            {
                if (r != null && Array.isArray(r))
                {
                    r.forEach((entity) => {
                        if (typeof (entity) === 'object' && entity["entity_id"] != null)
                        {
                            const attributes = entity["attributes"];
                            if ( attributes != null && typeof (attributes) === 'object')
                            {
                                const supported_color_modes = attributes["supported_color_modes"];
                                if ( supported_color_modes != null && Array.isArray(supported_color_modes) && supported_color_modes.length > 0)
                                {
                                    let colorMode = null;
                                    supported_color_modes.forEach((capability) => {
                                        if (capability === "xy" || capability === "rgb")
                                        {
                                            colorMode = "rgb";
                                        }
                                        else if (capability === "hs")
                                        {
                                            colorMode = "hsv";
                                        }                                       
                                    });
                                    if (colorMode != null)
                                    {
                                        var lamp = {};
                                        lamp.name = entity["entity_id"];
                                        lamp.colorModel = colorMode;
                                        haConfig.lamps.push(lamp);
                                    }
                                }
                            }
                        }
                    });

                    if (window.serverConfig.device != null && window.serverConfig.device.type == haConfig.type &&
                       Array.isArray(window.serverConfig.device.lamps) && window.serverConfig.device.lamps.length ==  haConfig.lamps.length)
                    {
                        for (var key in haConfig.lamps)
                        if (haConfig.lamps[key].name =  window.serverConfig.device.lamps[key].name)
                        {
                            haConfig.lamps[key].defaultPosition = window.serverConfig.device.lamps[key].defaultPosition;
                        }
                    }

                    window.serverConfig.device = haConfig;
                    requestWriteConfig({ "device": window.serverConfig.device });
                    createLedConfig(haConfig);
                }
                else
                {
                    showNotification('danger', $.i18n('edt_conf_fbs_timeout_title'), $.i18n('wiz_home_assistant_title'));
                }
            }
        );      
    });
}

function identify_ha_device(host, token, id)
{
    const address = host;
    const bearerToken = token;
    const deviceId = `{"entity_id": "${id}"}`;
    tunnel_home_assistant_post(address, '/api/services/light/turn_off', {'Authorization':`Bearer ${bearerToken}` }, deviceId).then(() =>
        {
            new Promise((resolve) => setTimeout(resolve, 1000)).then((r) =>
                {
                    tunnel_home_assistant_post(address, '/api/services/light/turn_on', {'Authorization':`Bearer ${bearerToken}` }, deviceId);
                });         
        }
    );
}
