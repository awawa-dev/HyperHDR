#requires -Version 5.1
<#
.SYNOPSIS
  Probe how much UDP stream traffic a Nanoleaf Essentials/HD controller can take
  before it freezes.

.DESCRIPTION
  Reads LED count from GET /length (no hardcoded strip size), enables extControl,
  then sweeps leds-per-datagram, inter-batch gap, and frame rate.

  After each test the script GETs /state. If that hangs or fails, the controller
  likely froze and you will need to power-cycle it.

.PARAMETER HostName
  Controller IP or hostname.

.PARAMETER Token
  OpenAPI token. Falls back to $env:NANOLEAF_TOKEN.

.EXAMPLE
  $env:NANOLEAF_TOKEN = 'your-token'
  .\Test-NanoleafStream.ps1 -HostName 192.168.86.154

.EXAMPLE
  .\Test-NanoleafStream.ps1 -HostName 192.168.86.154 -Token 'your-token' -SingleTest -LedsPerDatagram 150 -BatchGapMs 10 -FrameRateHz 20
#>
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$HostName,

	[string]$Token = $env:NANOLEAF_TOKEN,

	[int]$ApiPort = 16021,
	[int]$UdpPort = 60222,

	[int]$SecondsPerTest = 4,

	[int[]]$LedsPerDatagramList = @(50, 75, 100, 125, 150, 183, 300),
	[int[]]$BatchGapMsList = @(0, 5, 10, 20),
	[int[]]$FrameRateHzList = @(10, 20, 30, 60),

	[switch]$SingleTest,
	[switch]$Realtime,
	[int]$LedsPerDatagram = 150,
	[int]$BatchGapMs = 10,
	[int]$FrameRateHz = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Token))
{
	throw "Token is required. Pass -Token or set `$env:NANOLEAF_TOKEN."
}

function Get-ApiBase
{
	return "http://${HostName}:${ApiPort}/api/v1/$Token"
}

function Invoke-NanoleafGet
{
	param(
		[string]$Path = '',
		[int]$TimeoutSec = 3
	)

	$uri = (Get-ApiBase).TrimEnd('/') + $(if ($Path) { "/$Path" } else { '/' })
	return Invoke-RestMethod -Method Get -Uri $uri -TimeoutSec $TimeoutSec
}

function Invoke-NanoleafPut
{
	param(
		[string]$Path,
		[string]$Body,
		[int]$TimeoutSec = 5
	)

	$uri = (Get-ApiBase).TrimEnd('/') + "/$Path"
	return Invoke-RestMethod -Method Put -Uri $uri -ContentType 'application/json' -Body $Body -TimeoutSec $TimeoutSec
}

function Test-DeviceAlive
{
	try
	{
		$null = Invoke-NanoleafGet -Path 'state/on' -TimeoutSec 2
		return $true
	}
	catch
	{
		return $false
	}
}

function Convert-HsvToRgb
{
	param(
		[double]$Hue,
		[double]$Saturation = 1.0,
		[double]$Value = 1.0
	)

	$h = (($Hue % 360) + 360) % 360
	$c = $Value * $Saturation
	$hp = $h / 60.0
	$x = $c * (1.0 - [Math]::Abs(($hp % 2) - 1.0))
	$m = $Value - $c

	$r = 0.0; $g = 0.0; $b = 0.0
	if ($hp -lt 1) { $r = $c; $g = $x }
	elseif ($hp -lt 2) { $r = $x; $g = $c }
	elseif ($hp -lt 3) { $g = $c; $b = $x }
	elseif ($hp -lt 4) { $g = $x; $b = $c }
	elseif ($hp -lt 5) { $r = $x; $b = $c }
	else { $r = $c; $b = $x }

	return [byte[]]@(
		[byte][Math]::Round(($r + $m) * 255),
		[byte][Math]::Round(($g + $m) * 255),
		[byte][Math]::Round(($b + $m) * 255)
	)
}

function New-StreamFrame
{
	param(
		[int[]]$Ids,
		[byte[][]]$Rgb,
		[uint16]$Transition = 0
	)

	$count = $Ids.Length
	$buf = New-Object byte[] (2 + ($count * 8))
	$buf[0] = [byte](($count -shr 8) -band 0xFF)
	$buf[1] = [byte]($count -band 0xFF)
	$offset = 2

	for ($i = 0; $i -lt $count; $i++)
	{
		$id = $Ids[$i]
		$buf[$offset++] = [byte](($id -shr 8) -band 0xFF)
		$buf[$offset++] = [byte]($id -band 0xFF)
		$buf[$offset++] = $Rgb[$i][0]
		$buf[$offset++] = $Rgb[$i][1]
		$buf[$offset++] = $Rgb[$i][2]
		$buf[$offset++] = 0
		$buf[$offset++] = [byte](($Transition -shr 8) -band 0xFF)
		$buf[$offset++] = [byte]($Transition -band 0xFF)
	}

	return ,$buf
}

