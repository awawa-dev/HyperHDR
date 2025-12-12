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

function switchToHttps(ip)
{
    ip = ip.replace(/^https:\/\//, "");
    ip = ip.replace(/^http:\/\//, "");
    ip = ip.replace(/:80$/, "");
    ip = "https://" + ip;
    return ip;
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
                if (hueType == 'philipshueentertainment')
                {
                    ip = switchToHttps(ip);
                }

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
        //  flash the channel
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
        if (sc && sc.smoothing) {
            sc.smoothing.enable = false;
        }
    }

    if (hueType == 'philipshueentertainment')
    {
        d.useEntertainmentAPI = true;
        // using 'useV2Api' instead of 'useV2ApiConfig' because it includes the bridge check for api v2
        d.useEntertainmentAPIV2 = useV2Api;
        d.hardwareLedCount = groupLights.length;
        d.refreshTime = 20;
        //smoothing on
        if (sc && sc.smoothing) {
            sc.smoothing.enable = true;
        }
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
        if (hueType == 'philipshueentertainment')
        {
            ip = switchToHttps(ip);
        }
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
            var ip = $('#ip').val();
            if (hueType == 'philipshueentertainment')
            {
                ip = switchToHttps(ip);
            }
            hueIPs.unshift({ internalipaddress: ip });
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
                            if (r[0].error.type = 101)
                                console.log(`Link button not pressed: ${JSON.stringify(r[0].error)} (${connectionRetries})`);
                            else
                                console.log(`An error occurred: ${JSON.stringify(r[0].error)} (${connectionRetries})`);
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
