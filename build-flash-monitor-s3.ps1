# build-flash-monitor-s3.ps1
# All-in-one ESP32 SuperMini helper: compile -> upload -> interactive timestamped serial monitor.
# Default target is ESP32-S3. Use -Board C3, C3_LUAOS, or C3_LUAOS_NO_CH343 to target C3 variants.
#
# Examples:
#   .\build-flash-monitor-s3.ps1
#   .\build-flash-monitor-s3.ps1 -Port COM6
#   .\build-flash-monitor-s3.ps1 -Board C3_LUAOS -Port COM7
#   .\build-flash-monitor-s3.ps1 -Board C3_LUAOS_NO_CH343 -Port COM7
#   .\build-flash-monitor-s3.ps1 -Port COM6 -Clean
#   .\build-flash-monitor-s3.ps1 -Port COM6 -NoMonitor
#   .\build-flash-monitor-s3.ps1 -Monitor -Port COM6
#   .\build-flash-monitor-s3.ps1 -SoftApProbe -Port COM6
#   .\build-flash-monitor-s3.ps1 -Help

param(
    [Alias("Board", "Target")]
    [ValidateSet("S3", "C3", "C3_LUAOS", "C3_LUAOS_NO_CH343")]
    [string]$BoardType = "S3",
    [Alias("p")]
    [string]$Port = "COM6",
    [Alias("b")]
    [int]$Baud = 115200,
    [Alias("c")]
    [switch]$Clean,
    [Alias("nm")]
    [switch]$NoMonitor,
    [Alias("m")]
    [switch]$Monitor,
    [Alias("d")]
    [switch]$Dtr,
    [Alias("r")]
    [switch]$Rts,
    [Alias("sap")]
    [switch]$SoftApProbe,
    [Alias("Sketch")]
    [string]$SketchPath,
    [Alias("n")]
    [switch]$Notify,
    [Alias("nn")]
    [switch]$NoNotify,
    [Alias("h", "?")]
    [switch]$Help
)

$ErrorActionPreference = "Stop"

$DefaultSketchDir = Join-Path $PSScriptRoot "code"
$SoftApProbeSketchDir = Join-Path $env:TEMP "sms_softap_minimal_test"

if ($SoftApProbe -and -not $PSBoundParameters.ContainsKey("BoardType")) {
    $BoardType = "C3"
}

if ($SoftApProbe -and $SketchPath) {
    throw "-SoftApProbe and -SketchPath cannot be used together."
}

$boardConfigs = @{
    "S3" = @{
        Name = "ESP32-S3 SuperMini"
        ShortName = "S3"
        Fqbn = "esp32:esp32:esp32s3:CDCOnBoot=cdc,UploadSpeed=115200,FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled"
        BoardFlag = "-DSMS_BOARD_S3"
        LogSuffix = "s3"
    }
    "C3_LUAOS" = @{
        Name = "LuatOS ESP32C3-CORE"
        ShortName = "C3_LUAOS"
        Fqbn = "esp32:esp32:esp32c3:UploadSpeed=921600,CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=dio,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,EraseFlash=all,JTAGAdapter=default,ZigbeeMode=default"
        BoardFlag = "-DSMS_BOARD_C3_LUAOS"
        LogSuffix = "c3-luaos"
    }
    "C3_LUAOS_NO_CH343" = @{
        Name = "LuatOS ESP32C3-CORE (USB CDC)"
        ShortName = "C3_LUAOS_NO_CH343"
        Fqbn = "esp32:esp32:esp32c3:UploadSpeed=921600,CDCOnBoot=cdc,CPUFreq=160,FlashFreq=80,FlashMode=dio,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,EraseFlash=all,JTAGAdapter=default,ZigbeeMode=default"
        BoardFlag = "-DSMS_BOARD_C3_LUAOS"
        LogSuffix = "c3-luaos-no-ch343"
    }
    "C3" = @{
        Name = "ESP32-C3 SuperMini"
        ShortName = "C3"
        Fqbn = "esp32:esp32:esp32c3:UploadSpeed=921600,CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashSize=4M,PartitionScheme=huge_app,DebugLevel=none,EraseFlash=all,JTAGAdapter=default,ZigbeeMode=default"
        BoardFlag = "-DSMS_BOARD_C3"
        LogSuffix = "c3"
    }
}

