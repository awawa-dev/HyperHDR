//****************************
// Wizard zigbee2mqtt
//****************************

function startWizardZigbee2mqtt(e)
{

    requestLedDeviceDiscovery('zigbee2mqtt').then( (result) => {

        let zigbeeConfig = {};

        zigbeeConfig.type = 'zigbee2mqtt';
        zigbeeConfig.colorOrder = conf_editor.getEditor("root.generalOptions.colorOrder").getValue();
        zigbeeConfig.transition = conf_editor.getEditor("root.specificOptions.transition").getValue();
        zigbeeConfig.constantBrightness = conf_editor.getEditor("root.specificOptions.constantBrightness").getValue();
        zigbeeConfig.lamps = [];

        let discovered = result;
        if (discovered != null && discovered.success === true && discovered.info != null && discovered.info.devices != null &&
            Array.isArray(discovered.info.devices) && discovered.info.devices.length > 0)
        {
            discovered.info.devices.forEach(dev => {
                let lamp = {};
                lamp.name = dev.name;
                lamp.colorModel = dev.value;
                zigbeeConfig.lamps.push(lamp);
            });

            if (window.serverConfig.device != null && window.serverConfig.device.type == zigbeeConfig.type &&
                Array.isArray(window.serverConfig.device.lamps) && window.serverConfig.device.lamps.length ==  zigbeeConfig.lamps.length)
            {
                for (var key in zigbeeConfig.lamps)
                if (zigbeeConfig.lamps[key].name =  window.serverConfig.device.lamps[key].name)
                {
                    zigbeeConfig.lamps[key].defaultPosition = window.serverConfig.device.lamps[key].defaultPosition;
                }
            }

            createLedConfig(zigbeeConfig);

            let zigForm = new bootstrap.Modal($("#wizard_modal"), {
                backdrop: "static",
                keyboard: false
            });
            zigForm.show();
        }
        else
        {
            alert($.i18n('wiz_mqtt_error'));
        }

    });
}
