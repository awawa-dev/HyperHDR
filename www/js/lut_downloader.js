$(document).ready( function(){
	let selectedLut = {};

	performTranslation();

	$("#lut_path_id").text($.i18n("main_menu_grabber_lut_path", window.serverInfo.grabbers.lut_for_hdr_path));
	if (window.serverInfo.grabbers.lut_for_hdr_exists === 1)
	{
		$("#lut_found_id").removeClass("d-none");
		var d = new Date(0);
		d.setUTCSeconds(window.serverInfo.grabbers.lut_for_hdr_modified_date/1000);
		$("#lut_found_date").text($.i18n("main_menu_grabber_lut_path_found_date", d.toLocaleString()));
		$("#lut_found_date").removeClass("d-none");

		if (window.serverInfo.grabbers.lutFastCRC != null)
			$("#lut_found_CRC").text($.i18n("main_menu_grabber_lut_path_found_CRC", window.serverInfo.grabbers.lutFastCRC));
		else
			$("#lut_found_CRC").text($.i18n("main_menu_grabber_lut_path_found_CRC", $.i18n("main_menu_grabber_lut_path_found_NOCRC")));
		$("#lut_found_CRC").removeClass("d-none");
	}
	else
	{
		$("#lut_not_found_id").removeClass("d-none");
	}

	function startDownloadWizard() {
		$('#wiz_header').html('<i class="fa fa-magic fa-fw"></i>' + $.i18n("main_menu_grabber_lut"));
		$('#wizp1_body').html('<h4 style="font-weight:bold;text-transform:uppercase;">' + $.i18n("perf_please_wait") + '</h4><div class="row pe-1 ps-2"><div class="col-12 p-3 mt-3 text-center"><i class="fa fa-refresh fa-spin align-middle" style="font-size: 50px;"></i></div></div>');
		$('#wizp2_body').html('<h4 id="download_summary_header" style="font-weight:bold;text-transform:uppercase;"></h4><p id="download_summary"></p>');
		$('#wizp1_footer').html('');
		$('#wizp2_footer').html('<button type="button" class="btn btn-success" name="btn_wiz_closeme_download"><i class="fa fa-fw fa-close"></i>' + $.i18n('general_btn_ok') + '</button>');
		//open modal
		var downWiz= new bootstrap.Modal($("#wizard_modal"), {
			backdrop: "static",
			keyboard: false
		});

		var backupDownWizard = $("#wizard_modal").css("z-index");
		$("#wizard_modal").css("z-index", "10000");

		$('#wizp1').toggle(true);
		$('#wizp2').toggle(false);

		downWiz.show();

		$("[name='btn_wiz_closeme_download']").off().on('click', function () {
			$("#wizard_modal").css("z-index", backupDownWizard);
			downWiz.hide();
			resetWizard(true);
			reload();
		});
	}

	function installLut(evt)
	{
		var btn = evt.currentTarget;
		selectedLut.lutBaseAddress = btn.lutBaseAddress;
		selectedLut.lutBrightness = btn.lutBrightness;
		selectedLut.lutContrast = btn.lutContrast;
		selectedLut.lutSaturation = btn.lutSaturation;

		showInfoDialog('confirm', $.i18n('main_menu_grabber_lut'), $.i18n('main_menu_grabber_lut_confirm'));

		$('#id_btn_confirm').off().on('click', function()
		{
				startDownloadWizard();
				requestLutInstall(selectedLut.lutBaseAddress, selectedLut.lutBrightness,
						selectedLut.lutContrast, selectedLut.lutSaturation, Date.now());
		});
	}

	function createColumn(columnData)
	{
		let newCol = document.createElement("div");
		newCol.setAttribute("class", "col border lutdownloader");
		if (columnData != null)
		{
			let img = document.createElement("img");
			img.src = columnData.src+"/image.jpg";
			img.setAttribute("class", "mt-3 mb-2");
			newCol.appendChild(img);
			let tx = document.createElement("div");
			tx.innerHTML = "<h4>"+columnData.name+"</h4>";
			newCol.appendChild(tx);
			if (columnData.src != null && columnData.src.length > 0)
			{
				let btn = document.createElement("button");
				btn.lutBaseAddress = columnData.src;
				btn.lutBrightness = columnData.brightness ?? 0;
				btn.lutContrast = columnData.contrast ?? 0;
				btn.lutSaturation = columnData.saturation ?? 0;
				btn.setAttribute("class", "btn btn-warning mb-4");
				btn.innerHTML = $.i18n('update_button_install');
				btn.addEventListener('click', installLut, false);
				newCol.appendChild(btn);
			}

		}
		return newCol;
	}

	var httpRequest = new XMLHttpRequest();
	httpRequest.onreadystatechange = function() {
		if (httpRequest.readyState === 4) {
			if (httpRequest.status === 200) {
				let data = JSON.parse(httpRequest.responseText);
				console.log(data);
				if (Array.isArray(data))
				{
					let mainElement = document.getElementById("LutGrid");
					let newRow = document.createElement("div");
					newRow.setAttribute("class", "row");

					for(let i = 0; i < data.length; i++)
					{
						newRow.appendChild(createColumn(data[i]));
					}
					mainElement.appendChild(newRow);
				}
			}
		}
	};
	//httpRequest.open('GET', 'https://raw.githubusercontent.com/awawa-dev/LUT4HDR/master/manifest.json');
	httpRequest.open('GET', 'https://awawa-dev.github.io/lut/manifest.json');
	httpRequest.send();

	$(window.hyperhdr).off("cmd-lut-install-update").on("cmd-lut-install-update", function(event)
	{
		let resElement = document.getElementById("download_summary_header");

		if (event.response.data.status == 1)
		{
			$('#download_summary_header').css('color', 'green');
			resElement.innerHTML = '<i class="fa fa-check-circle fa-fw"></i>' + $.i18n('infoDialog_general_success_title');
			resElement = document.getElementById("download_summary");
			resElement.innerHTML = $.i18n("main_menu_grabber_lut_restart", selectedLut.lutBrightness, selectedLut.lutContrast, selectedLut.lutSaturation);
		}
		else
		{
			$('#download_summary_header').css('color', 'red');
			resElement.innerHTML = '<i class="fa fa-exclamation-circle fa-fw"></i>' + $.i18n('infoDialog_general_error_title');
			resElement = document.getElementById("download_summary");
			resElement.innerHTML = event.response.data.error;
		}


		$('#wizp1').toggle(false);
		$('#wizp2').toggle(true);
	});

});
