var prevTag;

function removeOverlay()
{
	$("#loading_overlay").removeClass("overlay");
}

function reload()
{
	location.reload();
}


function debugMessage(msg)
{
	if (window.debugMessagesActive)
	{
		console.log(msg);
	}
}

function validateDuration(d)
{
	if(typeof d === "undefined" || d < 0)
		return 0;
	else
		return d *= 1000;
}

function getHashtag()
{
	if(getStorage('lasthashtag', true) != null)
		return getStorage('lasthashtag', true);
	else
	{
		var tag = document.URL;
		tag = tag.substr(tag.indexOf("#") + 1);
		if(tag == "" || typeof tag === "undefined" || tag.startsWith("http"))
			tag = "overview";
		return tag;
	}
}

function loadContent(event, forceRefresh)
{
	var tag;

	var lastSelectedInstance = getStorage('lastSelectedInstance', false);

	if (lastSelectedInstance && (lastSelectedInstance != window.currentHyperHdrInstance))
		if ((typeof lastSelectedInstance !== 'undefined') && 
			(typeof window.serverInfo.instance[lastSelectedInstance] !== 'undefined') &&
			 typeof(window.serverInfo.instance[lastSelectedInstance].running) !== 'undefined' && window.serverInfo.instance[lastSelectedInstance].running)
		{
			$("#page-content").off();
			instanceSwitch(lastSelectedInstance);
			return;
		}
		else
			removeStorage('lastSelectedInstance', false);

	if(typeof event != "undefined")
	{
		tag = event.currentTarget.hash;
		tag = tag.substr(tag.indexOf("#") + 1);
		setStorage('lasthashtag', tag, true);
	}
	else
		tag = getHashtag();

	if(forceRefresh || prevTag != tag)
	{
		if (window.loggingStreamActive)
		{
			requestLoggingStop();
		}

		tag = encodeURIComponent(tag);
		prevTag = tag;
		$("#page-content").off();
		$("#page-content").load("/content/"+tag+".html", function(response,status,xhr){
			if(status == "error")
			{
				console.log("Could not find: "+tag+" page");
				removeStorage('lastSelectedInstance', false);
				tag = 'overview';
				setStorage('lasthashtag', tag, true);
				
				$("#page-content").load("/content/"+tag+".html", function(response,status,xhr){
					if(status == "error")
					{
						var resultContent = document.getElementById('page-content');
						resultContent.innerHTML = `<h3>${encodeURIComponent(tag)}</h3><br/>${$.i18n('info_404')}`;
						removeOverlay();
					}					
				});
			}
		});
	}
}

