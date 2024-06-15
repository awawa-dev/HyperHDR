$(document).ready(function()
{
	performTranslation();

	var importedConf;
	var confName;
	var conf_editor = null;
	var conf_editor_logs = null;

	$('#conf_cont_logs').append(createOptPanel('<svg data-src="svg/logs_panel.svg" width="18" height="18" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_log_heading_title"), 'logs_editor_container', 'btn_submit_logs'));
	$('#conf_cont_logs').append(createHelpTable(window.schema.logger.properties, $.i18n("edt_conf_log_heading_title")));
	$("#conf_cont_logs").children().first().removeClass();
	$("#conf_cont_logs").children().first().addClass("editor_column");

	conf_editor_logs = createJsonEditor('logs_editor_container', {
		logger : window.schema.logger
	}, true, true);

	conf_editor_logs.on('change',function() {
		conf_editor_logs.validate().length ? $('#btn_submit_logs').attr('disabled', true) : $('#btn_submit_logs').attr('disabled', false);
	});

	$('#btn_submit_logs').off().on('click',function() {
		requestWriteConfig(conf_editor_logs.getValue());
	});

	
	$('#conf_cont').append(createOptPanel('<svg data-src="svg/general_settings.svg" fill="currentColor" class="svg4hyperhdr"></svg>', $.i18n("edt_conf_gen_heading_title"), 'editor_container', 'btn_submit'));
	$('#conf_cont').append(createHelpTable(window.schema.general.properties, $.i18n("edt_conf_gen_heading_title")));
	

	conf_editor = createJsonEditor('editor_container',
	{
		general: window.schema.general
	}, true, true);

	conf_editor.on('change', function()
	{
		conf_editor.validate().length ? $('#btn_submit').attr('disabled', true) : $('#btn_submit').attr('disabled', false);
	});

	$('#btn_submit').off().on('click', function()
	{
		requestWriteConfig(conf_editor.getValue());
	});

	// Instance handling
	function handleInstanceRename(e)
	{

		conf_editor.on('change', function()
		{
			$('#btn_submit').attr('disabled', false);
			$('#btn_submit').attr('disabled', false);
		});

		var inst = e.currentTarget.id.split("_")[1];
		showInfoDialog('renameInstance', $.i18n('conf_general_inst_renreq_t'), getInstanceNameByIndex(inst));

		$("#id_btn_ok").off().on('click', function()
		{
			requestInstanceRename(inst, $('#renameInstance_name').val())
		});

		$('#renameInstance_name').off().on('input', function(e)
		{
			(e.currentTarget.value.length >= 5 && e.currentTarget.value != getInstanceNameByIndex(inst)) ? $('#id_btn_ok').attr('disabled', false): $('#id_btn_ok').attr('disabled', true);
		});
	}

	function handleInstanceStartStop(e)
	{
		var inst = e.currentTarget.id.split("_")[1];
		var start = (e.currentTarget.className.indexOf("btn-danger") >= 0);
		requestInstanceStartStop(inst, start);
	}

	function handleInstanceDelete(e)
	{
		var inst = e.currentTarget.id.split("_")[1];
		showInfoDialog('deleteInstance', $.i18n('conf_general_inst_delreq_h'), $.i18n('conf_general_inst_delreq_t', getInstanceNameByIndex(inst)));
		$("#id_btn_yes").off().on('click', function()
		{
			requestInstanceDelete(inst)
		});
	}

	function buildInstanceList()
	{
		var inst = serverInfo.instance;
		var header = $('#instanceList').children(":first");
		$('#instanceList').html("");
		$('#instanceList').append(header);
		for (var key in inst)
		{
			var startBtnColor = inst[key].running ? "success" : "danger";
			var startBtnIcon = inst[key].running ? "button_stop.svg" : "button_play.svg";
			var startBtnText = inst[key].running ? $.i18n('general_btn_stop')+' ' : $.i18n('general_btn_start');
			var renameBtn = '<button id="instren_' + inst[key].instance + '" type="button" class="h-100 w-100 btn btn-primary btn-sm"><svg data-src="svg/button_edit.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_rename') + '</button>';
			var startBtn = ""
			var delBtn = "";
			
			if (inst[key].instance > 0)
			{
				delBtn = '<button id="instdel_' + inst[key].instance + '" type="button" class="h-100 w-100 btn btn-warning btn-sm"><svg data-src="svg/button_delete.svg" fill="currentColor" class="svg4hyperhdr"></svg>' + $.i18n('general_btn_delete') + '</button>';
				startBtn = '<button id="inst_' + inst[key].instance + '" type="button" class="h-100 w-100 btn btn-sm btn-' + startBtnColor + `"><svg data-src="svg/${startBtnIcon}" fill="currentColor" class="svg4hyperhdr"></svg>` + startBtnText + '</button>';
			}
			var newRow = $('<div>', {class: "row"});
			var colHeader = $('<div>', {class: "col-4 border-top fw-bold d-flex align-items-center"}).append(inst[key].friendly_name);
			var colAction = $('<div>', {class: "col-8 border-top "}).append($('<div>', {class: "row"})
				.append($('<div>', {class: "col mt-2 mb-2"}).append(renameBtn))
				.append($('<div>', {class: "col mt-2 mb-2"}).append(startBtn))
				.append($('<div>', {class: "col mt-2 mb-2"}).append(delBtn)));
			newRow.append(colHeader).append(colAction);
			
			$('#instanceList').append(newRow);
			$('#instren_' + inst[key].instance).off().on('click', handleInstanceRename);
			$('#inst_' + inst[key].instance).off().on('click', handleInstanceStartStop);
			$('#instdel_' + inst[key].instance).off().on('click', handleInstanceDelete);

			$('#btn_submit').attr('disabled', false);
			$('#btn_submit').attr('disabled', false);
			$('#btn_submit').attr('disabled', false);
		}
	}
	
	buildInstanceList();

	$('#inst_name').off().on('input', function(e)
	{
		(e.currentTarget.value.length >= 5) ? $('#btn_create_inst').attr('disabled', false) : $('#btn_create_inst').attr('disabled', true);
		if (5 - e.currentTarget.value.length >= 1 && 5 - e.currentTarget.value.length <= 4)
			$('#inst_chars_needed').html(5 - e.currentTarget.value.length + " " + $.i18n('general_chars_needed'))
		else
			$('#inst_chars_needed').html("<br />")
	});

	$('#btn_create_inst').off().on('click', function(e)
	{
		requestInstanceCreate($('#inst_name').val());
		$('#inst_name').val("");
		$('#btn_create_inst').attr('disabled', true)
	});

	$(hyperhdr).off("instance-updated").on("instance-updated", function(event)
	{
		buildInstanceList()
	});

	//import
	function dis_imp_btn(state)
	{
		state ? $('#btn_import_conf').attr('disabled', true) : $('#btn_import_conf').attr('disabled', false);
	}

	function readFile(evt)
	{
		var f = evt.target.files[0];

		if (f)
		{
			var r = new FileReader();
			r.onload = function(e)
			{
				var content = e.target.result;

				//check file is json
				var check = isJsonString(content);
				if (check.length != 0)
				{
					showInfoDialog('error', "", $.i18n('infoDialog_import_jsonerror_text', f.name, JSON.stringify(check)));
					dis_imp_btn(true);
				}
				else
				{
					content = JSON.parse(content);
					if (typeof content.instances === 'undefined' || typeof content.settings === 'undefined' ||
						content.version == null || content.version.indexOf("HyperHDR_export_format_v") != 0)
					{
						showInfoDialog('error', "", $.i18n('infoDialog_import_hyperror_text', f.name));
						dis_imp_btn(true);
					}
					else
					{
						dis_imp_btn(false);
						importedConf = content;
						confName = f.name;
					}
				}
			}
			r.readAsText(f);
		}
	}

	$('#btn_import_conf').off().on('click', function()
	{
		showInfoDialog('confirm', $.i18n('infoDialog_import_confirm_title'), $.i18n('infoDialog_import_confirm_text', confName));

		$('#id_btn_confirm').off().on('click', function()
		{
			removeStorage('lastSelectedInstance', false);
			requestRestoreDB(importedConf);
		});
	});

	$('#select_import_conf').off().on('change', function(e)
	{
		if (window.File && window.FileReader && window.FileList && window.Blob)
			readFile(e);
		else
			showInfoDialog('error', "", $.i18n('infoDialog_import_comperror_text'));
	});

	//export
	$('#btn_export_conf').off().on('click', async () => 
	{
		var d = new Date();
		var month = d.getMonth() + 1;
		var day = d.getDate();

		var timestamp = d.getFullYear() + '-' +
			(month < 10 ? '0' : '') + month + '-' +
			(day < 10 ? '0' : '') + day;
			
		var backup = await requestGetDB();
		if (backup.success === true)
			download(JSON.stringify(backup.info, null, "\t"), 'HyperHDR-' + window.currentVersion + '-Backup-' + timestamp + '.json', "application/json");		
	});

	//create introduction
	if (window.showOptHelp)
	{
		createHint("intro", $.i18n('conf_general_intro'), "editor_container");
		createHint("intro", $.i18n('conf_general_tok_desc'), "tok_desc_cont");
		createHint("intro", $.i18n('conf_general_inst_desc'), "inst_desc_cont");
	}

	if (window.serverInfo.grabbers != null && window.serverInfo.grabbers != undefined &&
		window.serverInfo.grabbers.active != null && window.serverInfo.grabbers.active != undefined)
	{
		var grabbers = window.serverInfo.grabbers.active;
		if (grabbers.indexOf('Media Foundation') < 0)
		{
			conf_editor.getEditor('root.general.disableOnLocked').disable();
		}
	}

	removeOverlay();
});
