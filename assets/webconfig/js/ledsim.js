$(document).ready(function() {
	var modalOpened = false;
	var ledsim_width = 540;
	var ledsim_height = 489;
	var dialog;
	var leds;
	var lC = false;
	var imageCanvasNodeCtx;
	var ledsCanvasNodeCtx;
	var canvas_height;
	var canvas_width;
	var twoDPaths = [];
	var toggleLeds, toggleLedsNum = false;

	/// add prototype for simple canvas clear() method
	CanvasRenderingContext2D.prototype.clear = function(){
		this.clearRect(0, 0, this.canvas.width, this.canvas.height)
	};

	function create2dPaths(){
		twoDPaths = [];
		for(var idx=0; idx<leds.length; idx++)
		{
			var led = leds[idx];
			twoDPaths.push( build2DPath(led.hmin * canvas_width, led.vmin * canvas_height, (led.hmax-led.hmin) * canvas_width, (led.vmax-led.vmin) * canvas_height, 5) );
		}
	}

	/**
	 * Draws a rounded rectangle into a new Path2D object, returns the created path.
	 * If you omit the last three params, it will draw a rectangle
	 * outline with a 5 pixel border radius
	 * @param {Number} x The top left x coordinate
	 * @param {Number} y The top left y coordinate
	 * @param {Number} width The width of the rectangle
	 * @param {Number} height The height of the rectangle
	 * @param {Number} [radius = 5] The corner radius; It can also be an object
	 *                 to specify different radii for corners
	 * @param {Number} [radius.tl = 0] Top left
	 * @param {Number} [radius.tr = 0] Top right
	 * @param {Number} [radius.br = 0] Bottom right
	 * @param {Number} [radius.bl = 0] Bottom left
	 * @return {Path2D} The final path
	 */
	 function build2DPath(x, y, width, height, radius) {
	  if (typeof radius == 'number') {
	    radius = {tl: radius, tr: radius, br: radius, bl: radius};
	  } else {
	    var defaultRadius = {tl: 0, tr: 0, br: 0, bl: 0};
	    for (var side in defaultRadius) {
	      radius[side] = radius[side] || defaultRadius[side];
	    }
	  }

	  var path = new Path2D();

	  path.moveTo(x + radius.tl, y);
	  path.lineTo(x + width - radius.tr, y);
	  path.quadraticCurveTo(x + width, y, x + width, y + radius.tr);
	  path.lineTo(x + width, y + height - radius.br);
	  path.quadraticCurveTo(x + width, y + height, x + width - radius.br, y + height);
	  path.lineTo(x + radius.bl, y + height);
	  path.quadraticCurveTo(x, y + height, x, y + height - radius.bl);
	  path.lineTo(x, y + radius.tl);
	  path.quadraticCurveTo(x, y, x + radius.tl, y);

	  return path;
	}

	$(window.hyperion).one("ready",function(){
		leds = window.serverConfig.leds;

		if(window.showOptHelp)
		{
			createHint('intro', $.i18n('main_ledsim_text') + '<p style="color:red"> Refresh rate is greatly reduced in the preview window.</p>', 'ledsim_text');
			$('#ledsim_text').css({'margin':'10px 15px 0px 15px'});
			$('#ledsim_text .bs-callout').css("margin","0px")
		}

		if(getStorage('ledsim_width') != null)
		{
			ledsim_width = getStorage('ledsim_width');
			ledsim_height = getStorage('ledsim_height');
		}

		dialog = $("#ledsim_dialog").dialog({
			uiLibrary: 'bootstrap',
			resizable: true,
			modal: false,
			minWidth: 250,
			width: ledsim_width,
			minHeight: 350,
			height: ledsim_height,
			closeOnEscape: true,
			autoOpen: false,
			title: $.i18n('main_ledsim_title'),
			resize: function (e) {
				updateLedLayout();
			},
			opened: function (e) {
				if(!lC)
				{
					updateLedLayout();
					lC = true;
				}
				modalOpened = true;
				requestLedColorsStart();

				if($('#leds_toggle_live_video').hasClass('btn-success'))
					requestLedImageStart();
			},
			closed: function (e) {
				modalOpened = false;
			},
			resizeStop: function (e) {
				setStorage("ledsim_width", $("#ledsim_dialog").outerWidth());
				setStorage("ledsim_height", $("#ledsim_dialog").outerHeight());
			}
		});
		// apply new serverinfos
		$(window.hyperion).on("cmd-config-getconfig",function(event){
			leds = event.response.info.leds;
			updateLedLayout();
		});
	});

	function printLedsToCanvas(colors)
	{
		// toggle leds, do not print
		if(toggleLeds)
			return;

		var useColor = false;
		var cPos = 0;
		ledsCanvasNodeCtx.clear();
		if(typeof colors != "undefined")
			useColor = true;

		// check size of ledcolors with leds length
		if(colors && colors.length/3 < leds.length)
			return;

		for(var idx=0; idx<leds.length; idx++)
		{
			var led = leds[idx];
			// can be used as fallback when Path2D is not available
			//roundRect(ledsCanvasNodeCtx, led.hmin * canvas_width, led.vmin * canvas_height, (led.hmax-led.hmin) * canvas_width, (led.vmax-led.vmin) * canvas_height, 4, true, colors[idx])
			//ledsCanvasNodeCtx.fillRect(led.hmin * canvas_width, led.vmin * canvas_height, (led.hmax-led.hmin) * canvas_width, (led.vmax-led.vmin) * canvas_height);

			ledsCanvasNodeCtx.fillStyle = (useColor) ?  "rgba("+colors[cPos]+","+colors[cPos+1]+","+colors[cPos+2]+",0.75)"  : "hsla("+(idx*360/leds.length)+",100%,50%,0.75)";
			ledsCanvasNodeCtx.fill(twoDPaths[idx]);
			ledsCanvasNodeCtx.stroke(twoDPaths[idx]);

			if(toggleLedsNum)
			{
				//ledsCanvasNodeCtx.shadowOffsetX = 1;
				//ledsCanvasNodeCtx.shadowOffsetY = 1;
				//ledsCanvasNodeCtx.shadowColor = "black";
				//ledsCanvasNodeCtx.shadowBlur = 4;
				ledsCanvasNodeCtx.fillStyle = "white";
				ledsCanvasNodeCtx.textAlign = "center";
				ledsCanvasNodeCtx.fillText(((led.name) ? led.name : idx), (led.hmin * canvas_width) + ( ((led.hmax-led.hmin) * canvas_width) / 2), (led.vmin * canvas_height) + ( ((led.vmax-led.vmin) * canvas_height) / 2));
			}

			// increment colorsPosition
			cPos += 3;
		}
	}

	function updateLedLayout()
	{
		//calculate body size
		canvas_height = $('#ledsim_dialog').outerHeight()-$('#ledsim_text').outerHeight()-$('[data-role=footer]').outerHeight()-$('[data-role=header]').outerHeight()-40;
		canvas_width = $('#ledsim_dialog').outerWidth()-30;

		$('#leds_canvas').html("");
		var leds_html = '<canvas id="image_preview_canv" width="'+canvas_width+'" height="'+canvas_height+'"  style="position: absolute; left: 0; top: 0; z-index: 99998;"></canvas>';
		leds_html += '<canvas id="leds_preview_canv" width="'+canvas_width+'" height="'+canvas_height+'"  style="position: absolute; left: 0; top: 0; z-index: 99999;"></canvas>';

		$('#leds_canvas').html(leds_html);

		imageCanvasNodeCtx = document.getElementById("image_preview_canv").getContext("2d");
		ledsCanvasNodeCtx = document.getElementById("leds_preview_canv").getContext("2d");
		ledsCanvasNodeCtx.strokeStyle = "rgb(80,80,80)";
		create2dPaths();
		printLedsToCanvas();
		resetImage();
	}

	// ------------------------------------------------------------------
	$('#leds_toggle_num').off().on("click", function() {
		toggleLedsNum = !toggleLedsNum
		toggleClass('#leds_toggle_num', "btn-danger", "btn-success");
	});
	// ------------------------------------------------------------------

	$('#leds_toggle').off().on("click", function() {
		toggleLeds = !toggleLeds
		ledsCanvasNodeCtx.clear();
		toggleClass('#leds_toggle', "btn-success", "btn-danger");
	});

	// ------------------------------------------------------------------
	$('#leds_toggle_live_video').off().on("click", function() {
		setClassByBool('#leds_toggle_live_video',window.imageStreamActive,"btn-success","btn-danger");
		if ( window.imageStreamActive )
		{
			requestLedImageStop();
			resetImage();
		}
		else
		{
			requestLedImageStart();
		}
	});

	// ------------------------------------------------------------------
	$(window.hyperion).on("cmd-ledcolors-ledstream-update",function(event){
		if (!modalOpened)
		{
			requestLedColorsStop();
		}
		else
		{
			printLedsToCanvas(event.response.result.leds)
		}
	});

	// ------------------------------------------------------------------
	$(window.hyperion).on("cmd-ledcolors-imagestream-update",function(event){
		if (!modalOpened)
		{
			requestLedImageStop();
		}
		else
		{
			var imageData = (event.response.result.image);

			var image = new Image();
			image.onload = function() {
			    imageCanvasNodeCtx.drawImage(image, 0, 0, canvas_width, canvas_height);
			};
			image.src = imageData;
		}
	});

	$("#btn_open_ledsim").off().on("click", function(event) {
		dialog.open();
	});

	// ------------------------------------------------------------------
	$(window.hyperion).on("cmd-settings-update",function(event){
		var obj = event.response.data
		Object.getOwnPropertyNames(obj).forEach(function(val, idx, array) {
			window.serverInfo[val] = obj[val];
	  	});
		leds = window.serverConfig.leds
		updateLedLayout();
	});

	function resetImage(){
		imageCanvasNodeCtx.fillStyle = "rgb(0,0,0)"
		imageCanvasNodeCtx.fillRect(0, 0, canvas_width, canvas_height);
		var image = new Image();
		image.onload = function() {
			var x = canvas_width/2 - 130;
			if (x<0)
				x=0;
			var y = (canvas_height/2 - 42) + 4;
			if (y<0)
				y=0;
			imageCanvasNodeCtx.drawImage(image, x, y);
		};
		image.src ='data:image/png;base64,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';		
	}
});
