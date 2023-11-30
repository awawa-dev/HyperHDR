$(document).ready( function(){
	class ColorRgb {	  
	  constructor(_R,_G,_B)
	  {
		  this.r = _R;
		  this.g = _G;
		  this.b = _B;
	  }
	  set(_R,_G,_B)
	  {
		  this.r = _R;
		  this.g = _G;
		  this.b = _B;
	  }
	  equal(x)
	  {
		  if (this.r == x.r && this.g == x.g && this.b == x.b)
			  return true;
		  else
			  return false;
	  }
	}
	
	let primeColors = [
				new ColorRgb(255, 0, 0), new ColorRgb(0, 255, 0), new ColorRgb(0, 0, 255), new ColorRgb(255, 255, 0),
				new ColorRgb(255, 0, 255), new ColorRgb(0, 255, 255), new ColorRgb(255, 128, 0), new ColorRgb(255, 0, 128), new ColorRgb(0, 128, 255),
				new ColorRgb(128, 64, 0), new ColorRgb(128, 0, 64),
				new ColorRgb(128, 0, 0), new ColorRgb(0, 128, 0), new ColorRgb(0, 0, 128),
				new ColorRgb(16, 16, 16), new ColorRgb(32, 32, 32), new ColorRgb(48, 48, 48), new ColorRgb(64, 64, 64), new ColorRgb(96, 96, 96), new ColorRgb(120, 120, 120), new ColorRgb(144, 144, 144), new ColorRgb(172, 172, 172), new ColorRgb(196, 196, 196), new ColorRgb(220, 220, 220),
				new ColorRgb(255, 255, 255),
				new ColorRgb(0, 0, 0)
				];

	let currentColor = new ColorRgb(0,0,0);
	let startColor = new ColorRgb(0,0,0);
	let checksum = 0;
	let maxLimit = 255;
	let finish = false;
	let running = false;
	let limited = false;
	let saturation = 1;
	let luminance = 1;
	let gammaR = 1;
	let gammaG = 1;
	let gammaB = 1;
	let coef = 0;

	const canvas = document.getElementById("canvas");
	const ctx = canvas.getContext("2d");
	canvas.addEventListener('click', function() { 
			if (!running)
			{
				if (canvas.classList.contains("fullscreen-canvas"))
					canvas.classList.remove("fullscreen-canvas");
				else							
					canvas.classList.add("fullscreen-canvas");
			}
		}, false);

	performTranslation();
	$("#grabber_calibration_intro").html($.i18n("grabber_calibration_expl"));
		
	

	$("#startCalibration").off('click').on('click', function() { limited = false; coef = 0; startCalibration(); });
	
	sendToHyperhdr("serverinfo", "", '"subscribe":["lut-calibration-update"]');
	
	$(window.hyperhdr).off("cmd-lut-calibration-update").on("cmd-lut-calibration-update", function(event)
	{
		handleMessage(event);
	});
	
	resetImage();
	
	function resetImage()
	{
		ctx.fillStyle = "white";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
	
		var gradient = ctx.createConicGradient(0, canvas.width / 2, canvas.height / 2);

		
		var s = 1/13;
		gradient.addColorStop(s*0,	`rgb(255, 0,   255)`);
		gradient.addColorStop(s*1,	`rgb(128, 0,   255)`);		
		gradient.addColorStop(s*2,	`rgb(0,   0,   255)`);
		gradient.addColorStop(s*3,	`rgb(0,   128, 255)`);
		gradient.addColorStop(s*5,	`rgb(0,   255, 255)`);
		gradient.addColorStop(s*6, 	`rgb(0,   255, 128)`);
		gradient.addColorStop(s*7,	`rgb(0,   255,   0)`);
		gradient.addColorStop(s*8,	`rgb(128, 255,   0)`);
		gradient.addColorStop(s*9,	`rgb(255, 255,   0)`);
		gradient.addColorStop(s*10,	`rgb(255, 128,   0)`);		
		gradient.addColorStop(s*11,	`rgb(255, 0,     0)`);
		gradient.addColorStop(s*12,	`rgb(255, 0,   128)`);
		gradient.addColorStop(s*13,	`rgb(255, 0,   255)`);		
		

		// Set the fill style and draw a rectangle
		ctx.fillStyle = gradient;
		ctx.fillRect(0, 0, canvas.width, canvas.height);
	}
	
	function handleMessage(event)
	{
		let json = event.response.data;
		
		if (!running)
			return;
		
		if (json.limited == 1 && !limited)
		{
			limited = true;
			startCalibration();
			return;
		}
		
		if (typeof json.coef != 'undefined' && json.coef != null && !isNaN(json.coef))
		{
			console.log(json.coef);
			coef = json.coef;
			startCalibration();
			return;
		}
		
		if (json.status != 0)
		{
			document.body.style.overflow = 'visible';
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			alert("Error occured. Please consult the HyperHDR log.\n\n" + json.error);
			return;
		}
		
		if (json.validate != checksum)
		{
			document.body.style.overflow = 'visible';
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			alert("Unexpected CRC: "+json.validate+", waiting for: "+checksum);			
			return;
		}
		
		if (finish)
		{			
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			alert(`Finished!\n\nFinal section: ${checksum}.\nIf the new LUT file was successfully created then you can find the path in the HyperHDR logs.\n\nUsually it's 'lut_lin_tables.3d' in your home HyperHDR folder.`);
			document.body.style.overflow = 'visible';
			resetImage();
		}
		else
		{
			checksum++;
			drawImage();
			setTimeout(() => {			
				requestLutCalibration("capture", checksum, startColor, currentColor, limited, saturation, luminance, gammaR, gammaG, gammaB, coef);
				}, 15);
		}
	}
		
	function startCalibration()
	{
		if (matchMedia('(display-mode: fullscreen)').matches) 
		{
			document.body.style.overflow = 'hidden';
			canvas.classList.add("fullscreen-canvas");
			currentColor = new ColorRgb(0,0,0);
			startColor = new ColorRgb(0,0,0);
			checksum = 0;
			finish = false;
			running = true;
			saturation = document.getElementById('saturation').value;
			luminance =  document.getElementById('luminance').value;
			gammaR = document.getElementById('gammaR').value;
			gammaG = document.getElementById('gammaG').value;
			gammaB = document.getElementById('gammaB').value;			
			
			ctx.fillStyle = "black";
			ctx.fillRect(0, 0, canvas.width, canvas.height);
			
			drawImage();		
			setTimeout(() => {
				requestLutCalibration("capture", checksum, startColor, currentColor, limited, saturation, luminance, gammaR, gammaG, gammaB, coef);
			}, 1000); 
		}
		else
			alert('Please run fullscreen mode (F11)');
	};

	function drawImage()
	{		
		startColor = Object.assign({}, currentColor);
		
		let scaleX = canvas.width / 128;
		let scaleY = canvas.height / 72;
		
		for(let py = 0; py < 72; py++)
			for(let px = (py < 71 && py > 0) ? checksum % 2: 0; px < 128; px++)
			{
				let sx = px * scaleX;
				let ex = (px + 1) * scaleX;
				let sy = py * scaleY;
				let ey = (py + 1) * scaleY;
				
				if (py < 71 && py > 0)
				{
					ctx.fillStyle = `rgb(${currentColor.r}, ${currentColor.g}, ${currentColor.b})`;
					ctx.fillRect(sx, sy, ex - sx, ey - sy);

					increaseColor(currentColor);
				}
				else
				{
					if (px == 0)
						ctx.fillStyle = `rgb(255,255,255)`;
					else if (px == 1)
						ctx.fillStyle = `rgb(0,0,0)`;
					else if (px == 2)
						ctx.fillStyle = `rgb(255,0,0)`;
					else if (px == 3)
						ctx.fillStyle = `rgb(255,255,0)`;
					else if (px == 4)
						ctx.fillStyle = `rgb(255,0,255)`;
					else if (px == 5)
						ctx.fillStyle = `rgb(0,255,0)`;
					else if (px == 6)
						ctx.fillStyle = `rgb(0,255,255)`;
					else if (px == 7)
						ctx.fillStyle = `rgb(0,0,255)`;
					else if (px >= 8 && px < 24)
					{
						let sh = 1 << (15 - (px - 8));
						
						if (checksum & sh)
							ctx.fillStyle = `rgb(255,255,255)`;
						else
							ctx.fillStyle = `rgb(0,0,0)`;
					}
					else if (px >= 24 && px < 60)
						ctx.fillStyle = `rgb(255,255,255)`;
					else if (px >= 60 && px < 96)
						ctx.fillStyle = `rgb(0,0,0)`;
					else if (px >= 96)
						ctx.fillStyle = `rgb(128,128,128)`;
					
					ctx.fillRect(sx, sy, ex - sx, ey - sy);
				}
			}
	}
	
	function increaseColor(color)
	{				
		debugger;
		if (color.equal(primeColors[primeColors.length -1]))
			color.set(primeColors[0].r, primeColors[0].g, primeColors[0].b);
		else
		{
			for (let i = 0; i < primeColors.length; i++ )
				if (color.equal(primeColors[i]))
				{
					i++;
					color.set(primeColors[i].r, primeColors[i].g, primeColors[i].b);
					break;
				}
		}		
		finish = (checksum > 20) ? true : false;
	}	
});
