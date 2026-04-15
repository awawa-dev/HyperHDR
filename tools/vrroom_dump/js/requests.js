
const d2h = (d) => ("00" + (+d).toString(16)).toUpperCase().slice(-2); 

function hex_to_ascii(str1) {
    var hex = str1.toString();
    var str = '';
    for (var n = 0; n < hex.length; n += 2) {
        str += String.fromCharCode(parseInt(hex.substr(n, 2), 16));
    }
    return str;
}
function dec2hexString(dec) {
    return  (dec+0x100).toString(16).substr(-2).toUpperCase();
 }
const timeoutSecs = 5000;
const msgTimeout = 'CONNECTION FAILED';

function requestActiveVideo() {
    var xhttp = new XMLHttpRequest();

      xhttp.onreadystatechange = function () {
         if (this.readyState === 4 && this.status === 200) {
  
            if ( (this.response.portseltx0 === '4') ||
                (this.response.portseltx1 === '4') ||
                (this.response.portseltx0 === this.response.portseltx1) )
                    document.getElementById("portseltext").innerHTML = 'PORT SELECTOR:';
                else
                    document.getElementById("portseltext").innerHTML = 'PORT SELECTOR:';

                    
            if ( ( (this.response.portseltx1 === '4') || (this.response.portseltx0 === this.response.portseltx1) ) &&
                ( (this.response.opmode == "0") || (this.response.opmode == "1") ) )
            {
                document.getElementById("infoline__spd1").innerHTML = 'SPLITTER MODE';   
            }
            else
            {
                document.getElementById("infoline__spd1").innerHTML = this.response.SPDASC1;
                document.getElementById("infoline__rx1").innerHTML = this.response.RX1; 
            }

            if ( (this.response.portseltx0 === '4')  )
            {
                document.getElementById("infoline__spd").innerHTML = 'SPLITTER MODE';
            }
            else
            {
                document.getElementById("infoline__spd").innerHTML = this.response.SPDASC;
                document.getElementById("infoline__rx").innerHTML = this.response.RX0; 
            }
 
            document.getElementById("infoline__tx0").innerHTML = this.response.TX0;
            document.getElementById("infoline__tx1").innerHTML = this.response.TX1;
            document.getElementById("infoline__aud").innerHTML = this.response.AUD0;       
            document.getElementById("infoline__aud1").innerHTML = this.response.AUD1; 
            document.getElementById("infoline__audout").innerHTML = this.response.AUDOUT;       
            document.getElementById("infoline__earcrxaudio").innerHTML = this.response.EARCRX;         
            document.getElementById("infoline__sink0").innerHTML = this.response.SINK0;
            document.getElementById("infoline__sink0audio").innerHTML = this.response.EDIDA0;
            document.getElementById("infoline__sink1").innerHTML = this.response.SINK1;
            document.getElementById("infoline__sink1audio").innerHTML = this.response.EDIDA1;
            document.getElementById("infoline__sink2").innerHTML = this.response.SINK2;
            document.getElementById("infoline__sink2audio").innerHTML = this.response.EDIDA2;
            document.getElementById("portseltx0").value = this.response.portseltx0;
            document.getElementById("portseltx1").value = this.response.portseltx1;

            if (this.response.earcst == '1')
                document.getElementById("infoline__audioroute").innerHTML = "from TV eARC";
            else if (this.response.arcst == '1')
                document.getElementById("infoline__audioroute").innerHTML = "from TV ARC";
            else
                document.getElementById("infoline__audioroute").innerHTML = "from HDMI input";

            document.getElementById("opmode").value = this.response.opmode;
 
            if ( (this.response.opmode == "0") || (this.response.opmode == "1") )
                    document.getElementById("portseltx1").disabled = true;
            else
                    document.getElementById("portseltx1").disabled = false;
  
            if ( (this.response.pcbv == "2") || (this.response.pcbv == "3") ) 
            {
                    document.getElementById("pcbverinfo1").style.display = 'none';
                    document.getElementById("pcbverinfo2").style.display = 'none';
                    document.getElementById("pcbverinfo3").style.display = 'none';
            }
            else
            {
                    document.getElementById("pcbverinfo1").style.display = 'inline';
                    document.getElementById("pcbverinfo2").style.display = 'inline';
                    document.getElementById("pcbverinfo3").style.display = 'inline';
            }

            document.getElementById("status").innerHTML = 'INFO STATUS OK';
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/infopage.ssi", true);
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.responseType = "json";
    xhttp.send();
}

function requestEdidData(table) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            const byteArray = new Uint8Array(this.response);     
            let hexstr = [];
            let monitorName=[];
            let fileName=[];

            for (let i = 0; i < 4; ++i) {
                if (byteArray[0x5d + i * 18] === 0xfc) {
                    for (let j = 0; j < 13; j++)
                        hexstr[j] = byteArray[0x5d + i * 18 + 2 + j];
                }
            }       
            let intstr = [];
            for (i = 0; i < 13; i++) {
                 if ((hexstr[i] === 10) || (hexstr[i] === 13))   // CR or LF               
                   break;
                 else 
                    intstr[i] = dec2hexString(hexstr[i]);        
            } 

            const now = new Date();
            const date = now.toLocaleDateString();

            if (intstr[0] != 'AN')
            {
                monitorName = hex_to_ascii(intstr.join(''));
                fileName = `VRROOM-${(monitorName)?monitorName:''}-${table}-${date}.bin`;
            }
            else 
            {
                fileName = `VRROOM-${table}-${date}.bin`;
            }
            
            if (byteArray.length >= 512)
            {
            	download(byteArray.slice(0, 512), fileName, 'application/octet-stream');
            }
            else if (byteArray.length >= 384)
            {
            	download(byteArray.slice(0, 384), fileName, 'application/octet-stream');
            }
            else
            {
            	download(byteArray.slice(0, 256), fileName, 'application/octet-stream');
            }
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };

    switch (table) {
        case 'CDS': xhttp.open("GET", url + "/ssi/edidcds.ssi", true); break;
        case 'AUD': xhttp.open("GET", url + "/ssi/edidaud.ssi", true); break;
        case 'TX0': xhttp.open("GET", url + "/ssi/edidtx0.ssi", true); break;
        case 'TX1': xhttp.open("GET", url + "/ssi/edidtx1.ssi", true); break;
        case 'RX0': xhttp.open("GET", url + "/ssi/edidrx0.ssi", true); break;
        case 'RX1': xhttp.open("GET", url + "/ssi/edidrx1.ssi", true); break;
        case 'RX2': xhttp.open("GET", url + "/ssi/edidrx2.ssi", true); break;
        case 'RX3': xhttp.open("GET", url + "/ssi/edidrx3.ssi", true); break;
    }
    xhttp.responseType = "arraybuffer";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();

}


function requestBoardInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            document.getElementById("unit-info__hostname").innerHTML = "Hostname: http://" + this.response.hostname + `/ [mDNS: http://${this.response.hostname}.local/]`;
            document.getElementById("unit-info__ip").innerHTML = "IP Address:  " + this.response.ipaddress;
            document.getElementById("unit-info__version").innerHTML = this.response.version + `-PCB V${this.response.pcbv}`;
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/brdinfo.ssi", true);
    xhttp.responseType = "json";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();
}


function requestEdidPageInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
  
            if (this.response.edidmode === '0') 
                document.getElementById('edidautomix').checked = true;
            else if (this.response.edidmode === '1') 
                document.getElementById('edidfixed').checked = true;
            else if (this.response.edidmode === '2') 
                document.getElementById('edidcustom').checked = true;
            else if (this.response.edidmode === '3') 
                document.getElementById('edidcopytop').checked = true;
            else if (this.response.edidmode === '4') 
                document.getElementById('edidcopybot').checked = true;
  
            if (this.response.edidaudioflags & 0x40)
                document.getElementById('edid__videotx').value = "videotx0 on";   
            else if (this.response.edidaudioflags & 0x80)
                document.getElementById('edid__videotx').value = "videotx1 on";    
                
            if (this.response.edidaudioflags & 0x01)
                document.getElementById('edid__audiomode').value = "2ch on";   
            else if (this.response.edidaudioflags & 0x02)
                document.getElementById('edid__audiomode').value = "spdif on";      
            else if (this.response.edidaudioflags & 0x04)
                document.getElementById('edid__audiomode').value = "allaudio on";     
            else if (this.response.edidaudioflags & 0x08)
                document.getElementById('edid__audiomode').value = "audiotx0 on";    
            else if (this.response.edidaudioflags & 0x10)
                document.getElementById('edid__audiomode').value = "audiotx1 on";  
            else if (this.response.edidaudioflags & 0x20)
                document.getElementById('edid__audiomode').value = "audioout on";    
            else if (this.response.edidaudioflags & 0x100)
                 document.getElementById('edid__audiomode').value = "earcout on";    
            else if (this.response.edidaudioflags & 0x200)
                 document.getElementById('edid__audiomode').value = "audiocustom on";          
            else if (this.response.edidaudioflags & 0x400)
                 document.getElementById('edid__audiomode').value = "arcbeam on";    
            else if (this.response.edidaudioflags & 0x800)
                 document.getElementById('edid__audiomode').value = "arcddp on";  

            document.getElementById('edidtableinp1').value = this.response.edidtableinp1;
            document.getElementById('edidtableinp2').value = this.response.edidtableinp2;
            document.getElementById('edidtableinp3').value = this.response.edidtableinp3;
            document.getElementById('edidtableinp4').value = this.response.edidtableinp4;
            
            if (this.response.edidfrlflag == '1')
                document.getElementById('edid__frlcb').checked = true;  
            else
                document.getElementById('edid__frlcb').checked = false;  

            if (this.response.edidvrrflag == '1')
                document.getElementById('edid__vrrcb').checked = true;  
            else
                document.getElementById('edid__vrrcb').checked = false;  

            if (this.response.edidallmflag == '1')
                document.getElementById('edid__allmcb').checked = true;  
            else
                document.getElementById('edid__allmcb').checked = false;  

            if (this.response.edidhdrflag == '1')
                document.getElementById('edid__hdrcb').checked = true;  
            else
                document.getElementById('edid__hdrcb').checked = false;  

            if (this.response.ediddvflag == '1')
                document.getElementById('edid__dvcb').checked = true;  
            else
                document.getElementById('edid__dvcb').checked = false;  


            if (this.response.edidpcmflag == '1')
                document.getElementById('edid__pcmcb').checked = true;  
            else
                document.getElementById('edid__pcmcb').checked = false;  

            if (this.response.edidonebitflag == '1')
                document.getElementById('edid__onebitcb').checked = true;  
            else
                document.getElementById('edid__onebitcb').checked = false;  


            if (this.response.edidtruehdflag == '1')
                document.getElementById('edid__truehdcb').checked = true;  
            else
                document.getElementById('edid__truehdcb').checked = false;  

            if (this.response.edidddflag == '1')
                document.getElementById('edid__ddcb').checked = true;  
            else
                document.getElementById('edid__ddcb').checked = false;  

            if (this.response.edidddplusflag == '1')
                document.getElementById('edid__ddpluscb').checked = true;  
            else
                document.getElementById('edid__ddpluscb').checked = false;  

            if (this.response.ediddtsflag == '1')
                document.getElementById('edid__dtscb').checked = true;  
            else
                document.getElementById('edid__dtscb').checked = false;  

            if (this.response.ediddtshdflag == '1')
                document.getElementById('edid__dtshdcb').checked = true;  
            else
                document.getElementById('edid__dtshdcb').checked = false;  

            document.getElementById('edid__frl').value = this.response.edidfrlmode;
            document.getElementById('edid__vrr').value = this.response.edidvrrmode;
            document.getElementById('edid__allm').value = this.response.edidallmmode;
            document.getElementById('edid__dv').value = this.response.ediddvmode;
            document.getElementById('edid__hdr').value = this.response.edidhdrmode;
            document.getElementById('edid__pcmch').value = this.response.edidpcmchmode;
            document.getElementById('edid__pcmsr').value = this.response.edidpcmsrmode;
            document.getElementById('edid__pcmbw').value = this.response.edidpcmbwmode;
            document.getElementById('edid__truehd').value = this.response.edidtruehdmode;
            document.getElementById('edid__truehdsr').value = this.response.edidtruehdsrmode;
            document.getElementById('edid__dd').value = this.response.edidddmode;
            document.getElementById('edid__ddplus').value = this.response.edidddplusmode;
            document.getElementById('edid__ddplussr').value = this.response.edidddplussrmode;
            document.getElementById('edid__dts').value = this.response.ediddtsmode;
            document.getElementById('edid__dtshd').value = this.response.ediddtshdmode;
            document.getElementById('edid__dtshdsr').value = this.response.ediddtshdsrmode;
            document.getElementById('edid__onebit').value = this.response.edidonebitmode;
            document.getElementById('edid__onebitsp').value = this.response.edidonebitspmode;

            let edidOverflow = parseInt(this.response.edidoverflow,16);
            if (edidOverflow > 0)
            {
                if (edidOverflow & 0x01) 
                    document.getElementById('overflow--line1').style.display = 'block';
                else
                    document.getElementById('overflow--line1').style.display = 'none';

                if (edidOverflow & 0x02) 
                    document.getElementById('overflow--line2').style.display = 'block';
                else
                    document.getElementById('overflow--line2').style.display = 'none';

                if (edidOverflow & 0x04)
                    document.getElementById('overflow--line3').style.display = 'block';
                else
                    document.getElementById('overflow--line3').style.display = 'none';

                if (edidOverflow & 0x08) 
                    document.getElementById('overflow--line4').style.display = 'block';
                else
                    document.getElementById('overflow--line4').style.display = 'none';

                document.getElementById('edidoverflow').style.display = 'block';
            }
            else
                document.getElementById('edidoverflow').style.display = 'none';
            
            document.getElementById('status').innerHTML = 'EDID STATUS OK';
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/edidpage.ssi", true);
    xhttp.responseType = "json";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();
}

function requestScalerPageInfo() {
    var xhttp = new XMLHttpRequest(); 

    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            let byteArray = new Uint8Array(this.response);  
         
            if (byteArray[0] === 0) { // NONE
                document.getElementById("scaler__none").checked = true;
                document.getElementById("scaler__auto").checked = false;
                document.getElementById("scaler__custom").checked = false;
            } else if (byteArray[0] === 1) { // AUTO
                document.getElementById("scaler__none").checked = false;
                document.getElementById("scaler__auto").checked = true;
                document.getElementById("scaler__custom").checked = false;
            } else if (byteArray[0] === 2) { // CUSTOM
                document.getElementById("scaler__none").checked = false;
                document.getElementById("scaler__auto").checked = false;
                document.getElementById("scaler__custom").checked = true;
            }

            (byteArray[1] === 1)
            ? document.getElementById("scaler--noupscale").checked = true
            : document.getElementById("scaler--noupscale").checked = false;


            document.getElementById("port1autoscale__4K60SDR").value = byteArray[2];
            document.getElementById("port1autoscale__4K60HDR").value = byteArray[3];
            document.getElementById("port1autoscale__4K30").value = byteArray[4];

            (byteArray[10] === 1)
                    ? document.getElementById("port0scale__cb4k60").checked = true
                    : document.getElementById("port0scale__cb4k60").checked = false;
            document.getElementById('port0scale__cmb4k60cs').value = byteArray[12];
            document.getElementById('port0scale__cmb4k60dc').value = byteArray[13];

            (byteArray[14] === 1)
                    ? document.getElementById("port0scale__cb4k30").checked = true
                    : document.getElementById("port0scale__cb4k30").checked = false;
            document.getElementById('port0scale__cmb4k30cs').value = byteArray[16];
            document.getElementById('port0scale__cmb4k30dc').value = byteArray[17];


            (byteArray[18] === 1)
                    ? document.getElementById("port0scale__cb1080p60").checked = true
                    : document.getElementById("port0scale__cb1080p60").checked = false;        
            document.getElementById('port0scale__cmb1080p60scale').value = byteArray[19];
            document.getElementById('port0scale__cmb1080p60cs').value = byteArray[20];
            document.getElementById('port0scale__cmb1080p60dc').value = byteArray[21]; 
        

            (byteArray[22] === 1)
                    ? document.getElementById("port0scale__cb1080p30").checked = true
                    : document.getElementById("port0scale__cb1080p30").checked = false;        
            document.getElementById('port0scale__cmb1080p30scale').value = byteArray[23];
            document.getElementById('port0scale__cmb1080p30cs').value = byteArray[24];
            document.getElementById('port0scale__cmb1080p30dc').value = byteArray[25]; 
        
            (byteArray[26] === 1)
                    ? document.getElementById("port1scale__cb4k60").checked = true
                    : document.getElementById("port1scale__cb4k60").checked = false;
            document.getElementById('port1scale__cmb4k60scale').value = byteArray[27]; 
            document.getElementById('port1scale__cmb4k60cs').value = byteArray[28];
            document.getElementById('port1scale__cmb4k60dc').value = byteArray[29];

            (byteArray[30] === 1)
                    ? document.getElementById("port1scale__cb4k30").checked = true
                    : document.getElementById("port1scale__cb4k30").checked = false;
            document.getElementById('port1scale__cmb4k30scale').value = byteArray[31] 
            document.getElementById('port1scale__cmb4k30cs').value = byteArray[32];
            document.getElementById('port1scale__cmb4k30dc').value = byteArray[33];


            (byteArray[34] === 1)
                    ? document.getElementById("port1scale__cb1080p60").checked = true
                    : document.getElementById("port1scale__cb1080p60").checked = false;        
            document.getElementById('port1scale__cmb1080p60cs').value = byteArray[36];
            document.getElementById('port1scale__cmb1080p60dc').value = byteArray[37]; 
        

            (byteArray[38] === 1)
                    ? document.getElementById("port1scale__cb1080p30").checked = true
                    : document.getElementById("port1scale__cb1080p30").checked = false;        
            document.getElementById('port1scale__cmb1080p30cs').value = byteArray[40];
            document.getElementById('port1scale__cmb1080p30dc').value = byteArray[41]; 

            document.getElementById('status').innerHTML = 'SCALER STATUS OK';

        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };

    xhttp.open("GET", url + "/ssi/sclpage.ssi", true);
    xhttp.responseType = "arraybuffer";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();
}


function requestHdrPageInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
   
            document.getElementById('hdr__captured').value = this.response.HDR.toUpperCase();
            document.getElementById('spd__captured').innerHTML = this.response.SPD.toUpperCase();
            document.getElementById('avi__captured').value = this.response.AVI.toUpperCase();
            document.getElementById('hvs__captured').innerHTML = this.response.HVS.toUpperCase();
            document.getElementById('hfv__captured').innerHTML = this.response.HFV.toUpperCase();
            document.getElementById('aud__captured').innerHTML = this.response.AUD.toUpperCase();
            document.getElementById('vsi__captured').innerHTML = this.response.VSI.toUpperCase();

            document.getElementById('infoframe__customhdr').value = this.response.HDRCUSTOM;
            document.getElementById('infoframe__customavi').value = this.response.AVICUSTOM;
            document.getElementById('infoframe__customvsi').value = this.response.VSICUSTOM;
            

            (this.response.customhdr === '1')
            ? document.getElementById("infoframe_cbhdrcustom").checked = true
            : document.getElementById("infoframe_cbhdrcustom").checked = false;

            (this.response.disablehdr === '1')
            ? document.getElementById("infoframe_cbhdrdisable").checked = true
            : document.getElementById("infoframe_cbhdrdisable").checked = false;

            (this.response.hdrcustomhdronly === '1')
            ? document.getElementById("infoframe_cbhdrcustomhdronly").checked = true
            : document.getElementById("infoframe_cbhdrcustomhdronly").checked = false;

            (this.response.hdrcustomhlgonly === '1')
            ? document.getElementById("infoframe_cbhdrcustomhlgonly").checked = true
            : document.getElementById("infoframe_cbhdrcustomhlgonly").checked = false;
 
            (this.response.hdrcustomhlgonlyauto === '1')
            ? document.getElementById("infoframe_cbhdrcustomhlgonlyauto").checked = true
            : document.getElementById("infoframe_cbhdrcustomhlgonlyauto").checked = false;


            (this.response.hdrcustomlldvonly === '1')
            ? document.getElementById("infoframe_cbhdrcustomlldvonly").checked = true
            : document.getElementById("infoframe_cbhdrcustomlldvonly").checked = false;


            (this.response.hdr10disable === '1')
            ? document.getElementById("infoframe_cbhdr10disable").checked = true
            : document.getElementById("infoframe_cbhdr10disable").checked = false;

            (this.response.hlgdisable === '1')
            ? document.getElementById("infoframe_cbhlgdisable").checked = true
            : document.getElementById("infoframe_cbhlgdisable").checked = false;

            (this.response.lldvdisable === '1')
            ? document.getElementById("infoframe_cblldvdisable").checked = true
            : document.getElementById("infoframe_cblldvdisable").checked = false;
           
            (this.response.filmmaker === '1')
            ? document.getElementById("infoframe_cbfilmmaker").checked = true
            : document.getElementById("infoframe_cbfilmmaker").checked = false;

            (this.response.customavi === '1')
            ? document.getElementById("infoframe_cbavicustom").checked = true
            : document.getElementById("infoframe_cbavicustom").checked = false;

            (this.response.disableavi === '1')
            ? document.getElementById("infoframe_cbavidisable").checked = true
            : document.getElementById("infoframe_cbavidisable").checked = false;

            if (this.response.allmmode === '0') 
            {
                document.getElementById("allmmode_follow").checked = true
                document.getElementById("allmmode_on").checked = false;
                document.getElementById("allmmode_off").checked = false;
            } 
            else  if (this.response.allmmode === '1') // on
            {
                document.getElementById("allmmode_follow").checked = false
                document.getElementById("allmmode_on").checked = true;
                document.getElementById("allmmode_off").checked = false;
            }
            else  if (this.response.allmmode === '2') // off
            {
                document.getElementById("allmmode_follow").checked = false
                document.getElementById("allmmode_on").checked = false;
                document.getElementById("allmmode_off").checked = true;
            }
            document.getElementById('allmmode_target').value = this.response.allmtarget;

            const hdr_infoframe = this.response.HDR.split(':');
   
            if ( (hdr_infoframe[0] === "87") && (hdr_infoframe[1] === "01") ) {
            
                if (hdr_infoframe[4] === "00")
                    document.getElementById('infoframe__decode--eotf').innerHTML = "EOTF 0: SDR Luminance Range";
                else if (hdr_infoframe[4] === "01")
                    document.getElementById('infoframe__decode--eotf').innerHTML = "EOTF 1: HDR Luminance Range";
                else if (hdr_infoframe[4] === "02")
                    document.getElementById('infoframe__decode--eotf').innerHTML = "EOTF 2: SMPTE ST 2084 [PQ]";
                else if (hdr_infoframe[4] === "03")
                    document.getElementById('infoframe__decode--eotf').innerHTML = "EOTF 3: HLG ITU-R BT.2100-0";

                document.getElementById('infoframe__decode--eotf').style.fontWeight = 600;

                const whiteX = (parseInt(hdr_infoframe[19],16) << 8) | parseInt(hdr_infoframe[18],16);
                const whiteY = (parseInt(hdr_infoframe[21],16) << 8) | parseInt(hdr_infoframe[20],16);
                document.getElementById('infoframe__decode--primarywp').innerHTML = `WP: ${whiteX}, ${whiteY}, [${whiteX/50000}, ${whiteY/50000}]`;

                const maxDispLum = (parseInt(hdr_infoframe[23],16) << 8) | parseInt(hdr_infoframe[22],16);
                const minDispLum = ((parseInt(hdr_infoframe[25],16) << 8) | parseInt(hdr_infoframe[24],16)) * 0.0001;
                document.getElementById('infoframe__decode--maxlum').innerHTML = `Max/Min Lum: ${maxDispLum} / ${minDispLum} nits`;
        
                const maxCLL = (parseInt(hdr_infoframe[27],16) << 8) | parseInt(hdr_infoframe[26],16);
                const maxFALL = (parseInt(hdr_infoframe[29],16) << 8) | parseInt(hdr_infoframe[28],16);
                document.getElementById('infoframe__decode--maxcll').innerHTML = `MaxCLL/FALL: ${maxCLL} / ${maxFALL} nits`;
         
                const primaryX0 = (parseInt(hdr_infoframe[7],16) << 8) | parseInt(hdr_infoframe[6],16);
                const primaryY0 = (parseInt(hdr_infoframe[9],16) << 8) | parseInt(hdr_infoframe[8],16);
                const primaryX1 = (parseInt(hdr_infoframe[11],16) << 8) | parseInt(hdr_infoframe[10],16);
                const primaryY1 = (parseInt(hdr_infoframe[13],16) << 8) | parseInt(hdr_infoframe[12],16);
                const primaryX2 = (parseInt(hdr_infoframe[15],16) << 8) | parseInt(hdr_infoframe[14],16);
                const primaryY2 = (parseInt(hdr_infoframe[17],16) << 8) | parseInt(hdr_infoframe[16],16);
                document.getElementById('infoframe__decode--primary0').innerHTML = `GRN: ${primaryX0}, ${primaryY0} [${primaryX0/50000}, ${primaryY0/50000}]`;
                document.getElementById('infoframe__decode--primary1').innerHTML = `BLU: ${primaryX1}, ${primaryY1} [${primaryX1/50000}, ${primaryY1/50000}]`;
                document.getElementById('infoframe__decode--primary2').innerHTML = `RED: ${primaryX2}, ${primaryY2} [${primaryX2/50000}, ${primaryY2/50000}]`;
     
				if ( (primaryX0 === 13250) && (primaryY0 === 34500) &&
					(primaryX1 === 7500) && (primaryY1 === 3000) &&
                    (primaryX2 === 34000) && (primaryY2 === 16000)) 
                {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: DCI-P3";
                } 
                else if ( (primaryX0 === 8500) && (primaryY0 === 39850) &&
                (primaryX1 === 6550) && (primaryY1 === 2300) &&
                (primaryX2 === 35400) && (primaryY2 === 14600)  )               
                {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: BT2020";
                }  
                else if ( (primaryX0 === 15000) && (primaryY0 === 30000) &&
                (primaryX1 === 7500) && (primaryY1 === 3000) &&
                (primaryX2 === 32000) && (primaryY2 === 16500)  )                
                {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: REC-709";
                }  
                else if ( (primaryX0 === 34000) && (primaryY0 === 16000) &&
                (primaryX1 === 13250) && (primaryY1 === 34500) &&
                (primaryX2 === 7500) && (primaryY2 === 3000)  )                 
                {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: DCI-P3 [WO]";
                } 
                else if ( (primaryX0 === 35400) && (primaryY0 === 14600) &&
                (primaryX1 === 8500) && (primaryY1 === 39850) &&
                (primaryX2 === 6550) && (primaryY2 === 2300)  )                   
                {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: BT2020 [WO]";
                } 
                else if ( (primaryX0 === 32000) && (primaryY0 === 16500) &&
                (primaryX1 === 15000) && (primaryY1 === 30000) &&
                (primaryX2 === 7500) && (primaryY2 === 3000)  )              
                {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: REC-709 [WO]";
                }             
                else {
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", MT: unknown";
                }

                if ( (whiteX === 15635) && (whiteY === 16450))
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", WP: D65";
                else
                    document.getElementById('infoframe__decode--eotf').innerHTML += ", WP: unknown"; 
            }
            else /* no HDR received */
            {
                document.getElementById('infoframe__decode--eotf').style.fontWeight = 400;
            }

            /* Decode AVI */
            const avi_infoframe = this.response.AVI.split(':');
            if ( (avi_infoframe[0] === "82") && ((avi_infoframe[1] === "02")||(avi_infoframe[1] === "03")) ) 
            {

                document.getElementById('infoframe__decode--vic').innerHTML = `VIC: ${parseInt(avi_infoframe[7],16)} / 0x${avi_infoframe[7]}`;
                
                let cscInfo = "";
                switch(parseInt(avi_infoframe[4],16)&0xE0) {
                    case 0x00: cscInfo = "RGB"; break;
                    case 0x20: cscInfo = "YCbCr 422"; break;
                    case 0x40: cscInfo = "YCbCr 444"; break;
                    case 0x60: cscInfo = "YCbCr 420"; break;
                }
     
                switch(parseInt(avi_infoframe[5],16)&0xC0) {
                    case 0x40: cscInfo += " BT601"; break;
                    case 0x80: cscInfo += " BT709"; break;
                    case 0xC0: 
      
                        switch(parseInt(avi_infoframe[6],16)&0x70) {
                            case 0x00: cscInfo = "xvYCC601"; break;
                            case 0x10: cscInfo = "xvYCC709"; break;
                            case 0x20: cscInfo = "sYCC601"; break;
                            case 0x30: cscInfo = "AdobeYcc601"; break;
                            case 0x40: cscInfo = "AdobeRGB"; break;
                            case 0x50: cscInfo = "BT2020 YCcCbcCrc"; break;
                            case 0x60: cscInfo += " BT2020"; break;
                        }   
                    break;       
                } 
                document.getElementById('infoframe__decode--csc').innerHTML = `Colorimetry: ${cscInfo}`;

                let cscRange = "";
                switch(parseInt(avi_infoframe[6],16)&0x0C) {
                    case 0x00: cscRange = "Default / Not specified"; break;
                    case 0x04: cscRange = "Limited Range [16-235]"; break;
                    case 0x08: cscRange = "Full Range [0-255]"; break;
                }
                document.getElementById('infoframe__decode--range').innerHTML = `Quantization range: ${cscRange}`;

            }

            document.getElementById('status').innerHTML = 'HDR/DV STATUS OK';

        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/hdrpage.ssi", true);
    xhttp.responseType = "json";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    xhttp.send();
}

