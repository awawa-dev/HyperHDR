$(document).ready( function(){
	if (window.matchMedia("(color-gamut: srgb)").matches) {
	  console.log(`Screen supports approximately the sRGB gamut or more.`);
	}

	if (window.matchMedia("(color-gamut: p3)").matches) {
	  console.log(`Screen supports approximately the gamut specified by the DCI P3 Color Space or more.`);
	}

	if (window.matchMedia("(color-gamut: rec2020)").matches) {
	  console.log(`Screen supports approximately the gamut specified by the ITU-R Recommendation BT.2020 Color Space or more.`);
	}

	class ColorRgb {	  
	  constructor(_R,_G,_B)
	  {
		  this.r = _R;
		  this.g = _G;
		  this.b = _B;
	  }
	  clone(_p)
	  {
		  this.r = _p.r;
		  this.g = _p.g;
		  this.b = _p.b;
	  }
	  divide(_m, _n)
	  {
		  this.r -= Math.round(_m * (this.r + (Math.trunc(this.r) % 2)) / _n) - ((_m == 0) ? 0 : (Math.trunc(this.r % 2)));
		  this.g -= Math.round(_m * (this.g + (Math.trunc(this.g) % 2)) / _n) - ((_m == 0) ? 0 : (Math.trunc(this.g % 2)));
		  this.b -= Math.round(_m * (this.b + (Math.trunc(this.b) % 2)) / _n) - ((_m == 0) ? 0 : (Math.trunc(this.b % 2)));
	  }
	}

	let primeColors = [
		new ColorRgb(255, 255, 255),
		new ColorRgb(255, 0,   0  ),
		new ColorRgb(0,   255, 0  ),
		new ColorRgb(0,   0,   255),
		new ColorRgb(255, 255, 0  ),
		new ColorRgb(0,   255, 255),
		new ColorRgb(255,   0, 255),
		new ColorRgb(255, 128, 0  ),
		new ColorRgb(0,   255, 128),
		new ColorRgb(255,   0, 128),
		new ColorRgb(128, 255, 0  ),
		new ColorRgb(0,   128, 255),
		new ColorRgb(128,   0, 255) ];

	let checksum = 0;
	let finish = false;
	let running = false;
	let saturation = 1;
	let luminance = 1;
	let gammaR = 1;
	let gammaG = 1;
	let gammaB = 1;

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
	
	sendToHyperhdr("serverinfo", "", '"subscribe":["lut-calibration-update"]');
	
	$(window.hyperhdr).off("cmd-lut-calibration-update").on("cmd-lut-calibration-update", function(event)
	{
		handleMessage(event);
	});

	$("#startCalibration").off('click').on('click', function() { startCalibration(); });
	
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
				
		if (json.status != 0)
		{
			document.body.style.overflow = 'visible';
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			alert("Error occured. Please consult the HyperHDR log.\n\n" + json.error);
			return;
		}
		
		if (json.validate < 0)
		{
			finish = true;
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			document.body.style.overflow = 'visible';
			resetImage();
		}
		else
		{
			checksum = json.validate;
			drawImage();
			setTimeout(() => {			
				requestLutCalibration("capture", checksum, saturation, luminance, gammaR, gammaG, gammaB);
				}, 500);
		}
	}
		
	function startCalibration()
	{
		if (matchMedia('(display-mode: fullscreen)').matches) 
		{
			document.body.style.overflow = 'hidden';
			canvas.classList.add("fullscreen-canvas");
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
				requestLutCalibration("capture", checksum, saturation, luminance, gammaR, gammaG, gammaB);
			}, 100); 
		}
		else
			alert('Please run fullscreen mode (F11)');
	};

	const SCREEN_BLOCKS_X = 64;
	const SCREEN_BLOCKS_Y = 36;
	const LOGIC_BLOCKS_X_SIZE = 4;
	const LOGIC_BLOCKS_X =  Math.floor((SCREEN_BLOCKS_X - 1) / LOGIC_BLOCKS_X_SIZE);
	const COLOR_DIVIDES = 32;

	function draw(x, y, scaleX, scaleY)
	{
		let sX = Math.round((x * LOGIC_BLOCKS_X_SIZE + (y % 2) * 2 + 1)* scaleX);
		let sY = Math.round(y * scaleY);

		ctx.fillRect(sX, sY, scaleX, scaleY);
	}

	function getColor(index)
	{
		let color = new ColorRgb(0,0,0);
		let searching = 0;

		for(let i = 0; i < primeColors.length; i++)
			for(let j = 0; j < COLOR_DIVIDES; j++)
			{
				if (searching == index)
				{
					color.clone(primeColors[i]);
					color.divide(j, COLOR_DIVIDES);
					console.log(`[${color.r}, ${color.g}, ${color.b}]`)

					return color;
				}
				else
					searching++;
			}

		finish = true;
		return color;
	}

	function drawImage()
	{
		let scaleX = canvas.width / SCREEN_BLOCKS_X;
		let scaleY = canvas.height / SCREEN_BLOCKS_Y;
		let actual = 0;
		for(let py = 1; py < SCREEN_BLOCKS_Y - 1; py++)
			for(let px = 0; px < LOGIC_BLOCKS_X; px++)
			{				
				let currentColor = getColor(actual++);
				ctx.fillStyle = `rgb(${currentColor.r}, ${currentColor.g}, ${currentColor.b})`;
				draw(px, py, scaleX, scaleY);
			}

		ctx.fillStyle = `rgb(0, 0, 0)`;
		draw(0, 0, scaleX, scaleY); draw(0, SCREEN_BLOCKS_Y - 1, scaleX, scaleY);
		ctx.fillStyle = `rgb(255, 255, 255)`;
		draw(1, 0, scaleX, scaleY); draw(1, SCREEN_BLOCKS_Y - 1, scaleX, scaleY);
		
		for(let py = 0; py < SCREEN_BLOCKS_Y; py += SCREEN_BLOCKS_Y - 1)
			for(let px = 2; px < 8 + 2; px++)
			{
				let sh = 1 << (7 - (px - 2));
						
				if (checksum & sh)
					ctx.fillStyle = `rgb(255,255,255)`;
				else
					ctx.fillStyle = `rgb(0,0,0)`;

				draw(px, py, scaleX, scaleY);
			}
	}	
	
	startCalibration();
});
