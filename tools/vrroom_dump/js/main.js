const PRODUCTSTR = 'VRROOM';
var url = "";

//var url = "http://192.168.1.121";


function download(data, filename, type) {
    var file = new Blob([data], { type: type });
    if (window.navigator.msSaveOrOpenBlob) // IE10+
        window.navigator.msSaveOrOpenBlob(file, filename);
    else { 
        let a = document.createElement("a"),
            urlfile = URL.createObjectURL(file);
        a.href = urlfile;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        setTimeout(function () {
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
        }, 0);
    }
}

const statusMsgTimeout = () => {
    setTimeout(() => {
        document.getElementById('status').innerHTML = 'READY';
    }, 3000);
};

function SetActiveValues(name, value) {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            document.getElementById('status').innerHTML = 'COMMAND SENT OK';
            statusMsgTimeout();
        } else {
            if (this.readyState === 4 && this.status === 404) {
                document.getElementById('status').innerHTML = this.responseText;
            }
        }
    };
    xhttp.open("GET", url + "/cmd?" + name + "=" + value);
    xhttp.timeout = timeoutSecs;
    xhttp.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    xhttp.send();
}

async function uploadFw(event) {
    event.preventDefault();

    let uploadStart, uploadStart2, uploadStart3, uploadStart4, uploadStart5;

    var formElement = document.querySelector(".uploadfw__form");
    var formData = new FormData(formElement);

    uploadStart = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        document.getElementById('uploadfw__btn').disabled = true;
        document.getElementById('status').innerHTML = 'FW UPLOAD STARTED - WAIT UNTIL COMPLETED';

        xhr.open("POST", url+"/uploadfw", true);
        xhr.responseType = "text";
        xhr.onload = function(e) {
          resolve(xhr);
        };
        xhr.send(formData);
     }); 

     if (uploadStart.response === "ERR")
     {
        document.getElementById('uploadfw__btn').disabled = true;
        document.getElementById('status').innerHTML = 'FW UPLOAD STARTED - WAIT 2 COMPLETED';

        uploadStart2 = await new Promise(resolve => {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", url+"/uploadfw", true);
            xhr.responseType = "text";
            xhr.onload = function(e) {
                resolve(xhr);
            };

            xhr.send(formData);
        }); 
 
     } 
     else
     {
        document.getElementById('status').innerHTML = 'FW UPLOAD OK - WAIT FOR REBOOT AND THEN REFRESH BROWSER';
        document.getElementById('uploadfw__btn').disabled = false;
        return;
     }


    if (uploadStart2.response === "ERR")
    {
        document.getElementById('uploadfw__btn').disabled = true;
        document.getElementById('status').innerHTML = 'FW UPLOAD STARTED - WAIT 3 COMPLETED';

        uploadStart3 = await new Promise(resolve => {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", url+"/uploadfw", true); 
            xhr.responseType = "text";
            xhr.onload = function(e) {
            resolve(xhr);
            };

            xhr.send(formData);
        }); 

    }
    else
    {
       document.getElementById('status').innerHTML = 'FW UPLOAD OK - WAIT FOR REBOOT AND THEN REFRESH BROWSER';
       document.getElementById('uploadfw__btn').disabled = false;
       return;
    }

    if (uploadStart3.response === "ERR")
    {
        document.getElementById('uploadfw__btn').disabled = true;
        document.getElementById('status').innerHTML = 'FW UPLOAD STARTED - WAIT 4 COMPLETED';
        uploadStart4 = await new Promise(resolve => {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", url+"/uploadfw", true); 
            xhr.responseType = "text";
            xhr.onload = function(e) {
            resolve(xhr);
            };

            xhr.send(formData);
        }); 

    }
    else
    {
        document.getElementById('status').innerHTML = 'FW UPLOAD OK - WAIT FOR REBOOT AND THEN REFRESH BROWSER';
        document.getElementById('uploadfw__btn').disabled = false;
        return;
    }

    if (uploadStart4.response === "ERR")
    {
        document.getElementById('uploadfw__btn').disabled = true;
        document.getElementById('status').innerHTML = 'FW UPLOAD STARTED - WAIT 5 COMPLETED';

        uploadStart5 = await new Promise(resolve => {
            var xhr = new XMLHttpRequest();
            xhr.open("POST", url+"/uploadfw", true);
            xhr.responseType = "text";
            xhr.onload = function(e) {
            resolve(xhr);
            };

            xhr.send(formData);
        }); 

    }
    else
    {
       document.getElementById('status').innerHTML = 'FW UPLOAD OK - WAIT FOR REBOOT AND THEN REFRESH BROWSER';
       document.getElementById('uploadfw__btn').disabled = false;
       return;
    }

    if (uploadStart5.response === "ERR")
    {
        document.getElementById('uploadfw__btn').disabled = false;
        document.getElementById('status').innerHTML = 'FW UPLOAD FAILED - VRROOM BUSY/INCORRECT FW - SEND AGAIN';
    }
}



function uploadEdid(event) {
    event.preventDefault();

    var formElement = document.querySelector(".uploadedid__form");
    var formData = new FormData(formElement);
  
    var xhttp = new XMLHttpRequest();
    xhttp.onload = function () {
        if (this.readyState === 4 && this.response === "OK") 
        {
            document.getElementById('status').innerHTML = 'EDID UPLOAD OK';
        } 


    };

    xhttp.open("POST", "/uploadedid", true); 
    xhttp.responseType = "text";
    xhttp.send(formData);
}


