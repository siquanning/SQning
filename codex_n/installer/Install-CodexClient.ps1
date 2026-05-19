param(
    [string]$InstallDir = "E:\CodexClient",
    [int]$Port = 3790
)

$ErrorActionPreference = "Stop"

$InstallerDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourceRoot = Split-Path -Parent $InstallerDir
$InstallDir = [System.IO.Path]::GetFullPath($InstallDir)
$PublicTarget = Join-Path $InstallDir "public"
$DataTarget = Join-Path $InstallDir ".codex-client"
$Compiler = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\csc.exe"

if (-not (Test-Path $Compiler)) {
    throw "C# compiler not found: $Compiler"
}

New-Item -ItemType Directory -Force -Path $InstallDir, $PublicTarget, $DataTarget | Out-Null
Remove-Item -LiteralPath (Join-Path $DataTarget "server.out.log"), (Join-Path $DataTarget "server.err.log") -Force -ErrorAction SilentlyContinue

Copy-Item -LiteralPath (Join-Path $SourceRoot "server.js") -Destination $InstallDir -Force
Copy-Item -LiteralPath (Join-Path $SourceRoot "package.json") -Destination $InstallDir -Force
Copy-Item -LiteralPath (Join-Path $SourceRoot "README.md") -Destination $InstallDir -Force
Copy-Item -LiteralPath (Join-Path $SourceRoot "public\index.html") -Destination $PublicTarget -Force
Copy-Item -LiteralPath (Join-Path $SourceRoot "public\styles.css") -Destination $PublicTarget -Force
Copy-Item -LiteralPath (Join-Path $SourceRoot "public\app.js") -Destination $PublicTarget -Force
Copy-Item -LiteralPath (Join-Path $InstallerDir "Stop-CodexClient.ps1") -Destination $InstallDir -Force

$Workspace = [System.IO.Path]::GetFullPath($SourceRoot)
@"
port=$Port
workspace=$Workspace
"@ | Set-Content -LiteralPath (Join-Path $InstallDir "launcher.ini") -Encoding UTF8

$LauncherSource = Join-Path $InstallerDir "CodexClientLauncher.cs"
$LauncherExe = Join-Path $InstallDir "CodexClient.exe"
& $Compiler /nologo /target:winexe /r:System.Windows.Forms.dll /out:$LauncherExe $LauncherSource
if ($LASTEXITCODE -ne 0) {
    throw "Launcher compilation failed with exit code $LASTEXITCODE"
}

$DesktopDir = [Environment]::GetFolderPath("DesktopDirectory")
$StartMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
New-Item -ItemType Directory -Force -Path $StartMenuDir | Out-Null

$WshShell = New-Object -ComObject WScript.Shell
foreach ($ShortcutPath in @((Join-Path $DesktopDir "Codex Client.lnk"), (Join-Path $StartMenuDir "Codex Client.lnk"))) {
    $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
    $Shortcut.TargetPath = $LauncherExe
    $Shortcut.WorkingDirectory = $InstallDir
    $Shortcut.IconLocation = "$LauncherExe,0"
    $Shortcut.Description = "Open Codex Client"
    $Shortcut.Save()
}

Write-Host "Installed Codex Client to $InstallDir"
Write-Host "Launcher: $LauncherExe"
Write-Host "URL: http://127.0.0.1:$Port"