function updateHyperhdrInstanceListing()
{
	let hasItems = false;
	let data = window.serverInfo.instance.filter(entry => entry.running);
	$('#hyperhdr_instances_list').html("");

	if (data.length > 1)
	{
		hasItems = true;
		for(var key in data)
		{
			var currInstMarker = (data[key].instance == window.currentHyperHdrInstance) ? 'data-src="svg/instances_top_menu_indicator.svg"' : '';
			var currInstMarkerBackground = (data[key].instance == window.currentHyperHdrInstance) ? 'text-success' : 'instance-unselected-marker';
			var currTextMarker = (data[key].instance == window.currentHyperHdrInstance) ? "my-text-success" : "";
		
			var myName = data[key].friendly_name;
		
			if (myName.length>20)
				myName = myName.slice(0,17) + '...';
		
			var html = `<li id="hyperhdrinstance_${data[key].instance}"><a>`+
							'<div class="d-flex" style="cursor: pointer;">'+							
								`<div class="flex ps-2 pe-1 ${currInstMarkerBackground}">`+
									`<svg xmlns="http://www.w3.org/2000/svg" ${currInstMarker} width="16" height="16" fill="currentColor" style="position: relative;top: -2px;"></svg>`+
								'</div>'+
								`<div class="flex pe-2 ${currTextMarker}">`+
									`<span class="h-100" style="display: inline-flex; align-items: center;">${myName}</span>`+
								'</div>'+
							'</div>'+
					   '</a></li>';

			if(data.length-1 > key)
				html += '<li class="dropdown-divider"></li>';

			$('#hyperhdr_instances_list').append(html);

			$('#hyperhdrinstance_'+data[key].instance).off().on("click",function(e){
				instanceSwitch(e.currentTarget.id.split("_")[1]);
			});
		}
	}

	let hyperHDRs = window.serverInfo.sessions;
	if (hyperHDRs != null && hyperHDRs.length > 0)
	{
		for(var i = 0; i< hyperHDRs.length; i++)
			if(hyperHDRs[i].name == "HyperHDR")
			{				
				if (i == 0)
				{
					if (hasItems)
						$('#hyperhdr_instances_list').append('<li class="dropdown-divider bg-info" style="border-top-width:2px;"></li>');
				}
				else if (hasItems)
					$('#hyperhdr_instances_list').append('<li class="dropdown-divider bg-info"></li>');
				

				var hostName = hyperHDRs[i].host;

				if (hostName.length > 0)
				{
					hostName = hostName.replace(/"/g, "'");
				}

				var html = `<li id="remote_hyperhdrinstance_${i}" class="text-info" data-toggle="tooltip" data-placement="right" title="${hostName}"><a>`+
						'<div class="d-flex" style="cursor: pointer;">'+							
							`<div class="flex ps-2 pe-1">`+
								`<svg xmlns="http://www.w3.org/2000/svg" data-src="svg/button_link.svg" width="16" height="16" fill="currentColor" style="position: relative;top: -2px;"></svg>`+
							'</div>'+
							`<div class="flex pe-2">`+
								`<span class="h-100" style="display: inline-flex; align-items: center;">${hyperHDRs[i].address}:${hyperHDRs[i].port}</span>`+
							'</div>'+
						'</div>'+
					'</a></li>';
				$('#hyperhdr_instances_list').append(html);

				const destAddress = `http://${hyperHDRs[i].address}:${hyperHDRs[i].port}`;
				$(`#remote_hyperhdrinstance_${i}`).off().on("click",function(e){
					$("#loading_overlay").addClass("overlay");
					window.location.href = destAddress;
				});

				hasItems = true;
			}		
	}

	
	if (hasItems)
	{			
		$('#btn_hypinstanceswitch').removeClass('disabled');		
	}
	else
	{
		$('#btn_hypinstanceswitch').addClass('disabled');
		$('#hyperhdr_instances_list').removeClass('show');
	}
}

function switchLang(newLang)
{
	if (newLang !== storedLang)
	{		
		setStorage("langcode", newLang);
		reload();
	}
}

function initLanguageSelection()
{		
	var langLocale = storedLang;

	// If no language has been set, resolve browser locale
	if ( langLocale === 'auto' )
	{
		langLocale = $.i18n().locale.substring(0,2);
	}

	// Resolve text for language code
	var langText = 'Please Select';

	//Test, if language is supported by hyperhdr
	var langIdx = availLang.indexOf(langLocale);
	if ( langIdx > -1 )
	{
		langText = availLangText[langIdx];
	}
	else
	{
		// If language is not supported by hyperhdr, try fallback language
		langLocale = $.i18n().options.fallbackLocale.substring(0,2);	
		langIdx = availLang.indexOf(langLocale);
		if ( langIdx > -1 )
		{
			langText = availLangText[langIdx];
		}
	}
	
	var newTree = $("#language_container_menu");
	newTree.empty();
	for (var i = 0; i < availLang.length; i++)
	{		
		var newLang = $('<li>');
		
		var newLink = $('<a>');		
		newLink.addClass("nav-link");
		
		const sendId = availLang[i];
		newLink.on("click", function(){
			switchLang(sendId);
		});
		
		var ilink = `<svg ${(i == langIdx) ? 'data-src="svg/main_menu_lang_selected.svg"' : ''} fill="currentColor" class="svg4hyperhdr"></svg>`;		
		var item = $('<p>');
		item.html(availLangText[i]); 
		
		$(newLink).append(ilink);
		$(newLink).append(item);
		$(newLang).append(newLink);
		$(newTree).append(newLang);		
	}
}


function instanceSwitch(inst)
{
	if (window.serverInfo == null || window.serverInfo.currentInstance != inst)
		requestInstanceSwitch(inst);

	window.currentHyperHdrInstance = inst;
	window.currentHyperHdrInstanceName = getInstanceNameByIndex(inst);
	setStorage('lastSelectedInstance', inst, false)
	updateHyperhdrInstanceListing();
}

function loadContentTo(containerId, fileName)
{
	$(containerId).load("/content/"+fileName+".html");
}

function toggleClass(obj,class1,class2)
{
	if ( $(obj).hasClass(class1))
	{
		$(obj).removeClass(class1);
		$(obj).addClass(class2);
	}
	else
	{
		$(obj).removeClass(class2);
		$(obj).addClass(class1);
	}
}


function setClassByBool(obj,enable,class1,class2)
{
	if (enable)
	{
		$(obj).removeClass(class2);
		$(obj).addClass(class1);
	}
	else
	{
		$(obj).removeClass(class1);
		$(obj).addClass(class2);
	}
}

function toaster(type, header, message, delay)
{
	let textelemHeader = document.getElementById('toast_message_header_id');
	textelemHeader.innerText  = header;
	let textelemBody = document.getElementById('toast_message_body_id');
	textelemBody.innerText  = message;
	let myToastEl = document.getElementById('toast_success_message');			
	let myToast = bootstrap.Toast.getOrCreateInstance(myToastEl);
	if (delay <= 0)
		myToast.show();
	else
		setTimeout(function(){bootstrap.Toast.getOrCreateInstance(document.getElementById('toast_success_message')).show();}, delay);
}

function showInfoDialog(type,header,message)
{
	let headerControl = $('#new_modal_dialog_header');
	let masterControl = $('#new_modal_dialog');

	headerControl.removeClass();
	headerControl.addClass("modal-header modal-hyperhdr-header");
	masterControl.removeClass();
	masterControl.addClass("modal fade");	

	if (type=="error" || type=="warning" || type=="success" || type == "confirm")
	{
		let selectedButton = "";
		let selectedIcon = "";
		let selectedButtonText = "";

		if (type=="error")
		{
			if (header == "") header = $.i18n('infoDialog_general_error_title');
			headerControl.addClass("modal-hyperhdr-header-danger");
			selectedButton = 'btn-danger';
			selectedIcon = 'modal-icon-error';
			selectedSvg = "message_error.svg";
			selectedButtonText = $.i18n('general_btn_continue');
		}
		else if (type=="warning" || type == "confirm")
		{
			if (header == "") header = $.i18n('infoDialog_general_warning_title');
			headerControl.addClass("modal-hyperhdr-header-warning");
			selectedButton = 'btn-warning';
			selectedIcon = 'modal-icon-warning';
			selectedSvg = "message_warning.svg";
			selectedButtonText = $.i18n('general_btn_continue');
		}
		else if (type=="success")
		{
			if (header == "") header = $.i18n('infoDialog_general_success_title');
			headerControl.addClass("modal-hyperhdr-header-success");
			selectedButton = 'btn-success';
			selectedIcon = 'modal-icon-check';
			selectedSvg = "message_success.svg";
			selectedButtonText = $.i18n('general_btn_ok');
		}


		if (message.length > 120)
			masterControl.addClass("modal-lg");
		masterControl.addClass("modal-hyperhdr-danger-warning-success");	

		$('#new_modal_dialog_title').html('<h4 class="text-center">'+ header +'</h4>');
		$('#new_modal_dialog_body').html('<div style="align-items: center; display: flex;">'+
											`<div style="position: relative; left: 0px; ">`+
												`<svg data-src="svg/${selectedSvg}" fill="currentColor" class="svg4hyperhdr ${selectedIcon}" style="position:static;"></svg>`+
											'</div>'+
											'<h5 class="ps-3">'+ message +'</h5>'+											
										 '</div>');

		if (type == "confirm")
		{
			$('#new_modal_dialog_footer').html('<button type="button" id="id_btn_confirm" class="btn btn-warning" data-bs-dismiss="modal"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_yes')+'</button>');
			$('#new_modal_dialog_footer').append('<button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_cancel.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_cancel')+'</button>');
		}
		else
			$('#new_modal_dialog_footer').html(`<button type="button" class="btn ${selectedButton}" data-bs-dismiss="modal" data-bs-target="#new_modal_dialog"><svg data-src="svg/button_continue.svg" fill="currentColor" class="svg4hyperhdr"></svg>`+selectedButtonText+'</button>');
	}
	else if (type == "deleteInstance")
	{
		$('#new_modal_dialog_title').html('<h4><svg data-src="svg/button_delete.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+ header +'</h4>');
		$('#new_modal_dialog_body').html('<div class="mb-3">'+ message +'</div>');
		$('#new_modal_dialog_footer').html('<button type="button" id="id_btn_yes" class="btn btn-warning" data-bs-dismiss="modal" data-bs-target="#new_modal_dialog"><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_yes')+'</button>');
		$('#new_modal_dialog_footer').append('<button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_cancel.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_cancel')+'</button>');
	}
	else if (type == "renameInstance")
	{
		$('#new_modal_dialog_title').html('<h4><svg data-src="svg/button_edit.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_rename')+'</h4>');
		$('#new_modal_dialog_body').html('<div class="mb-3"><label class="form-label required">'+ header +'</label><input  id="renameInstance_name" type="text" class="form-control"></div>');
		$('#new_modal_dialog_footer').html('<button type="button" id="id_btn_ok" class="btn btn-success" data-bs-dismiss="modal" data-bs-target="#new_modal_dialog" disabled><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_ok')+'</button>');
		$('#new_modal_dialog_footer').append('<button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_cancel.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_cancel')+'</button>');
	}
	else if (type == "changePassword")
	{
		$('#new_modal_dialog_title').html('<h4><svg data-src="svg/change_password.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+header+'</h4>');
		$('#new_modal_dialog_body').html('<div class="mb-3"><label class="form-label required">'+$.i18n('modal_old_password')+'</label><input  id="oldPw" type="text" class="form-control"></div>');
		$('#new_modal_dialog_body').append('<div class="mb-3"><label class="form-label required">'+$.i18n('modal_new_password')+'</label><input  id="newPw" type="text" class="form-control"></div>');
		$('#new_modal_dialog_footer').html('<button type="button" id="id_btn_ok" class="btn btn-success" data-bs-dismiss="modal" data-bs-target="#new_modal_dialog" disabled><svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_ok')+'</button>');
		$('#new_modal_dialog_footer').append('<button type="button" class="btn btn-danger" data-bs-dismiss="modal"><svg data-src="svg/button_cancel.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_btn_cancel')+'</button>');
	}
	else if (type == "checklist")
	{
		$('#id_body').html('<img style="margin-bottom:20px" src="img/hyperhdr/hyperhdrlogo.png" alt="Redefine ambient light!">');
		$('#id_body').append('<h4 style="font-weight:bold;text-transform:uppercase;">'+$.i18n('infoDialog_checklist_title')+'</h4>');
		$('#id_body').append(header);
		$('#id_footer').html('<button type="button" class="btn btn-primary" data-bs-dismiss="modal">'+$.i18n('general_btn_ok')+'</button>');
	}
	else if (type == "newToken")
	{
		$('#id_body').html('<img style="margin-bottom:20px" src="img/hyperhdr/hyperhdrlogo.png" alt="Redefine ambient light!">');
		$('#id_footer').html('<button type="button" class="btn btn-primary" data-bs-dismiss="modal">'+$.i18n('general_btn_ok')+'</button>');
	}
	else if (type == "grantToken")
	{
		$('#id_body').html('<img style="margin-bottom:20px" src="img/hyperhdr/hyperhdrlogo.png" alt="Redefine ambient light!">');
		$('#id_footer').html('<button type="button" class="btn btn-primary" data-bs-dismiss="modal" id="tok_grant_acc">'+$.i18n('general_btn_grantAccess')+'</button>');
		$('#id_footer').append('<button type="button" class="btn btn-danger" data-bs-dismiss="modal" id="tok_deny_acc">'+$.i18n('general_btn_denyAccess')+'</button>');
	}

	if (type != "confirm" && type != "renameInstance" && type != "changePassword" && type != "deleteInstance" &&
		type != "error" && type != "warning" && type != "success")
	{
		$('#id_body').append('<h4 style="font-weight:bold;text-transform:uppercase;">'+header+'</h4>');
		$('#id_body').append(message);
	}
	
	const myTarget = ((type == "renameInstance" || type == "changePassword" || type == "deleteInstance" ||
						type=="error" || type=="warning" || type=="success" || type == "confirm") ? "#new_modal_dialog" : "#modal_dialog");
	
	const modal = new bootstrap.Modal($(myTarget), {
		backdrop : "static",
		keyboard: false
	});
	
	modal.show();

	$(document).on('click', myTarget, function (event) {
		var source = $(event.target);
		const target = modal;
		if (source.hasClass('modal-static'))
			target.hide();
	});
}

