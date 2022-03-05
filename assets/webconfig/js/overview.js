if (perfTimer == undefined || perfTimer == null)
{
	var perfTimer = setInterval(function ()
	{
		let counters = document.getElementsByClassName("perf_counter");
		if (counters != null && counters.length > 0)
		{
			for (let i = 0; i < counters.length; i++)
			{
				let pElem = counters[i].innerHTML;
				pElem = pElem.substring(1, pElem.length - 1);
				if (!isNaN(pElem))
				{
					counters[i].innerHTML = `(${Math.max(Number(pElem) - 1, 0)})`;
				}
			}
		}
		else
		{			
			clearInterval(perfTimer);
			perfTimer = null;
		}
	}, 1000);
}

if (sysPerfTimer == undefined || sysPerfTimer == null)
{
	var sysPerfTimer = setInterval(function ()
	{
		let counters = document.getElementById("performance_counters_card");
		if (counters != null)
		{
			requestPerformanceCounter(false);
		}
		else
		{			
			clearInterval(sysPerfTimer);
			sysPerfTimer = null;
		}
	}, 1000);
}

$(document).ready(function ()
{
	let performance = [];

	performTranslation();

	putInstanceName(document.getElementById("compent_status_header"));

	function updateGrabber()
	{
		$("#dash_current_video_device").html(window.serverInfo.grabberstate.device);
		$("#dash_current_video_mode").html(window.serverInfo.grabberstate.videoMode);
	}

	function insertReport(report)
	{
		let inserted = false;

		for (var i = 0; i < performance.length; i++)
		{
			if (performance[i].type == report.type && performance[i].id == report.id)
			{
				if (report.remove == 0)
				{
					performance[i] = report;
					inserted = true;
				}
				else
				{
					performance.splice(i, 1);

					if (report.type == 1)
					{
						document.getElementById("perf_usb_data_holder").innerHTML = "";
						let grabberContainer = document.getElementById("perf_grabber_data");
						if (grabberContainer != null)
						{
							grabberContainer.classList.add("d-none");
						}
					}
					else if (report.type == 2 || report.type == 3)
					{
						let holder = document.getElementById("perf_per_instance_data_row" + report.id);

						if (!(holder === null))
						{
							let placer = (report == 2) ? holder.firstElementChild : holder.lastElementChild;

							if (!(placer === null))
								placer.parentNode.removeChild(placer);

							if (holder.childElementCount == 0)
								holder.parentNode.removeChild(holder);
						}
					}

					return;
				}
			}
		}

		if (!inserted && report.remove == 0)
		{
			performance.push(report);
		}
	}

	function updatePerformance(report)
	{
		if (document.getElementById("perf_per_instance_data_row") == null)
			return;

		if (report.hasOwnProperty("broadcast"))
		{
			for (var i = 0; i < report.broadcast.length; i++)
			{
				insertReport(report.broadcast[i]);
			}
		}
		else
			insertReport(report);

		renderReport();
	}

	function renderReport()
	{
		const waitingSpinner = '<i class="fa fa-spinner fa-spin"></i> ' + $.i18n("perf_please_wait");
		const fpsTag = '<span class="card-tools"><span class="badge bg-secondary" style="font-weight: normal;">fps</span></span>';

		for (var i = 0; i < performance.length; i++)
		{
			if (performance[i].hasOwnProperty("rendered"))
				continue;

			const curElem = performance[i];

			curElem.rendered = true;

			if (curElem.type == 1)
			{
				let render = (curElem.token <= 0) ? waitingSpinner :
					`<span class="card-tools"><span class="badge bg-secondary" style="font-size: .9em;font-weight: normal;">${curElem.param1.toFixed(2)} fps</span></span>` +
					` <small>${$.i18n("perf_decoding_time")}: ${curElem.param2}ms, ${$.i18n("perf_frames")}: ${curElem.param3}, ${$.i18n("perf_invalid_frames")}: ${curElem.param4}</small>`;
				render += ` <span class='perf_counter small text-muted'>(${curElem.refresh})</span>`;

				let content = document.getElementById("perf_usb_data_holder");

				if (content!=null)
					content.innerHTML = render;
				
				let grabberContainer = document.getElementById("perf_grabber_data");
				if (grabberContainer != null)
				{
					grabberContainer.classList.remove("d-none");
				}
			}
			else if (curElem.type == 2 || curElem.type == 3)
			{
				let idElem = "perf_per_instance_data_row" + curElem.id;
				let holder = document.getElementById(idElem);

				if (holder == null)
				{
					let hook = document.getElementById("perf_per_instance_data_holder");
					if (hook != null)
					{
						holder = document.getElementById("perf_per_instance_data_row").cloneNode(true);
						holder.id = idElem;
						holder.classList.add("border-bottom");
						hook.appendChild(holder);
					}
				}

				if (holder != null)
				{
					let placer = (curElem.type == 2) ? holder.firstElementChild : holder.lastElementChild;

					if (placer != null)
					{
						let render = (curElem.token <= 0) ? ((curElem.type == 2) ? `<span class="card-tools"><span class="badge bg-danger" style="font-size: .9em;font-weight: normal;">${curElem.name}</span></span>&nbsp;` : "") + waitingSpinner : (curElem.type == 2) ?
							`<span class="card-tools"><span class="badge bg-danger" style="font-size: .9em;font-weight: normal;">${curElem.name}</span></span> <span class="card-tools"><span class="badge bg-secondary" style="font-size: .9em;font-weight: normal;">${curElem.param1.toFixed(2)} fps</span></span> <small>${curElem.param2}</small> <i class="fa fa-long-arrow-down" aria-hidden="true"></i><i class="fa fa-long-arrow-up" aria-hidden="true"></i>` :
							`<span class="card-tools"><span class="badge bg-success" style="font-size: .9em;font-weight: normal;">${curElem.name}</span></span> <span class="card-tools"><span class="badge bg-secondary" style="font-size: .9em;font-weight: normal;">${curElem.param1.toFixed(2)} fps</span></span> <small>${curElem.param3} </small><i class="fa fa-long-arrow-down" aria-hidden="true"></i>  <small>${curElem.param2}</small> <i class="fa fa-long-arrow-up" aria-hidden="true"></i>`;
						render += ` <span class='perf_counter small text-muted'>(${curElem.refresh})</span>`;
						placer.innerHTML = render;
					}
				}
			}
			else if (curElem.type == 4)
			{
				let holderCPU = document.getElementById("perf_cpu_usage");
				if (holderCPU != null)
				{
					holderCPU.innerHTML = curElem.name;
				}
				holderCPU = document.getElementById("perf_cell_cpu_usage");
				if (holderCPU != null)
				{
					holderCPU.classList.remove("d-none");
				}
				holderCPU = document.getElementById("perf_cell_hardware");
				if (holderCPU != null)
				{
					holderCPU.classList.remove("d-none");
				}
			}
			else if (curElem.type == 5)
			{
				let holderRAM = document.getElementById("perf_ram_usage");
				if (holderRAM != null)
				{
					holderRAM.innerHTML = curElem.name;
				}
				holderRAM = document.getElementById("perf_cell_ram_usage");
				if (holderRAM != null)
				{
					holderRAM.classList.remove("d-none");
				}
				holderRAM = document.getElementById("perf_cell_hardware");
				if (holderRAM != null)
				{
					holderRAM.classList.remove("d-none");
				}
			}
			else if (curElem.type == 6)
			{				
				let holderTEMP = document.getElementById("perf_temperature");
				if (holderTEMP != null)
				{
					holderTEMP.innerHTML = curElem.name + "℃";
				}
				holderTEMP = document.getElementById("perf_cell_temperature");
				if (holderTEMP != null)
				{
					holderTEMP.classList.remove("d-none");
				}
				holderTEMP = document.getElementById("perf_cell_linux");
				if (holderTEMP != null)
				{
					holderTEMP.classList.remove("d-none");
				}
			}
			else if (curElem.type == 7)
			{
				let holderVOLTAGE = document.getElementById("perf_undervoltage");
				if (holderVOLTAGE != null)
				{
					holderVOLTAGE.innerHTML = (curElem.name != "1") ? "<font color='ForestGreen'><b>" + $.i18n('perf_no') + "</b></font>" : "<font color='red'><b>" + $.i18n('general_btn_yes') + "</b>&nbsp;<i class='fa fa-exclamation-triangle' aria-hidden='true'></i></font>";
				}
				holderVOLTAGE = document.getElementById("perf_cell_undervoltage");
				if (holderVOLTAGE != null)
				{
					holderVOLTAGE.classList.remove("d-none");
				}
				holderVOLTAGE = document.getElementById("perf_cell_linux");
				if (holderVOLTAGE != null)
				{
					holderVOLTAGE.classList.remove("d-none");
				}
			}
		}
	}

	function updateComponents()
	{
		var components = window.comps;
		var components_html = "";
		for (var idx = 0; idx < components.length; idx++)
		{
			if (components[idx].name != "ALL" && components[idx].name != "GRABBER")
				components_html += '<tr><td>' + $.i18n('general_comp_' + components[idx].name) + '</td><td style="padding-left: 1.5em;"><i class="fa fa-circle component-' + (components[idx].enabled ? "on" : "off") + '"></i></td></tr>';
		}
		$("#tab_components").html(components_html);

		//info
		var hyperhdr_enabled = true;

		components.forEach(function (obj)
		{
			if (obj.name == "ALL")
			{
				hyperhdr_enabled = obj.enabled
			}
		});

		var instancename = window.currentHyperHdrInstanceName;

		$('#dash_statush').html(hyperhdr_enabled ? '<span style="color:green">' + $.i18n('general_btn_on') + '</span>' : '<span style="color:red">' + $.i18n('general_btn_off') + '</span>');
		$('#btn_hsc').html(hyperhdr_enabled ? '<button class="btn btn-sm btn-danger" onClick="requestSetComponentState(\'ALL\',false)">' + $.i18n('dashboard_infobox_label_disableh', instancename) + '</button>' : '<button class="btn btn-sm btn-success" onClick="requestSetComponentState(\'ALL\',true)">' + $.i18n('dashboard_infobox_label_enableh', instancename) + '</button>');
	}

	// add more info
	$('#dash_leddevice').html(window.serverConfig.device.type);
	$('#dash_currv').html(window.currentVersion);
	$('#dash_instance').html(window.currentHyperHdrInstanceName);

	getReleases(function (callback)
	{
		if (callback)
		{
			$('#dash_latev').html(window.latestVersion.tag_name);

			if (compareHyperHdrVersion(window.latestVersion.tag_name, window.currentVersion))
				$('#versioninforesult').html('<div class="callout callout-warning" style="margin:0px"><a target="_blank" href="' + window.latestVersion.html_url + '">' + $.i18n('dashboard_infobox_message_updatewarning', window.latestVersion.tag_name) + '</a></div>');
			else
				$('#versioninforesult').html('<div class="callout callout-success" style="margin:0px">' + $.i18n('dashboard_infobox_message_updatesuccess') + '</div>');

		}
	});


	//determine platform
	var html = "";

	if (window.serverInfo.grabbers != null && window.serverInfo.grabbers != undefined &&
		window.serverInfo.grabbers.active != null && window.serverInfo.grabbers.active != undefined)
	{
		var grabbers = window.serverInfo.grabbers.active;
		if (grabbers.indexOf('Media Foundation') > -1)
			html += 'Windows (Microsoft Media Foundation)';
		else if (grabbers.indexOf('macOS AVF') > -1)
			html += 'macOS (AVF)';
		else if (grabbers.indexOf('V4L2') > -1)
			html += 'Linux (V4L2)';
		else
			html += 'Unknown';
	}
	else
	{
		html = 'None';
	}

	$('#dash_platform').html(html);


	//interval update
	updateComponents();
	updateGrabber();

	$(window.hyperhdr).on("components-updated", updateComponents);
	$(window.hyperhdr).on("grabberstate-update", updateGrabber);

	sendToHyperhdr("serverinfo", "", '"subscribe":["performance-update"]');
	$(window.hyperhdr).off("cmd-performance-update").on("cmd-performance-update", function (event)
	{
		updatePerformance(event.response.data);
	});

	requestPerformanceCounter(true);

	removeOverlay();
});
