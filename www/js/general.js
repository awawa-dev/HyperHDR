$(document).ready(function()
{
	performTranslation();

	var importedConf;
	var confName;
	var conf_editor = null;

	
	$('#conf_cont').append(createOptPanel('fa-wrench', $.i18n("edt_conf_gen_heading_title"), 'editor_container', 'btn_submit'));
	$('#conf_cont').append(createHelpTable(window.schema.general.properties, $.i18n("edt_conf_gen_heading_title")));
	

	conf_editor = createJsonEditor('editor_container',
	{
		general: window.schema.general
	}, true, true);

	conf_editor.on('change', function()
	{
		conf_editor.validate().length || window.readOnlyMode ? $('#btn_submit').attr('disabled', true) : $('#btn_submit').attr('disabled', false);
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
			window.readOnlyMode ? $('#btn_cl_save').attr('disabled', true) : $('#btn_submit').attr('disabled', false);
			window.readOnlyMode ? $('#btn_ma_save').attr('disabled', true) : $('#btn_submit').attr('disabled', false);
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
			var startBtnIcon = inst[key].running ? "stop" : "play";
			var startBtnText = inst[key].running ? $.i18n('general_btn_stop')+' ' : $.i18n('general_btn_start');
			var renameBtn = '<button id="instren_' + inst[key].instance + '" type="button" class="h-100 w-100 btn btn-primary btn-sm"><i class="fa fa-fw fa-pencil"></i>' + $.i18n('general_btn_rename') + '</button>';
			var startBtn = ""
			var delBtn = "";
			
			if (inst[key].instance > 0)
			{
				delBtn = '<button id="instdel_' + inst[key].instance + '" type="button" class="h-100 w-100 btn btn-warning btn-sm"><i class="fa fa-fw fa-remove"></i>' + $.i18n('general_btn_delete') + '</button>';
				startBtn = '<button id="inst_' + inst[key].instance + '" type="button" class="h-100 w-100 btn btn-sm btn-' + startBtnColor + '"><i class="fa fa-fw fa-' + startBtnIcon + '"></i>' + startBtnText + '</button>';
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

			window.readOnlyMode ? $('#instren_' + inst[key].instance).attr('disabled', true) : $('#btn_submit').attr('disabled', false);
			window.readOnlyMode ? $('#inst_' + inst[key].instance).attr('disabled', true) : $('#btn_submit').attr('disabled', false);
			window.readOnlyMode ? $('#instdel_' + inst[key].instance).attr('disabled', true) : $('#btn_submit').attr('disabled', false);
		}
	}
	
	buildInstanceList();

	$('#inst_name').off().on('input', function(e)
	{
		(e.currentTarget.value.length >= 5) && !window.readOnlyMode ? $('#btn_create_inst').attr('disabled', false) : $('#btn_create_inst').attr('disabled', true);
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
		state || window.readOnlyMode ? $('#btn_import_conf').attr('disabled', true) : $('#btn_import_conf').attr('disabled', false);
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
					if (typeof content.instances === 'undefined' || typeof content.settings === 'undefined' || content.version !== "HyperHDR_export_format_v17")
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

	removeOverlay();
});