function createHintH(type, text, container)
{	
	$('#'+container).prepend('<div class="callout '+type+'"><h4 style="font-size:16px">'+text+'</h4></div><hr/>');
}

function createHint(type, text, container, buttonid, buttontxt)
{
	var fe, tclass;

	if(type == "intro")
	{
		fe = '';
		tclass = "intro-hint";
	}
	else if(type == "wizard")
	{
		fe = '<svg data-src="svg/wizard.svg" fill="currentColor" class="svg4hyperhdr"></svg>';
		tclass = "wizard-hint";
	}

	if(buttonid)
		buttonid = '<p><button id="'+buttonid+'" class="btn btn-wizard" style="margin-top:15px;">'+text+'</button></p>';
	else
		buttonid = "";

	if(type == "intro")
		$('#'+container).prepend('<div class="callout callout-primary" style="margin-top:0px"><h4 class="bs-main-text">'+ fe + $.i18n("conf_helptable_expl")+'</h4>'+text+'</div>');
	else if(type == "wizard")
		$('#'+container).prepend('<div class="callout callout-wizard" style="margin-top:0px"><h4 class="bs-main-text">'+ fe + $.i18n("wiz_wizavail")+'</h4>'+$.i18n('wiz_guideyou',text)+buttonid+'</div>');
}

