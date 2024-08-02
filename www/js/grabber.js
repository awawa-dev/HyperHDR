$(document).ready( function(){
	performTranslation();
		
	var conf_video_section_editor = null;
	var conf_editor_videoControl = null;
	var conf_system_section_editor = null;
	var conf_editor_systemControl = null;	
	var globalCurrentDevice = null;
	var wizardTimer;
	var backupWizard;
	
	var VIDEO_AVAILABLE = window.serverInfo.grabbers != null && window.serverInfo.grabbers.available != null &&
							window.serverInfo.grabbers.available.find(element => element.indexOf("Video capturing")!=-1);
	var SYSTEM_AVAILABLE = (window.serverInfo.systemGrabbers != null);

	function startSignalWizard() {		
		$('#wiz_header').html('<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n("edt_conf_stream_wizard_calibration_title"));
		$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n("edt_conf_stream_wizard_prepare_for_calibration") + '</h4><p>' + $.i18n("edt_conf_stream_wizard_prepare_for_calibration_details") + '</p>');
		$('#wizp2_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n("edt_conf_stream_wizard_calibrating") + '</h4><div class="row pe-1 ps-2"><div class="col-12 p-3 mt-3 text-center"><svg data-src="svg/spinner_large.svg" fill="currentColor" class="svg4hyperhdr"></svg><br/><span class="ps-3 align-middle" id="live_calibration_log">'+$.i18n("edt_conf_stream_wizard_calibrating_message_waiting")+'</span></div></div>');
		$('#wizp3_body').html('<h4 id="live_calibration_summary_header" style="font-weight:bold;text-transform:uppercase;">' + $.i18n("edt_conf_stream_wizard_calibrating_summary") + '</h4><p id="live_calibration_summary"></p>');
		$('#wizp1_footer').html('<button type="button" class="btn btn-primary" id="btn_wiz_cont_calibration"><svg data-src="svg/button_play.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_continue') + '</button><button type="button" class="btn btn-danger" name="btn_wiz_closeme_calibration"><svg data-src="svg/button_close.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
		$('#wizp2_footer').html('<button type="button" class="btn btn-danger" id="btn_wiz_disrupt_calibration"><svg data-src="svg/button_stop.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_cancel') + '</button>');
		$('#wizp3_footer').html('<button type="button" class="btn btn-success" name="btn_wiz_closeme_calibration"><svg data-src="svg/button_success.svg" fill="currentColor" class="svg4hyperhdr"></svg></i>' + $.i18n('general_btn_ok') + '</button>');
		//open modal
		var sigWiz= new bootstrap.Modal($("#wizard_modal"), {
			backdrop: "static",
			keyboard: false
		});
		
		backupWizard = $("#wizard_modal").css("z-index");
		$("#wizard_modal").css("z-index", "10000");		
		
		$('#wizp1').toggle(true);
		$('#wizp2').toggle(false);
		$('#wizp3').toggle(false);

		sigWiz.show();
		
		
		var rectSim = $("#live_preview_dialog")[0].getBoundingClientRect();
		if (rectSim.width > 0 && rectSim.height > 0)
		{			
			setTimeout(function(){
				var mainWizEl = document.getElementById("wiz_header");
				if (mainWizEl != null && mainWizEl.parentElement != null)
				{
					var linkWiz = mainWizEl.parentElement;
					var rectWiz = linkWiz.getBoundingClientRect();
					$("#live_preview_dialog").css('top', (rectWiz.bottom + window.scrollY +10)+'px');
					$("#live_preview_dialog").css('left', (rectWiz.left + window.scrollX)+'px');
					if ( !window.imageStreamActive )
						$('#leds_toggle_live_video').trigger( "click" );					
				}
			},200);
		}
		

		//listen for continue
		$('#btn_wiz_cont_calibration').off().on('click', function () {			
			$('#wizp1').toggle(false);
			$('#wizp2').toggle(true);
			
			requestCalibrationStart();
			
			wizardTimer = window.setInterval(getWizardInfo, 400);
		});
		
		$('#btn_wiz_disrupt_calibration').off().on('click', function () {
			requestCalibrationStop();
			
			window.clearInterval(wizardTimer);
			$("#wizard_modal").css("z-index", backupWizard);
			sigWiz.hide();
			resetWizard(true);
		});
		
		$("[name='btn_wiz_closeme_calibration']").off().on('click', function () {
			$("#wizard_modal").css("z-index", backupWizard);
			sigWiz.hide();
			resetWizard(true);
		});
	}
	
	async function getWizardInfo()
	{	
		var backup = await requestCalibrationData();
				
		
		if (backup.info.running!== true)
		{
			var firstMes;
			var secondMes = $.i18n('edt_conf_stream_wizard_calibrating_final');;
			
			window.clearInterval(wizardTimer);
			
			if (backup.info.isSuccess === true)
			{
				firstMes = $.i18n('edt_conf_stream_wizard_calibrating_success');
				$('#live_calibration_summary_header').css('color', 'green');
			}
			else
			{
				firstMes = $.i18n('edt_conf_stream_wizard_calibrating_failed');
				$('#live_calibration_summary_header').css('color', 'red');
			}
			
			firstMes = firstMes.replace("<m_odel>", backup.info.model).replace("<q_uality>", backup.info.quality);
			secondMes = secondMes.replace("<sdrStat>", backup.info.sdrStat).replace("<sdrPoint>", backup.info.sdrPoint).replace("<hdrStat>", backup.info.hdrStat).replace("<hdrPoint>", backup.info.hdrPoint);
			
			$('#live_calibration_summary').html(firstMes + " "+secondMes);
			$('#wizp2').toggle(false);
			$('#wizp3').toggle(true);
		}
		else
		{
			$('#live_calibration_log').html(backup.info.status.
											replace("Processing SDR frames:", $.i18n("edt_conf_stream_wizard_calibrating_message_SDR")).
											replace("Processing HDR frames:", $.i18n("edt_conf_stream_wizard_calibrating_message_HDR")).
											replace("Waiting for the first SDR frame", $.i18n("edt_conf_stream_wizard_calibrating_message_waiting")).
											replace("Preparing for capturing SDR frame", $.i18n("edt_conf_stream_wizard_calibrating_message_prepare_SDR")).
											replace("Preparing for capturing HDR frame", $.i18n("edt_conf_stream_wizard_calibrating_message_prepare_HDR")).
											replace("Waiting for first HDR frame", $.i18n("edt_conf_stream_wizard_calibrating_message_waiting_first_HDR"))
											);
		}		
	}
	
	$('#ready_to_set').click(function(e){
		var rateEl = document.getElementsByClassName("button_rate_mode_selected");
		var videoEl = document.getElementsByClassName("button_vid_mode_selected");
		
		if (rateEl.length == 1 && videoEl.length == 1)
		{
			var obj = $(videoEl[0]);
			var objRate = $(rateEl[0]);
			
			var width = obj.data('width');
			var height = obj.data('height');
			var pf = obj.data('pf');			
			var fps = objRate.data('fps');
			
			if (width != null && height != null && pf != null && fps != null)
			{
				var path = 'root.videoGrabber.';
				conf_video_section_editor.getEditor(path + 'videoMode').setValue(width+"x"+height);
				conf_video_section_editor.getEditor(path + 'fps').setValue(fps);
				conf_video_section_editor.getEditor(path + 'videoEncoding').setValue(pf);

				$("#videoDeviceInfoPanel").modal('toggle');
				
				var commit = $('#btn_submit_videoGrabber');
				if (commit.attr('disabled') !== 'disabled')
				{
					commit.click();
				}
			}
		}
	});
	
	
	function BlockUnsupported()
	{
		if (window.serverInfo.grabbers != null && window.serverInfo.grabbers != undefined &&
		  window.serverInfo.grabbers.active != null && window.serverInfo.grabbers.active != undefined)
		{
			var grabbers = window.serverInfo.grabbers.active;
			if (grabbers.indexOf('macOS AVF') > -1)
			{
				conf_video_section_editor.getEditor('root.videoGrabber.input').disable();
				conf_video_section_editor.getEditor('root.videoGrabber.hardware_brightness').disable();
				conf_video_section_editor.getEditor('root.videoGrabber.hardware_contrast').disable();
				conf_video_section_editor.getEditor('root.videoGrabber.hardware_hue').disable();
				conf_video_section_editor.getEditor('root.videoGrabber.hardware_saturation').disable();
			}
			if (grabbers.indexOf('Media Foundation') > -1)
			{
				conf_video_section_editor.getEditor('root.videoGrabber.input').disable();         
			}
		}
	}
		
	
	if(VIDEO_AVAILABLE)
	{		
		function addRow4(col1, col2, col3, col4)
		{
			var parentRow = $("<div class='row mb-2 pt-2'>");
			
			var newRow = $("<div class='col-12 col-lg-3 vidcol4'>");
			newRow.append(col1);
			parentRow.append(newRow);
			
			newRow = $("<div class='col-12 col-lg-3 vidcol4'>");
			newRow.append(col2);
			parentRow.append(newRow);
			
			newRow = $("<div class='col-12 col-lg-3 vidcol4'>");
			newRow.append(col3);
			parentRow.append(newRow);
			
			newRow = $("<div class='col-12 col-lg-3 vidcol4'>");
			newRow.append(col4);			
			parentRow.append(newRow);
			
			$("#videoDeviceInfoTable").append(parentRow);
		}
		
		function addRow2(col1, col2, style = "", classCol1 = "", classCol2 = "")
		{
			var parentRow = $("<div class='row mb-2'>");
			
			var newRow = $("<div class='col-3 col-lg-2 text-bold " + classCol1 + "'>");
			newRow.append(col1);
			parentRow.append(newRow);
			
			newRow = $("<div class='col-9 col-lg-10 " + classCol2 + "' style='" + style + "'>");
			newRow.append(col2);
			parentRow.append(newRow);
			
			parentRow.append(newRow);
			$("#videoDeviceInfoTable").append(parentRow);
		}
		
		function addRow1(col1, classCol1 = "")
		{
			var parentRow = $("<div class='row mt-2'>");
			
			var newRow = $("<div class='col-12 text-bold " + classCol1 + "'>");
			newRow.append(col1);
			
			parentRow.append(newRow);
			$("#videoDeviceInfoTable").append(parentRow);
		}
		
		function myPad(inputText, len)
		{
			var ret = String(inputText);
			
			while (ret.length < len)
			{
				ret = ret + " ";
			}
			return ret;
		}
		
		function unButton(e, tag)
		{
			if (e.classList.contains(tag))
			{
				e.classList.add("btn-light");
				e.classList.remove("btn-success");
				e.classList.remove(tag);
			}
		}
		function selectMe(e, tag)
		{
			if (!e.currentTarget.classList.contains(tag))
			{
				var but = document.getElementsByClassName(tag);
				Array.prototype.forEach.call(but, function(button, index) {
					unButton(button, tag);
				});
				
				e.currentTarget.classList.add(tag);
				e.currentTarget.classList.remove("btn-light");
				e.currentTarget.classList.add("btn-success");
			}
			else
				unButton(e.currentTarget, tag);			
		}
		
		function cleanButtons()
		{
			if (document.getElementsByClassName("button_rate_mode_selected").length == 0 && 
				document.getElementsByClassName("button_vid_mode_selected").length == 0)
				{
					var butr = document.getElementsByClassName("button_rate_mode");
					Array.prototype.forEach.call(butr, function(button, index) {																								
							button.classList.remove("disabled");
						});
						
					var butv = document.getElementsByClassName("button_vid_mode");
					Array.prototype.forEach.call(butv, function(button, index) {																								
							button.classList.remove("disabled");
						});
				}
			
		}
		
		function CheckIsReady()
		{
			var trg = document.getElementById('ready_to_set');
			
			if (document.getElementsByClassName("button_rate_mode_selected").length == 1 && 
				document.getElementsByClassName("button_vid_mode_selected").length == 1)
				trg.classList.remove("disabled");
			else
				trg.classList.add("disabled");			
		}
		
		function videoClick(e)		
		{
			selectMe(e, "button_vid_mode_selected");
			
			if (e.currentTarget.classList.contains("button_vid_mode_selected"))
			{
				window.serverInfo.grabbers.video_devices.forEach(function(el) {
					if (el.device == globalCurrentDevice)
					{
						var obj = $(e.currentTarget);
						var width = obj.data('width');
						var height = obj.data('height');
						var pf = obj.data('pf');
					
						if (width != null && height != null && pf != null)
						{
							var controlList = new Array();
							
							el.videoModeList.forEach(function(mode) {
								if (!controlList.includes(mode.fps) && mode.width == width && mode.height == height && mode.pixel_format_info == pf)
								{
									controlList.push(mode.fps);
								}
							});
							
							var but = document.getElementsByClassName("button_rate_mode");
							Array.prototype.forEach.call(but, function(button, index) {																
								var objBtn = $(button);
								var fps = objBtn.data('fps');
								
								if (fps == null || !controlList.includes(fps))
								{
									unButton(button, "button_rate_mode_selected");
									button.classList.add("disabled");
								}
								else
									button.classList.remove("disabled");
							});
						};
					};
				});
			}
			else
				cleanButtons();
			
			CheckIsReady();
		}
		
		function rateClick(e)
		{			
			selectMe(e, "button_rate_mode_selected");
			
			if (e.currentTarget.classList.contains("button_rate_mode_selected"))
			{
				window.serverInfo.grabbers.video_devices.forEach(function(el) {
					if (el.device == globalCurrentDevice)
					{
						var obj = $(e.currentTarget);
						var fps = obj.data('fps');
					
						if (fps != null)
						{
							var controlList = new Array();
							
							el.videoModeList.forEach(function(mode) {
								var mark = mode.width+"_"+mode.height+"_"+mode.pixel_format_info;
								if (!controlList.includes(mark) && mode.fps == fps)
								{
									controlList.push(mark);
								}
							});

							var but = document.getElementsByClassName("button_vid_mode");
							Array.prototype.forEach.call(but, function(button, index) {																
								var objBtn = $(button);
								var width = objBtn.data('width');
								var height = objBtn.data('height');
								var pf = objBtn.data('pf');
								var mark = width+"_"+height+"_"+pf;
								
								if (width == null || height == null || pf == null || !controlList.includes(mark))
								{
									unButton(button, "button_vid_mode_selected");
									button.classList.add("disabled");
								}
								else
									button.classList.remove("disabled");
							});
						};
					};
				});
			}
			else
				cleanButtons();
			
			CheckIsReady();
		}
		
		function checkAndCreateMode(source, parentRow, tag)
		{
			var content = "";
			var modeButton = $("<button>");
			
			modeButton.addClass('btn btn-light w-100 ' + tag);
			
			if (typeof source === 'string' || source instanceof String)
			{
				content = source;
				modeButton.data('fps', Number(source));
			}
			else
			{
				content = myPad(source.width + "x" + source.height, 10) + myPad(source.pixel_format_info, 5);
				modeButton.data('width',source.width);
				modeButton.data('height',source.height);
				modeButton.data('pf',source.pixel_format_info);
			}
				
			
			modeButton.append(content);
			
			if (tag == 'button_vid_mode')
				modeButton.click(function(e){    
					videoClick(e);
				});
			else
				modeButton.click(function(e){    
					rateClick(e);
				});			
			
			var newRow = $("<div class='col-12 col-xs-6 col-sm-6 col-lg-3 p-1'>");
			newRow.append(modeButton);
			
			parentRow.append(newRow);
		}
		
		function buildModes(videoDevice)
		{				
			var controlList = new Array();
			var parentRow = $("<div class='row'>");
			
			addRow1($.i18n("edt_conf_stream_av_modes"),'videoDeviceInfoImpColumn border-top pt-2');
			videoDevice.videoModeList.forEach(function(el) {
				var control = el.width + "_" + el.height+"_"+el.pixel_format_info;
				if (!controlList.includes(control))
				{
					controlList.push(control);
					checkAndCreateMode(el, parentRow, 'button_vid_mode');
				};
			});

			
			$("#videoDeviceInfoTable").append(parentRow);
			
			if (controlList.length > 24)
			{
				var butr = document.getElementsByClassName("button_vid_mode");
				Array.prototype.forEach.call(butr, function(button, index) {																								
						button.classList.add("btn-sm");
					});
			}
			
		}
		
		function buildRefreshRates(videoDevice)
		{
			var parentRow = $("<div class='row'>");
			
			addRow1($.i18n("edt_conf_stream_ref_rates"),'videoDeviceInfoImpColumn border-top pt-2');		
			videoDevice.framerates.forEach(function(el) {
				checkAndCreateMode(el, parentRow, 'button_rate_mode');
			});
			
			$("#videoDeviceInfoTable").append(parentRow);	
		}
		
		function buildInfoPanel(selectedDevice)
		{			
			var button = $('<button>');
			var buttonIcon = $('<svg data-src="svg/info.svg" aria-hidden="true" fill="currentColor" class="svg4hyperhdr me-1" />');
			var correction = 0;
			
			if (document.getElementById('page-content').clientWidth <= 800)
				correction = 11;
			else if (document.getElementById('page-content').clientWidth <= 1024)
				correction = 7;
					  
			button.attr('id','video_info_button')
				  .attr('data-bs-toggle','modal')
				  .attr('data-bs-target','#videoDeviceInfoPanel')
				  .attr('style', "float:right;")
				  .css('width', String(21 + correction)+'%')
				  .addClass('btn btn-warning')
				  .append(buttonIcon)
				  .append('Info');
				  
			document.getElementById('root[videoGrabber][device]').style.width = String(77 - correction) + "%";
			document.getElementById('root[videoGrabber][device]').parentElement.appendChild(button[0]);
							
			addRow2($.i18n("edt_conf_stream_device_title"), selectedDevice, '', 'videoDeviceInfoImpColumn pt-2', 'videoDeviceInfoImpColumn pt-2 my-text-success');			
			
			window.serverInfo.grabbers.video_devices.forEach(function(el) {
				if (el.device == selectedDevice)
				{
					if (el.name != selectedDevice)
					{
						if (el.name.length > 24)
							addRow2($.i18n("edt_conf_stream_device_info"), el.name, 'overflow-wrap: break-word; font-size:85%;', 'videoDeviceInfoImpColumn border-top pt-2', 'videoDeviceInfoImpColumn text-secondary border-top pt-2');
						else
							addRow2($.i18n("edt_conf_stream_device_info"), el.name, 'overflow-wrap: break-word; ', 'videoDeviceInfoImpColumn border-top pt-2', 'videoDeviceInfoImpColumn text-secondary border-top pt-2');
					}
					
					addRow1($.i18n("edt_conf_stream_hard_cap"),'videoDeviceInfoImpColumn border-top pt-2');
					addRow4(el.videoControls.BrightnessDef.replace("Brightness:", $.i18n("edt_conf_stream_control_brightness")),
							el.videoControls.ContrastDef.replace("Contrast:", $.i18n("edt_conf_stream_control_contrast")),
							el.videoControls.SaturationDef.replace("Saturation:", $.i18n("edt_conf_stream_control_saturation")),
							el.videoControls.HueDef.replace("Hue:", $.i18n("edt_conf_stream_control_hue")));

					buildModes(el);
					
					buildRefreshRates(el);
				}
			});
		}
		
		function BuildCalibrationButton(enabled)
		{	
			$('#video_calibration_button').remove();		
			
			if (!enabled)
				return;			
			
			var placeHolder = document.getElementsByName('root[videoGrabber][autoSignalDetection]');
			if (placeHolder.length > 0)
			{
				var button = $('<button>');
				var buttonIcon = $('<svg data-src="svg/button_play.svg" aria-hidden="true" fill="currentColor" class="svg4hyperhdr me-1" />');			
								  
				button.attr('id','video_calibration_button')				 
				  .attr('style', "position:absolute; right:0px; top:0px; border-top-left-radius: 4px; border-bottom-left-radius: 4px;")
				  .addClass('btn btn-success')
				  .append(buttonIcon)
				  .append($.i18n("edt_conf_stream_calibrate_button"));
				  
				placeHolder[0].parentElement.appendChild(button[0]);
				
				button.off().on('click', function () {
					startSignalWizard();
				});				
			}			
		}
		
		// Watch all video dynamic fields
		function SelectedDeviceChanged(){
			var path = 'root.videoGrabber.';
			var val = conf_video_section_editor.getEditor(path+'device').getValue();				
			
			['input', 'videoMode', 'fps', 'videoEncoding'].forEach(function(item) {						
					if (val === 'auto')
					{
						conf_video_section_editor.getEditor(path + item).setValue('auto');
						conf_video_section_editor.getEditor(path + item).disable();
					}
					else
						conf_video_section_editor.getEditor(path + item).enable();
			});											
			
			BlockUnsupported();
			
			$('#video_info_button').remove();			
			$("#videoDeviceInfoTable").empty();						
			
			if (document.getElementById('root[videoGrabber][device]') != null)
			{
				if (val !== 'auto')										
					buildInfoPanel(val);			
				else
					document.getElementById('root[videoGrabber][device]').style.width = "100%";
			}
		}
		
		var setWatchers = function() {
			conf_video_section_editor.watch('root.videoGrabber.device', function() {
				BuildVideoEditor(conf_video_section_editor.getEditor('root.videoGrabber.device').getValue());
				SelectedDeviceChanged();		
			});
			
			conf_video_section_editor.watch('root.videoGrabber.autoSignalDetection', function() {
				BuildCalibrationButton(conf_video_section_editor.getEditor('root.videoGrabber.autoSignalDetection').getValue());
			});
		};	
	}

	
	 		
	if(VIDEO_AVAILABLE) 
	{
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/capturing_video.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_stream_heading_title"), 'editor_container_video_device', 'btn_submit_videoGrabber'));
		$('#conf_cont').append(createHelpTable(window.schema.videoGrabber.properties, $.i18n("edt_conf_stream_heading_title")));
	
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/capturing_video.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_instCapture_heading_title"), 'editor_container_videoControl', 'btn_submit_videoControl'));
		$('#conf_cont').append(createHelpTable(window.schema.videoControl.properties, $.i18n("edt_conf_instCapture_heading_title")));			

		// Instance Capture
		conf_editor_videoControl = createJsonEditor('editor_container_videoControl', { videoControl: window.schema.videoControl}, true, true, undefined, true);

		conf_editor_videoControl.on('change',function() {
			(conf_editor_videoControl.validate().length ) ? $('#btn_submit_videoControl').attr('disabled', true) : $('#btn_submit_videoControl').attr('disabled', false);
		});

		$('#btn_submit_videoControl').off().on('click',function() {
			requestWriteConfig(conf_editor_videoControl.getValue());
		});
	}

	if(VIDEO_AVAILABLE)
		BuildVideoEditor();	
		
	if(SYSTEM_AVAILABLE) 
	{
		window.schema.systemGrabber.properties.device.enum = ["auto"];
		window.schema.systemGrabber.properties.device.options = {};
		window.schema.systemGrabber.properties.device.options.enum_titles = ["edt_conf_enum_automatic"];
		
		for(var i = 0; i < window.serverInfo.systemGrabbers.modes.length; i++)
		{
			var name = (window.serverInfo.systemGrabbers.modes[i]).toString();			
			window.schema.systemGrabber.properties.device.enum.push(name);
			window.schema.systemGrabber.properties.device.options.enum_titles.push(name);
		}
		
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/capturing_software.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_system_heading_title"), 'editor_container_system_device', 'btn_submit_systemGrabber'));
		$('#conf_cont').append(createHelpTable(window.schema.systemGrabber.properties, $.i18n("edt_conf_system_heading_title")));
		
		$('#conf_cont').append(createOptPanel('<svg data-src="svg/capturing_software.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_system_control_heading_title"), 'editor_container_systemControl', 'btn_submit_systemControl'));
		$('#conf_cont').append(createHelpTable(window.schema.systemControl.properties, $.i18n("edt_conf_system_control_heading_title")));			
		
		conf_system_section_editor = createJsonEditor('editor_container_system_device', { systemGrabber : window.schema.systemGrabber}, true, true);
		conf_editor_systemControl = createJsonEditor('editor_container_systemControl', { systemControl: window.schema.systemControl}, true, true, undefined, true);

		conf_editor_systemControl.on('change',function() {
			(conf_editor_systemControl.validate().length ) ? $('#btn_submit_systemControl').attr('disabled', true) : $('#btn_submit_systemControl').attr('disabled', false);
		});

		$('#btn_submit_systemControl').off().on('click',function() {
			requestWriteConfig(conf_editor_systemControl.getValue());
		});
		
		$('#btn_submit_systemGrabber').off().on('click',function() 
		{
			var systemSaveOptions = conf_system_section_editor.getValue();
			requestWriteConfig(systemSaveOptions);
		});
		
		if (window.serverInfo.systemGrabbers.device.indexOf("DirectX11")<0)
		{
			if (window.serverInfo.systemGrabbers.device.indexOf("macOS")>=0)
				createHint("intro", $.i18n('conf_grabber_macOs_intro'), "editor_container_system_device");
			else if (window.serverInfo.systemGrabbers.device.indexOf("X11")>=0)
				createHint("intro", $.i18n('conf_grabber_x11_intro'), "editor_container_system_device");
			else if (window.serverInfo.systemGrabbers.device.indexOf("pipewire")>=0)
				createHint("intro", $.i18n('conf_grabber_pipewire_intro'), "editor_container_system_device");
			else if (window.serverInfo.systemGrabbers.device.indexOf("framebuffer")>=0)
				createHint("intro", $.i18n('conf_grabber_framebuffer_intro'), "editor_container_system_device");			
			$('[data-schemapath="root.systemGrabber.hdrToneMapping"]').toggle(false);
			$('[data-schemapath="root.systemGrabber.hardware"]').toggle(false);
		}
		else
			createHint("intro", $.i18n('conf_grabber_dx11_intro'), "editor_container_system_device");
	}
	
	if (window.serverInfo.hasCEC != 1)
	{	
		$('[data-schemapath="root.videoGrabber.cecHdrStart"]').toggle(false);
		$('[data-schemapath="root.videoGrabber.cecHdrStop"]').toggle(false);
		$('[data-schemapath="root.videoControl.cecControl"]').toggle(false);
		$('[data-schemapath="root.systemControl.cecControl"]').toggle(false);
	}
	
	function checkExists(arr, newVal)
	{
		if (arr.includes(newVal))
			return false;
		else
			arr.push(newVal);
		
		return true;
	}
	
	function BuildVideoEditor(currentDevice = null)
	{		
		if (currentDevice == null)
			currentDevice = window.serverConfig.videoGrabber.device;

		if (conf_video_section_editor != null)
			conf_video_section_editor.destroy();
		
		window.schema.videoGrabber.properties.device.enum = ["auto"];
		window.schema.videoGrabber.properties.device.options = {};
		window.schema.videoGrabber.properties.device.options.enum_titles = ["edt_conf_enum_automatic"];
		window.schema.videoGrabber.properties.input.enum = [-1];
		window.schema.videoGrabber.properties.input.options = {};
		window.schema.videoGrabber.properties.input.options.enum_titles = ["edt_conf_enum_automatic"];
		window.schema.videoGrabber.properties.videoMode.enum = ["auto"];
		window.schema.videoGrabber.properties.videoMode.options = {};
		window.schema.videoGrabber.properties.videoMode.options.enum_titles = ["edt_conf_enum_automatic"];
		window.schema.videoGrabber.properties.fps.enum = [0];
		window.schema.videoGrabber.properties.fps.options = {};
		window.schema.videoGrabber.properties.fps.options.enum_titles = ["edt_conf_enum_automatic"];
		window.schema.videoGrabber.properties.videoEncoding.enum = ["auto"];
		window.schema.videoGrabber.properties.videoEncoding.options = {};
		window.schema.videoGrabber.properties.videoEncoding.options.enum_titles = ["edt_conf_enum_automatic"];
		
		for(var i = 0; i < window.serverInfo.grabbers.video_devices.length; i++)
		{
			var name = (window.serverInfo.grabbers.video_devices[i].device).toString();			
			window.schema.videoGrabber.properties.device.enum.push(name);
			window.schema.videoGrabber.properties.device.options.enum_titles.push(name);
		}
		
		globalCurrentDevice = currentDevice;
		
		var currentInfo = window.serverInfo.grabbers.video_devices.find(el => el.device === currentDevice);
		
		if (typeof currentInfo !== "undefined")
		{			
			for(var i = 0; i < currentInfo.videoModeList.length; i++)
			{
				var name = (currentInfo.videoModeList[i].width).toString() + "x" + (currentInfo.videoModeList[i].height).toString();			
				if (checkExists(window.schema.videoGrabber.properties.videoMode.enum,name))
					window.schema.videoGrabber.properties.videoMode.options.enum_titles.push(name);
			
				var valFps = parseInt((currentInfo.videoModeList[i].fps).toString());
				if (checkExists(window.schema.videoGrabber.properties.fps.enum, valFps))
					window.schema.videoGrabber.properties.fps.options.enum_titles.push(valFps.toString());
				
				name = (currentInfo.videoModeList[i].pixel_format_info).toString();
				if (checkExists(window.schema.videoGrabber.properties.videoEncoding.enum, name))
					window.schema.videoGrabber.properties.videoEncoding.options.enum_titles.push(name);
			}
			
			for(var i = 0; i < currentInfo.inputs.length && currentInfo.inputs.length > 1; i++)
			{
				var inputnr = parseInt((currentInfo.inputs[i].inputIndex).toString());            
				var name = (currentInfo.inputs[i].inputName).toString();
				if (checkExists(window.schema.videoGrabber.properties.input.enum, inputnr))
					window.schema.videoGrabber.properties.input.options.enum_titles.push(inputnr.toString() + ": " + name);
			}
		}
		
		
		conf_video_section_editor = createJsonEditor('editor_container_video_device', { videoGrabber : window.schema.videoGrabber}, true, true);
		conf_video_section_editor.getEditor('root.videoGrabber.device').setValue(currentDevice);
		setWatchers();
		BuildCalibrationButton(window.serverConfig.videoGrabber.autoSignalDetection);
		
		conf_video_section_editor.on('change',function() {
			if (conf_video_section_editor.validate().length )
				$('#btn_submit_videoGrabber').attr('disabled', true);
			else
				$('#btn_submit_videoGrabber').attr('disabled', false);
		});

		conf_video_section_editor.on('ready', function() {
			SelectedDeviceChanged();
		});

		$('#btn_submit_videoGrabber').off().on('click',function() 
		{
			var videoSaveOptions = conf_video_section_editor.getValue();
			requestWriteConfig(videoSaveOptions);
		});
		
		//create introduction
		if(window.showOptHelp) 
		{    
			createHint("intro", $.i18n('conf_grabber_video_intro'), "editor_container_video_device");
		};

	};  

	removeOverlay();
});
