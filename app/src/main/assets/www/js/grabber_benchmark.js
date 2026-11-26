$(document).ready( function(){
	let mode = "";
	let indexer = 0;
	let internalLatency = 0;
	let whiteIndexer = 0;
	let redIndexer = 0;
	let greenIndexer = 0;
	let blueIndexer = 0;
	let blackIndexer = 0;
	let whiteTimer = 0;
	let redTimer = 0;
	let greenTimer = 0;
	let blueTimer = 0;
	let blackTimer = 0;
	let totalDelay = 0;
	let direction = 0;


	const canvas = document.getElementById("canvas");
	const ctx = canvas.getContext("2d");

	performTranslation();
	$("#grabber_benchmark_intro").html($.i18n("grabber_benchmark_intro"));
		
	ctx.fillStyle = "black";
	ctx.fillRect(0, 0, canvas.width, canvas.height);

	$("#startBenchmark").off('click').on('click',startBenchmark);
	
	resetData();
	
	$(window.hyperhdr).off("cmd-benchmark-update").on("cmd-benchmark-update", function(event)
	{
		handleMessage(event);
	});
	
	function resetData()
	{
		mode = "";
		indexer = 0;
		internalLatency = 0;
		whiteIndexer = 0;
		redIndexer = 0;
		greenIndexer = 0;
		blueIndexer = 0;
		blackIndexer = 0;
		testTimer = 0;
		redTimer = 0;
		greenTimer = 0;
		blueTimer = 0;
		blackTimer = 0;
		totalDelay = 0;
		direction = 0;
	}
	
	function handleMessage(event)
	{
		if (mode == "ping")
		{
			if (event.response.data.message == "pong" && indexer == event.response.data.status)
			{				
				if (event.response.data.status > 50)
				{
					alert("Bad indexer calibration: " + indexer);
				}
				else
				{
					pingInternal();
				}
			}
			else
				$("#logmessages").append("<code class='db_error'>Bad internal data: "+indexer+":"+event.response.data.status +"</code><br/>");
		}
		
		else if (mode == "white")
		{
			if (event.response.data.message == "white" && whiteIndexer == event.response.data.status)
			{				
				if (event.response.data.status > 50)
				{
					alert("Bad total calibration: " + whiteIndexer);
				}
				else
				{
					let delta = Math.max(((new Date()).getTime() -  whiteTimer) - internalLatency,0);
					$("#logmessages").append("<code class='db_debug'>Frame white: "+whiteIndexer+", delta: "+delta+"ms</code><br/>");
					
					if (direction == 0)
						pingRed();
					else
						pingBlue();
				}
			}
			else
				$("#logmessages").append("<code class='db_error'>Bad white data: "+whiteIndexer+":"+redIndexer+":"+greenIndexer+":"+blueIndexer+":"+blackIndexer+":"+event.response.data.status +"</code><br/>");
		}
		
		else if (mode == "red")
		{
			if (event.response.data.message == "red" && redIndexer == event.response.data.status)
			{						
				if (event.response.data.status > 50)
				{
					alert("Bad total red calibration: " + redIndexer);
				}
				else
				{
					let delta = Math.max(((new Date()).getTime() -  redTimer) - internalLatency,0);
					$("#logmessages").append("<code class='db_debug'>Frame red: "+redIndexer+", delta: "+delta+"ms</code><br/>");
					
					if (direction == 0)
						pingGreen();
					else
						pingBlack();
				}
			}
			else
				$("#logmessages").append("<code class='db_error'>Bad red data: "+whiteIndexer+":"+redIndexer+":"+greenIndexer+":"+blueIndexer+":"+blackIndexer+":"+event.response.data.status +"</code><br/>");
		}
		
		else if (mode == "green")
		{
			if (event.response.data.message == "green" && greenIndexer == event.response.data.status)
			{				
				if (event.response.data.status > 50)
				{
					alert("Bad total green calibration: " + greenIndexer);
				}
				else
				{
					let delta = Math.max(((new Date()).getTime() -  greenTimer) - internalLatency,0);
					$("#logmessages").append("<code class='db_debug'>Frame green: "+greenIndexer+", delta: "+delta+"ms</code><br/>");
					
					if (direction == 0)
						pingBlue();
					else
						pingRed();
				}
			}
			else
				$("#logmessages").append("<code class='db_error'>Bad green data: "+whiteIndexer+":"+redIndexer+":"+greenIndexer+":"+blueIndexer+":"+blackIndexer+":"+event.response.data.status +"</code><br/>");
		}
		
		else if (mode == "blue")
		{
			if (event.response.data.message == "blue" && blueIndexer == event.response.data.status)
			{						
				if (event.response.data.status > 50)
				{
					alert("Bad total blue calibration: " + blueIndexer);
				}
				else
				{
					let delta = Math.max(((new Date()).getTime() -  blueTimer) - internalLatency,0);
					$("#logmessages").append("<code class='db_debug'>Frame blue: "+blueIndexer+", delta: "+delta+"ms</code><br/>");
					
					if (direction == 0)
						pingBlack();
					else
						pingGreen();
				}
			}
			else
				$("#logmessages").append("<code class='db_error'>Bad blue data: "+whiteIndexer+":"+redIndexer+":"+greenIndexer+":"+blueIndexer+":"+blackIndexer+":"+event.response.data.status +"</code><br/>");
		}
		
		else if (mode == "black")
		{
			if (event.response.data.message == "black" && blackIndexer == event.response.data.status)
			{				
				if (event.response.data.status == 50)
				{
					if (whiteIndexer == 50 && blackIndexer == 50 && redIndexer == 50 && greenIndexer == 50&& blueIndexer == 50)
						finishBenchmark();
					else
						$("#logmessages").append("<code class='db_error'>Some frames get lost: "+whiteIndexer+":"+redIndexer+":"+greenIndexer+":"+blueIndexer+":"+blackIndexer +"</code><br/>");
						
				}
				else
				{
					let delta = Math.max(((new Date()).getTime() -  blackTimer) - internalLatency,0);
					$("#logmessages").append("<code class='db_debug'>Frame black: "+blackIndexer+", delta: "+delta+"ms</code><br/>");
					direction =  (direction + 1) % 2;
					pingWhite();
				}
			}
			else
				$("#logmessages").append("<code class='db_error'>Bad black data: "+whiteIndexer+":"+redIndexer+":"+greenIndexer+":"+blueIndexer+":"+blackIndexer+":"+event.response.data.status +"</code><br/>");
		}
	}
	
	
	function startBenchmark()
	{
		resetData();
		mode = "ping";
		
		$("#logmessages").empty();
		
		internalLatency = (new Date()).getTime();
		pingInternal();
		
	};
	
	function pingInternal()
	{	
		mode = 'ping';	
		
		if (indexer == 50)
		{
			firstStep();
			return;
		}
		
		ctx.fillStyle = ((indexer % 2) == 0) ? "black" : "white";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		requestBenchmark(mode, ++indexer);
	};

	function sleep(ms) {
		return new Promise(resolve => setTimeout(resolve, ms));
	}

	async function firstStep()
	{		
		internalLatency = ((new Date()).getTime() - internalLatency)/indexer;
						
		ctx.fillStyle = "#FFFF00";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		await sleep(1000);
		
		totalDelay = (new Date()).getTime();
		
		ctx.fillStyle = "black";
		ctx.fillRect(0, 0, canvas.width, canvas.height);			
		
		$("#logmessages").append("<code class='db_debug'>Internal latency: "+internalLatency+"ms</code><br/>");
		$("#logmessages").append("<code class='db_debug'>Waiting for the frames...make sure that you put the rectangle in the center and the grabber is capturing it</code><br/>");
		pingWhite();
	};

	function pingWhite()
	{	
		mode = 'white';	
		
		whiteTimer = (new Date()).getTime();
		
		ctx.fillStyle = "#FFFFFF";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		requestBenchmark(mode, ++whiteIndexer);
	};
	
	function pingRed()
	{	
		mode = 'red';	
		
		redTimer = (new Date()).getTime();
		
		ctx.fillStyle = "#FF0000";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		requestBenchmark(mode, ++redIndexer);
	};
	
	function pingGreen()
	{	
		mode = 'green';	
		
		greenTimer = (new Date()).getTime();
		
		ctx.fillStyle = "#00FF00";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		requestBenchmark(mode, ++greenIndexer);
	};
	
	function pingBlue()
	{	
		mode = 'blue';	
		
		blueTimer = (new Date()).getTime();
		
		ctx.fillStyle = "#0000FF";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		requestBenchmark(mode, ++blueIndexer);
	};

	function pingBlack()
	{
		mode = 'black';	
		
		blackTimer = (new Date()).getTime();
		
		ctx.fillStyle = "#000000";
		ctx.fillRect(0, 0, canvas.width, canvas.height);
		
		requestBenchmark(mode, ++blackIndexer);
	};

	function finishBenchmark()
	{
		let total = (whiteIndexer + blackIndexer + blueIndexer + greenIndexer + redIndexer);
		
		if (total != 250)
			$("#logmessages").append("<code class='db_error'>Unexpected total: "+total+"</code><br/>");
		else
		{			
			totalDelay = ((new Date()).getTime() - totalDelay);
			
			let finalDelay = Math.max(( totalDelay / total ) - internalLatency, 0);

			if (window.serverInfo != null && window.serverInfo.grabberstate != null && window.serverInfo.grabberstate.device != null && window.serverInfo.grabberstate.videoMode != null)
			{
				$("#logmessages").append("<code class='db_info'>"+$.i18n("dashboard_current_video_device")+": "+window.serverInfo.grabberstate.device+"</code><br/>");
				$("#logmessages").append("<code class='db_info'>"+$.i18n("dashboard_current_video_mode")+": "+window.serverInfo.grabberstate.videoMode+"</code><br/>");
			
				var mode1 = window.serverInfo.grabberstate.videoMode.split(' ');
				if (mode1.length == 2)
				{
					var mode2 = mode1[0].split('x');
					if (mode2.length == 3 && Number(mode2[2]) > 0)
						$("#logmessages").append("<code class='db_info'>"+$.i18n("benchmark_exp_delay")+": "+(1000/Number(mode2[2])).toFixed(2)+"ms</code><br/>");
				}
			}
			
			$("#logmessages").append("<code class='db_info'>"+$.i18n("benchmark_av_delay")+": "+Number((finalDelay).toFixed(2))+"ms</code><br/>");
		}
		
		resetData();
		requestBenchmark("stop", -1);
		
		$('#logmessages').stop().animate({
						scrollTop: $('#logmessages')[0].scrollHeight
					}, 800);
	};

});
