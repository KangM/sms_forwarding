# build-flash-monitor-s3.ps1
# All-in-one ESP32-S3 helper: compile -> upload -> interactive timestamped serial monitor.
# Compile options match the ESP32-S3 settings in .github/workflows/build.yml.
#
# Examples:
#   .\build-flash-monitor-s3.ps1 -Port COM6
#   .\build-flash-monitor-s3.ps1 -Port COM6 -Clean
#   .\build-flash-monitor-s3.ps1 -Port COM6 -NoMonitor
#   .\build-flash-monitor-s3.ps1 -Monitor -Port COM6
#   .\build-flash-monitor-s3.ps1 -Help

param(
    [string]$Port = "COM6",
    [int]$Baud = 115200,
    [switch]$Clean,
    [switch]$NoMonitor,
    [switch]$Monitor,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

$Fqbn = "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=huge_app,PSRAM=disabled"
$BoardFlag = "-DSMS_BOARD_S3"
$SketchDir = Join-Path $PSScriptRoot "code"

function Show-Usage {
    $scriptName = Split-Path -Leaf $PSCommandPath
    Write-Host @"
Usage:
  .\$scriptName -Port COM6 [options]

Purpose:
  Compile the ESP32-S3 firmware, upload it to the selected serial port,
  then open an interactive timestamped serial monitor by default.

Options:
  -Port <COM port>   Target serial port, for example COM6. Default: COM6
  -Baud <baud>       Serial monitor baud rate. Default: 115200
  -Clean             Clean build cache before compiling
  -NoMonitor         Do not open the serial monitor after upload
  -Monitor           Only open the serial monitor; do not compile or upload
  -Help              Show this help

Examples:
  .\$scriptName -Port COM6
      Compile, upload to COM6, then open the interactive timestamped serial monitor.

  .\$scriptName -Port COM6 -Clean
      Clean all cached build output first, then compile, upload, and monitor.

  .\$scriptName -Port COM6 -NoMonitor
      Compile and upload only. Use this when another serial monitor is already open.

  .\$scriptName -Monitor -Port COM6
      Open only the interactive timestamped serial monitor. No compile or upload.
"@
}

if ($Help -or $PSBoundParameters.Count -eq 0) {
    Show-Usage
    return
}

function Start-SerialMonitor {
    param([string]$PortName, [int]$BaudRate)

    Write-Host "`n=== Serial monitor $PortName @ $BaudRate ===" -ForegroundColor Cyan
    Write-Host "Type a line and press Enter to send. Local commands: :help, :q" -ForegroundColor DarkCyan

    $sp = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $sp.Encoding = [System.Text.Encoding]::UTF8
    $sp.ReadTimeout = 200
    $sp.NewLine = "`r`n"
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true

    try {
        $sp.Open()
    } catch {
        Write-Host "Failed to open serial port $PortName`: $($_.Exception.Message)" -ForegroundColor Red
        return
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
            try {
                $chunk = $sp.ReadExisting()
            } catch [TimeoutException] {
                $chunk = ""
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
                        $sp.WriteLine($lineToSend)
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
    Start-SerialMonitor -PortName $Port -BaudRate $Baud
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

Write-Host "=== Compile firmware (S3) ===" -ForegroundColor Green

$totalCompileUnits = Get-CompileUnitCount $compileArgs
if ($totalCompileUnits) {
    Write-Host ("Compile units: {0}" -f $totalCompileUnits) -ForegroundColor DarkGray
} else {
    Write-Host "Compile units: unknown" -ForegroundColor DarkGray
}

$logFile = Join-Path $env:TEMP "arduino-build-s3.log"
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
    exit $exit
}
Write-Host ("Compile complete. Compiled {0} files. Full log: {1}" -f $count, $logFile) -ForegroundColor Green

Write-Host "`n=== Upload to $Port ===" -ForegroundColor Green
& arduino-cli upload -p $Port --fqbn $Fqbn $SketchDir
if ($LASTEXITCODE -ne 0) {
    Write-Host "Upload failed." -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "Upload complete." -ForegroundColor Green

if (-not $NoMonitor) {
    Start-Sleep -Milliseconds 800
    Start-SerialMonitor -PortName $Port -BaudRate $Baud
}