function setScalerParam(event) {
    event.preventDefault();

    const x = document.getElementById('setscalerform');
  
    let buffer = new ArrayBuffer(40);
    var WriteBuf = new Uint8Array(buffer);

    WriteBuf[0] = document.getElementById('port0scale__cb4k60').checked;
    WriteBuf[1] = 0; 
    WriteBuf[2] = document.getElementById('port0scale__cmb4k60cs').value;
    WriteBuf[3] = document.getElementById('port0scale__cmb4k60dc').value;
    
    WriteBuf[4] = document.getElementById('port0scale__cb4k30').checked;
    WriteBuf[5] = 0; 
    WriteBuf[6] = document.getElementById('port0scale__cmb4k30cs').value;
    WriteBuf[7] = document.getElementById('port0scale__cmb4k30dc').value;

    WriteBuf[8] = document.getElementById('port0scale__cb1080p60').checked;
    WriteBuf[9] = document.getElementById('port0scale__cmb1080p60scale').value;
    WriteBuf[10] = document.getElementById('port0scale__cmb1080p60cs').value;
    WriteBuf[11] = document.getElementById('port0scale__cmb1080p60dc').value;

    WriteBuf[12] = document.getElementById('port0scale__cb1080p30').checked;
    WriteBuf[13] = document.getElementById('port0scale__cmb1080p30scale').value;
    WriteBuf[14] = document.getElementById('port0scale__cmb1080p30cs').value;
    WriteBuf[15] = document.getElementById('port0scale__cmb1080p30dc').value;

    WriteBuf[16] = document.getElementById('port1scale__cb4k60').checked;
    WriteBuf[17] = document.getElementById('port1scale__cmb4k60scale').value; 
    WriteBuf[18] = document.getElementById('port1scale__cmb4k60cs').value;
    WriteBuf[19] = document.getElementById('port1scale__cmb4k60dc').value;

    WriteBuf[20] = document.getElementById('port1scale__cb4k30').checked;
    WriteBuf[21] = document.getElementById('port1scale__cmb4k30scale').value; 
    WriteBuf[22] = document.getElementById('port1scale__cmb4k30cs').value;
    WriteBuf[23] = document.getElementById('port1scale__cmb4k30dc').value;

    WriteBuf[24] = document.getElementById('port1scale__cb1080p60').checked;
    WriteBuf[25] = 0;
    WriteBuf[26] = document.getElementById('port1scale__cmb1080p60cs').value;
    WriteBuf[27] = document.getElementById('port1scale__cmb1080p60dc').value;

    WriteBuf[28] = document.getElementById('port1scale__cb1080p30').checked;
    WriteBuf[29] = 0;
    WriteBuf[30] = document.getElementById('port1scale__cmb1080p30cs').value;
    WriteBuf[31] = document.getElementById('port1scale__cmb1080p30dc').value;

    var xhttp = new XMLHttpRequest();
    xhttp.onload = function () {
        document.getElementById('status').innerHTML = this.response;
    };

    xhttp.open("POST", url+"/scalerparam", true); 
    xhttp.responseType = "text"; 
    xhttp.send(WriteBuf);
}



function setOsdParam(event) {
    event.preventDefault();
  
    let buffer = new ArrayBuffer(40);
    var WriteBuf = new Uint8Array(buffer);

    const osdLocX = document.getElementById('osd_x').value;
    const osdLocY = document.getElementById('osd_y').value;
    const osdMask1X0 = document.getElementById('osdmask_x').value;
    const osdMask1Y0 = document.getElementById('osdmask_y').value;
    const osdMask1X1 = document.getElementById('osdmask_x1').value;
    const osdMask1Y1 = document.getElementById('osdmask_y1').value;

    const cbOsdCapSpd = document.getElementById('osd__item--source').checked;
    const cbOsdCapVid = document.getElementById('osd__item--videofield').checked;
    const cbOsdCapVidIf = document.getElementById('osd__item--videoif').checked;
    const cbOsdCapHdr = document.getElementById('osd__item--hdrrxtx').checked;
    const cbOsdCapHdrIf = document.getElementById('osd__item--hdrif').checked;
    const cbOsdCapAud = document.getElementById('osd__item--audiofield').checked;
    const cbOsdCapHdrSource = document.getElementById('osd__item--rxhdrinfo').checked;
    const cbOsdCapVideoRXTX = document.getElementById('osd__item--rxvideoinfo').checked;
    const cbOsdIgnoreMetadata = document.getElementById('osd__item--ignoremeta').checked;
       
    WriteBuf[0] = document.getElementById('osdenable').checked;
    WriteBuf[1] = document.getElementById('osdmaskenable').checked
    
    WriteBuf[2] = document.getElementById('osd__mask--level').value;
    WriteBuf[3] = document.getElementById('osd__color--fg').value;
    WriteBuf[4] = document.getElementById('osd__color--bg').value;
    WriteBuf[5] = document.getElementById('osd__color--level').value;

    WriteBuf[6] = cbOsdCapSpd |
           (cbOsdCapVid << 1) |
           (cbOsdCapVidIf << 2) |
           (cbOsdCapHdr << 3) |
           (cbOsdCapHdrIf << 4) |
           (cbOsdCapAud << 5) |
           (cbOsdCapHdrSource << 6) |
           (cbOsdCapVideoRXTX << 7); 

    WriteBuf[7] = document.getElementById('osd__fade').value;
    WriteBuf[8] = document.getElementById('osd__mask--gs').value;
    WriteBuf[10] = document.getElementById('osd__fadeinfo').value;

    WriteBuf[9] = cbOsdIgnoreMetadata;

    WriteBuf[16] = (osdLocX >>> 8) & 255;
    WriteBuf[17] = osdLocX & 255;
    WriteBuf[18] = (osdLocY >>> 8) & 255;
    WriteBuf[19] = osdLocY & 255;

    WriteBuf[20] = (osdMask1X0 >>> 8) & 255;
    WriteBuf[21] = osdMask1X0 & 255;
    WriteBuf[22] = (osdMask1Y0 >>> 8) & 255;
    WriteBuf[23] = osdMask1Y0 & 255;

    WriteBuf[24] = (osdMask1X1 >>> 8) & 255;
    WriteBuf[25] = osdMask1X1 & 255;
    WriteBuf[26] = (osdMask1Y1 >>> 8) & 255;
    WriteBuf[27] = osdMask1Y1 & 255;
    
    var xhttp = new XMLHttpRequest();
    xhttp.onload = function () {
        document.getElementById('status').innerHTML = this.response;

    };

    xhttp.open("POST", url+"/osdparam", true); 
    xhttp.responseType = "text";  
    xhttp.send(WriteBuf);
}