const maxCalc = (a,b) => ((a) > (b) ? (a) : (b));
const minCalc = (a,b) => ((a) < (b) ? (a) : (b))

const calcEotf = (lum) => {

    const skPq_m1 = 2610/(4096*4);
    const skPq_m2 = (2523*128)/4096;
    const skPq_c1 = 3424/4096;
    const skPq_c2 = (2413*32)/4096;
    const skPq_c3 = (2392*32)/4096; 
 
    const num=maxCalc(Math.pow(lum, 1/skPq_m2) - skPq_c1, 0);
    const den = maxCalc(skPq_c2 -skPq_c3*Math.pow(lum, 1/skPq_m2), 0.0000001);
    return( Math.pow(num/den, 1/skPq_m1)*10000 );

}
const calcInvEotf = (lum) => {

    const skPq_m1 = 2610/(4096*4);
    const skPq_m2 = (2523*128)/4096;
    const skPq_c1 = 3424/4096;
    const skPq_c2 = (2413*32)/4096;
    const skPq_c3 = (2392*32)/4096; 
 
    const num = skPq_c1 + skPq_c2 * Math.pow(lum, skPq_m1);
    const den = 1 + skPq_c3 * Math.pow(lum,skPq_m1);
    
    return( Math.pow(num/den, skPq_m2) );

}
function requestDVPageInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
      
            let lldvActive = 0;
            let dvActive = 0;
            let dvEdidCheck = [];
            document.getElementById('infoframe__lldvdatablock').value = this.response.DVCUSTOM.toUpperCase().slice(0, 35);
           
            dvEdidCheck = this.response.DV0.split(':');   
            const dvVsiCheck = this.response.VSI.split(':');
            const hvsInfoframe = this.response.HVS.split(':');

            if ( (dvVsiCheck[0] === "81") && (dvVsiCheck[4] === "46") && (dvVsiCheck[5] === "d0") && (dvVsiCheck[6] === "00") )
                lldvActive = 1;
            else if  ( (hvsInfoframe[0] === "81") && (hvsInfoframe[2] === "18") && (hvsInfoframe[4] === "03")  ) 
                dvActive = 1;

            if (dvActive==1)
            {
                document.getElementById('infoframe__decode--dvstatus').innerHTML = "active";
                document.getElementById('infoframe__decode--dvinterface').innerHTML = "standard (non-lldv)";
                document.getElementById('infoframe__decode--dvbacklightmeta').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvbacklightmaxlum').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvauxrunmode').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvauxrunversion').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvauxdebug').innerHTML = "not present";

                document.getElementById('dv__captured').value = this.response.HVS.toUpperCase().slice(0, 83);
                document.getElementById('vsi__captured').innerHTML = this.response.VSI.toUpperCase();
            } 
            else if (lldvActive == 1)
            {
            
                document.getElementById('dv__captured').value = this.response.VSI.toUpperCase().slice(0, 62);
                document.getElementById('vsi__captured').innerHTML = "";
                if (parseInt(dvVsiCheck[7],16)&0x02)
                    document.getElementById('infoframe__decode--dvstatus').innerHTML = "active";
                else
                    document.getElementById('infoframe__decode--dvstatus').innerHTML = "not active";

                if (parseInt(dvVsiCheck[7],16)&0x01)
                    document.getElementById('infoframe__decode--dvinterface').innerHTML = "low latency";
                else
                    document.getElementById('infoframe__decode--dvinterface').innerHTML = "standard";

                if (parseInt(dvVsiCheck[8],16)&0x80)
                    document.getElementById('infoframe__decode--dvbacklightmeta').innerHTML = "present";
                else
                    document.getElementById('infoframe__decode--dvbacklightmeta').innerHTML = "not present";

                const effTmax = ((parseInt(dvVsiCheck[8],16)&0x0f) << 4) | parseInt(dvVsiCheck[9],16);
                if (effTmax > 0)
                    document.getElementById('infoframe__decode--dvbacklightmaxlum').innerHTML = effTmax;
                else
                    document.getElementById('infoframe__decode--dvbacklightmaxlum').innerHTML = "not present";

                const auxRunMode = parseInt(dvVsiCheck[10],16);
                if (auxRunMode > 0)
                    document.getElementById('infoframe__decode--dvauxrunmode').innerHTML = auxRunMode;
                else
                    document.getElementById('infoframe__decode--dvauxrunmode').innerHTML = "not present";

                const auxRunVersion = parseInt(dvVsiCheck[11],16);
                if (auxRunVersion > 0)
                    document.getElementById('infoframe__decode--dvauxrunversion').innerHTML = auxRunVersion;
                 else
                    document.getElementById('infoframe__decode--dvauxrunversion').innerHTML = "not present";
    
                const auxDebug = parseInt(dvVsiCheck[12],16);
                if (auxDebug > 0)
                    document.getElementById('infoframe__decode--dvauxdebug').innerHTML = auxDebug;
                    else
                    document.getElementById('infoframe__decode--dvauxdebug').innerHTML = "not present";
    
                    
            } else {
                document.getElementById('infoframe__decode--dvstatus').innerHTML = "not active";
                document.getElementById('infoframe__decode--dvinterface').innerHTML = "not active";
                document.getElementById('infoframe__decode--dvbacklightmeta').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvbacklightmaxlum').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvauxrunmode').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvauxrunversion').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvauxdebug').innerHTML = "not present";

                document.getElementById('dv__captured').value = "";
                document.getElementById('vsi__captured').innerHTML = this.response.VSI.toUpperCase();
            }


            if ( (parseInt(dvEdidCheck[0],16) >> 5) === 0x07) 
            {
                let Rx, Ry, Gx, Gy, Bx, By;
                let dvVersion = parseInt(dvEdidCheck[5],16) >> 5;

                const dvDataBlockLength = parseInt(dvEdidCheck[0],16) & 0x1f;
                if (dvDataBlockLength > 25) dvDataBlockLength = 25;
                document.getElementById('dv__tx0edidblock').value = this.response.DV0.toUpperCase().slice(0,dvDataBlockLength*3+2);

                if ( dvVersion === 0x00)
                {
                        document.getElementById('infoframe__decode--dvversion').innerHTML = "0";  
                        document.getElementById('infoframe__decode--dvdmversion').innerHTML = `${parseInt(dvEdidCheck[21],16) >> 4}.${parseInt(dvEdidCheck[21],16) & 0x0f}`;

                        if (parseInt(dvEdidCheck[5],16)&0x04)
                            document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "not supported";
    
                        if (parseInt(dvEdidCheck[5],16)&0x02)
                            document.getElementById('infoframe__decode--support2160').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--support2160').innerHTML = "not supported";

                        if (parseInt(dvEdidCheck[5],16)&0x01)
                            document.getElementById('infoframe__decode--supportYuv422').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--supportYuv422').innerHTML = "not supported";

                        document.getElementById('infoframe__decode--supportBackLight').innerHTML = "not supported";
                        Rx = (parseInt(dvEdidCheck[7],16) << 4) | (parseInt(dvEdidCheck[6],16)>>4) & 0x0f;
                        Ry = (parseInt(dvEdidCheck[8],16) << 4) | parseInt(dvEdidCheck[6],16) & 0x0f;
                     
                        Gx = (parseInt(dvEdidCheck[10],16) << 4) | (parseInt(dvEdidCheck[9],16)>>4) & 0x0f;
                        Gy = (parseInt(dvEdidCheck[11],16) << 4) | parseInt(dvEdidCheck[9],16) & 0x0f;

                        Bx = (parseInt(dvEdidCheck[13],16) << 4) | (parseInt(dvEdidCheck[12],16)>>4) & 0x0f;
                        By = (parseInt(dvEdidCheck[14],16) << 4) | parseInt(dvEdidCheck[13],16) & 0x0f;

                        Wx = (parseInt(dvEdidCheck[16],16) << 4) | (parseInt(dvEdidCheck[15],16)>>4) & 0x0f;
                        Wy = (parseInt(dvEdidCheck[17],16) << 4) | parseInt(dvEdidCheck[15],16) & 0x0f;

                        const minDispLum = (parseInt(dvEdidCheck[19],16) << 4) | (parseInt(dvEdidCheck[18],16)>>4) & 0x0f;
                        const maxDispLum  = (parseInt(dvEdidCheck[20],16) << 4) | parseInt(dvEdidCheck[18],16) & 0x0f;
                        const maxDispLumNits = calcEotf(maxDispLum/4095);
                        const minDispLumNits = calcEotf(minDispLum/4095);

                        document.getElementById('infoframe__decode--dvlum').innerHTML = `${Math.round(maxDispLumNits)} / ${minDispLumNits.toFixed(3)} nits`;    

                        document.getElementById('infoframe__decode--Rprim').innerHTML = `${(Rx/4096).toFixed(3)}, ${(Ry/4096).toFixed(3)}`;
                        document.getElementById('infoframe__decode--Gprim').innerHTML = `${(Gx/4096).toFixed(3)}, ${(Gy/4096).toFixed(3)}`;
                        document.getElementById('infoframe__decode--Bprim').innerHTML = `${(Bx/4096).toFixed(3)}, ${(By/4096).toFixed(3)}`;

                        document.getElementById('infoframe__decode--dvInterface').innerHTML = "standard DV only"; 

                }
                else if ( dvVersion === 0x01)  /* DV version 1, short or long style */
                {
                        let dvVersionLength = parseInt(dvEdidCheck[0],16)&0x1F;
                        if (dvVersionLength == 0x0b) 
                            document.getElementById('infoframe__decode--dvversion').innerHTML = "1 [short 12-byte version]";  
                        else if (dvVersionLength == 0x0e) 
                            document.getElementById('infoframe__decode--dvversion').innerHTML = "1, [long 15-byte version]";  
      
                        const dvDmVersion = (parseInt(dvEdidCheck[5],16)&0x1c) >> 2;
                        if (dvDmVersion == 0)
                            document.getElementById('infoframe__decode--dvdmversion').innerHTML ='v2.x';  
                        else if (dvDmVersion == 1)
                            document.getElementById('infoframe__decode--dvdmversion').innerHTML ='v3.x';  
                        else
                            document.getElementById('infoframe__decode--dvdmversion').innerHTML ='reserved';  
 
                        if (parseInt(dvEdidCheck[5],16)&0x02)
                            document.getElementById('infoframe__decode--support2160').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--support2160').innerHTML = "not supported";

                        if (parseInt(dvEdidCheck[5],16)&0x01)
                            document.getElementById('infoframe__decode--supportYuv422').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--supportYuv422').innerHTML = "not supported";

                        document.getElementById('infoframe__decode--supportBackLight').innerHTML = "not supported";

                        const maxDispLum = (parseInt(dvEdidCheck[6],16) >> 1) * 50 + 100;

                        const minDispLumVal = (parseInt(dvEdidCheck[7],16) >> 1);
                        const minDispLum = Math.pow(minDispLumVal/127, 2);
                        document.getElementById('infoframe__decode--dvlum').innerHTML = `${maxDispLum} / ${minDispLum.toFixed(3)} nits`;    
     
                        if (parseInt(dvEdidCheck[6],16)&0x01)
                            document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "not supported";
    
                       
                        if (dvVersionLength == 0x0b) 
                        {
                            Rx = 0xA0 | (parseInt(dvEdidCheck[11],16) >> 3);
                            Ry = 0x40 | ( ( (parseInt(dvEdidCheck[11],16)&0x07) << 2) | ((parseInt(dvEdidCheck[10],16)&0x01) << 1) |  (parseInt(dvEdidCheck[9],16)&0x01) );

                            Gx = 0x00 | (parseInt(dvEdidCheck[9],16) >> 1);
                            Gy = 0x80 | (parseInt(dvEdidCheck[10],16) >> 1);

                            Bx = 0x20 | (parseInt(dvEdidCheck[8],16) >> 5);
                            By = 0x08 | (parseInt(dvEdidCheck[8],16) >> 2) & 0x07;

                            const dvLowLatency = parseInt(dvEdidCheck[8],16)&0x03;
                            switch (dvLowLatency) {
                                case 0:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "standard DV only"; break;
                                case 1:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "standard and low latency 422 12bit"; break;
                                case 2:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "reserved"; break;
                                case 3:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "reserved"; break;
                            }
                            document.getElementById('infoframe__decode--Rprim').innerHTML = `${(Rx/256).toFixed(3)}, ${(Ry/256).toFixed(3)}`;
                            document.getElementById('infoframe__decode--Gprim').innerHTML = `${(Gx/256).toFixed(3)}, ${(Gy/256).toFixed(3)}`;
                            document.getElementById('infoframe__decode--Bprim').innerHTML = `${(Bx/256).toFixed(3)}, ${(By/256).toFixed(3)}`;
    
                        } 
                        else if (dvVersionLength == 0x0e)   // long version
                        {
                            Rx = parseInt(dvEdidCheck[9],16)
                            Ry = parseInt(dvEdidCheck[10],16)
 
                            Gx = parseInt(dvEdidCheck[11],16)
                            Gy = parseInt(dvEdidCheck[12],16)
 
                            Bx = parseInt(dvEdidCheck[13],16)
                            By = parseInt(dvEdidCheck[14],16)

                            document.getElementById('infoframe__decode--Rprim').innerHTML = `${(Rx/256).toFixed(3)}, ${(Ry/256).toFixed(3)}`;
                            document.getElementById('infoframe__decode--Gprim').innerHTML = `${(Gx/256).toFixed(3)}, ${(Gy/256).toFixed(3)}`;
                            document.getElementById('infoframe__decode--Bprim').innerHTML = `${(Bx/256).toFixed(3)}, ${(By/256).toFixed(3)}`;
                       
                            document.getElementById('infoframe__decode--dvInterface').innerHTML = "standard DV only"; 
                        }
                        
                }
                else if ( dvVersion === 0x02)  /* DV version 2 */
                {
                        document.getElementById('infoframe__decode--dvversion').innerHTML = "2";  
      
                        const dvDmVersion = (parseInt(dvEdidCheck[5],16)&0x1c) >> 2;
                        if (dvDmVersion == 0)
                            document.getElementById('infoframe__decode--dvdmversion').innerHTML ='v2.x';  
                        else if (dvDmVersion == 1)
                            document.getElementById('infoframe__decode--dvdmversion').innerHTML ='v3.x';  
                        else
                            document.getElementById('infoframe__decode--dvdmversion').innerHTML ='reserved';  

                        const backLightMinLuma = parseInt(dvEdidCheck[6],16)&0x03; 
                        if (parseInt(dvEdidCheck[5],16)&0x02)
                        {
                            switch (backLightMinLuma){
                                case 0: document.getElementById('infoframe__decode--supportBackLight').innerHTML = "supported 25 nits"; break;
                                case 1: document.getElementById('infoframe__decode--supportBackLight').innerHTML = "supported 50 nits"; break;
                                case 2: document.getElementById('infoframe__decode--supportBackLight').innerHTML = "supported 75 nits"; break;
                                case 3: document.getElementById('infoframe__decode--supportBackLight').innerHTML = "supported 100 nits"; break;
                            }   
                        }
                        else
                            document.getElementById('infoframe__decode--supportBackLight').innerHTML = "not supported, default 100 nits";

                          
                        if (parseInt(dvEdidCheck[6],16)&0x04)
                            document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "not supported";

                        document.getElementById('infoframe__decode--support2160').innerHTML = "supported";

                        if (parseInt(dvEdidCheck[5],16)&0x01)
                            document.getElementById('infoframe__decode--supportYuv422').innerHTML = "supported";
                        else
                            document.getElementById('infoframe__decode--supportYuv422').innerHTML = "not supported";
                            
                        const minDispLum = ( (parseInt(dvEdidCheck[6],16) >> 3) * 20);            
                        const maxDispLum = (  parseInt(dvEdidCheck[7],16) >> 3) * 65 + 2055;
                        const maxDispLumNits = calcEotf(maxDispLum/4095);
                        const minDispLumNits = calcEotf(minDispLum/4095);

                        // range is from 2055/4095 to 4070/4095 ... 0.50183 to 0.993895 (100 nits to 10000nits after PQ)
                     
                        document.getElementById('infoframe__decode--dvlum').innerHTML = `${Math.round(maxDispLumNits)} / ${minDispLumNits.toFixed(3)} nits`;    
     
                        Rx = 0xA0 | (parseInt(dvEdidCheck[10],16) >> 3);
                        Ry = 0x40 | (parseInt(dvEdidCheck[11],16) >> 3);                
 
                        Gx = 0x00 | (parseInt(dvEdidCheck[8],16) >> 1);
                        Gy = 0x80 | (parseInt(dvEdidCheck[9],16) >> 1);

                        Bx = 0x20 | parseInt(dvEdidCheck[10],16) & 0x07;
                        By = 0x08 | parseInt(dvEdidCheck[11],16) & 0x07;

                        const dvInterface = parseInt(dvEdidCheck[7],16)&0x03;
                        switch (dvInterface) {
                            case 0:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "low latency 422 12bit"; break;
                            case 1:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "low latency 422 12bit and RGB/444 10/12bit"; break;
                            case 2:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "standard and low latency 422 12bit"; break;
                            case 3:  document.getElementById('infoframe__decode--dvInterface').innerHTML = "standard and low latency 422/444/RGB 10/12bit "; break;
                        }
                        document.getElementById('infoframe__decode--Rprim').innerHTML = `${(Rx/256).toFixed(3)}, ${(Ry/256).toFixed(3)}`;
                        document.getElementById('infoframe__decode--Gprim').innerHTML = `${(Gx/256).toFixed(3)}, ${(Gy/256).toFixed(3)}`;
                        document.getElementById('infoframe__decode--Bprim').innerHTML = `${(Bx/256).toFixed(3)}, ${(By/256).toFixed(3)}`;   
                }
            } 
            else
            {
                document.getElementById('dv__tx0edidblock').value = "";
                document.getElementById('infoframe__decode--dvversion').innerHTML = "not present";  
                document.getElementById('infoframe__decode--dvdmversion').innerHTML ="not present";  
                document.getElementById('infoframe__decode--dvInterface').innerHTML = "not present";
                document.getElementById('infoframe__decode--supportYuv422').innerHTML = "not present";
                document.getElementById('infoframe__decode--supportBackLight').innerHTML = "not present"; 
                document.getElementById('infoframe__decode--supportGlobDim').innerHTML = "not present";
                document.getElementById('infoframe__decode--support2160').innerHTML = "not present";
                document.getElementById('infoframe__decode--Rprim').innerHTML = "not present";
                document.getElementById('infoframe__decode--Gprim').innerHTML = "not present";
                document.getElementById('infoframe__decode--Bprim').innerHTML = "not present";
                document.getElementById('infoframe__decode--dvlum').innerHTML = "not present";    
            }


            document.getElementById('status').innerHTML = 'DV STATUS OK';

        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/hdrpage.ssi", true);
    xhttp.responseType = "json";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    xhttp.send();
}

