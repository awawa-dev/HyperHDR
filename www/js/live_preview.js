$(document).ready(function() {

	var dialog;
	var modalOpened = false;
	var ledsim_width = 550;
	var ledsim_height = 489;

	var canvas_height;
	var canvas_width;
	var imageCanvasNodeCtx;
	var ledsCanvasNodeCtx;

	var leds;
	var lC = false;
	var twoDPaths = [];
	var lastFeedFrame = Date.now();
	var toggleLedsNum = false;

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
	};
	
	function build2DPath(x, y, width, height, radius) {

		if (typeof radius == 'number')
		{
			radius = {tl: radius, tr: radius, br: radius, bl: radius};
		}
		else
		{
			var defaultRadius = {tl: 0, tr: 0, br: 0, bl: 0};
			for (var side in defaultRadius)
			{
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
	};

	function takeCareButton(force = false)
	{
		if($("#live_preview_dialog").outerWidth() < 550 || force)
		{
			$('#vid_btn_1').text($.i18n('main_ledsim_btn_toggleleds').substring(0,4));
			$('#vid_btn_2').text($.i18n('main_ledsim_btn_togglelednumber').substring(0,4));
			$('#vid_btn_3').text($.i18n('main_ledsim_btn_togglelivevideo').substring(0,4));
			$('#vid_btn_4').text($.i18n('main_ledsim_btn_screenshot').substring(0,4));
		}
		else
		{			
			$('#vid_btn_1').text($.i18n('main_ledsim_btn_toggleleds'));
			$('#vid_btn_2').text($.i18n('main_ledsim_btn_togglelednumber'));
			$('#vid_btn_3').text($.i18n('main_ledsim_btn_togglelivevideo'));
			$('#vid_btn_4').text($.i18n('main_ledsim_btn_screenshot'));
		}
	};

	function getReady()
	{
		leds = window.serverConfig.leds;

		if(getStorage('ledsim_width') != null)
		{
			ledsim_width = getStorage('ledsim_width');
			ledsim_height = getStorage('ledsim_height');
		}

		dialog = $("#live_preview_dialog").dialog({
			uiLibrary: 'bootstrap5',
			resizable: true,
			modal: false,
			minWidth: 330,
			width: ledsim_width,
			minHeight: 320,
			height: ledsim_height,
			closeOnEscape: true,
			autoOpen: false,
			title: '<svg data-src="svg/live_preview.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('main_ledsim_title'),
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

				if($('#leds_toggle').hasClass('btn-success'))
					requestLedColorsStart();

				if($('#leds_toggle_live_video').hasClass('btn-success'))
					requestLedImageStart();
			},
			closed: function (e) {
				modalOpened = false;
				requestLedColorsStop();
				requestLedImageStop();
			},
			resizeStop: function (e) {
				setStorage("ledsim_width", $("#live_preview_dialog").outerWidth());
				setStorage("ledsim_height", $("#live_preview_dialog").outerHeight());
				
				takeCareButton();
				updateLedLayout();
			}
		});

		var ua = window.navigator.userAgent;
		var iOS = !!ua.match(/iPad/i) || !!ua.match(/iPhone/i);
		var webkitDefined = !!ua.match(/WebKit/i);
		var iOSSafari = iOS && webkitDefined && !ua.match(/CriOS/i);

		if (navigator.maxTouchPoints > 0 || iOSSafari)
		{
			var btnn = $("#live_preview_dialog").find("button.btn-close");
			if (btnn.length == 1 && !$(btnn[0]).parent().is($("#live_preview_dialog")))
			{
				$(btnn[0]).css('position','absolute');
				$(btnn[0]).css('right','10px');
				$(btnn[0]).css('top','20px');
				$("#live_preview_dialog").append( $(btnn[0]) );
			}
		}
		
		$(window.hyperhdr).on("cmd-config-getconfig",function(event){
			leds = event.response.info.leds;
			updateLedLayout();
		});

		$(window.hyperhdr).on("cmd-instance-switchTo", function (event) {
			setTimeout(function(){
				if($('#live_preview_dialog').is(':visible'))
				{
					if ( $('#leds_toggle_live_video').hasClass("btn-success"))
					{
						requestLedImageStart();					
					}
					if ( $('#leds_toggle').hasClass("btn-success"))
					{
						requestLedColorsStart();					
					}
				}
			}, 500);
		});
	};

	function resetImage(){
		imageCanvasNodeCtx.fillStyle = "rgb(0,0,0)"
		imageCanvasNodeCtx.fillRect(0, 0, canvas_width, canvas_height);

		var image = new Image();
		var sourceImg = document.getElementById("left_top_hyperhdr_logo");
		if (sourceImg.naturalWidth > 0 && sourceImg.naturalHeight > 0)
		{
			var x = Math.max(canvas_width/2 - 130, 0);
			var y = Math.max(canvas_height/2 - 38, 0);
			imageCanvasNodeCtx.drawImage(sourceImg, x, y, 262, 83);

			imageCanvasNodeCtx.font = "16px monospace";
			imageCanvasNodeCtx.textAlign = "center";
			imageCanvasNodeCtx.fillStyle = "white";

			if (!window.imageStreamActive)
			{
				imageCanvasNodeCtx.fillText($.i18n('preview_live_video_is_paused'), x + 131, y + 100);
			}
			else
			{
				imageCanvasNodeCtx.fillText($.i18n('preview_no_signal'), x + 131, y + 100);
			}
		}
	};	
	
	$(window.hyperhdr).one("ready",function(){
		getReady();		
	});

	function printLedsToCanvas(colors)
	{
		if(!window.ledStreamActive)
			return;

		var useColor = false;
		var cPos = 0;
		ledsCanvasNodeCtx.clear();

		if(typeof colors != "undefined")
			useColor = true;

		if(colors && colors.length/3 < leds.length)
			return;

		for(var idx=0; idx<leds.length; idx++)
		{
			var led = leds[idx];

			ledsCanvasNodeCtx.fillStyle = (useColor) ?  "rgba("+colors[cPos]+","+colors[cPos+1]+","+colors[cPos+2]+",0.75)"  : "hsla("+(idx*360/leds.length)+",100%,50%,0.75)";
			ledsCanvasNodeCtx.fill(twoDPaths[idx]);
			ledsCanvasNodeCtx.stroke(twoDPaths[idx]);

			if(toggleLedsNum)
			{
				ledsCanvasNodeCtx.fillStyle = "white";
				ledsCanvasNodeCtx.textAlign = "center";
				ledsCanvasNodeCtx.fillText(((led.name) ? led.name : idx), (led.hmin * canvas_width) + ( ((led.hmax-led.hmin) * canvas_width) / 2), 3 + (led.vmin * canvas_height) + ( ((led.vmax-led.vmin) * canvas_height) / 2));
			}

			cPos += 3;
		}
	};

	function leftPad(num, size) {
		num = num.toString();
		while (num.length < size) num = "0" + num;
		return num;
	};

	$('#leds_screenshot').off().on("click", function() {
		var link = document.createElement('a');
		var d = new Date();
		link.download = 'shot' + leftPad(d.getHours(),2) + leftPad(d.getMinutes(),2) + leftPad(d.getSeconds(),2) + '.png';
		link.href = document.getElementById("image_preview_canv").toDataURL()
		link.click();
		link.remove();
	});

	$('#leds_toggle').off().on("click", function() {		
		ledsCanvasNodeCtx.clear();
		setClassByBool('#leds_toggle', window.ledStreamActive, "btn-danger", "btn-success");
		if ( window.ledStreamActive )
		{
			requestLedColorsStop();
		}
		else
		{
			requestLedColorsStart();
		}
	});

	$('#leds_toggle_num').off().on("click", function() {
		toggleLedsNum = !toggleLedsNum;
		toggleClass('#leds_toggle_num', "btn-danger", "btn-success");
	});
	
	$('#leds_toggle_live_video').off().on("click", function() {
		setClassByBool('#leds_toggle_live_video', window.imageStreamActive, "btn-danger", "btn-success");
		if ( window.imageStreamActive )
		{
			lastFeedFrame = Date.now();
			requestLedImageStop();
			resetImage();
		}
		else
		{
			lastFeedFrame = Date.now();			
			requestLedImageStart();
			resetImage();
			feedWatcher();			
		}
	});

	$("#btn_open_ledsim").off().on("click", function(event) {
		
		if (dialog == null ||typeof dialog === 'undefined')
			getReady();
		
		if (window.innerWidth < 740)
		{
			$("#live_preview_dialog").width(330);
			$("#live_preview_dialog").height(320);
			
			$("#live_preview_dialog").css('top', '10px');
			$("#live_preview_dialog").css('left', '20px');			
		}
		else
		{
			var t = parseInt($('#live_preview_dialog').css('top'), 10);
			var l = parseFloat($('#live_preview_dialog').css('left'), 20);
			if ( !isNaN(t) && (t < 10 || t + 25 > window.innerHeight))
				$("#live_preview_dialog").css('top', '10px');
			if ( !isNaN(l) && (l < 20 || l + 50 > window.innerWidth))
				$("#live_preview_dialog").css('left', '20px');
		}
		$("#live_preview_dialog").css('position', 'fixed');
		
		takeCareButton();
		
		dialog.open();
		
		updateLedLayout();
	});

	function updateLedLayout()
	{
		canvas_height = $('#live_preview_dialog').outerHeight()-$('[data-role=footer]').outerHeight()-$('[data-role=header]').outerHeight()-20;
		canvas_width = $('#live_preview_dialog').outerWidth()-20;

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
	};


	function feedWatcher()
	{
		setTimeout(function(){
			if ( window.imageStreamActive )
			{
				var delta = Date.now() - lastFeedFrame;
				if (delta > 2000 && delta < 7000)
					resetImage();

				feedWatcher();
			};
		}, 2000);
	};


	$(window.hyperhdr).on("cmd-ledcolors-imagestream-update",function(event){
		if (!modalOpened)
		{
			requestLedImageStop();
		}
		else if (window.imageStreamActive)
		{
			lastFeedFrame = Date.now();

			var imageData = (event.response.result.image);			
			var image = new Image();

			image.onload = function() {
			    imageCanvasNodeCtx.drawImage(image, 0, 0, canvas_width, canvas_height);
			};

			image.src = imageData;
		}
	});

	
	$(window.hyperhdr).on("cmd-ledcolors-ledstream-update",function(event){
		if (!modalOpened)
		{
			requestLedColorsStop();
		}
		else
		{
			printLedsToCanvas(event.response.result.leds)
		}
	});

	
	$(window.hyperhdr).on("cmd-settings-update",function(event){
		var obj = event.response.data
		Object.getOwnPropertyNames(obj).forEach(function(val, idx, array) {
			window.serverInfo[val] = obj[val];
	  	});
		leds = window.serverConfig.leds
		updateLedLayout();
	});
	
});