function setOsdText(event) {
    event.preventDefault();

    let buffer = new ArrayBuffer(50);
    let WriteBuf = new Uint8Array(buffer);

    const customText = document.getElementById('osdtextcontent').value;
    const customTextLen = customText.length;
    const osdLocX = document.getElementById('osdtext_x').value;
    const osdLocY = document.getElementById('osdtext_y').value;
    const cbOsdCustomText = document.getElementById('cbosdtext').checked;
    const cbOsdCustomTextFade = document.getElementById('cbosdtextneverfade').checked;

    WriteBuf[0] = customTextLen;
    WriteBuf[1] = cbOsdCustomText | (cbOsdCustomTextFade << 1);
    WriteBuf[2] = (osdLocX >>> 8) & 255;
    WriteBuf[3] = osdLocX & 255;
    WriteBuf[4] = (osdLocY >>> 8) & 255;
    WriteBuf[5] = osdLocY & 255;

    for (let i=0; i<customTextLen; i++) 
        WriteBuf[i+6] = customText.charCodeAt(i);

    var xhttp = new XMLHttpRequest();
    xhttp.onload = function () {
        document.getElementById('status').innerHTML = this.response;
    };

    xhttp.open("POST", url+"/osdtext", true); 
    xhttp.responseType = "text";  
    xhttp.send(WriteBuf);
}


