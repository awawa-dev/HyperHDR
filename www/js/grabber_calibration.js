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

	let calibWiz = null;
	let myInterval;
	let currentTestBoard = 0;
	let finish = false;
	let running = false;
	let saturation = 1;
	let luminance = 1;
	let gammaR = 1;
	let gammaG = 1;
	let gammaB = 1;

	$("#select_classic_calibration_label").html($.i18n("option_calibration_classic"));
	$("#select_video_calibration_label").html($.i18n("option_calibration_video"));
	$("#video_calibration_overview").html($.i18n("video_calibration_overview"));
	$("#btn_select_calibration").off('click').on('click', function() {
		if($('#select_classic_calibration').is(':checked'))
		{
			$('#calibration_select_intro').hide();
			$('#classic_calibration').show();
		}
		if($('#select_video_calibration').is(':checked'))
		{
			$('#calibration_select_intro').hide();			
			$('#video_calibration').show();
		}
	});

	$("#btn_start_video_calibration").off('click').on('click', function() {
			finish = false;
			running = true;			
			startCalibrationWizard();
			const calDebug = document.getElementById('chk_debug').checked;
			const calPostprocessing = document.getElementById('chk_postprocessing').checked;
			requestLutCalibration("capture", calDebug, calPostprocessing);
	});



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

	function startCalibrationWizard() {		
		$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n("main_menu_grabber_calibration_token"));
		$('#wizp1_body').html('<h4 id="calibration_running_header" style="font-weight:bold;text-transform:uppercase;">' + $.i18n("perf_please_wait") + '</h4><div class="row pe-1 ps-2"><div class="col-12 p-3 mt-3 text-center"><svg data-src="svg/spinner_large.svg" fill="currentColor" class="svg4hyperhdr mb-2"></svg><br/></div></div>');
		$('#wizp2_body').html('<h4 id="calibration_summary_header" style="font-weight:bold;text-transform:uppercase;"></h4><p id="calibration_summary"></p>');
		$('#wizp1_footer').html('<button type="button" class="btn btn-danger" name="btn_wiz_abort"><svg data-src="svg/button_cancel.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
		$('#wizp2_footer').html('<button type="button" class="btn btn-success" name="btn_wiz_closeme_download"><svg data-src="svg/button_success.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_ok') + '</button>');
		//open modal
		calibWiz= new bootstrap.Modal($("#wizard_modal"), {
			backdrop: "static",
			keyboard: false
		});
		
		const backupCalibWizard = $("#wizard_modal").css("z-index");
		$("#wizard_modal").css("z-index", "10000");		
		
		$('#wizp1').toggle(true);
		$('#wizp2').toggle(false);

		calibWiz.show();

		$("[name='btn_wiz_abort']").off().on('click', function () {
			$("#wizard_modal").css("z-index", backupCalibWizard);
			requestLutCalibration("stop", false, false);
			calibWiz.hide();
			resetWizard(true);
			reload();
		});

		$("[name='btn_wiz_closeme_download']").off().on('click', function () {
			$("#wizard_modal").css("z-index", backupCalibWizard);
			calibWiz.hide();
			resetWizard(true);
			reload();
		});
	}
	
	function handleMessage(event)
	{
		let json = event.response.data;

		if (!running)
			return;
				
		if (json.error != null)
		{
			clearInterval(myInterval);
			document.body.style.overflow = 'visible';
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			resetImage();
			if (calibWiz != null)
			{
				calibWiz.hide();
				resetWizard(true);
			}
			alert("Error occured. Please consult the HyperHDR log.\n\n" + json.error);
			return;
		}

		if (json.message != null)
		{
			if (json.start && calibWiz == null)
			{
				clearInterval(myInterval);
				document.body.style.overflow = 'visible';
				canvas.classList.remove("fullscreen-canvas");				
				resetImage();
				startCalibrationWizard();
			}
			if (calibWiz != null)
			{
				let resElement = document.getElementById("calibration_running_header");
				resElement.innerHTML = json.message;
			}
		}
		
		if (json.finished != null)
		{
			clearInterval(myInterval);
			finish = true;
			canvas.classList.remove("fullscreen-canvas");
			running = false;
			document.body.style.overflow = 'visible';
			resetImage();

			let resElement = document.getElementById("calibration_summary_header");
			resElement.innerHTML = `Finished!<br/>If the new LUT file was successfully created then you can find the path in the HyperHDR logs.<br/>Usually it's 'lut_lin_tables.3d' in your home HyperHDR folder.`;
			$('#wizp1').toggle(false);
			$('#wizp2').toggle(true);
			return;
		}		
	}
		
	function startCalibration()
	{
		if (matchMedia('(display-mode: fullscreen)').matches) 
		{
			document.body.style.overflow = 'hidden';
			canvas.classList.add("fullscreen-canvas");
			finish = false;
			running = true;
			
			ctx.fillStyle = "black";
			ctx.fillRect(0, 0, canvas.width, canvas.height);
			
			drawImage();		
			setTimeout(() => {
				const calDebug = document.getElementById('chk_debug2').checked;
				const calPostprocessing = document.getElementById('chk_postprocessing2').checked;
				requestLutCalibration("capture", calDebug, calPostprocessing);				
			}, 100); 
		}
		else
			alert('Please run fullscreen mode (F11)');
	};

	const SCREEN_BLOCKS_X = 48;
	const SCREEN_BLOCKS_Y = 30;
	const SCREEN_COLOR_STEP = 16;
	const SCREEN_COLOR_DIMENSION = Math.floor(256 / SCREEN_COLOR_STEP) + 1;

	const SCREEN_YUV_RANGE_LIMIT = 2;

	const SCREEN_CRC_LINES = 2;
	const SCREEN_CRC_COUNT = 5;
	const SCREEN_MAX_CRC_BRIGHTNESS_ERROR = 1;
	const SCREEN_MAX_COLOR_NOISE_ERROR = 8;
	const SCREEN_SAMPLES_PER_BOARD = Math.floor(SCREEN_BLOCKS_X / 2) * (SCREEN_BLOCKS_Y - SCREEN_CRC_LINES);
	const SCREEN_LAST_BOARD_INDEX = Math.floor(Math.pow(SCREEN_COLOR_DIMENSION, 3) / SCREEN_SAMPLES_PER_BOARD);

	class int2 {
		constructor(_x, _y) {
		  this.x = _x;
		  this.y = _y;	  
		}
	}

	class byte3 {
		constructor(_x, _y, _z) {
		  this.x = _x;
		  this.y = _y;
		  this.z = _z;
		}
	}

	function indexToColorAndPos(index, color, position)
	{
		let currentIndex = index % SCREEN_SAMPLES_PER_BOARD;
		let boardIndex = Math.floor(index / SCREEN_SAMPLES_PER_BOARD);

		position.y = 1 + Math.floor(currentIndex / Math.floor(SCREEN_BLOCKS_X / 2));
		position.x = (currentIndex % Math.floor(SCREEN_BLOCKS_X / 2)) * 2 + ((position.y  + boardIndex  )% 2);

		const B = (index % SCREEN_COLOR_DIMENSION) * SCREEN_COLOR_STEP;
		const G = (Math.floor(index / (SCREEN_COLOR_DIMENSION)) % SCREEN_COLOR_DIMENSION) * SCREEN_COLOR_STEP;
		const R = Math.floor(index / (SCREEN_COLOR_DIMENSION * SCREEN_COLOR_DIMENSION)) * SCREEN_COLOR_STEP;

		color.x = Math.min(R, 255);
		color.y = Math.min(G, 255);
		color.z = Math.min(B, 255);

		return boardIndex;
	}

	function saveImage(delta, boardIndex, margin)
			{
				for (let line = 0; line < SCREEN_BLOCKS_Y; line += SCREEN_BLOCKS_Y - 1)
				{
					let position = new int2 ((line + boardIndex) % 2, line);

					for (let x = 0; x < boardIndex + SCREEN_CRC_COUNT; x++, position.x += 2)
					{
						const start = new int2 (position.x * delta.x, position.y * delta.y);
						const end = new int2 ( ((position.x + 1) * delta.x) - 1, ((position.y + 1) * delta.y) - 1);
						fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, 255, 255, 255);
					}
				}
			};

	function createTestBoards(boardIndex)
	{
		const margin = new int2(3, 2);
		let maxIndex = Math.pow(SCREEN_COLOR_DIMENSION, 3);
		const image = new int2(canvas.width, canvas.height);
		const delta = new int2(image.x / SCREEN_BLOCKS_X, image.y / SCREEN_BLOCKS_Y);

		ctx.fillStyle = 'black';
		ctx.fillRect(0,0, image.x, image.y);

		let lastBoard = 0;
		for (let index = 0; index < maxIndex; index++)
		{
			let color = new byte3(0,0,0);
			let position = new int2(0,0);
			let currentBoard = indexToColorAndPos(index, color, position);

			if (currentBoard < 0)
				return;

			if (boardIndex + 1 == currentBoard)
			{
				saveImage(delta, boardIndex, margin);
				return;
			}
			else if (boardIndex > currentBoard)
				continue;

			if (lastBoard != currentBoard)
			{
				ctx.fillStyle = 'black';
				ctx.fillRect(0,0, image.x, image.y);
			}

			lastBoard = currentBoard;

			const start = new int2 ( position.x * delta.x, position.y * delta.y);
			const end = new int2 ( ((position.x + 1) * delta.x) - 1, ((position.y + 1) * delta.y) - 1);

			fastBox(start.x + margin.x, start.y + margin.y, end.x - margin.x, end.y - margin.y, color.x, color.y, color.z);
		}
		saveImage(delta, boardIndex, margin);
	}

	function fastBox(x1, y1, x2, y2, c1, c2 ,c3)
	{
		ctx.fillStyle = `rgb(${c1}, ${c2}, ${c3})`;
		ctx.fillRect(x1, y1, (x2 - x1), (y2 - y1));
	}

	function drawImage()
	{
		clearInterval(myInterval);
		myInterval= setInterval( function(){          
			currentTestBoard = (currentTestBoard + 1) % (SCREEN_LAST_BOARD_INDEX + 1);
			createTestBoards(currentTestBoard);
		}, 3000);
		currentTestBoard = 0;
		createTestBoards(currentTestBoard);
	}
});
