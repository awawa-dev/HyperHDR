"use strict";

function pad(str) {
	let val=[];
	let size = str.length;
	if(size >= 512)
	{
		size = 512;
	}
	else if(size >= 384)
	{
		size = 384;
	}
	else
	{
		size = 256;
	}
	for (let i=0; i<size; i++)
	{
		val[i] = str[i].toString(16);
		if (val[i].length < 2)
			val[i] = `0${val[i]}`
	}
	return (val.join(' '));
}

function parseCs(val) {
    switch (val)
    {
      case 35: return 'follow'; break;
      case 2: return 'rgb full'; break;
      case 6: return 'rgb 2020'; break;
      case 18: return 'YCbCr 444 709'; break;
      case 24: return 'YCbCr 444 2020'; break;
      case 9: return 'YCbCr 422 709'; break;
      case 15: return 'YCbCr 422 2020'; break;
      case 27: return 'YCbCr 420 709'; break;
      case 33: return 'YCbCr 420 2020'; break;
    }
} 

function revParseCs(val) {
  switch (val)
  {
    case 'follow': return 35; break;
    case 'rgb full': return 2; break;
    case 'rgb 2020': return 6 ; break;
    case 'YCbCr 444 709': return 18; break;
    case 'YCbCr 444 2020': return 24; break;
    case 'YCbCr 422 709': return 9; break;
    case 'YCbCr 422 2020': return 15; break;
    case 'YCbCr 420 709': return 27; break;
    case 'YCbCr 420 2020': return 33; break;
  }
} 
function parseDc(val) {
  switch (val)
  {
    case 6: return 'follow'; break;
    case 1: return '8-bit'; break;
    case 3: return '10-bit'; break;
    case 4: return '12-bit'; break;
    case 5: return '16-bit'; break;
  }
} 

function revParseDc(val) {
  switch (val)
  {
    case 'follow': return 6; break;
    case '8-bit': return 1; break;
    case '10-bit': return 3; break;
    case '12-bit': return 4; break;
    case '16-bit': return 5; break;
  }
} 

function stringToArrayBuffer(str) {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);

    for (var i=0, strLen=str.length; i<strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }

    return buf;
}
function ab2str(buf) {
    return String.fromCharCode.apply(null, new Uint8Array(buf));
  }
async function requestConfigData () {
    let headerInfo = {};

    let infoBoardGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/brdinfo.ssi", true);  
        xhr.responseType = "json";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
     }); 

    let infoVideoGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/infopage.ssi", true); 
        xhr.responseType = "json"; 
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 

    let infoEdidGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidpage.ssi", true); 
        xhr.responseType = "json"; 
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
    
    let infoSclGet = await new Promise(resolve => {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url + "/ssi/sclpage.ssi", true);  
      xhr.responseType = "arraybuffer";
      xhr.onload = function(e) {
        resolve(xhr.response);
      };
      xhr.send();
    }); 

    let infoHdrGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/hdrpage.ssi", true);  
        xhr.responseType = "json";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
  
    let infoOsdGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/osdpage.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 

    let infoCecGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/cecpage.ssi", true);  
        xhr.responseType = "json";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 

    let infoMacroGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/jvcpage.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 

    let infoConfigGet = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/confpage.ssi", true); 
        xhr.responseType = "json"; 
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
 
    let infoEdidRx0Get = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidrx0.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 

    let infoEdidRx1Get = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidrx1.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
    
    let infoEdidRx2Get = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidrx2.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
    
    let infoEdidRx3Get = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidrx3.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
    
    let infoEdidTx0Get = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidtx0.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
    
    let infoEdidTx1Get = await new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url + "/ssi/edidtx1.ssi", true);  
        xhr.responseType = "arraybuffer";
        xhr.onload = function(e) {
          resolve(xhr.response);
        };
        xhr.send();
    }); 
    
    let infoEdidAudGet = await new Promise(resolve => {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url + "/ssi/edidaud.ssi", true);  
      xhr.responseType = "arraybuffer";
      xhr.onload = function(e) {
        resolve(xhr.response);
      };
      xhr.send();
    }); 

    let infoEarcCdsGet = await new Promise(resolve => {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url + "/ssi/edidcds.ssi", true);  
      xhr.responseType = "arraybuffer";
      xhr.onload = function(e) {
        resolve(xhr.response);
      };
      xhr.send();
    });
    

let infoVideoDec = infoVideoGet;

if (infoVideoDec.tx0hpd === '1')
    infoVideoDec.tx0hpd = 'on';
else  infoVideoDec.tx0hpd = 'off';  

if (infoVideoDec.tx0rs === '1')
    infoVideoDec.tx0rs = 'on';
