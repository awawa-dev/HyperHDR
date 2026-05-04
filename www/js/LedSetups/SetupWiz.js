//****************************
// Wizard Philips WiZ (LAN)
//****************************

function startWizardWiz(e)
{
    requestLedDeviceDiscovery('wiz').then((result) => {

        let wizConfig = {};

        wizConfig.type = 'wiz';
        wizConfig.port = conf_editor.getEditor("root.specificOptions.port")?.getValue() ?? 38899;
        wizConfig.dimming = conf_editor.getEditor("root.specificOptions.dimming")?.getValue() ?? 100;
        wizConfig.lamps = [];

        let discovered = result;
        if (discovered != null && discovered.success === true && discovered.info != null && discovered.info.devices != null &&
            Array.isArray(discovered.info.devices) && discovered.info.devices.length > 0)
        {
            discovered.info.devices.forEach(dev => {
                let lamp = {};
                lamp.name = dev.name;
                lamp.ipAddress = dev.ipAddress;
                lamp.macAddress = dev.macAddress;
                wizConfig.lamps.push(lamp);
            });

            createLedConfig(wizConfig);

            let wizForm = new bootstrap.Modal($("#wizard_modal"), {
                backdrop: "static",
                keyboard: false
            });
            wizForm.show();
        }
        else
        {
            alert($.i18n('wiz_wiz_error'));
        }

    });
}

