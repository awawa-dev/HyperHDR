
var createdCont = false;

performTranslation();

function CopyToClipboard(par)
{
	var textarea = document.createElement("textarea");
	textarea.textContent = par;
	textarea.style.position = "fixed";
	document.body.appendChild(textarea);
	textarea.select();
	try {
		document.execCommand("copy");
	}
	catch (ex) {
		console.warn("Copy to clipboard failed.", ex);		
	}
	finally {
		document.body.removeChild(textarea);
	}
}

$(document).ready(function() {
	var messages;
	
	if(window.showOptHelp)
	{		
		createHintH("callout-info", $.i18n('conf_logging_label_intro'), "log_head");
	}	

	$('#btn_autoscroll').off().on('click',function() {
		toggleClass('#btn_autoscroll', "btn-success", "btn-danger");
		if($("#btn_autoscroll").hasClass('btn-success'))
		{
			document.getElementById('logs_autorefresh').beginElement();
		}
		else
		{
			document.getElementById('logs_autorefresh').endElement();
		}
	});

	$('#btn_copylogs').off().on('click',function() {
		let copyText = $("#logmessages").text();
		CopyToClipboard(copyText);
	});	
	
	$(window.hyperhdr).unbind("cmd-logging-update");
	$("#logmessages").empty();
	requestLoggingStart();

	$(window.hyperhdr).on("cmd-logging-update",
		function(event)
		{
			messages = (event.response.result.messages);				
				
			var utcDate = new Date();
			var offsetDate = utcDate.getTimezoneOffset();

			$("#has_logs").removeClass("d-none");
			$("#has_no_logs").addClass("d-none");

			for(var idx = 0; idx < messages.length; idx++)
			{
				var logger_name = messages[idx].loggerName;
				var function_ = messages[idx].function;
				var line = messages[idx].line;
				var file_name = messages[idx].fileName;
				var msg = messages[idx].message;
				var level_string = messages[idx].levelString;
				var utime = messages[idx].utime;

				var debug = "";					
					
				if(level_string == "DEBUG")
				{
					debug = "("+file_name+":"+line+") ";
				}					

				var date = new Date(parseInt(utime) - offsetDate * 60000);

				LogLine($("#logmessages"), date, logger_name, level_string, debug, msg);					
			}

			if($("#btn_autoscroll").hasClass('btn-success'))
			{
				$('#logmessages').stop().animate({
					scrollTop: $('#logmessages')[0].scrollHeight
				}, 800);
			}
		}
	);


	removeOverlay();
});

function LogLine(logger,date,logger_name,level_string,debug,msg)
{
	const REPORT_TOKEN = "<Report>";

	var style="";
	if (level_string=="INFO")
		style = "db_info";
	else if (level_string=="DEBUG")
		style = "db_debug";
	else if (level_string=="WARNING")
		style = "db_warning";
	else if (level_string=="ERROR")
		style = "db_error";

	if (msg.indexOf(REPORT_TOKEN) == 0)
	{
		msg = msg.substring(REPORT_TOKEN.length, msg.length);
		logger.append(`<code class='${style}'>${msg}</code>\n`);
	}
	else
	{
		logger.append(`<code class='${style}'>${date.toISOString()} [${logger_name.trim()}] ${debug}${msg}</code>\n`);
	}
}