else  infoVideoDec.tx0rs = 'off';  

if (infoVideoDec.tx0tmds === '1')
    infoVideoDec.tx0tmds = 'on';
else  infoVideoDec.tx0tmds = 'off';  

if (infoVideoDec.tx1hpd === '1')
    infoVideoDec.tx1hpd = 'on';
else  infoVideoDec.tx1hpd = 'off';  

if (infoVideoDec.tx1rs === '1')
    infoVideoDec.tx1rs = 'on';
else  infoVideoDec.tx1rs = 'off';  

if (infoVideoDec.tx1tmds === '1')
    infoVideoDec.tx1tmds = 'on';
else  infoVideoDec.tx1tmds = 'off';  

if (infoVideoDec.txaudhpd === '1')
    infoVideoDec.txaudhpd = 'on';
else  infoVideoDec.txaudhpd = 'off';  

if (infoVideoDec.txaudrs === '1')
    infoVideoDec.txaudrs = 'on';
else  infoVideoDec.txaudrs = 'off';  

if (infoVideoDec.rx0in5v === '2')
    infoVideoDec.rx0in5v = 'on';
else  infoVideoDec.rx0in5v = 'off';  

if (infoVideoDec.rx1in5v === '2')
    infoVideoDec.rx1in5v = 'on';
else  infoVideoDec.rx1in5v = 'off';  

if (infoVideoDec.rx2in5v === '2')
    infoVideoDec.rx2in5v = 'on';
else  infoVideoDec.rx2in5v = 'off';  

if (infoVideoDec.rx3in5v === '2')
    infoVideoDec.rx3in5v = 'on';
else  infoVideoDec.rx3in5v = 'off';  

if (infoVideoDec.arcst === '1')
    infoVideoDec.arcst = 'on';
else infoVideoDec.arcst = 'off'

if (infoVideoDec.earcst === '1')
    infoVideoDec.earcst = 'on';
else infoVideoDec.earcst = 'off'

if (infoVideoDec.earcout5v === '1')
    infoVideoDec.earcout5v = 'on';