function createEffHint(title, text)
{
	return '<div class="callout callout-primary" style="margin-top:0px"><h4>'+title+'</h4>'+text+'</div>';
}

function valValue(id,value,min,max)
{
	if(typeof max === 'undefined' || max == "")
		max = 999999;

	if(Number(value) > Number(max))
	{
		$('#'+id).val(max);
		showInfoDialog("warning","",$.i18n('edt_msg_error_maximum_incl',max));
		return max;
	}
	else if(Number(value) < Number(min))
	{
		$('#'+id).val(min);
		showInfoDialog("warning","",$.i18n('edt_msg_error_minimum_incl',min));
		return min;
	}
	return value;
}

function readImg(input,cb)
{
    if (input.files && input.files[0]) {
        var reader = new FileReader();
		// inject fileName property
		reader.fileName = input.files[0].name

        reader.onload = function (e) {
			cb(e.target.result, e.target.fileName);
        }
        reader.readAsDataURL(input.files[0]);
    }
}

function isJsonString(str)
{
	try
	{
		JSON.parse(str);
	}
	catch (e)
	{
		return e;
	}
	return "";
}

function isSmallScreen()
{
	return ($("#page-content").outerWidth() < 380);
}

function putInstanceName(targetplace)
{
	var lastSelectedInstance = getStorage('lastSelectedInstance', false);
	var instName = (lastSelectedInstance!=null) ? getInstanceNameByIndex(lastSelectedInstance) : getInstanceNameByIndex(0);
	var maxLen = 22;
	
	if (isSmallScreen())
		maxLen = 19;
	
	if (instName.length > maxLen+1+3)
		instName= instName.slice(0,maxLen)+'...';
	
	var con = document.createElement('div');
	con.classList.add("card-tools");
	var span = document.createElement('span');
	span.classList.add("badge", "bg-danger");
	span.style.fontWeight = "normal";
	
	span.innerHTML = instName;
	con.appendChild(span);
	
	targetplace.appendChild(con);
}