function setMacrosParam(event) {
    event.preventDefault();

    let buffer = new ArrayBuffer(230);
    var WriteBuf = new Uint8Array(buffer);

    const modeSdrtv = document.getElementById('macros__sdrtv').value;
    const modeSdrfilm = document.getElementById('macros__sdrfilm').value;
    const modeSdrbt2020 = document.getElementById('macros__sdrbt2020').value;
    const mode3d = document.getElementById('macros__3d').value;
    const modeXvcolor = document.getElementById('macros__xvcolor').value;
    const modeHlgbt709 = document.getElementById('macros__hlgbt709').value;
    const modeHlgbt2020 = document.getElementById('macros__hlgbt2020').value;
    const modeHdr10main = document.getElementById('macros__hdr10main').value;
    const modeHdr10optional = document.getElementById('macros__hdr10optional').value;
    const modeHdr10bt709 = document.getElementById('macros__hdr10bt709').value;
    const modeLldv = document.getElementById('macros__lldv').value;
    
    const syncDelay = document.getElementById('macros__syncdelay').value;
    const hdr10Mode = document.getElementById('macros__hdr10mode').value;
    const macroEnable = document.getElementById('macro__enable').checked;
    const macroEverySync = document.getElementById('macro__everysync').checked;
  
    const macroExecuteCustomCode = document.getElementById('macro__execcustomcode').checked;
    const macroSelBeforeAfter = document.getElementById('macros__selbeforeafter').value;
    const macroExecDelay = document.getElementById('macros__execdelay').value;

    const customCb1 = document.getElementById('macro__customcb1').checked;
    const customCb2 = document.getElementById('macro__customcb2').checked;
    const customCb3 = document.getElementById('macro__customcb3').checked;
    const customCb4 = document.getElementById('macro__customcb4').checked;
    const customCb5 = document.getElementById('macro__customcb5').checked;
    const customCb6 = document.getElementById('macro__customcb6').checked;
    const customCb7 = document.getElementById('macro__customcb7').checked;
  
    WriteBuf[0] = macroEnable | 
                    (macroEverySync<<1) |
                    (macroExecuteCustomCode<<2) |
                    (macroSelBeforeAfter<<3) |
                    (macroExecDelay<<4);

    WriteBuf[1] = hdr10Mode;
    
    WriteBuf[2] = customCb1 |
           (customCb2 << 1) |
           (customCb3 << 2) |
           (customCb4 << 3) |
           (customCb5 << 4) |
           (customCb6 << 5) |
           (customCb7 << 6); 


    WriteBuf[3] = document.getElementById('macros__custommode1').value;
    WriteBuf[4] = document.getElementById('macros__sb1').value & 0xff;
    WriteBuf[5] = (document.getElementById('macros__sb1').value >>> 8) & 0xff;
    WriteBuf[6] = document.getElementById('macros__customaction1').value;
  
    WriteBuf[7] = document.getElementById('macros__custommode2').value;
    WriteBuf[8] = document.getElementById('macros__sb2').value & 0xff;
    WriteBuf[9] = (document.getElementById('macros__sb2').value >>> 8) & 0xff;
    WriteBuf[10] = document.getElementById('macros__customaction2').value;

    WriteBuf[11] = document.getElementById('macros__custommode3').value;
    WriteBuf[12] = document.getElementById('macros__sb3').value & 0xff;
    WriteBuf[13] = (document.getElementById('macros__sb3').value >>> 8) & 0xff;
    WriteBuf[14] = document.getElementById('macros__customaction3').value;

    WriteBuf[15] = document.getElementById('macros__custommode4').value;
    WriteBuf[16] = document.getElementById('macros__sb4').value & 0xff;
    WriteBuf[17] = (document.getElementById('macros__sb4').value >>> 8) & 0xff;
    WriteBuf[18] = document.getElementById('macros__customaction4').value;

    WriteBuf[19] = document.getElementById('macros__custommode5').value;
    WriteBuf[20] = document.getElementById('macros__sb5').value & 0xff;
    WriteBuf[21] = (document.getElementById('macros__sb5').value >>> 8) & 0xff;
    WriteBuf[22] = document.getElementById('macros__customaction5').value;

    WriteBuf[23] = document.getElementById('macros__custommode6').value;
    WriteBuf[24] = document.getElementById('macros__sb6').value & 0xff;
    WriteBuf[25] = (document.getElementById('macros__sb6').value >>> 8) & 0xff;
    WriteBuf[26] = document.getElementById('macros__customaction6').value;

    WriteBuf[27] = document.getElementById('macros__customaction7').value;

    WriteBuf[28] = syncDelay;
    WriteBuf[29] = modeSdrtv;
    WriteBuf[30] = modeSdrfilm;
    WriteBuf[31] = modeSdrbt2020;
    WriteBuf[32] = mode3d;
    WriteBuf[33] = modeXvcolor;
    WriteBuf[34] = modeHlgbt709;
    WriteBuf[35] = modeHlgbt2020;
    WriteBuf[36] = modeHdr10main;
    WriteBuf[37] = modeHdr10optional;
    WriteBuf[38] = modeHdr10bt709;
    WriteBuf[39] = modeLldv;
    let val=[];

    val = document.getElementById('macros__sdrtvcustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0x30] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0x30+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0x30] = 1;
        WriteBuf[0x31] = 0x0a;
    }

    val = document.getElementById('macros__sdrfilmcustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0x40] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0x40+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0x40] = 1;
        WriteBuf[0x41] = 0x0a;
   }

   val = document.getElementById('macros__sdrbt2020custom').value;
   if ((val.length !== 0) && (val !== '0') )
   {
       const cmd = val.split(':').map( val => parseInt(val,16) ); 
       WriteBuf[0x50] = cmd.unshift(cmd.length);
       for (let i=1; i<16; i++)  WriteBuf[0x50+i] = cmd[i];
   } 
   else
   {
       WriteBuf[0x50] = 1;
       WriteBuf[0x51] = 0x0a;
  }

    val = document.getElementById('macros__3dcustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0x60] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0x60+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0x60] = 1;
        WriteBuf[0x61] = 0x0a;
    }

    val = document.getElementById('macros__xvcolorcustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0x70] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0x70+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0x70] = 1;
        WriteBuf[0x71] = 0x0a;
    }

    val = document.getElementById('macros__hlgbt709custom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0x80] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0x80+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0x80] = 1;
        WriteBuf[0x81] = 0x0a;
    }

    val = document.getElementById('macros__hlgbt2020custom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0x90] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0x90+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0x90] = 1;
        WriteBuf[0x91] = 0x0a;
    }


    val = document.getElementById('macros__hdr10maincustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0xa0] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0xa0+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0xa0] = 1;
        WriteBuf[0xa1] = 0x0a;
    }

    val = document.getElementById('macros__hdr10optionalcustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0xb0] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0xb0+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0xb0] = 1;
        WriteBuf[0xb1] = 0x0a;
    }

    val = document.getElementById('macros__hdr10bt709custom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0xc0] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0xc0+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0xc0] = 1;
        WriteBuf[0xc1] = 0x0a;
    }

    val = document.getElementById('macros__lldvcustom').value;
    if ((val.length !== 0) && (val !== '0') )
    {
        const cmd = val.split(':').map( val => parseInt(val,16) ); 
        WriteBuf[0xd0] = cmd.unshift(cmd.length);
        for (let i=1; i<16; i++)  WriteBuf[0xd0+i] = cmd[i];
    } 
    else
    {
        WriteBuf[0xd0] = 1;
        WriteBuf[0xd1] = 0x0a;
    }

    var xhttp = new XMLHttpRequest();
    xhttp.onload = function () {
        document.getElementById('status').innerHTML = this.response;
    };

    xhttp.open("POST", url+"/macrosparam", true);
    xhttp.responseType = "text";  
    xhttp.send(WriteBuf);
    
}

function createAviIf() {
    let aviIf = [ 0x82,0x02,0x0d,0x00,     
        0x00,0x00,0x00,				      
        0x00,                              
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 ]; 

    switch(document.getElementById('createavi__vic').value) {
        case "0": aviIf[7] = 0x10; break;
        case "1": aviIf[7] = 0x20; break;        
        case "2": aviIf[7] = 0x61; break;
        case "3": aviIf[7] = 0x5d; break;
    }

    switch(document.getElementById('createavi__csc').value) {
        case "0": aviIf[4] = 0x00; aviIf[5] = 0x08; aviIf[6] = 0x00; break;
        case "1": aviIf[4] = 0x40; aviIf[5] = 0x88; aviIf[6] = 0x00; break;        
        case "2": aviIf[4] = 0x20; aviIf[5] = 0x88; aviIf[6] = 0x00; break;
        case "3": aviIf[4] = 0x60; aviIf[5] = 0x88; aviIf[6] = 0x00; break;
        case "4": aviIf[4] = 0x00; aviIf[5] = 0xc8; aviIf[6] = 0x60; break;
        case "5": aviIf[4] = 0x40; aviIf[5] = 0xc8; aviIf[6] = 0x60; break;
        case "6": aviIf[4] = 0x20; aviIf[5] = 0xc8; aviIf[6] = 0x60; break;
        case "7": aviIf[4] = 0x60; aviIf[5] = 0xc8; aviIf[6] = 0x60; break;
    }

    switch(document.getElementById('createavi__range').value) {
        case "0": aviIf[6] &= 0xf3; break;
        case "1": aviIf[6] &= 0xf3; aviIf[6] |= 0x04; break;
        case "2": aviIf[6] &= 0xf3; aviIf[6] |= 0x08; break;
    }



    let ChkSum = 0x91; // header checksum 
       
	for (let i = 4; i<17; i++) 
        ChkSum += aviIf[i];
      
    ChkSum = 0xff - (ChkSum & 0xff) + 1;
    aviIf[3] = ChkSum;

    val=[];
    for (let i=0; i<17; i++) val[i] = ("0" + aviIf[i].toString(16)).slice(-2);
    document.getElementById('infoframe__customavi').value = val.join(':');
}

