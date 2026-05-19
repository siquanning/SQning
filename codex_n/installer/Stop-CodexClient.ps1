$ErrorActionPreference = "SilentlyContinue"

$AppDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PidFile = Join-Path $AppDir ".codex-client\server.pid"

if (Test-Path $PidFile) {
    $ServerPid = (Get-Content -LiteralPath $PidFile -Raw).Trim()
    if ($ServerPid -match '^\d+$') {
        Stop-Process -Id ([int]$ServerPid) -Force
    }
}

Get-CimInstance Win32_Process |
    Where-Object {
        $_.Name -in @("node.exe", "CodexClient.exe") -and
        $_.CommandLine -like "*$AppDir*"
    } |
    ForEach-Object {
        Stop-Process -Id $_.ProcessId -Force
    }