function createJsonEditor(container, schema, setconfig, usePanel, arrayre = undefined, showInstance = false)
{
	var targetPlace = document.getElementById(container);
	
	if (targetPlace.parentElement != null && targetPlace.parentElement.parentElement !=null &&
		!targetPlace.parentElement.parentElement.classList.contains('editor_column'))
	{
		targetPlace.parentElement.parentElement.classList.add('editor_column');
	}
	
	$('#'+container).off();
	$('#'+container).html("");

	if (typeof arrayre === 'undefined')
		arrayre = true;
	
	var editor = new JSONEditor(targetPlace,
	{
		theme: 'bootstrap5hyperhdr',
		disable_collapse: 'true',
		disable_edit_json: true,
		disable_properties: true,
		disable_array_reorder: arrayre,
		no_additional_properties: true,
		disable_array_delete_all_rows: true,
		disable_array_delete_last_row: true,
		schema: {
			title:'',
			properties: schema
		},
		translateProperty: function (key) {
			return $.i18n(key);
		}
	});
	//alert(JSON.stringify(editor));
	if(usePanel)
	{
		$('#'+container+' .well').first().removeClass('well well-sm');
		$('#'+container+' h4').first().remove();
		$('#'+container+' .well').first().removeClass('well well-sm');
	}
	
	if (showInstance && targetPlace != null && targetPlace.parentElement != null && targetPlace.parentElement.firstElementChild.classList.contains('card-header'))
	{
		putInstanceName(targetPlace.parentElement.firstElementChild);
	}

	if (setconfig)
	{
		for(var key in editor.root.editors)
		{
			editor.getEditor("root."+key).setValue(Object.assign({}, editor.getEditor("root."+key).value, window.serverConfig[key] ));
		}
	}

	return editor;
}

function buildWL(link,linkt,cl)
{
	
	return '';
}

function rgbToHex(rgb)
{
	if(rgb.length == 3)
	{
		return "#" +
		("0" + parseInt(rgb[0],10).toString(16)).slice(-2) +
		("0" + parseInt(rgb[1],10).toString(16)).slice(-2) +
		("0" + parseInt(rgb[2],10).toString(16)).slice(-2);
	}
	else
		debugMessage('rgbToHex: Given rgb is no array or has wrong length');
}