$boardConfig = $boardConfigs[$BoardType]
$BoardDisplayName = $boardConfig.Name
$BoardShortName = $boardConfig.ShortName
$Fqbn = $boardConfig.Fqbn
$BoardFlag = $boardConfig.BoardFlag
$LogSuffix = $boardConfig.LogSuffix

$SketchDir = $DefaultSketchDir
$SketchDisplayName = "project firmware"
if ($SoftApProbe) {
    $SketchDir = $SoftApProbeSketchDir
    $SketchDisplayName = "SoftAP probe sketch"
    $LogSuffix = "$LogSuffix-softap-probe"
} elseif ($SketchPath) {
    $SketchDir = $SketchPath
    $SketchDisplayName = "custom sketch"
    $LogSuffix = "$LogSuffix-custom"
}

if (Test-Path -LiteralPath $SketchDir -PathType Leaf) {
    $SketchDir = Split-Path -Parent (Resolve-Path -LiteralPath $SketchDir)
} elseif (Test-Path -LiteralPath $SketchDir -PathType Container) {
    $SketchDir = (Resolve-Path -LiteralPath $SketchDir).Path
} else {
    throw "Sketch path not found: $SketchDir"
}

function Show-Usage {
    $scriptName = Split-Path -Leaf $PSCommandPath
    Write-Host @"
Usage:
  .\$scriptName
  .\$scriptName -Port COM6 [options]
  .\$scriptName -Board C3 -Port COM7 [options]
  .\$scriptName -SoftApProbe -Port COM6 [options]
  .\$scriptName -SketchPath <path> -Board C3 -Port COM6 [options]
  .\$scriptName -p COM6 [options]

Purpose:
  Compile the selected ESP32 SuperMini firmware, upload it to the selected
  serial port, then open an interactive timestamped serial monitor by default.
  Default board: ESP32-S3 SuperMini. Default port: COM6.

Options:
  -BoardType, -Board, -Target <S3|C3|C3_LUAOS|C3_LUAOS_NO_CH343>
                     Select the target board. Default: S3
  -Port, -p <COM>    Target serial port, for example COM6. Default: COM6
  -Baud, -b <baud>   Serial monitor baud rate. Default: 115200
  -Clean, -c         Clean build cache before compiling
  -NoMonitor, -nm    Do not open the serial monitor after upload
  -Monitor, -m       Only open the serial monitor; do not compile or upload
  -Dtr, -d           Assert DTR while monitoring. Default: off
  -Rts, -r           Assert RTS while monitoring. Default: off
  -SoftApProbe, -sap Build/upload the temporary minimal C3 SoftAP probe sketch
                     from %TEMP%\sms_softap_minimal_test. Defaults board to C3.
  -SketchPath, -Sketch <path>
                     Build/upload a custom sketch folder or .ino file
  -Notify, -n        Force local notification on completion/failure
  -NoNotify, -nn     Do not show the completion/failure notification
  -Help, -h, -?      Show this help

Examples:
  .\$scriptName
      Compile, upload, and monitor the default ESP32-S3 firmware on COM6.

  .\$scriptName -Port COM6
      Compile, upload to COM6, then open the interactive timestamped serial monitor.

  .\$scriptName -Board C3 -Port COM7
      Compile the ESP32-C3 SuperMini firmware, upload it to COM7, then open the monitor.

  .\$scriptName -Board C3_LUAOS -Port COM7
      Compile for LuatOS ESP32C3-CORE board, upload it to COM7, then open the monitor.

  .\$scriptName -Board C3_LUAOS_NO_CH343 -Port COM7
      Compile for LuatOS ESP32C3-CORE without CH343, with USB CDC enabled.

  .\$scriptName -p COM6 -c
      Same as -Port/-Clean, but with the short aliases for faster typing.

  .\$scriptName -Port COM6 -Clean
      Clean all cached build output first, then compile, upload, and monitor.

  .\$scriptName -Port COM6 -NoMonitor
      Compile and upload only. Use this when another serial monitor is already open.

  .\$scriptName -Monitor -Port COM6
      Open only the interactive timestamped serial monitor. No compile or upload.

  .\$scriptName -SoftApProbe -Port COM6
      Compile and upload the minimal C3 SoftAP probe sketch, then open the monitor.
      Use this to check whether the board can broadcast C3-AP-PROBE outside this project.

  .\$scriptName -SketchPath "$env:TEMP\sms_softap_minimal_test" -Board C3 -Port COM6
      Compile and upload a custom sketch directory. Useful for one-off hardware probes.

  .\$scriptName -Port COM6 -NoNotify
      Compile, upload, and monitor without the local completion/failure notification.
"@
}

