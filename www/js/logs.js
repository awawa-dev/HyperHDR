var conf_editor = null;
var createdCont = false;

performTranslation();

if (window.loggingStreamActive)
{
	requestLoggingStop();
}

$(document).ready(function() {
	var messages;

	requestLoggingStart();

	$('#conf_cont').append(createOptPanel('fa-reorder', $.i18n("edt_conf_log_heading_title"), 'editor_container', 'btn_submit'));
	$('#conf_cont').append(createHelpTable(window.schema.logger.properties, $.i18n("edt_conf_log_heading_title")));
	
	if(window.showOptHelp)
	{		
		createHintH("callout-info", $.i18n('conf_logging_label_intro'), "log_head");
	}	

	conf_editor = createJsonEditor('editor_container', {
		logger : window.schema.logger
	}, true, true);

	conf_editor.on('change',function() {
		conf_editor.validate().length || window.readOnlyMode ? $('#btn_submit').attr('disabled', true) : $('#btn_submit').attr('disabled', false);
	});

	$('#btn_submit').off().on('click',function() {
		requestWriteConfig(conf_editor.getValue());
	});
	
	if (!window.loggingHandlerInstalled)
	{
		window.loggingHandlerInstalled = true;
		
		$(window.hyperhdr).unbind("cmd-logging-update");
		$("#logmessages").empty();

		$(window.hyperhdr).on("cmd-logging-update",function(event){
			
			if (!window.location.href.includes("logs")&& 
				 window.location.href.includes("#"))
			{
				window.loggingHandlerInstalled = false;
		
				$(window.hyperhdr).unbind("cmd-logging-update");
				$("#logmessages").empty();
				
				requestLoggingStop();
			}
			else
			{
				messages = (event.response.result.messages);
				
				if(!createdCont)
				{
					$('#log_content').html('<pre><div id="logmessages" style="overflow:scroll;max-height:400px"></div></pre><button class="btn btn-primary" id="btn_autoscroll"><i class="fa fa-long-arrow-down fa-fw"></i>'+$.i18n('conf_logging_btn_autoscroll')+'</button>');
					createdCont = true;

					$('#btn_autoscroll').off().on('click',function() {
						toggleClass('#btn_autoscroll', "btn-success", "btn-danger");
					});
				}
				
				
				var utcDate = new Date();
				var offsetDate = utcDate.getTimezoneOffset();

				for(var idx = 0; idx < messages.length; idx++)
				{
					var app_name = messages[idx].appName;
					var logger_name = messages[idx].loggerName;
					var function_ = messages[idx].function;
					var line = messages[idx].line;
					var file_name = messages[idx].fileName;
					var msg = messages[idx].message;
					var level_string = messages[idx].levelString;
					var utime = messages[idx].utime;

					var debug = "";
					
					
					if(level_string == "DEBUG") {
						debug = "("+file_name+":"+line+") ";
					}
					

					var date = new Date(parseInt(utime) - offsetDate * 60000);

					LogLine($("#logmessages"), date, app_name, logger_name, level_string, debug,msg);					
				}

				if($("#btn_autoscroll").hasClass('btn-success'))
				{
					$('#logmessages').stop().animate({
						scrollTop: $('#logmessages')[0].scrollHeight
					}, 800);
				}
			}
		});
	}

	removeOverlay();
});

function LogLine(logger,date,app_name,logger_name,level_string,debug,msg)
{
	var style="";
	if (level_string=="INFO")
		style = " class='db_info'";
	else if (level_string=="DEBUG")
		style = " class='db_debug'";
	else if (level_string=="WARNING")
		style = " class='db_warning'";
	else if (level_string=="ERROR")
		style = " class='db_error'";
	
	if (logger.text().length > 0)
		logger.append("\n<code"+style+">"+date.toISOString()+" ["+(app_name+" "+logger_name).trim()+"] "+debug+msg+"</code>");
	else
		logger.append("<code"+style+">"+date.toISOString()+" ["+(app_name+" "+logger_name).trim()+"] "+debug+msg+"</code>");
}