function hexToRgb(hex) {
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16)
    } : {
        r: 0,
        g: 0,
        b: 0
    };
}

/*
	Show a notification
	@param type     Valid types are "info","success","warning","danger"
	@param message  The message to show
	@param title     A title (optional)
	@param addhtml   Add custom html to the notification end
 */
function showNotification(type, message, title="", addhtml="")
{
	if(title == "")
	{
		switch(type)
		{
			case "info":
			title = $.i18n('infoDialog_general_info_title');
			break;
			case "success":
			title = $.i18n('infoDialog_general_success_title');
			break;
			case "warning":
			title = $.i18n('infoDialog_general_warning_title');
			break;
			case "danger":
			title = $.i18n('infoDialog_general_error_title');
			break;
		}
	}
	let alertId = 'alert_' + Date.now();
	let progressId = 'progress_' + alertId;
	let code = `<div id="${alertId}" class="alert alert-dismissible fade show mt-2 pe-1 pb-1 parentAlert" role="alert">
		<div class="notIcon hyperhdr-vcenter">
			<svg data-src="svg/notification_warning.svg" style="width:2em;padding-bottom: 15px;" fill="currentColor" class="svg4hyperhdr hidden-xs me-0"></svg>
		</div><div class="alertProgress alertProgressAnim" style="height:0%;z-index:1;"></div><div class="alertProgress bg-secondary h-100" style="z-index:0;"></div>
		<h5><b>${title}</b></h5><hr/>${message}${addhtml}	
		<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>					
	</div>`;

	let targetPlace = document.getElementById("notification-target");
	targetPlace.innerHTML += code;
	
	setTimeout(function(i) {
		let target = document.getElementById(i);
		if (target!=null)
		{
			target.classList.add("remove-alert");		
			setTimeout(function(j) {
				let k = document.getElementById(j);
				if (k != null)
					k.remove();
			}, 500, i);
		}
	}, 5000, alertId);
}


// Creates a table with thead and tbody ids
// @param string hid  : a class for thead
// @param string bid  : a class for tbody
// @param string cont : a container id to html() the table
// @param string bless: if true the table is borderless
function createTable(hid, bid, cont, bless, tclass)
{
	var table = document.createElement('table');
	var thead = document.createElement('thead');
	var tbody = document.createElement('tbody');

	table.className = "table";
	if(bless === true)
		table.className += " borderless";
	if(typeof tclass !== "undefined")
		table.className += " "+tclass;
	table.style.marginBottom = "0px";
	if(hid != "")
		thead.className = hid;
	tbody.className = bid;
	if(hid != "")
		table.appendChild(thead);
	table.appendChild(tbody);

	$('#'+cont).append(table);
}

function createDivTable(hid, bid, cont)
{
	var table = document.createElement('div');
	var thead = document.createElement('div');
	var tbody = document.createElement('div');

	table.className = "table";
	table.style.marginBottom = "0px";
	thead.className = hid + " container";
	tbody.className = bid + " container";
		
	table.appendChild(thead);	
	table.appendChild(tbody);

	$('#'+cont).append(table);
}

// Creates a table row <tr>
// @param array list :innerHTML content for <td>/<th>
// @param bool head  :if null or false it's body
// @param bool align :if null or false no alignment
//
// @return : <tr> with <td> or <th> as child(s)
function createTableRow(list, head, align)
{
	var row = document.createElement('div');
	
	row.className = 'row mb-2';

	for(var i = 0; i < list.length; i++)
	{
		var el = document.createElement('div');
		
		if (list.length == 2)
		{
			if (i==0)
				el.className = 'col-3';
			else
				el.className = 'col-9';
		}
		else
			el.className = 'col';
		
		if(head === true)
			el.style.fontWeight = "bold";
		
		if(align)
			el.style.verticalAlign = "middle";

		el.innerHTML = list[i];
		row.appendChild(el);
	}
	return row;
}

function createTableFlex(hid, bid, cont, bless, tclass)
{	
	var table = document.createElement('div');
	var thead = document.createElement('div');
	var tbody = document.createElement('div');

	table.className = "container";
	
	if(bless === true)
		table.className += " borderless";
	else
		thead.style.borderBottom = "thin solid lightgray";
	
	if(typeof tclass !== "undefined")
		table.className += " "+tclass;
	table.style.marginBottom = "0px";
	if(hid != "")
		thead.className = hid;
	tbody.className = bid;
	if(hid != "")
		table.appendChild(thead);
	table.appendChild(tbody);

	$('#'+cont).append(table);
}