else infoVideoDec.earcout5v = 'off'

 
 let infoOsd = new Uint8Array(infoOsdGet); 
 let infoOsdDec = {};
 infoOsdDec.osdenable = infoOsd[10];
 infoOsdDec.osdfadesecs = infoOsd[17];
 infoOsdDec.osdnifadesecs = infoOsd[20];
 infoOsdDec.osdx = (infoOsd[26]<<8) | infoOsd[27];
 infoOsdDec.osdy = (infoOsd[28]<<8) | infoOsd[29];
 infoOsdDec.osdcolorfg = infoOsd[13];
 infoOsdDec.osdcolorbg = infoOsd[14];
 infoOsdDec.osdlevel = infoOsd[15];

 infoOsdDec.osdmaskenable = infoOsd[11];
 infoOsdDec.osdmaskx = (infoOsd[30]<<8) | infoOsd[31];
 infoOsdDec.osdmasky = (infoOsd[32]<<8) | infoOsd[33];
 infoOsdDec.osdmaskx1 = (infoOsd[34]<<8) | infoOsd[35];
 infoOsdDec.osdmasky1 = (infoOsd[36]<<8) | infoOsd[37];
 infoOsdDec.osdmasklevel = infoOsd[12];
 infoOsdDec.osdmaskgray = infoOsd[18];

 infoOsdDec.osdtextenable = infoOsd[39]&0x01;

 infoOsdDec.osdtextneverfade = infoOsd[39]&0x02;
 infoOsdDec.osdtextx = (infoOsd[40]<<8) | infoOsd[41];
 infoOsdDec.osdtexty = (infoOsd[42]<<8) | infoOsd[43];

 infoOsdDec.osdfieldsource = infoOsd[16]&0x01;
 infoOsdDec.osdfieldvideo = (infoOsd[16]>>>1)&0x01;
 infoOsdDec.osdfieldvideoif = (infoOsd[16]>>>2)&0x01;
 infoOsdDec.osdfieldhdr = (infoOsd[16]>>>3)&0x01;
 infoOsdDec.osdfieldhdrif = (infoOsd[16]>>>4)&0x01;
 infoOsdDec.osdfieldaudio = (infoOsd[16]>>>5)&0x01;
 infoOsdDec.osdfieldrxhdrforce = (infoOsd[16]>>>6)&0x01;
 infoOsdDec.osdfieldrxvideoforce = (infoOsd[16]>>>7)&0x01;
 infoOsdDec.osdfieldignoremetadata = infoOsd[19];

 infoOsdDec.osdtextstr = '';
 for (let i=0; i<infoOsd[38];i++)
     infoOsdDec.osdtextstr = infoOsdDec.osdtextstr + String.fromCharCode(infoOsd[44+i]);

 
    let infoMacro = new Uint8Array(infoMacroGet); 
    let infoMacroDec = {};
    infoMacroDec.macroenable = infoMacro[0]&0x01;
    infoMacroDec.macroeverysync = (infoMacro[0]&0x02)>>>1;
    infoMacroDec.macroalwayshex = (infoMacro[0]&0x04)>>>2;
    infoMacroDec.macroalwayshexorder = (infoMacro[0]&0x08)>>>3;
    infoMacroDec.macroalwayshexdelay = (infoMacro[0]&0x70)>>>4;

    infoMacroDec.macrohdr10mode = infoMacro[1];
    infoMacroDec.macrocustomen1 = infoMacro[2]&0x01;
    infoMacroDec.macrocustomen2 = (infoMacro[2]&0x02)>>>1;
    infoMacroDec.macrocustomen3 = (infoMacro[2]&0x04)>>>2;
    infoMacroDec.macrocustomen4 = (infoMacro[2]&0x08)>>>3;
    infoMacroDec.macrocustomen5 = (infoMacro[2]&0x10)>>>4;
    infoMacroDec.macrocustomen6 = (infoMacro[2]&0x20)>>>5;
    infoMacroDec.macrocustomen7 = (infoMacro[2]&0x40)>>>6;

    infoMacroDec.macrocustom1mode = infoMacro[3];
    infoMacroDec.macrocustom1value = (infoMacro[5]<<8) | infoMacro[4];
    infoMacroDec.macrocustom1action = infoMacro[6];

    infoMacroDec.macrocustom2mode = infoMacro[7];
    infoMacroDec.macrocustom2value = (infoMacro[9]<<8) | infoMacro[8];
    infoMacroDec.macrocustom2action = infoMacro[10];

    infoMacroDec.macrocustom3mode = infoMacro[11];
    infoMacroDec.macrocustom3value = (infoMacro[13]<<8) | infoMacro[12];
    infoMacroDec.macrocustom3action = infoMacro[14];

    infoMacroDec.macrocustom4mode = infoMacro[15];
    infoMacroDec.macrocustom4value = (infoMacro[17]<<8) | infoMacro[16];
    infoMacroDec.macrocustom4action = infoMacro[18];

    infoMacroDec.macrocustom5mode = infoMacro[19];
    infoMacroDec.macrocustom5value = (infoMacro[21]<<8) | infoMacro[20];
    infoMacroDec.macrocustom5action = infoMacro[22];

    infoMacroDec.macrocustom6mode = infoMacro[23];
    infoMacroDec.macrocustom6value = (infoMacro[25]<<8) | infoMacro[24];
    infoMacroDec.macrocustom6action = infoMacro[26];

    infoMacroDec.macrocustom7action = infoMacro[27];

    infoMacroDec.macrodelay = infoMacro[28];

    infoMacroDec.macrohdrhdtv = infoMacro[29];
    infoMacroDec.macrohdrfilm = infoMacro[30];
    infoMacroDec.macrohdrbt2020 = infoMacro[31];
    infoMacroDec.macro3d = infoMacro[32];
    infoMacroDec.macroxvcolor = infoMacro[33];
    infoMacroDec.macrohlgbt709 = infoMacro[34];
    infoMacroDec.macrohlgbt2020 = infoMacro[35];
    infoMacroDec.macrohdr10 = infoMacro[36];
    infoMacroDec.macrohdr10opt = infoMacro[37];
    infoMacroDec.macrohdr10bt709 = infoMacro[38];
    infoMacroDec.macrolldv = infoMacro[39];

    let val=[];
    for (let i=0; i<infoMacro[0x30]-1; i++) val[i] = infoMacro[0x31+i].toString(16);
    infoMacroDec.macrosdrtvcustom = val.join(':');

    val=[];
    for (let i=0; i<infoMacro[0x40]-1; i++) val[i] = infoMacro[0x41+i].toString(16);
    infoMacroDec.macrosdrfilmcustom = val.join(':');
   
    val=[];
    for (let i=0; i<infoMacro[0x50]-1; i++) val[i] = infoMacro[0x51+i].toString(16);
    infoMacroDec.macrosdrbt2020custom = val.join(':');

    val=[];
    for (let i=0; i<infoMacro[0x60]-1; i++) val[i] = infoMacro[0x61+i].toString(16);
    infoMacroDec.macro3dcustom = val.join(':');;

    val=[];
    for (let i=0; i<infoMacro[0x70]-1; i++) val[i] = infoMacro[0x71+i].toString(16);
    infoMacroDec.macroxvcolorcustom = val.join(':');

    val=[];
    for (let i=0; i<infoMacro[0x80]-1; i++) val[i] = infoMacro[0x81+i].toString(16);
    infoMacroDec.macrohlgbt709custom = val.join(':');
   
    val=[];
    for (let i=0; i<infoMacro[0x90]-1; i++) val[i] = infoMacro[0x91+i].toString(16);
    infoMacroDec.macrohlgbt2020custom = val.join(':');
  
    val=[];
    for (let i=0; i<infoMacro[0xa0]-1; i++) val[i] = infoMacro[0xa1+i].toString(16);
    infoMacroDec.macrohdr10maincustom = val.join(':');
   
    val=[];
    for (let i=0; i<infoMacro[0xb0]-1; i++) val[i] = infoMacro[0xb1+i].toString(16);
    infoMacroDec.macrohdr10optionalcustom = val.join(':');
   
    val=[];
    for (let i=0; i<infoMacro[0xc0]-1; i++) val[i] = infoMacro[0xc1+i].toString(16);
    infoMacroDec.macrohdr10bt709custom = val.join(':');
 
    val=[];
    for (let i=0; i<infoMacro[0xd0]-1; i++) val[i] = infoMacro[0xd1+i].toString(16);
    infoMacroDec.macrolldvcustom = val.join(':');

    let infoEdidRx0 = new Uint8Array(infoEdidRx0Get); 
    let infoEdidRx0Dec = pad(infoEdidRx0);

    let infoEdidRx1 = new Uint8Array(infoEdidRx1Get); 
    let infoEdidRx1Dec = pad(infoEdidRx1)

    let infoEdidRx2 = new Uint8Array(infoEdidRx2Get); 
    let infoEdidRx2Dec = pad(infoEdidRx2);

    let infoEdidRx3 = new Uint8Array(infoEdidRx3Get); 
    let infoEdidRx3Dec = pad(infoEdidRx3);

    let infoEdidTx0 = new Uint8Array(infoEdidTx0Get); 
    let infoEdidTx0Dec = pad(infoEdidTx0);

    let infoEdidTx1 = new Uint8Array(infoEdidTx1Get); 
    let infoEdidTx1Dec = pad(infoEdidTx1);

    let infoEdidAud = new Uint8Array(infoEdidAudGet); 
    let infoEdidAudDec = pad(infoEdidAud);

    let infoEarcCds = new Uint8Array(infoEarcCdsGet); 
    let infoEarcCdsDec = pad(infoEarcCds);
    
    let infoEdidDec = infoEdidGet;

    if (infoEdidDec.edidmode === '0')
        infoEdidDec.edidmode = 'automix';
    else  if (infoEdidDec.edidmode === '1')
        infoEdidDec.edidmode = 'fixed';
    else  if (infoEdidDec.edidmode === '2')
        infoEdidDec.edidmode = 'custom';
    else  if (infoEdidDec.edidmode === '3')
        infoEdidDec.edidmode = 'copytx1';
    else  if (infoEdidDec.edidmode === '4')
        infoEdidDec.edidmode = 'copytx0';

    if (infoEdidDec.edidaudioflags & 0x40)
        infoEdidDec.edidvideoflags = 'tx0';
    else  if (infoEdidDec.edidaudioflags & 0x80)
        infoEdidDec.edidvideoflags = 'tx1';


    if (infoEdidDec.edidaudioflags & 0x01)
        infoEdidDec.edidaudioflags = 'stereo';
    else  if (infoEdidDec.edidaudioflags & 0x02)
        infoEdidDec.edidaudioflags = 'spdif';
    else  if (infoEdidDec.edidaudioflags & 0x04)
        infoEdidDec.edidaudioflags = 'full';       
    else if (infoEdidDec.edidaudioflags & 0x08)
        infoEdidDec.edidaudioflags = 'tx0';
    else  if (infoEdidDec.edidaudioflags & 0x10)
        infoEdidDec.edidaudioflags = 'tx1';
    else  if (infoEdidDec.edidaudioflags & 0x20)
        infoEdidDec.edidaudioflags = 'audioout';
    else  if (infoEdidDec.edidaudioflags & 0x100)
        infoEdidDec.edidaudioflags = 'earcout';
    else  if (infoEdidDec.edidaudioflags & 0x200)
        infoEdidDec.edidaudioflags = 'custom';        
    else  if (infoEdidDec.edidaudioflags & 0x400)
        infoEdidDec.edidaudioflags = 'arcbeam';  
    else  if (infoEdidDec.edidaudioflags & 0x800)
        infoEdidDec.edidaudioflags = 'arcddp';  
        
        
    let edidOverflowParse = parseInt(infoEdidDec.edidoverflow, 16);
    if (edidOverflowParse > 0)
    {
        infoEdidDec.edidoverflow = '';
        if (edidOverflowParse & 0x01) 
          infoEdidDec.edidoverflow += "AVSDB "

        if (edidOverflowParse & 0x02) 
          infoEdidDec.edidoverflow += "HDR10+ "
          
        if (edidOverflowParse & 0x04)
          infoEdidDec.edidoverflow += "ATMOS "

        if (edidOverflowParse & 0x08)
          infoEdidDec.edidoverflow += "DV "
    }
    else
    {
      infoEdidDec.edidoverflow = "none"
    }
    
    let infoCecDec = infoCecGet;
        if (infoCecDec.cec === '1')
              infoCecDec.cec = 'on';
        else infoCecDec.cec = 'off'

    if (infoCecDec.cecla === '14')
          infoCecDec.cecla = 'video';
    else infoCecDec.cecla = 'audio'

    if (infoCecDec.earcforce === '0')
      infoCecDec.earcforce = 'autoearc';
    else if (infoCecDec.earcforce === '1')
      infoCecDec.earcforce = 'earc';   
    else if (infoCecDec.earcforce === '2')
      infoCecDec.earcforce = 'hdmi';  
    else if (infoCecDec.earcforce === '3')
      infoCecDec.earcforce = 'autoarc';  
    else if (infoCecDec.earcforce === '4')
      infoCecDec.earcforce = 'arc';  

    const now = new Date();
    const date = now.toLocaleDateString();
    const time = now.toLocaleString();
    const fileName = `${PRODUCTSTR}-CONFIG-${date}.txt`;

    headerInfo.product= `${PRODUCTSTR}`;
    headerInfo.time = time;

    let configData = {
        ...headerInfo, 
        ...infoBoardGet, 
        ...infoVideoDec, 
        ...infoEdidDec, 
        ...infoHdrGet, 
        ...infoOsdDec,
        ...infoCecDec,
        ...infoMacroDec,
        ...infoConfigGet,
        EDIDRX0: infoEdidRx0Dec,
        EDIDRX1: infoEdidRx1Dec,
        EDIDRX2: infoEdidRx2Dec,
        EDIDRX3: infoEdidRx3Dec,
        EDIDTX0: infoEdidTx0Dec,
        EDIDTX1: infoEdidTx1Dec,
        EDIDAUD: infoEdidAudDec,
        EARCCDS: infoEarcCdsDec
    }

    let conf = JSON.stringify(configData, null, 4);
    download(conf, fileName, 'json');
}


