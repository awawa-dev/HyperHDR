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
		if (window.serverInfo.grabberstate != null)
		{
			$("#dash_current_video_device").html(window.serverInfo.grabberstate.device);
			$("#dash_current_video_mode").html(window.serverInfo.grabberstate.videoMode);
		}
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
		const waitingSpinner = '<svg data-src="svg/spinner_small.svg" fill="currentColor" class="svg4hyperhdr ms-1"></svg>' + $.i18n("perf_please_wait");
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
					`<span class="card-tools"><span class="badge bg-secondary" style="font-size: 1em;font-weight: normal;">${curElem.param1.toFixed(1)} fps</span></span>` +
					` <small>&nbsp;${curElem.param2}ms</small><svg data-src="svg/performance_clock.svg" fill="currentColor" class="svg4hyperhdr ms-1 me-2"></svg><small>${curElem.param3}</small><svg data-src="svg/performance_two_ways.svg" fill="currentColor" class="svg4hyperhdr ms-0 me-0"></svg>`+
					((curElem.param4 != 0)?`, ${$.i18n("perf_invalid_frames")}: <small>${curElem.param4}</small>`:``);
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
						let warningM = "";
						if (curElem.param4 > 0)
						 	warningM = ` <small>${curElem.param4}</small><svg data-src="svg/trash.svg" fill="currentColor" class="svg4hyperhdr"></svg>`;
						if (curElem.param4 > 120)
							warningM = `<span style="color:red">${warningM}</span>`;
						let render = (curElem.token <= 0) ? ((curElem.type == 2) ? `<span class="card-tools"><span class="badge bg-danger" style="font-size: 1em;font-weight: normal;">${curElem.name}</span></span>&nbsp;` : "") + waitingSpinner : (curElem.type == 2) ?
							`<span class="card-tools"><span class="badge bg-danger" style="font-size: 1em;font-weight: normal;">${curElem.name}</span></span> <span class="card-tools me-1"><span class="badge bg-secondary" style="font-size: 1em;font-weight: normal;">${curElem.param1.toFixed(1)} fps</span></span> <small>${curElem.param2}</small><svg data-src="svg/performance_two_ways.svg" fill="currentColor" class="svg4hyperhdr ms-0 me-0"></svg>` :
							`<span class="card-tools"><span class="badge bg-success" style="font-size: 1em;font-weight: normal;">${curElem.name}</span></span> <span class="card-tools me-1"><span class="badge bg-secondary" style="font-size: 1em;font-weight: normal;">${curElem.param1.toFixed(1)} fps</span></span> <small>${curElem.param3}</small><svg data-src="svg/performance_in.svg" style="width:8px;top:0px;" fill="currentColor" class="svg4hyperhdr ms-0 me-0"></svg> <small>${curElem.param2}</small><svg data-src="svg/performance_out.svg" style="width:8px;top:-2.5px;" fill="currentColor" class="svg4hyperhdr ms-0 me-0"></svg>${warningM}`;
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
					holderVOLTAGE.innerHTML = (curElem.name != "1") ? "<span class='stats-report-ok fw-bold'>" + $.i18n('perf_no') + "</span>" : "<span class='stats-report-error fw-bold'>" + $.i18n('general_btn_yes') + "</span>" + '<svg data-src="svg/performance_undervoltage.svg" class="svg4hyperhdr ms-1"></svg>';
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
		var components_html = '<div class="row border-bottom hyperhdr-vcenter text-primary" style="height:3em;">'+
								'<div class="col-9 col-md-8 fw-bold ps-3" data-i18n="dashboard_componentbox_label_comp">Component</div>'+
								'<div class="col-3 col-md-4 fw-bold text-center" data-i18n="dashboard_componentbox_label_status">Status</div>'+
							  '</div>';
		for (var idx = 0; idx < components.length; idx++)
		{
			if (components[idx].name != "ALL" && components[idx].name != "GRABBER")
				components_html += `<div class="row border-bottom hyperhdr-vcenter" style="height:3em;"><div class="col-9 col-md-8 ps-4 ${(components[idx].enabled ? "" : "text-muted")}">${$.i18n('general_comp_' + components[idx].name)}</div><div class="col-3 col-md-4 text-center"><svg data-src="svg/overview_component_${(components[idx].enabled ? "on" : "off")}.svg" fill="currentColor" class="svg4hyperhdr top-0 ps-1"></svg></div></div>`;
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

	function updateNetworkServices()
	{
		let networkSessions = window.serverInfo.sessions;
		if (networkSessions == null || networkSessions.length == 0)
		{
			$('#device_discovery_panel').addClass('d-none');
			return;
		}

		var networkSessions_html =
					'<div class="row border-bottom text-primary hyperhdr-vcenter" style="min-height:3em;">'+
						'<div class="col-4 col-md-3 fw-bold ps-1 ps-md-4 pe-2 pe-md-1" data-i18n="edt_conf_stream_device_title">Device</div>'+
						'<div class="col-4 col-md-4 fw-bold ps-0 pe-1 pe-md-2" data-i18n="device_address">Address</div>'+
						'<div class="col-4 col-md-5 fw-bold ps-0 pe-0 pe-md-1" data-i18n="edt_dev_spec_lights_name">Name</div>'+
					'</div>';

		for(var i = 0; i< networkSessions.length; i++)
		{
			let portName = (networkSessions[i].port >= 0) ? `:${networkSessions[i].port}` : "";

			networkSessions_html += `<div class="row border-bottom hyperhdr-vcenter" style="min-height:3em;">`+
										`<div class="col-4 col-md-3 ps-1 ps-md-4 pe-2 pe-md-1">${networkSessions[i].name}</div>`+
										`<div class="col-4 col-md-4 ps-0 pe-1 pe-md-2">${networkSessions[i].address}${portName}</div>`+
										`<div class="col-4 col-md-5 ps-0 pe-0 pe-md-1">${networkSessions[i].host}</div>`+
									`</div>`;
		}
		$("#device_discovery_container").html(networkSessions_html);

		$('#device_discovery_panel').removeClass('d-none');
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
	updateNetworkServices();
	updateGrabber();

	$(window.hyperhdr).on("components-updated", updateComponents);
	$(window.hyperhdr).off("cmd-sessions-received").on("cmd-sessions-received", updateNetworkServices);
	$(window.hyperhdr).on("grabberstate-update", updateGrabber);

	sendToHyperhdr("serverinfo", "", '"subscribe":["performance-update"]');
	$(window.hyperhdr).off("cmd-performance-update").on("cmd-performance-update", function (event)
	{
		updatePerformance(event.response.data);
	});

	requestPerformanceCounter(true);

	removeOverlay();
});