function createTableRowFlex(list, head, align)
{
	var row = document.createElement('div');
	
	row.className = 'd-flex flex-row mb-2';

	for(var i = 0; i < list.length; i++)
	{
		var el = document.createElement('div');
		
		if (list.length == 2)
		{
			if (i==0)
				el.className = 'd-flex w-100 p-2';
			else
				el.className = 'w-100 d-flex p-2 justify-content-center';
		}
		else 
			el.className = 'w-100 d-flex p-2';
		
		if(head === true)
			el.style.fontWeight = "bold";
		
		if(align)
			el.style.verticalAlign = "middle";

		el.innerHTML = list[i];
		row.appendChild(el);
	}
	return row;
}

function createRow(id)
{
	var el = document.createElement('div');
	el.className = "row";
	el.setAttribute('id', id);
	return el;
}

function createOptPanel(phicon, phead, bodyid, footerid)
{
	phead = phicon+phead;

	var saveBtn = document.createElement('button');
	saveBtn.className = "btn btn-primary";
	saveBtn.setAttribute("id", footerid);
	saveBtn.innerHTML = '<svg data-src="svg/button_save.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('general_button_savesettings');

	const helpBtn = document.createElement('button');
	helpBtn.className = "btn btn-warning btn-warning-noset";
	helpBtn.style.cssFloat = "left";
	helpBtn.innerHTML = '<svg data-src="svg/button_help.svg" fill="currentColor" class="svg4hyperhdr"></svg>'+$.i18n('panel_help_button');
	helpBtn.addEventListener("click", function() 
		{
			var clientObj = helpBtn.parentElement.parentElement.parentElement.parentElement;
			var target = clientObj.nextElementSibling;
			var act = helpBtn.classList.contains("btn-warning-set");
			if (!act)
			{
				target.classList.remove("d-none");
				helpBtn.classList.add("btn-warning-set");
				helpBtn.classList.remove("btn-warning-noset");
				if($(window).width() >= 977)
				{
					var searching = target.parentElement.firstElementChild;
					var index = 0;
					
					while (searching != null && searching != clientObj)
					{
						if (searching.classList.contains('editor_column') ||
							(searching.classList.contains('help-column') && ! searching.classList.contains('d-none')) )
							index++;
						searching = searching.nextElementSibling;
					}
					
					if ((index % 2) && searching == clientObj)
					{
						var breaker = document.createElement('div');
						breaker.classList.add("w-100");
						breaker.classList.add("breaker");
						clientObj.parentNode.insertBefore(breaker, clientObj);
						clientObj.scrollIntoView({behavior: "smooth"}); 
					}
				}
			}
			else
			{
				target.classList.add("d-none");
				helpBtn.classList.remove("btn-warning-set");
				helpBtn.classList.add("btn-warning-noset");
				
				if (clientObj.previousElementSibling != null && 
					clientObj.previousElementSibling.classList.contains("breaker"))
				{
					(clientObj.previousElementSibling).remove();
					clientObj.scrollIntoView({behavior: "smooth"});
				}
			}			
		}
	); 
	var common = document.createElement("div");	
	common.appendChild(helpBtn);
	common.appendChild(saveBtn);	
	
	return createPanel(phead, "", common, "card-default", bodyid);	
}

function sortProperties(list)
{
	for(var key in list)
	{
		list[key].key = key;
	}
	list = $.map(list, function(value, index) {
				return [value];
		});
	return list.sort(function(a,b) {
				return a.propertyOrder - b.propertyOrder;
		});
}

function createHelpTable(list, phead){
	var table = document.createElement('div');
	table.className = "container";
	
	list = sortProperties(list);

	phead = '<svg data-src="svg/help_table_icon.svg" fill="#FFE810" class="svg4hyperhdr"></svg>' +phead + ' '+$.i18n("conf_helptable_expl");

	table.className = 'table table-hover borderless';

	table.appendChild(createTableRow([$.i18n('conf_helptable_option'), $.i18n('conf_helptable_expl')], true, false));

	for (var key in list)
	{
		if(list[key].access != 'system')
		{
			// break one iteration (in the loop), if the schema has the entry hidden=true
			if ("options" in list[key] && "hidden" in list[key].options && (list[key].options.hidden))
				continue;
			var text = list[key].title.replace('title', 'expl');
			table.appendChild(createTableRow([$.i18n(list[key].title), $.i18n(text)], false, false));

			if(list[key].items && list[key].items.properties)
			{
				var ilist = sortProperties(list[key].items.properties);
				for (var ikey in ilist)
				{
					// break one iteration (in the loop), if the schema has the entry hidden=true
					if ("options" in ilist[ikey] && "hidden" in ilist[ikey].options && (ilist[ikey].options.hidden))
						continue;
					var itext = ilist[ikey].title.replace('title', 'expl');
					table.appendChild(createTableRow([$.i18n(ilist[ikey].title), $.i18n(itext)], false, false));
				}
			}
		}
	}
	
	var finalHelp =  createPanel(phead, table);
	finalHelp.classList.add('d-none');
	finalHelp.classList.add('help-column');
	return finalHelp;
}