const delaySend = ms => new Promise(res => setTimeout(res, ms));
   
const sendImportItem = cmd => new Promise(resolve => {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", cmd, true);   
      xhr.onload = function(e) {
        resolve(xhr.response);
      };
      xhr.send();
}); 

const sendImportItemMacro = cmd => new Promise(resolve => {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", url+"/macrosparam", true);   
  xhr.onload = function(e) {
    resolve(xhr.response);
  };
  xhr.send(cmd);
}); 

const sendImportItemOsd = cmd => new Promise(resolve => {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", url+"/osdparam", true);   
    xhr.onload = function(e) {
      resolve(xhr.response);
    };
    xhr.send(cmd);
  }); 

const sendImportItemOsdText = cmd => new Promise(resolve => {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", url+"/osdtext", true);   
    xhr.onload = function(e) {
      resolve(xhr.response);
    };
    xhr.send(cmd);
  }); 

  
const openFileImport = async (event) => {
    event.preventDefault();
    var input = document.getElementById('importfile');
    var reader = new FileReader();


    reader.onload = async function()
    {

        var text = reader.result;
        const res = JSON.parse(text)

        if (res.product !== 'VRROOM')
        {
          document.getElementById('status').innerHTML = 'IMPORT FILE ERROR - WRONG PRODUCT?';
          return;
        }

        document.getElementById('status').innerHTML = 'IMPORT STARTED .. WAIT';

        if (res.edidvideoflags === 'tx0')
            res.edidvideoflags = 'videotx0 on';
        else if (res.edidvideoflags === 'tx1')
            res.edidvideoflags = 'videotx1 on';

        if (res.edidaudioflags === 'stereo')
            res.edidaudioflags = '2ch on';
        else  if (res.edidaudioflags === 'spdif')
            res.edidaudioflags = 'spdif on';
        else  if (res.edidaudioflags === 'full')
            res.edidaudioflags = 'allaudio on';
        else  if (res.edidaudioflags === 'arcbeam')
            res.edidaudioflags = 'arcbeam on';
        else  if (res.edidaudioflags === 'arcddp')
            res.edidaudioflags = 'arcddp on';
        else  if (res.edidaudioflags === 'tx0')
            res.edidaudioflags = 'audiotx0 on';
        else  if (res.edidaudioflags === 'tx1')
            res.edidaudioflags = 'audiotx1 on';
        else  if (res.edidaudioflags === 'audioout')
            res.edidaudioflags = 'audioout on';
        else  if (res.edidaudioflags === 'earcout')
            res.edidaudioflags = 'earcout on';
        else  if (res.edidaudioflags === 'custom')
            res.edidaudioflags = 'audiocustom on';

        SetActiveValues('edid', res.edidvideoflags);
        SetActiveValues('edid', res.edidaudioflags);
 
        SetActiveValues('hdrcustom', (res.customhdr==='1')?'on':'off');
        SetActiveValues('avicustom', (res.customavi==='1')?'on':'off');
        SetActiveValues('hdrdisable', (res.disablehdr==='1')?'on':'off');
        SetActiveValues('avidisable', (res.disableavi==='1')?'on':'off');
        SetActiveValues('hdrcustomhdronly', (res.hdrcustomhdronly==='1')?'on':'off');
        SetActiveValues('hdrcustomhlgonly', (res.hdrcustomhlgonly==='1')?'on':'off');
        SetActiveValues('hdrcustomlldvonly', (res.hdrcustomlldvonly==='1')?'on':'off');
        SetActiveValues('hdrcustomhlgonlyauto', (res.hdrcustomhlgonlyauto==='1')?'on':'off');
 
        SetActiveValues('hdr10onlydisable', (res.hdrcustomhdronly==='1')?'on':'off');
        SetActiveValues('hlgonlydisable', (res.hlgdisable==='1')?'on':'off');
        SetActiveValues('lldvonlydisable', (res.lldvdisable==='1')?'on':'off');

        SetActiveValues('archbr', (res.archbr==='1')?'on':'off');

        SetActiveValues('filmmaker', (res.filmmaker==='1')?'on':'off');
        SetActiveValues('audiooutreso', (res.audiooutreso==='1')?'720':'1080');

        if (res.cecla === 'video')
            res.cecla = '14';
        else res.cecla = '5'

       if (res.earcforce === 'autoearc')
            res.earcforce = '0';
        else if (res.earcforce === 'earc')
            res.earcforce = '1';   
        else if (res.earcforce === 'hdmi')
            res.earcforce = '2';    
        else if (res.earcforce === 'autoarc')
            res.earcforce = '3';   
        else if (res.earcforce === 'arc')
            res.earcforce = '4';              
            
          switch (res.baudrate) {
            case '9600': SetActiveValues('baud','0');  break;
            case '19200': SetActiveValues('baud', '1');  break;
            case '57600': SetActiveValues('baud', '2');  break;
            case '115200': SetActiveValues('baud', '3');  break;
        }

        SetActiveValues('oled', (res.oled==='1')?'on':'off'); 

        SetActiveValues('autosw', (res.autosw==='1')?'on':'off'); 
        SetActiveValues('iractive', (res.iractive==='1')?'on':'off'); 

        SetActiveValues('htpcmode0', (res.htpcmode0==='1')?'on':'off'); 
        SetActiveValues('htpcmode1', (res.htpcmode1==='1')?'on':'off'); 
        SetActiveValues('htpcmode2', (res.htpcmode2==='1')?'on':'off'); 
        SetActiveValues('htpcmode3', (res.htpcmode3==='1')?'on':'off'); 

        SetActiveValues('encode', (res.enclevel==='1')?'14':'auto');

        SetActiveValues('mutetx0audio', (res.mutetx0==='1')?'on':'off'); 
        SetActiveValues('mutetx1audio', (res.mutetx1==='1')?'on':'off'); 

        SetActiveValues('edidfrlflag', (res.edidfrlflag==='1')?'on':'off'); 
        SetActiveValues('edidvrrflag', (res.edidvrrflag==='1')?'on':'off'); 
        SetActiveValues('edidallmflag', (res.edidallmflag==='1')?'on':'off'); 
        SetActiveValues('edidhdrflag', (res.edidhdrflag==='1')?'on':'off'); 
        SetActiveValues('ediddvflag', (res.ediddvflag==='1')?'on':'off'); 
        SetActiveValues('edidpcmflag', (res.edidpcmflag==='1')?'on':'off'); 
        SetActiveValues('edidtruehdflag', (res.edidtruehdflag==='1')?'on':'off'); 
        SetActiveValues('edidddflag', (res.edidddflag==='1')?'on':'off'); 
        SetActiveValues('edidddplusflag', (res.edidddplusflag==='1')?'on':'off'); 
        SetActiveValues('edidonebitflag', (res.edidonebitflag==='1')?'on':'off'); 
        SetActiveValues('ediddtsflag', (res.ediddtsflag==='1')?'on':'off'); 
        SetActiveValues('ediddtshdflag', (res.ediddtshdflag==='1')?'on':'off'); 

        SetActiveValues('dhcp', (res.dhcp==='1')?'on':'off'); 
        SetActiveValues('mdns', (res.mdns==='1')?'on':'off'); 
        SetActiveValues('ipinterrupt', (res.ipinterrupt==='1')?'on':'off'); 

        SetActiveValues('staticip', res.staticip.slice(9));
        res.hdrasc = res.HDRCUSTOM;
        res.aviasc = res.AVICUSTOM;

        let cmd = "";  
        for (var key in res) {

              switch(key) 
              {
                  case "opmode":
                  case "edidmode":
                  case "edidtableinp1":
                  case "edidtableinp2": 
                  case "edidtableinp3":
                  case "edidtableinp4":    
                  
                  case "edidfrlmode":    
                  case "ediddvmode":    
                  case "edidhdrmode":    
                  case "edidpcmchmode":    
                  case "edidpcmsrmode":    
                  case "edidpcmbwmode":    
                  case "edidtruehdmode":    
                  case "edidtruehdsrmode":    
                  case "edidddmode":    
                  case "edidddplusmode":    
                  case "edidddplussrmode":    
                  case "ediddtshdmode":    
                  case "ediddtshdsrmode":      
                  case "edidvrrmode":    
                  case "edidallmmode":    
                  case "edidonebitmode":    
                  case "edidonebitspmode":      
                  case "ediddtsmode":    
                 
                  case "cec": 
                  case "cecla": 
                  case "cec0en": 
                  case "cec1en": 
                  case "cec2en": 
                  case "cec3en":                  
                  case "earcforce": 
                  case "earctxmode": 
                  case "unmutecnt": 
                  case "earcunmutecnt":  
                  case "azpoweronmode": 
                  case "azpoweroffmode": 
                  case "earcspeakermap":
                  case "earcmutemode": 
                  case "analogvolume":
                  case "analogbass": 
                  case "analogtreble":
                  case "tcpport": 
                  case "ircodeset":
                  case "oledpage": 
                  case "oledfade": 
                  case "hdrasc":               
                  case "aviasc":  

                        cmd = url + "/cmd?" + key + "=" + res[key];

                        await sendImportItem(cmd); 
                        await delaySend(50);

                  break;
              }
        }

        cmd = url + "/cmd?insel=" + `${res.portseltx0} ${res.portseltx1}`;
        await sendImportItem(cmd); 

        let macrosBuffer = new ArrayBuffer(230);
        let WriteBuf = new Uint8Array(macrosBuffer);

        WriteBuf[0] = res.macroenable |
                      (res.macroeverysync<<1) |
                      (res.macroalwayshex<<2) |
                      (res.macroalwayshexorder<<3) |
                      (res.macroalwayshexdelay<<4);

        WriteBuf[1] = res.macrohdr10mode;
        
        WriteBuf[2] = res.macrocustomen1 |
              (res.macrocustomen2 << 1) |
              (res.macrocustomen3 << 2) |
              (res.macrocustomen4 << 3) |
              (res.macrocustomen5 << 4) |
              (res.macrocustomen6 << 5) |
              (res.macrocustomen7 << 6); 

        WriteBuf[3] = res.macrocustom1mode;
        WriteBuf[4] = res.macrocustom1value & 0xff;   
        WriteBuf[5] = (res.macrocustom1value >>> 8) & 0xff;
        WriteBuf[6] = res.macrocustom1action;

        WriteBuf[7] = res.macrocustom2mode;
        WriteBuf[8] = res.macrocustom2value & 0xff;   
        WriteBuf[9] = (res.macrocustom2value >>> 8) & 0xff;
        WriteBuf[10] = res.macrocustom2action;

        WriteBuf[11] = res.macrocustom3mode;
        WriteBuf[12] = res.macrocustom3value & 0xff;   
        WriteBuf[13] = (res.macrocustom3value >>> 8) & 0xff;
        WriteBuf[14] = res.macrocustom3action;

        WriteBuf[15] = res.macrocustom4mode;
        WriteBuf[16] = res.macrocustom4value & 0xff;   
        WriteBuf[17] = (res.macrocustom4value >>> 8) & 0xff;
        WriteBuf[18] = res.macrocustom4action;

        WriteBuf[19] = res.macrocustom5mode;
        WriteBuf[20] = res.macrocustom5value & 0xff;   
        WriteBuf[21] = (res.macrocustom5value >>> 8) & 0xff;
        WriteBuf[22] = res.macrocustom5action;

        WriteBuf[23] = res.macrocustom6mode;
        WriteBuf[24] = res.macrocustom6value & 0xff;   
        WriteBuf[25] = (res.macrocustom6value >>> 8) & 0xff;
        WriteBuf[26] = res.macrocustom6action;

        WriteBuf[27] = res.macrocustom7action;

        WriteBuf[28] = res.macrodelay;
        WriteBuf[29] = res.macrohdrhdtv;
        WriteBuf[30] = res.macrohdrfilm;
        WriteBuf[31] = res.macrohdrbt2020;
        WriteBuf[32] = res.macro3d;
        WriteBuf[33] = res.macroxvcolor;
        WriteBuf[34] = res.macrohlgbt709;
        WriteBuf[35] = res.macrohlgbt2020;
        WriteBuf[36] = res.macrohdr10;
        WriteBuf[37] = res.macrohdr10opt;
        WriteBuf[38] = res.macrohdr10bt709;
        WriteBuf[39] = res.macrolldv;


        let val=[];

        val = res.macrosdrtvcustom;
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
    
        val = res.macrosdrfilmcustom;
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
        
        val = res.macrosdrbt2020custom;
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

        val = res.macro3dcustom;
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

        val = res.macroxvcolorcustom;
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
        
        
        val = res.macrohlgbt709custom;
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
   
        val = res.macrohlgbt2020custom;
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

        val = res.macrohdr10maincustom;
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
        
        val = res.macrohdr10optionalcustom;
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

        val = res.macrohdr10bt709custom;
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

        val = res.macrolldvcustom;
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

        await sendImportItemMacro(WriteBuf); 
        await delaySend(50);



        let osdBuffer = new ArrayBuffer(40);
        WriteBuf = new Uint8Array(osdBuffer);

        WriteBuf[0] = res.osdenable;
        WriteBuf[1] = res.osdmaskenable;
        WriteBuf[2] = res.osdmasklevel;
        WriteBuf[3] = res.osdcolorfg;
        WriteBuf[4] = res.osdcolorbg;
        WriteBuf[5] = res.osdlevel;
        WriteBuf[6] = res.osdfieldsource |
              (res.osdfieldvideo << 1) |
              (res.osdfieldvideoif << 2) |
              (res.osdfieldhdr << 3) |
              (res.osdfieldhdrif << 4) |
              (res.osdfieldaudio << 5) |
              (res.osdfieldrxhdrforce << 6) |
              (res.osdfieldrxvideoforce << 7); 

        WriteBuf[7] = res.osdfadesecs;
        WriteBuf[8] = res.osdmaskgray;
        WriteBuf[9] = res.osdfieldignoremetadata;
        WriteBuf[10] = res.osdnifadesecs;

        WriteBuf[16] = (res.osdx >>> 8) & 255;
        WriteBuf[17] = res.osdx & 255;
        WriteBuf[18] = (res.osdy >>> 8) & 255;
        WriteBuf[19] = res.osdy & 255;

        WriteBuf[20] = (res.osdmaskx >>> 8) & 255;
        WriteBuf[21] = res.osdmaskx & 255;
        WriteBuf[22] = (res.osdmasky >>> 8) & 255;
        WriteBuf[23] = res.osdmasky & 255;

        WriteBuf[24] = (res.osdmaskx1 >>> 8) & 255;
        WriteBuf[25] = res.osdmaskx1 & 255;
        WriteBuf[26] = (res.osdmasky1 >>> 8) & 255;
        WriteBuf[27] = res.osdmasky1 & 255;
          
        await sendImportItemOsd(WriteBuf); 


        let osdTextbuffer = new ArrayBuffer(50);
        WriteBuf = new Uint8Array(osdTextbuffer);

        WriteBuf[0] = res.osdtextstr.length;
        WriteBuf[1] = res.osdtextenable | (res.osdtextneverfade << 1);
        WriteBuf[2] = (res.osdtextx >>> 8) & 255;
        WriteBuf[3] = res.osdtextx & 255;
        WriteBuf[4] = (res.osdtexty >>> 8) & 255;
        WriteBuf[5] = res.osdtexty & 255;

        for (let i=0; i<res.osdtextstr.length; i++) 
              WriteBuf[i+6] = res.osdtextstr.charCodeAt(i);

        await sendImportItemOsdText(WriteBuf); 


        document.getElementById('status').innerHTML = 'IMPORT COMPLETED, REBOOTING..';

        SetActiveValues('reboot', 'on');    

  };

  reader.readAsText(input.files[0]);

};
