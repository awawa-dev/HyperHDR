//****************************
// Wizard lifx
//****************************

function startWizardLifx(e)
{

    requestLedDeviceDiscovery('lifx').then( (result) => {

        let lifxConfig = {};
                    
        lifxConfig.type = 'lifx';  
        lifxConfig.transition = conf_editor.getEditor("root.specificOptions.transition").getValue();
        lifxConfig.lamps = [];

        let discovered = result;
        if (discovered != null && discovered.success === true && discovered.info != null && discovered.info.devices != null &&
            Array.isArray(discovered.info.devices) && discovered.info.devices.length > 0)
        {
            discovered.info.devices.forEach(dev => {
                let lamp = {};
                lamp.name = dev.name;
                lamp.ipAddress = dev.ipAddress;
                lamp.macAddress = dev.macAddress;
                lifxConfig.lamps.push(lamp);
            });

            if (window.serverConfig.device != null && window.serverConfig.device.type == lifxConfig.type &&
                Array.isArray(window.serverConfig.device.lamps) && window.serverConfig.device.lamps.length ==  lifxConfig.lamps.length)
            {
                for (var key in lifxConfig.lamps)
                if (lifxConfig.lamps[key].name =  window.serverConfig.device.lamps[key].name)
                {
                    lifxConfig.lamps[key].defaultPosition = window.serverConfig.device.lamps[key].defaultPosition;
                }
            }

            createLedConfig(lifxConfig);

            let lifxForm = new bootstrap.Modal($("#wizard_modal"), {
                backdrop: "static",
                keyboard: false
            });
            lifxForm.show();
        }
        else
        {
            alert($.i18n('wiz_lifx_error'));
        }
                
    });
}