function createLldvDataBlock() 
{

    const ITU709 =    [ 0.300, 0.600, 0.150, 0.060, 0.640, 0.330, 0.3127, 0.3290 ];
	const ITUBT2020 = [ 0.170, 0.797, 0.131, 0.046, 0.708, 0.292, 0.3127, 0.3290 ];
    const DCIP3 =     [ 0.265, 0.690, 0.150, 0.060, 0.680, 0.320, 0.3127, 0.3290 ];
    
    let lldvIf = [ 0x81,0x01, 
        0x1b,   
        0x00,   
        0x46,0xd0,0x00,				
        0x03,   
        0x00,   
        0x00,  
        0x00,  
        0x00,  
        0x00,   
        0x00, 0x00, 0x00, 0x00,   
        0x00, 0x00, 0x00, 0x00];

    let lldvDataBlock = [ 
            0xEB,0x01,          
            0x46,0xd0,0x00,		
            0x48,   
            0x03,  
            0x74, 
            0x82,   
            0x5e, 
            0x6d,  
            0x95]; 

    const dvVersion = 2;
    const dvDmVersion = 1;
    const backLightCtrl = document.getElementById('createdv__backlight').value;
    const backLightMinLuma = document.getElementById('createdv__backlightluma').value;
    const lldv1012b = document.getElementById('createdv__lldv1012b').value;   
    const supportYuv422 = document.getElementById('createdv__yuv422').value;
    const supportGlobalDim = document.getElementById('createdv__gdim').value;
    const interface = document.getElementById('createdv__interface').value;
    const minLuma = document.getElementById('createdv__minlum').value;
    const maxLuma = document.getElementById('createdv__maxlum').value;
    
    maxLumaNits =  calcInvEotf(maxLuma/10000) * 4095; 
    maxLumaNits5bit = Math.round((maxLumaNits - 2055) / 65);

    minLumaNits =  calcInvEotf(minLuma/10000) * 4095; 
    minLumaNits5bit = Math.round(minLumaNits / 20);

    const Rx = document.getElementById('createdv__rx').value * 256;
    const Ry = document.getElementById('createdv__ry').value * 256;
    const Gx = document.getElementById('createdv__gx').value * 256;
    const Gy = document.getElementById('createdv__gy').value * 256;
    const Bx = document.getElementById('createdv__bx').value * 256;
    const By = document.getElementById('createdv__by').value * 256;

    lldvDataBlock[0] = 0xeb; 
    lldvDataBlock[1] = 0x01; 
    lldvDataBlock[2] = 0x46; 
    lldvDataBlock[3] = 0xd0; 
    lldvDataBlock[4] = 0x00; 

    lldvDataBlock[5] = (dvVersion << 5) | (dvDmVersion << 2) | (backLightCtrl << 1) | supportYuv422;
    lldvDataBlock[6] = (minLumaNits5bit << 3) | (supportGlobalDim << 2) | backLightMinLuma;
    lldvDataBlock[7] = (maxLumaNits5bit << 3) | interface;
 
    lldvDataBlock[8] = (Gx << 1) | ((lldv1012b & 0x03) >> 1) ; 
    lldvDataBlock[9] = ((Gy - 0x80) << 1) | (lldv1012b & 0x01) ;
    lldvDataBlock[10] = ((Rx - 0xA0) << 3) | (Bx - 0x20); 
    lldvDataBlock[11] = ((Ry - 0x40) << 3) | (By - 0x08); 
 
    val=[];
    for (let i=0; i<12; i++) val[i] = ("0" + lldvDataBlock[i].toString(16)).slice(-2).toUpperCase();
    document.getElementById('infoframe__lldvdatablock').value = val.join(':');

}

const setDefaultLldvDataBlock = () => {
    let cmd1 = "EB:01:46:D0:00:48:03:74:82:5e:6d:95";
    const cmd = document.getElementById('infoframe__lldvdatablock').value = cmd1;
};

const setCustomLldvDataBlock = () => {
    const cmd = document.getElementById('infoframe__lldvdatablock').value;
   
    if (cmd.length > 77) 
        document.getElementById('status').innerHTML = 'DV STRING TOO LONG - 26 BYTE MAX';
    else {
        document.getElementById('status').innerHTML = 'DV SENT';
        SetActiveValues('dvstring', cmd);
    }  
};