if ($Help) {
    Show-Usage
    return
}

function Send-LocalNotification {
    param(
        [string]$Title,
        [string]$Message,
        [ValidateSet("Info", "Success", "Warning", "Error")]
        [string]$Level = "Info"
    )

    if ($NoNotify) {
        return
    }

    try {
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction SilentlyContinue
        Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue

        switch ($Level) {
            "Success" { [System.Media.SystemSounds]::Asterisk.Play(); $icon = [System.Windows.Forms.ToolTipIcon]::Info }
            "Warning" { [System.Media.SystemSounds]::Exclamation.Play(); $icon = [System.Windows.Forms.ToolTipIcon]::Warning }
            "Error" { [System.Media.SystemSounds]::Hand.Play(); $icon = [System.Windows.Forms.ToolTipIcon]::Error }
            default { [System.Media.SystemSounds]::Asterisk.Play(); $icon = [System.Windows.Forms.ToolTipIcon]::Info }
        }

        $burntToast = Get-Command New-BurntToastNotification -ErrorAction SilentlyContinue
        if ($burntToast) {
            New-BurntToastNotification -Text $Title, $Message | Out-Null
            return
        }

        $notifyIcon = New-Object System.Windows.Forms.NotifyIcon
        $notifyIcon.Icon = [System.Drawing.SystemIcons]::Information
        $notifyIcon.BalloonTipIcon = $icon
        $notifyIcon.BalloonTipTitle = $Title
        $notifyIcon.BalloonTipText = $Message
        $notifyIcon.Visible = $true
        $notifyIcon.ShowBalloonTip(5000)
        Start-Sleep -Milliseconds 800
        $notifyIcon.Dispose()
    } catch {
        Write-Host "Notification failed: $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

function Start-SerialMonitor {
    param([string]$PortName, [int]$BaudRate, [bool]$AssertDtr, [bool]$AssertRts)

    Write-Host "`n=== Serial monitor $PortName @ $BaudRate ===" -ForegroundColor Cyan
    Write-Host "Type a line and press Enter to send. Local commands: :help, :q" -ForegroundColor DarkCyan
    Write-Host ("DTR={0}, RTS={1}" -f $AssertDtr, $AssertRts) -ForegroundColor DarkGray

    $sp = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $sp.Encoding = [System.Text.Encoding]::UTF8
    $sp.ReadTimeout = 200
    $sp.NewLine = "`r`n"
    $sp.DtrEnable = $AssertDtr
    $sp.RtsEnable = $AssertRts

    function Open-SerialPort {
        if ($sp.IsOpen) {
            return $true
        }
        try {
            $sp.Open()
            $sp.DtrEnable = $AssertDtr
            $sp.RtsEnable = $AssertRts
            Write-Host "Serial port opened." -ForegroundColor DarkGray
            return $true
        } catch {
            return $false
        }
    }

    if (-not (Open-SerialPort)) {
        Write-Host "Waiting for serial port $PortName..." -ForegroundColor Yellow
    }

    $buffer = ""
    $inputLine = ""

    function Write-SerialPrompt {
        param([string]$Text)
        Write-Host -NoNewline ("> " + $Text)
    }

    function Show-MonitorHelp {
        Write-Host ""
        Write-Host "Local monitor commands:" -ForegroundColor Cyan
        Write-Host "  :help    Show this help"
        Write-Host "  :q       Quit monitor"
        Write-Host ""
        Write-Host "Any other line is sent to the serial port with CRLF."
        Write-SerialPrompt $inputLine
    }

    try {
        Write-SerialPrompt $inputLine
        while ($true) {
            if (-not $sp.IsOpen) {
                if (Open-SerialPort) {
                    Write-SerialPrompt $inputLine
                } else {
                    Start-Sleep -Milliseconds 500
                    continue
                }
            }

            try {
                $chunk = $sp.ReadExisting()
            } catch [TimeoutException] {
                $chunk = ""
            } catch [System.InvalidOperationException] {
                Write-Host "`nSerial port disconnected, waiting for reconnect..." -ForegroundColor Yellow
                try { $sp.Close() } catch {}
                Start-Sleep -Milliseconds 500
                continue
            } catch [System.IO.IOException] {
                Write-Host "`nSerial port I/O error, waiting for reconnect..." -ForegroundColor Yellow
                try { $sp.Close() } catch {}
                Start-Sleep -Milliseconds 500
                continue
            }

            if ($chunk.Length -gt 0) {
                $buffer += $chunk
                while ($buffer.Contains("`n")) {
                    $idx = $buffer.IndexOf("`n")
                    $line = $buffer.Substring(0, $idx).TrimEnd("`r")
                    $buffer = $buffer.Substring($idx + 1)
                    $ts = (Get-Date).ToString("HH:mm:ss.fff")
                    Write-Host "`r$(' ' * ([Console]::CursorLeft + 2))`r" -NoNewline
                    Write-Host "[$ts] $line"
                    Write-SerialPrompt $inputLine
                }
            }

            while ([Console]::KeyAvailable) {
                $key = [Console]::ReadKey($true)

                if ($key.Key -eq [ConsoleKey]::Enter) {
                    Write-Host ""
                    $lineToSend = $inputLine
                    $inputLine = ""

                    if ($lineToSend -eq ":q") {
                        return
                    }
                    if ($lineToSend -eq ":help") {
                        Show-MonitorHelp
                        continue
                    }
                    if ($lineToSend.Length -gt 0) {
                        if ($sp.IsOpen) {
                            try {
                                $sp.WriteLine($lineToSend)
                            } catch {
                                Write-Host "Send failed, serial port is reconnecting." -ForegroundColor Yellow
                                try { $sp.Close() } catch {}
                            }
                        } else {
                            Write-Host "Send skipped, serial port is not open." -ForegroundColor Yellow
                        }
                    }
                    Write-SerialPrompt $inputLine
                    continue
                }

                if ($key.Key -eq [ConsoleKey]::Backspace) {
                    if ($inputLine.Length -gt 0) {
                        $inputLine = $inputLine.Substring(0, $inputLine.Length - 1)
                        Write-Host "`b `b" -NoNewline
                    }
                    continue
                }

                if ($key.Key -eq [ConsoleKey]::Escape) {
                    $inputLine = ""
                    Write-Host "`r$(' ' * ([Console]::CursorLeft + 2))`r" -NoNewline
                    Write-SerialPrompt $inputLine
                    continue
                }

                if (-not [char]::IsControl($key.KeyChar)) {
                    $inputLine += $key.KeyChar
                    Write-Host -NoNewline $key.KeyChar
                }
            }

            if ($chunk.Length -eq 0) {
                Start-Sleep -Milliseconds 20
            }
        }
    } finally {
        if ($sp.IsOpen) { $sp.Close() }
        Write-Host "`nSerial port closed." -ForegroundColor Cyan
    }
}

if ($Monitor) {
    Write-Host ("=== Monitor only ({0}) ===" -f $BoardDisplayName) -ForegroundColor Green
    Start-SerialMonitor -PortName $Port -BaudRate $Baud -AssertDtr $Dtr.IsPresent -AssertRts $Rts.IsPresent
    return
}

$compileArgs = @(
    "compile", "-v",
    "--fqbn", $Fqbn,
    "--build-property", "compiler.cpp.extra_flags=$BoardFlag"
)
if ($Clean) { $compileArgs += "--clean" }
$compileArgs += $SketchDir

function Get-CompileSourceLabel {
    param([string]$Line)

    $source = $null
    $matches = [regex]::Matches($Line, '"?([^"\s]+?\.(?:ino\.cpp|cpp|c|S|s))"?')
    if ($matches.Count -gt 0) {
        $source = $matches[$matches.Count - 1].Groups[1].Value
    }

    if (-not $source) {
        return $null
    }

    $source = $source -replace '/', '\'
    $source = $source.Trim('"')

    $markers = @(
        "\sketch\",
        "\libraries\",
        "\cores\",
        "\variants\",
        "\src\"
    )

    foreach ($marker in $markers) {
        $idx = $source.LastIndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase)
        if ($idx -ge 0) {
            return $source.Substring($idx + 1)
        }
    }

    return Split-Path $source -Leaf
}

function Get-CompileUnitCount {
    param([string[]]$Args)

    $dryRunArgs = @()
    foreach ($arg in $Args) {
        if ($arg -ne "-v") {
            $dryRunArgs += $arg
        }
    }
    $dryRunArgs += "--dry-run"

    try {
        $count = 0
        & arduino-cli @dryRunArgs 2>&1 | ForEach-Object {
            $line = [string]$_
            if ((Get-CompileSourceLabel $line) -and $line -match '\s-c\s') {
                $count++
            }
        }
        if ($LASTEXITCODE -eq 0 -and $count -gt 0) {
            return $count
        }
    } catch {
    }

    return $null
}

Write-Host ("=== Compile {0} ({1}) ===" -f $SketchDisplayName, $BoardShortName) -ForegroundColor Green
Write-Host ("Sketch: {0}" -f $SketchDir) -ForegroundColor DarkGray

$totalCompileUnits = Get-CompileUnitCount $compileArgs
if ($totalCompileUnits) {
    Write-Host ("Compile units: {0}" -f $totalCompileUnits) -ForegroundColor DarkGray
} else {
    Write-Host "Compile units: unknown" -ForegroundColor DarkGray
}

$logFile = Join-Path $env:TEMP ("arduino-build-{0}.log" -f $LogSuffix)
$logLines = New-Object System.Collections.Generic.List[string]
$count = 0

& arduino-cli @compileArgs 2>&1 | ForEach-Object {
    $line = [string]$_
    $logLines.Add($line)

    $sourceLabel = Get-CompileSourceLabel $line
    if ($sourceLabel -and $line -match '\s-c\s') {
        $count++
        if ($totalCompileUnits) {
            $msg = "  compiling [{0}/{1}] {2}" -f $count, $totalCompileUnits, $sourceLabel
        } else {
            $msg = "  compiling [{0}/?] {1}" -f $count, $sourceLabel
        }
        Write-Host ("`r{0}{1}" -f $msg, (' ' * [Math]::Max(0, 60 - $msg.Length))) -NoNewline -ForegroundColor DarkGray
    }
    elseif ($line -match '(?i)\b(error|fatal)\b|error:') {
        Write-Host "`r$line" -ForegroundColor Red
    }
    elseif ($line -match '(?i)warning:') {
        Write-Host "`r$line" -ForegroundColor Yellow
    }
    elseif ($line -match '^(Sketch uses|Global variables|Used platform|Used library)') {
        Write-Host "`r$line" -ForegroundColor Cyan
    }
}
$exit = $LASTEXITCODE

Write-Host ""
[System.IO.File]::WriteAllLines($logFile, $logLines)

if ($exit -ne 0) {
    Write-Host "Compile failed. Full log: $logFile" -ForegroundColor Red
    Send-LocalNotification -Title ("SMS Forwarding {0} build failed" -f $BoardShortName) -Message "Compile failed. Full log: $logFile" -Level Error
    exit $exit
}
Write-Host ("Compile complete. Compiled {0} files. Full log: {1}" -f $count, $logFile) -ForegroundColor Green

Write-Host ("`n=== Upload {0} ({1}) to {2} ===" -f $SketchDisplayName, $BoardShortName, $Port) -ForegroundColor Green
& arduino-cli upload -p $Port --fqbn $Fqbn $SketchDir
if ($LASTEXITCODE -ne 0) {
    Write-Host "Upload failed." -ForegroundColor Red
    Send-LocalNotification -Title ("SMS Forwarding {0} upload failed" -f $BoardShortName) -Message "Upload to $Port failed." -Level Error
    exit $LASTEXITCODE
}
Write-Host "Upload complete." -ForegroundColor Green
Send-LocalNotification -Title ("SMS Forwarding {0} upload complete" -f $BoardShortName) -Message "$SketchDisplayName uploaded to $Port successfully." -Level Success

if (-not $NoMonitor) {
    Start-Sleep -Milliseconds 800
    Start-SerialMonitor -PortName $Port -BaudRate $Baud -AssertDtr $Dtr.IsPresent -AssertRts $Rts.IsPresent
}