function createPanel(head, body, footer, type, bodyid){
	var cont = document.createElement('div');
	var p = document.createElement('div');
	var phead = document.createElement('div');
	var pbody = document.createElement('div');
	var pfooter = document.createElement('div');

	cont.className = "col-lg-6";

	if(typeof type == 'undefined')
		type = 'card-default';

	p.className = 'card '+type;
	phead.className = 'card-header';
	pbody.className = 'card-body';
	pfooter.className = 'card-footer';

	phead.innerHTML = head;

	if(typeof bodyid != 'undefined')
	{
		pfooter.style.textAlign = 'right';
		pbody.setAttribute("id", bodyid);
	}

	if(typeof body != 'undefined' && body != "")
		pbody.appendChild(body);

	if(typeof footer != 'undefined')
		pfooter.appendChild(footer);

	p.appendChild(phead);
	p.appendChild(pbody);

	if(typeof footer != 'undefined')
	{
		pfooter.style.textAlign = "right";
		p.appendChild(pfooter);
	}

	cont.appendChild(p);

	return cont;
}

function createSelGroup(group)
{
	var el = document.createElement('optgroup');
	el.setAttribute('label', group);
	return el;
}

function createSelOpt(opt, title)
{
	var el = document.createElement('option');
	el.setAttribute('value', opt);
	if (typeof title == 'undefined')
		el.innerHTML = opt;
	else
		el.innerHTML = title;
	return el;
}

function createSel(array, group, split)
{
	if (array.length != 0)
	{
		var el = createSelGroup(group);
		for(var i=0; i<array.length; i++)
		{
			var opt;
			if(split)
			{
				opt = array[i].split(":")
				opt = createSelOpt(opt[0],opt[1])
			}
			else
				opt = createSelOpt(array[i])
			el.appendChild(opt);
		}
		return el;
	}
}

function createSelWithGroupId(array, group, groupId, split)
{
	if (array.length != 0)
	{
		var el = document.createElement('optgroup');
		el.setAttribute('label', group);
		el.setAttribute('data-group-id', groupId);
		for(var i=0; i<array.length; i++)
		{
			var opt;
			if(split)
			{
				opt = array[i].split(":")
				opt = createSelOpt(opt[0],opt[1])
			}
			else
				opt = createSelOpt(array[i])
			el.appendChild(opt);
		}
		return el;
	}
}


function performTranslation()
{
	$('[data-i18n]').i18n();
}

function encode_utf8(s)
{
	return unescape(encodeURIComponent(s));
}

function getReleases(callback)
{
	$.ajax({
	    url: window.gitHubReleaseApiUrl,
	    method: 'get',
	    error: function(XMLHttpRequest, textStatus, errorThrown)
			{
					callback(false);
	    },
	    success: function(releases)
			{
				window.gitHubVersionList = releases;

				var highestRelease = {
					tag_name: '0.0.0.0'
				};

				for(var i in releases) {
					if(releases[i].draft)
						continue;

					//if(!releases[i].tag_name.includes('alpha') && !releases[i].tag_name.includes('beta'))
					{
						if (compareHyperHdrVersion(releases[i].tag_name, highestRelease.tag_name))
							highestRelease = releases[i];
					}
				}

				window.latestVersion = highestRelease;


				callback(true);

			}
	});
}

function handleDarkMode()
{
	$('body').addClass('dark-mode');
	$('aside').addClass('dark-mode');	

	setStorage("darkMode", "on", false);
	$('#btn_darkmode_moon_icon').addClass('d-none');
	$('#btn_darkmode_sun_icon').removeClass('d-none');
}

function handleLightMode()
{
	$('body').removeClass('dark-mode');
	$('aside').removeClass('dark-mode');	

	setStorage("darkMode", "on", false);
	$('#btn_darkmode_moon_icon').removeClass('d-none');
	$('#btn_darkmode_sun_icon').addClass('d-none');
}