const setDvCreatePrimaries = () => {
 
    const ITU709 =    [ 0.300, 0.600, 0.150, 0.060, 0.640, 0.330, 0.3127, 0.3290 ];
	const ITUBT2020 = [ 0.170, 0.797, 0.131, 0.046, 0.708, 0.292, 0.3127, 0.3290 ];
    const DCIP3 =     [ 0.265, 0.690, 0.150, 0.060, 0.680, 0.320, 0.3127, 0.3290 ];
  
    switch(document.getElementById('createdv__primaries').value) {
        case "0":   
            document.getElementById('createdv__gx').value = ITU709[0].toFixed(3);
            document.getElementById('createdv__gy').value = ITU709[1].toFixed(3);
            document.getElementById('createdv__bx').value = ITU709[2].toFixed(3);
            document.getElementById('createdv__by').value = ITU709[3].toFixed(3);
            document.getElementById('createdv__rx').value = ITU709[4].toFixed(3);
            document.getElementById('createdv__ry').value = ITU709[5].toFixed(3);  
        break;
        case "1":   
            document.getElementById('createdv__gx').value = ITUBT2020[0].toFixed(3);
            document.getElementById('createdv__gy').value = ITUBT2020[1].toFixed(3);
            document.getElementById('createdv__bx').value = ITUBT2020[2].toFixed(3);
            document.getElementById('createdv__by').value = ITUBT2020[3].toFixed(3);
            document.getElementById('createdv__rx').value = ITUBT2020[4].toFixed(3);
            document.getElementById('createdv__ry').value = ITUBT2020[5].toFixed(3);  
        break;
        case "2":   
            document.getElementById('createdv__gx').value = DCIP3[0].toFixed(3);
            document.getElementById('createdv__gy').value = DCIP3[1].toFixed(3);
            document.getElementById('createdv__bx').value = DCIP3[2].toFixed(3);
            document.getElementById('createdv__by').value = DCIP3[3].toFixed(3);
            document.getElementById('createdv__rx').value = DCIP3[4].toFixed(3);
            document.getElementById('createdv__ry').value = DCIP3[5].toFixed(3);  
        break;

    }
 
}


function createHdrIf() {

    const ITU709 =    [ 0.300001, 0.600001, 0.150001, 0.060001, 0.640, 0.330, 0.3127001, 0.3290 ];
	const ITUBT2020 = [ 0.170001, 0.797001, 0.131, 0.046001, 0.708, 0.292001, 0.3127001, 0.3290 ];
    const DCIP3 =     [ 0.265001, 0.690001, 0.150001, 0.060001, 0.680, 0.320, 0.3127001, 0.3290 ];
    
    let hdrIf = [ 0x87,0x01,0x1a,0x00,    /* header */
        0x00,0x00,				/* 0x04 - eotf */
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,  /* 0x06 - primaries */
        0x00,0x00, 0x00,0x00,	/* 0x12 - white point*/
        0x00,0x00, 0x00,0x00,	/* 0x16 - max/min luminance*/
        0x00,0x00,  /* 0x1a - maxcll*/
        0x00,0x00];	/* 0x1c - maxfall*/  // 30 items


    switch(document.getElementById('createhdr__eotf').value) {
        case "0": hdrIf[4] = 0; break;
        case "1": hdrIf[4] = 1; break;        
        case "2": hdrIf[4] = 2; break;
        case "3": hdrIf[4] = 3; break;
    }
    const hdrPrim = document.getElementById('createhdr__primaries').value;

    if (hdrPrim === "0") {
		for (let i = 0; i < 8; i++) {
			hdrIf[0x6 + i * 2] = (ITU709[i] * 50000) & 0xff;
			hdrIf[0x7 + i * 2] = ((ITU709[i] * 50000) >> 8) & 0xff;
		}
	}
	else if (hdrPrim === "1") {
		for (let i = 0; i < 8; i++) {
			hdrIf[0x6 + i * 2] = (ITUBT2020[i] * 50000) & 0xff;
			hdrIf[0x7 + i * 2] = ((ITUBT2020[i] * 50000) >> 8) & 0xff;
		}
	}
	else if (hdrPrim === "2") {
		for (let i = 0; i < 8; i++) {
			hdrIf[0x6 + i * 2] = (DCIP3[i] * 50000) & 0xff;
			hdrIf[0x7 + i * 2] = ((DCIP3[i] * 50000) >> 8) & 0xff;
		}
    }
    
	hdrIf[0x16] = document.getElementById('createhdr__maxlum').value & 0xff;
	hdrIf[0x17] = (document.getElementById('createhdr__maxlum').value >>> 8) & 0xff;

	hdrIf[0x18] = (document.getElementById('createhdr__minlum').value * 10000 ) & 0xff;
	hdrIf[0x19] = ((document.getElementById('createhdr__minlum').value * 10000 ) >>> 8) & 0xff;

	hdrIf[0x1a] = document.getElementById('createhdr__maxcll').value & 0xff;
	hdrIf[0x1b] = (document.getElementById('createhdr__maxcll').value >>> 8) & 0xff;

	hdrIf[0x1c] = document.getElementById('createhdr__maxfall').value & 0xff;
	hdrIf[0x1d] = (document.getElementById('createhdr__maxfall').value >> 8) & 0xff;

    let ChkSum = 0xA2;

	for (let i = 4; i<30; i++) 
        ChkSum += hdrIf[i];

    ChkSum = 0xff - (ChkSum & 0xff) + 1;
    hdrIf[3] = ChkSum;

    val=[];
    for (let i=0; i<30; i++) val[i] = ("0" + hdrIf[i].toString(16)).slice(-2);
    document.getElementById('infoframe__customhdr').value = val.join(':');

}

function SetData(name, value) {
    document.getElementById('status').innerHTML = "name = " + name + "value = " + value;
}

function GetDate() {
    document.getElementById('status').innerHTML = Date();
}