function requestOsdPageInfo() {
    var xhttp = new XMLHttpRequest();

    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {

            let byteArray = new Uint8Array(this.response);  

            document.getElementById('osdenable').checked = byteArray[10];
            document.getElementById('osd__fade').value = byteArray[17];
            document.getElementById('osd__fadeinfo').value = byteArray[20];

            document.getElementById('osd_x').value = (byteArray[26]<<8) | byteArray[27];
            document.getElementById('osd_y').value = (byteArray[28]<<8) | byteArray[29];
            document.getElementById('osd__color--fg').value = byteArray[13];
            document.getElementById('osd__color--bg').value = byteArray[14];
            document.getElementById('osd__color--level').value = byteArray[15];
            

            document.getElementById('osd__item--ignoremeta').checked = byteArray[19];
 

            document.getElementById('osdmaskenable').checked = byteArray[11];      
            document.getElementById('osdmask_x').value = (byteArray[30]<<8) | byteArray[31];
            document.getElementById('osdmask_y').value = (byteArray[32]<<8) | byteArray[33];
            document.getElementById('osdmask_x1').value = (byteArray[34]<<8) | byteArray[35];
            document.getElementById('osdmask_y1').value = (byteArray[36]<<8) | byteArray[37];

            document.getElementById('osd__mask--level').value = byteArray[12];
            document.getElementById('osd__mask--gs').value = byteArray[18];
 
            document.getElementById('cbosdtext').checked = byteArray[39]&0x01;
            document.getElementById('cbosdtextneverfade').checked = (byteArray[39]>>>1)&0x01;
            document.getElementById('osdtext_x').value = (byteArray[40]<<8) | byteArray[41];
            document.getElementById('osdtext_y').value = (byteArray[42]<<8) | byteArray[43];
            let str = '';
            for (let i=0; i<byteArray[38];i++)
                str = str + String.fromCharCode(byteArray[44+i]);
            document.getElementById('osdtextcontent').value = str;

            document.getElementById("osd__item--source").checked = byteArray[16]&0x01;
            document.getElementById("osd__item--videofield").checked = (byteArray[16]>>>1)&0x01;
            document.getElementById("osd__item--videoif").checked = (byteArray[16]>>>2)&0x01;
            document.getElementById("osd__item--hdrrxtx").checked = (byteArray[16]>>>3)&0x01;
            document.getElementById("osd__item--hdrif").checked = (byteArray[16]>>>4)&0x01;
            document.getElementById("osd__item--audiofield").checked = (byteArray[16]>>>5)&0x01;
            document.getElementById("osd__item--rxvideoinfo").checked = (byteArray[16]>>>1)&0x01;
            document.getElementById("osd__item--rxhdrinfo").checked = (byteArray[16]>>>6)&0x01;
            document.getElementById("osd__item--rxvideoinfo").checked = (byteArray[16]>>>7)&0x01;

            document.getElementById('status').innerHTML = 'OSD STATUS OK';
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/osdpage.ssi", true);
    xhttp.responseType = "arraybuffer";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    xhttp.send();
}

function requestCecPageInfo() {
    var xhttp = new XMLHttpRequest();
    
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {

            if (this.response.cec === '0')
                document.getElementById('cec__enable').checked = 0;
            else 
                document.getElementById('cec__enable').checked = 1;

            if (this.response.cec0en === '0')
                document.getElementById('cec__enable0').checked = 0;
            else 
                document.getElementById('cec__enable0').checked = 1;


            if (this.response.cec1en === '0')
                document.getElementById('cec__enable1').checked = 0;
            else 
                document.getElementById('cec__enable1').checked = 1;

                if (this.response.cec2en === '0')
                document.getElementById('cec__enable2').checked = 0;
            else 
                document.getElementById('cec__enable2').checked = 1;

            if (this.response.cec3en === '0')
                document.getElementById('cec__enable3').checked = 0;
            else 
                document.getElementById('cec__enable3').checked = 1;



 /*           if (this.response.archbr === '1')
                document.getElementById('cec__archbr').checked = 1;
            else 
                document.getElementById('cec__archbr').checked = 0;

               
            if (this.response.arcforce === '0')
            {
                document.getElementById("arcforce_auto").checked = true;
                document.getElementById("arcforce_arc").checked = false;
                document.getElementById("arcforce_hdmi").checked = false;    
            } 
            else if (this.response.arcforce === '1')
            {
                document.getElementById("arcforce_auto").checked = false;
                document.getElementById("arcforce_arc").checked = true;
                document.getElementById("arcforce_hdmi").checked = false;  
            } 
            else if (this.response.arcforce === '2')
            {
                document.getElementById("arcforce_auto").checked = false;
                document.getElementById("arcforce_arc").checked = false;
                document.getElementById("arcforce_hdmi").checked = true;  
            }
*/

            document.getElementById('earcforce').value = this.response.earcforce;
            document.getElementById('earcspeakermap').value = this.response.earcspeakermap;
            document.getElementById('earcmutemode').value = this.response.earcmutemode;          
            document.getElementById('macros__ceclamode').value = this.response.cecla;
            document.getElementById('earctxmode').value = this.response.earctxmode;
            document.getElementById('azpoweronmode').value = this.response.azpoweronmode;
            document.getElementById('azpoweroffmode').value = this.response.azpoweroffmode;

            document.getElementById('baudrate').value = this.response.baudrate;

            switch (this.response.baudrate) {
                case '9600': document.getElementById('baudrate').value = 0; break;
                case '19200': document.getElementById('baudrate').value = 1; break;
                case '57600': document.getElementById('baudrate').value = 2; break;
                case '115200': document.getElementById('baudrate').value = 3; break;
            }

            document.getElementById('status').innerHTML = 'CEC/RS232 STATUS OK';

        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/cecpage.ssi", true);
    xhttp.responseType = "json";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();

}

function requestMacrosPageInfo() {
    var xhttp = new XMLHttpRequest();

    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            let byteArray = new Uint8Array(this.response);  

            document.getElementById('macro__enable').checked = byteArray[0]&0x01;
            document.getElementById('macro__everysync').checked = (byteArray[0]&0x02)>>>1;

            document.getElementById('macro__execcustomcode').checked = (byteArray[0]&0x04)>>>2;
            document.getElementById('macros__selbeforeafter').value = (byteArray[0]&0x08)>>>3;
            document.getElementById('macros__execdelay').value = (byteArray[0]&0x70)>>>4;

            document.getElementById('macros__hdr10mode').value = byteArray[1];

            document.getElementById('macro__customcb1').checked =  byteArray[2]&0x01;
            document.getElementById('macro__customcb2').checked = (byteArray[2]&0x02)>>1;
            document.getElementById('macro__customcb3').checked = (byteArray[2]&0x04)>>2;
            document.getElementById('macro__customcb4').checked = (byteArray[2]&0x08)>>3;
            document.getElementById('macro__customcb5').checked = (byteArray[2]&0x10)>>4;
            document.getElementById('macro__customcb6').checked = (byteArray[2]&0x20)>>5;
            document.getElementById('macro__customcb7').checked = (byteArray[2]&0x40)>>6;
    
            document.getElementById('macros__custommode1').value = byteArray[3];
            document.getElementById('macros__sb1').value = (byteArray[5]<<8) | byteArray[4];
            document.getElementById('macros__customaction1').value = byteArray[6];

            document.getElementById('macros__custommode2').value = byteArray[7];
            document.getElementById('macros__sb2').value = (byteArray[9]<<8) | byteArray[8];
            document.getElementById('macros__customaction2').value = byteArray[10];
       
            document.getElementById('macros__custommode3').value = byteArray[11];
            document.getElementById('macros__sb3').value = (byteArray[13]<<8) | byteArray[12];
            document.getElementById('macros__customaction3').value = byteArray[14];
 
            document.getElementById('macros__custommode4').value = byteArray[15];
            document.getElementById('macros__sb4').value = (byteArray[17]<<8) | byteArray[16];
            document.getElementById('macros__customaction4').value = byteArray[18];
 
            document.getElementById('macros__custommode5').value = byteArray[19];
            document.getElementById('macros__sb5').value = (byteArray[21]<<8) | byteArray[20];
            document.getElementById('macros__customaction5').value = byteArray[22];
            
            document.getElementById('macros__custommode6').value = byteArray[23];
            document.getElementById('macros__sb6').value = (byteArray[25]<<8) | byteArray[24];
            document.getElementById('macros__customaction6').value = byteArray[26];
 
            document.getElementById('macros__customaction7').value = byteArray[27];
 
            document.getElementById('macros__syncdelay').value = byteArray[28];
            document.getElementById('macros__sdrtv').value = byteArray[29];
            document.getElementById('macros__sdrfilm').value = byteArray[30];
            document.getElementById('macros__sdrbt2020').value = byteArray[31];
            document.getElementById('macros__3d').value = byteArray[32];      
            document.getElementById('macros__xvcolor').value = byteArray[33];
            document.getElementById('macros__hlgbt709').value = byteArray[34];
            document.getElementById('macros__hlgbt2020').value = byteArray[35];
            document.getElementById('macros__hdr10main').value =  byteArray[36];
            document.getElementById('macros__hdr10optional').value = byteArray[37];
            document.getElementById('macros__hdr10bt709').value =  byteArray[38];
            document.getElementById('macros__lldv').value =  byteArray[39];

            let val=[];
            for (let i=0; i<byteArray[0x30]-1; i++) val[i] = byteArray[0x31+i].toString(16);
            document.getElementById('macros__sdrtvcustom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0x40]-1; i++) val[i] = byteArray[0x41+i].toString(16);
            document.getElementById('macros__sdrfilmcustom').value = val.join(':');
           
            val=[];
            for (let i=0; i<byteArray[0x50]-1; i++) val[i] = byteArray[0x51+i].toString(16);
            document.getElementById('macros__sdrbt2020custom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0x60]-1; i++) val[i] = byteArray[0x61+i].toString(16);
            document.getElementById('macros__3dcustom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0x70]-1; i++) val[i] = byteArray[0x71+i].toString(16);
            document.getElementById('macros__xvcolorcustom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0x80]-1; i++) val[i] = byteArray[0x81+i].toString(16);
            document.getElementById('macros__hlgbt709custom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0x90]-1; i++) val[i] = byteArray[0x91+i].toString(16);
            document.getElementById('macros__hlgbt2020custom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0xa0]-1; i++) val[i] = byteArray[0xa1+i].toString(16);
            document.getElementById('macros__hdr10maincustom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0xb0]-1; i++) val[i] = byteArray[0xb1+i].toString(16);
            document.getElementById('macros__hdr10optionalcustom').value = val.join(':');

            val=[];
            for (let i=0; i<byteArray[0xc0]-1; i++) val[i] = byteArray[0xc1+i].toString(16);
            document.getElementById('macros__hdr10bt709custom').value = val.join(':');
            
            val=[];
            for (let i=0; i<byteArray[0xd0]-1; i++) val[i] = byteArray[0xd1+i].toString(16);
            document.getElementById('macros__lldvcustom').value = val.join(':');

            document.getElementById('status').innerHTML = 'MACROS STATUS OK';
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = 'CONNECTION FAILED';
            }
        }
    };
    xhttp.open("GET", url + "/ssi/jvcpage.ssi", true);
    xhttp.responseType = "arraybuffer";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    xhttp.send();
}


function requestConfigPageInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {

             if (this.response.autosw === '1') {
                document.getElementById("autosw_on").checked = true;
                document.getElementById("autosw_off").checked = false;
            } else {
                document.getElementById("autosw_on").checked = false;
                document.getElementById("autosw_off").checked = true;
            }
       


            if (this.response.enclevel === '0') {
                document.getElementById("encode_auto").checked = true;
                document.getElementById("encode_14").checked = false;
            } else if (this.response.enclevel === '1') {
                document.getElementById("encode_auto").checked = false;
                document.getElementById("encode_14").checked = true;
            }
            if (this.response.htpcmode0 === '1') {
                document.getElementById("htpcmode0_on").checked = true;
                document.getElementById("htpcmode0_off").checked = false;
            } else {
                document.getElementById("htpcmode0_on").checked = false;
                document.getElementById("htpcmode0_off").checked = true;
            }                      
            if (this.response.htpcmode1 === '1') {
                document.getElementById("htpcmode1_on").checked = true;
                document.getElementById("htpcmode1_off").checked = false;
            } else {
                document.getElementById("htpcmode1_on").checked = false;
                document.getElementById("htpcmode1_off").checked = true;
            } 
            if (this.response.htpcmode2 === '1') {
                document.getElementById("htpcmode2_on").checked = true;
                document.getElementById("htpcmode2_off").checked = false;
            } else {
                document.getElementById("htpcmode2_on").checked = false;
                document.getElementById("htpcmode2_off").checked = true;
            } 
            if (this.response.htpcmode3 === '1') {
                document.getElementById("htpcmode3_on").checked = true;
                document.getElementById("htpcmode3_off").checked = false;
            } else {
                document.getElementById("htpcmode3_on").checked = false;
                document.getElementById("htpcmode3_off").checked = true;
            } 
                  
            if (this.response.oled === '1') {
                document.getElementById("oled_on").checked = true;
                document.getElementById("oled_off").checked = false;
            } else {
                document.getElementById("oled_on").checked = false;
                document.getElementById("oled_off").checked = true;
            }
                
            document.getElementById('oledpage').value = this.response.oledpage;

            document.getElementById("unmutecnt").value = this.response.unmutecnt;
            document.getElementById("earcunmutecnt").value = this.response.earcunmutecnt;

            document.getElementById("fadevaluetext").value = this.response.oledfade;
            document.getElementById("reboottext").value = this.response.reboottimer;
           
            if (this.response.audiooutreso === '0') {
                document.getElementById("audiooutreso_720").checked = false;
                document.getElementById("audiooutreso_1080").checked = true;
            } else if (this.response.audiooutreso === '1') {
                document.getElementById("audiooutreso_720").checked = true;
                document.getElementById("audiooutreso_1080").checked = false;
            }

            if (this.response.mutetx0 === '1') {
                document.getElementById("mutetx0audio_on").checked = true;
                document.getElementById("mutetx0audio_off").checked = false;
            } else {
                document.getElementById("mutetx0audio_on").checked = false;
                document.getElementById("mutetx0audio_off").checked = true;
            }
            if (this.response.mutetx1 === '1') {
                document.getElementById("mutetx1audio_on").checked = true;
                document.getElementById("mutetx1audio_off").checked = false;
            } else {
                document.getElementById("mutetx1audio_on").checked = false;
                document.getElementById("mutetx1audio_off").checked = true;
            }

            if (this.response.iractive === '1') {
                document.getElementById("iractive_on").checked = true;
                document.getElementById("iractive_off").checked = false;
            } else {
                document.getElementById("iractive_on").checked = false;
                document.getElementById("iractive_off").checked = true;
            }

            if (this.response.ircodeset === '1') {
                document.getElementById("ircode_1").checked = true;
                document.getElementById("ircode_2").checked = false;
                document.getElementById("ircode_3").checked = false;
            } else if (this.response.ircodeset === '2') {
                document.getElementById("ircode_1").checked = false;
                document.getElementById("ircode_2").checked = true;
                document.getElementById("ircode_3").checked = false;
            } else if (this.response.ircodeset === '3') {
                document.getElementById("ircode_1").checked = false;
                document.getElementById("ircode_2").checked = false;
                document.getElementById("ircode_3").checked = true;
            } 
/*
            if (this.response.tx0plus5 === '1') {
                document.getElementById("tx0plus5_on").checked = true;
                document.getElementById("tx0plus5_off").checked = false;
            } else {
                document.getElementById("tx0plus5_on").checked = false;
                document.getElementById("tx0plus5_off").checked = true;
            }
*/
            if (this.response.tx1plus5 === '1') {
                document.getElementById("tx1plus5_on").checked = true;
                document.getElementById("tx1plus5_off").checked = false;
            } else {
                document.getElementById("tx1plus5_on").checked = false;
                document.getElementById("tx1plus5_off").checked = true;
            }      

            if (this.response.relay === '1') {
                document.getElementById("relay_on").checked = true;
                document.getElementById("relay_off").checked = false;
            } else {
                document.getElementById("relay_on").checked = false;
                document.getElementById("relay_off").checked = true;
            }  

            document.getElementById("audiovolume").value = this.response.analogvolume;
            document.getElementById("audiobass").value = this.response.analogbass;
            document.getElementById("audiotreb").value = this.response.analogtreble;

            if (this.response.dhcp === '1') {
                document.getElementById("dhcp_on").checked = true;
                document.getElementById("dhcp_off").checked = false;
            } else {
                document.getElementById("dhcp_on").checked = false;
                document.getElementById("dhcp_off").checked = true;
            }
            if (this.response.mdns === '1') {
                document.getElementById("mdns_on").checked = true;
                document.getElementById("mdns_off").checked = false;
            } else {
                document.getElementById("mdns_on").checked = false;
                document.getElementById("mdns_off").checked = true;
            }
            if (this.response.ipinterrupt === '1') {
                document.getElementById("ipinterrupt_on").checked = true;
                document.getElementById("ipinterrupt_off").checked = false;
            } else {
                document.getElementById("ipinterrupt_on").checked = false;
                document.getElementById("ipinterrupt_off").checked = true;
            }            
            document.getElementById("macaddr").value = this.response.macaddr;      
            
            let ipInfo;
            ipInfo = this.response.activeip.slice(9).split('/');
            document.getElementById("ipaddr").value = ipInfo[0];
            document.getElementById("ipmask").value = ipInfo[1];
            document.getElementById("ipgw").value = ipInfo[2];

            ipInfo = this.response.staticip.slice(9).split('/');
            document.getElementById("ipsaddr").value = ipInfo[0];
            document.getElementById("ipsmask").value = ipInfo[1];
            document.getElementById("ipsgw").value = ipInfo[2];
            
            document.getElementById("tcpport").value = this.response.tcpport;
            
            if (document.getElementById("dhcp_on").checked)
            {
                document.getElementById("setipbtn").disabled = true;
                document.getElementById("ipsaddr").disabled = true;
                document.getElementById("ipsmask").disabled = true;
                document.getElementById("ipsgw").disabled = true;
            }
            else
            {
                document.getElementById("setipbtn").disabled = false;
                document.getElementById("ipsaddr").disabled = false;
                document.getElementById("ipsmask").disabled = false;
                document.getElementById("ipsgw").disabled = false;
            }
        

            document.getElementById('status').innerHTML = 'CONFIG STATUS OK';

        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.response;
            }
        }
    };
    xhttp.open("GET", url + "/ssi/confpage.ssi", true);
    xhttp.responseType = "json";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();

}


function requestRTOSInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onload  = function () {    
        document.getElementById('status').innerHTML = this.response;     
    };
    const link = url + "/ssi/rtosinfo.ssi";
    xhttp.open("GET", link, true);
    xhttp.responseType = "text";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();
}


function requestCFSInfo() {
    var xhttp = new XMLHttpRequest();
    xhttp.onload = function () {       
        document.getElementById('status').innerHTML = this.response;     
    };
 
    xhttp.open("GET", url + "/ssi/cfslog.ssi", true);
    xhttp.responseType = "text";
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;  
    xhttp.send();
}