function Send-StreamFrame
{
	param(
		[System.Net.Sockets.UdpClient]$Udp,
		[int]$NumLeds,
		[int]$LedsPerDatagram,
		[int]$BatchGapMs,
		[int]$FrameIndex
	)

	# Every LED is addressed every frame, the same way HyperHDR sends a full layout.
	# A moving rainbow makes it obvious that the whole strip is live, not a small window.
	$hueShift = ($FrameIndex * 8) % 360

	for ($start = 0; $start -lt $NumLeds; $start += $LedsPerDatagram)
	{
		$count = [Math]::Min($LedsPerDatagram, $NumLeds - $start)
		$ids = New-Object int[] $count
		$rgb = New-Object 'byte[][]' $count

		for ($i = 0; $i -lt $count; $i++)
		{
			$led = $start + $i
			$ids[$i] = $led
			$hue = $hueShift + (360.0 * $led / $NumLeds)
			$rgb[$i] = Convert-HsvToRgb -Hue $hue
		}

		$packet = New-StreamFrame -Ids $ids -Rgb $rgb -Transition 0
		[void]$Udp.Send($packet, $packet.Length)

		if (($start + $count) -lt $NumLeds -and $BatchGapMs -gt 0)
		{
			Start-Sleep -Milliseconds $BatchGapMs
		}
	}
}

function Invoke-StreamTrial
{
	param(
		[System.Net.Sockets.UdpClient]$Udp,
		[int]$NumLeds,
		[int]$LedsPerDatagram,
		[int]$BatchGapMs,
		[int]$FrameRateHz,
		[int]$Seconds
	)

	$intervalMs = [Math]::Max(1, [int](1000 / $FrameRateHz))
	$frames = [int]($Seconds * 1000 / $intervalMs)
	$sw = [System.Diagnostics.Stopwatch]::StartNew()

	for ($f = 0; $f -lt $frames; $f++)
	{
		Send-StreamFrame -Udp $Udp -NumLeds $NumLeds -LedsPerDatagram $LedsPerDatagram -BatchGapMs $BatchGapMs -FrameIndex $f
		$target = ($f + 1) * $intervalMs
		$remain = $target - [int]$sw.ElapsedMilliseconds
		if ($remain -gt 0)
		{
			Start-Sleep -Milliseconds $remain
		}
	}

	$alive = Test-DeviceAlive
	[pscustomobject]@{
		LedsPerDatagram = $LedsPerDatagram
		PacketsPerFrame = [Math]::Ceiling($NumLeds / [double]$LedsPerDatagram)
		BytesPerPacket  = 2 + ([Math]::Min($LedsPerDatagram, $NumLeds) * 8)
		BatchGapMs      = $BatchGapMs
		FrameRateHz     = $FrameRateHz
		Seconds         = $Seconds
		Alive           = $alive
		Result          = $(if ($alive) { 'OK' } else { 'FROZEN' })
	}
}

Write-Host "Querying device at ${HostName}:${ApiPort} ..."
$info = Invoke-NanoleafGet
$length = Invoke-NanoleafGet -Path 'length'
$numLeds = [int]$length.numLEDs

Write-Host ("Name={0}  Model={1}  FW={2}  LEDs={3}" -f $info.name, $info.model, $info.firmwareVersion, $numLeds)
if ($numLeds -le 0)
{
	throw "GET /length returned no LEDs."
}

Write-Host "Enabling extControl v2 ..."
Invoke-NanoleafPut -Path 'effects' -Body '{"write":{"command":"display","animType":"extControl","extControlVersion":"v2"}}' | Out-Null
Start-Sleep -Milliseconds 200

$udp = New-Object System.Net.Sockets.UdpClient
$udp.Connect($HostName, $UdpPort)

$results = New-Object System.Collections.Generic.List[object]