const setOledFade = () => {
    const val = document.getElementById('fadevaluetext').value;
    SetActiveValues('oledfade', val);
};

const setUnmuteCnt = () => {
    const val = document.getElementById('unmutecnt').value;
    SetActiveValues('unmutecnt', val);
};

const setEarcUnmuteCnt = () => {
    const val = document.getElementById('earcunmutecnt').value;
    SetActiveValues('earcunmutecnt', val);
};

const setReboot = () => {
    const val = document.getElementById('reboottext').value;
    SetActiveValues('reboottimer', val);
};

const setCustomHdr = () => {
    const cmd = document.getElementById('infoframe__customhdr').value;
    SetActiveValues('hdrasc', cmd);
};

const setDefaultHdrIf = () => {
 
    let cmd1 = "87:01:1a:b0:02:00:c2:33:c4:86:4c";
    let cmd2 = ":1d:b8:0b:d0:84:80:3e:13:3d:42:40:a0:0f:32:00:e8:03:90:01:00";

    const cmd = document.getElementById('infoframe__customhdr').value = cmd1 + cmd2;
};

const setDefaultVsiIf = () => {
 
    let cmd1 = "81:01:1b:63:00:00:00:00:00:00:00";
    let cmd2 = ":00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00";

    const cmd = document.getElementById('infoframe__customvsi').value = cmd1 + cmd2;
};


const setCustomAvi = () => {
    const cmd = document.getElementById('infoframe__customavi').value;
    SetActiveValues('aviasc', cmd);
};

const setCustomVsi= () => {
    const cmd = document.getElementById('infoframe__customvsi').value;
    SetActiveValues('vsiasc', cmd);
};

const sendCec = () => {
    const cmd = document.getElementById('ceccmd').value;
    SetActiveValues('cecasc', cmd);
};

const sendCecCmd = (x) => {
    SetActiveValues('cecasc', x);
};

const setCustomCds = () => {
    const cmd = document.getElementById('cdsstring').value;

    if (cmd.length > 92) 
        document.getElementById('status').innerHTML = 'CDS STRING TOO LONG - 31 BYTE MAX';
    else {
        document.getElementById('status').innerHTML = 'CDS SENT';
        SetActiveValues('cdsstring', cmd);
    }  
};

const setDefaultCds = () => {
    
    let defaultCds = [
        0x01, 

        0x01, 
        0x1a, 

        0x35, 
        0x67, 0x7E, 0x01, // MAT / ATMOS
        0x57, 0x06, 0x01, // DD+ and Atmos
        0x15, 0x07, 0x50, // DD  
        0x0F, 0x7F, 0x07, // 24bit LPCM 23khz - 192kHz	       
        0x3D, 0x1E, 0xC0, // AAC
        0x4D, 0x02, 0x00, // DTS	
        0x5F, 0x7E, 0x01, // DTSHD	

        0x83, 
        0x4F, 0x00, 0x00, 

        0x00,
            
    ];

    val=[];
    for (let i=0; i<30; i++) val[i] = ("0" + defaultCds[i].toString(16)).slice(-2);
    document.getElementById('cdsstring').value = val.join(':');
};

const setOpMode = (val) => {
 
    SetActiveValues('opmode', val);

    if ( (val == 0) ||  (val == 1) )
    {
        document.getElementById("portseltx1").disabled = true;
    }
    else
    {
        document.getElementById("portseltx1").disabled = false;
    }
    
};

const setDhcp = (val) => {
 
    SetActiveValues('dhcp', val);

    if (val == "on")
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
    
};


const sendRs232 = (type) => {
 
    if (type === 'ascii') 
    {
        val = encodeURIComponent(document.getElementById('rs232text--asc').value);
        SetActiveValues('rs232a', val);
    }    
    else if (type === 'binary')
    {
        val = document.getElementById('rs232text--bin').value;
        SetActiveValues('rs232b', val);
    }
    
};

const setAnalogAudio = (evt) => {
    evt.preventDefault();
    var request = new XMLHttpRequest();
    request.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            document.getElementById('status').innerHTML = 'COMMAND SENT OK';
            statusMsgTimeout();
        } else {
            if (this.readyState === 4 && this.status === 404) { 
                document.getElementById('status').innerHTML = this.responseText;
            }
        }
    };
    request.open("GET", url + "/cmd?" + "analogvolume=" + document.getElementById("audiovolume").value + "&analogbass=" + document.getElementById("audiobass").value + "&analogtreble=" + document.getElementById("audiotreb").value);
    request.timeout = timeoutSecs;
    request.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    request.send();
}


const setIpConfig = () => {
    var request = new XMLHttpRequest();
    request.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            document.getElementById('status').innerHTML = 'REBOOT VRROOM WHEN CHANGING DHCP/STATIC IP';
            statusMsgTimeout();
        } else {
            if (this.readyState === 4 && this.status === 404) { 
                document.getElementById('status').innerHTML = this.responseText;
            }
        }
    };
    let staticip = `${document.getElementById('ipsaddr').value}/${document.getElementById('ipsmask').value}/${document.getElementById('ipsgw').value}`;

    request.open("GET", url + "/cmd?" + "staticip=" +staticip);
    request.timeout = timeoutSecs;
    request.ontimeout = (e) => document.getElementById('status').innerHTML = msgTimeout;
    request.send();
}
const setTcpPort = () => {
    const val = document.getElementById('tcpport').value;
    SetActiveValues('tcpport', val);
};

const setTestPattern = () => {
    if (document.getElementById('testpattern__cb').checked) {
        const reso = document.getElementById('testpattern__reso').value;
        const pattern = document.getElementById('testpattern__pattern').value;
        const hdcp = document.getElementById('testpattern__hdcp').value;
       
        SetActiveValues('siggen', `${reso} ${pattern} ${hdcp}`);
    } else 
    {

        SetActiveValues('siggen', 'off');

    }
};


const setInputPort = () => {
    const tx0 = document.getElementById('portseltx0').value;
    const tx1 = document.getElementById('portseltx1').value;
    SetActiveValues('insel', `${tx0} ${tx1}`);
    
      document.getElementById("infoline__spd").innerHTML = 'Refresh after resync';
      document.getElementById("infoline__spd1").innerHTML = '';
      document.getElementById("infoline__rx").innerHTML = '';  
      document.getElementById("infoline__rx1").innerHTML = '';   
      document.getElementById("infoline__tx0").innerHTML = '';
      document.getElementById("infoline__tx1").innerHTML = '';
      document.getElementById("infoline__aud").innerHTML = '';       
      document.getElementById("infoline__aud1").innerHTML = '';       
      document.getElementById("infoline__audout").innerHTML = '';       
};



const factoryReset = () => {
    const answer =   document.getElementById('freset').value;
    const resetSettings =   document.getElementById('resetsettings__cb').checked;
    const resetEdid =   document.getElementById('resetedid__cb').checked;

    if (answer==='RESET') 
    {
        if ( (resetSettings==true)&&(resetEdid==true) )
            SetActiveValues('factoryreset', '3');
        else if ( (resetSettings==true)&&(resetEdid==false) )
            SetActiveValues('factoryreset', '1');
        else if ( (resetSettings==false)&&(resetEdid==true) )
            SetActiveValues('factoryreset', '2');
        else if ( (resetSettings==false)&&(resetEdid==false) ) {
            document.getElementById('freset').value = 'Select reset action above';
            return;
        }
  
        document.getElementById('freset').value = 'VRROOM will reboot...';
    } else {
        document.getElementById('freset').value = 'Type RESET here';
    }
}

const refreshTab = (evt) => {
    let currentTab;
   
    const tablinks = document.getElementsByClassName("tab__btnlinks");
    for (let i = 0; i < tablinks.length; i++) {
        if (tablinks[i].classList.contains('active'))
            currentTab = i;
    }
    switch (currentTab) {
        case 0: openTab(evt, 'routing'); break;
        case 1: openTab(evt, 'edid'); break;
        case 2: openTab(evt, 'hdr'); break;
        case 3: openTab(evt, 'dvdecode'); break;
        case 4: openTab(evt, 'osd'); break;
        case 5: openTab(evt, 'cec'); break;
        case 6: openTab(evt, 'macros'); break;
        case 7: openTab(evt, 'config'); break;
    }
    requestBoardInfo();
};

function openTab(evt, tabName) {
    var i, tabcontent, tablinks;

    tabcontent = document.getElementsByClassName("tab__content");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }
    tablinks = document.getElementsByClassName("tab__btnlinks");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
    document.getElementById(tabName).style.display = "block";
    document.getElementById('status').innerHTML = 'CONNECTING';

    switch (tabName) {
        case 'routing':
            requestActiveVideo();
            tablinks[0].className += " active"; 
            break;
        case 'edid':
            requestEdidPageInfo();
            tablinks[1].className += " active"; 
            break;
        case 'hdr':
            requestHdrPageInfo();
            tablinks[2].className += " active"; 
            break;
        case 'dvdecode':
            requestDVPageInfo();
            tablinks[3].className += " active"; 
            break;
       case 'osd':
            requestOsdPageInfo();
            tablinks[4].className += " active"; 
            break;
        case 'cec':
            requestCecPageInfo();
            tablinks[5].className += " active"; 
            break;
        case 'macros':   
            requestMacrosPageInfo();       
            tablinks[6].className += " active"; 
            break;
        case 'config':
            requestConfigPageInfo();
            tablinks[7].className += " active"; 
            break;                      
        default:
            break;
    }
}

requestBoardInfo();
openTab(undefined, 'routing');
createHdrIf();
createAviIf();

const edidtable1 = document.getElementById('edidtableinp1');
const edidtable2 = document.getElementById('edidtableinp2');
const edidtable3 = document.getElementById('edidtableinp3');

const edidtable4 = document.getElementById('edidtableinp4');
edidtable2.innerHTML = edidtable2.innerHTML + edidtable1.innerHTML;
edidtable3.innerHTML = edidtable3.innerHTML + edidtable1.innerHTML;
edidtable4.innerHTML = edidtable4.innerHTML + edidtable1.innerHTML;

const macroSelect = document.getElementById('macros__sdrtv');
document.getElementById('macros__sdrfilm').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__sdrbt2020').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__3d').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__xvcolor').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__hlgbt709').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__hlgbt2020').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__hdr10main').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__hdr10optional').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__hdr10bt709').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__lldv').innerHTML += macroSelect.innerHTML;

document.getElementById('macros__customaction1').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__customaction2').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__customaction3').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__customaction4').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__customaction5').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__customaction6').innerHTML += macroSelect.innerHTML;
document.getElementById('macros__customaction7').innerHTML += macroSelect.innerHTML;

const macroSelectCustom = document.getElementById('macros__sdrtvcustom');
document.getElementById('macros__sdrfilmcustom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__sdrbt2020custom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__3dcustom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__xvcolorcustom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__hlgbt709custom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__hlgbt2020custom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__hdr10maincustom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__hdr10optionalcustom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__hdr10bt709custom').innerHTML += macroSelectCustom.innerHTML;
document.getElementById('macros__lldvcustom').innerHTML += macroSelectCustom.innerHTML;