try
{
	Write-Host "Warmup: 50 LEDs/datagram, 10ms gap, 10 Hz ..."
	$warmup = Invoke-StreamTrial -Udp $udp -NumLeds $numLeds -LedsPerDatagram 50 -BatchGapMs 10 -FrameRateHz 10 -Seconds 2
	$results.Add($warmup)
	if (-not $warmup.Alive)
	{
		Write-Warning "Device froze during warmup. Power-cycle the controller and retry with a smaller first batch."
		$results | Format-Table -AutoSize
		return
	}

	if ($Realtime)
	{
		Write-Host ("Realtime: addressing all {0} LEDs in one datagram, {1} Hz, {2}s" -f $numLeds, $FrameRateHz, $SecondsPerTest)
		$results.Add((Invoke-StreamTrial -Udp $udp -NumLeds $numLeds -LedsPerDatagram $numLeds -BatchGapMs 0 -FrameRateHz $FrameRateHz -Seconds $SecondsPerTest))
	}
	elseif ($SingleTest)
	{
		Write-Host ("Single test: {0} LEDs/datagram, {1}ms gap, {2} Hz, {3}s" -f $LedsPerDatagram, $BatchGapMs, $FrameRateHz, $SecondsPerTest)
		$results.Add((Invoke-StreamTrial -Udp $udp -NumLeds $numLeds -LedsPerDatagram $LedsPerDatagram -BatchGapMs $BatchGapMs -FrameRateHz $FrameRateHz -Seconds $SecondsPerTest))
	}
	else
	{
		Write-Host "`n=== Phase 1: max LEDs per datagram (10 Hz, 10ms gap) ==="
		$bestBatch = 50
		foreach ($batch in $LedsPerDatagramList)
		{
			Write-Host ("  Trying {0} LEDs/datagram ..." -f $batch)
			$row = Invoke-StreamTrial -Udp $udp -NumLeds $numLeds -LedsPerDatagram $batch -BatchGapMs 10 -FrameRateHz 10 -Seconds $SecondsPerTest
			$results.Add($row)
			if (-not $row.Alive)
			{
				Write-Warning "Device stopped responding at $batch LEDs/datagram. Stop the sweep and power-cycle if needed."
				break
			}
			$bestBatch = $batch
		}

		if ((Test-DeviceAlive))
		{
			Write-Host "`n=== Phase 2: inter-batch gap at $bestBatch LEDs/datagram, 10 Hz ==="
			$bestGap = 10
			foreach ($gap in $BatchGapMsList)
			{
				Write-Host ("  Trying {0}ms gap ..." -f $gap)
				$row = Invoke-StreamTrial -Udp $udp -NumLeds $numLeds -LedsPerDatagram $bestBatch -BatchGapMs $gap -FrameRateHz 10 -Seconds $SecondsPerTest
				$results.Add($row)
				if (-not $row.Alive)
				{
					Write-Warning "Device stopped responding at ${gap}ms gap."
					break
				}
				$bestGap = $gap
			}
		}

		if ((Test-DeviceAlive))
		{
			Write-Host "`n=== Phase 3: frame rate at $bestBatch LEDs/datagram, ${bestGap}ms gap ==="
			foreach ($hz in $FrameRateHzList)
			{
				Write-Host ("  Trying {0} Hz ..." -f $hz)
				$row = Invoke-StreamTrial -Udp $udp -NumLeds $numLeds -LedsPerDatagram $bestBatch -BatchGapMs $bestGap -FrameRateHz $hz -Seconds $SecondsPerTest
				$results.Add($row)
				if (-not $row.Alive)
				{
					Write-Warning "Device stopped responding at ${hz} Hz."
					break
				}
			}
		}
	}
}
finally
{
	$udp.Close()
}

Write-Host "`n=== Results ==="
$results | Format-Table -AutoSize

$ok = @($results | Where-Object { $_.Alive -and $_.LedsPerDatagram })
if ($ok.Count -gt 0)
{
	$safe = $ok | Sort-Object FrameRateHz -Descending | Select-Object -First 1
	Write-Host "Suggested HyperHDR device JSON extras (tune from a passing row):"
	Write-Host ('  "streamLedsPerDatagram": {0},' -f $safe.LedsPerDatagram)
	Write-Host ('  "streamBatchGapMs": {0},' -f $safe.BatchGapMs)
	Write-Host ('  "streamMinIntervalMs": {0}' -f [int](1000 / [Math]::Max(1, $safe.FrameRateHz)))
}

if (-not (Test-DeviceAlive))
{
	Write-Warning "Controller is not answering HTTP. Power-cycle it before using HyperHDR."
}
