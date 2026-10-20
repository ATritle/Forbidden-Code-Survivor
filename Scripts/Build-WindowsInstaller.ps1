param([string]$CompilerPath='',[string]$PackageDirectory='',[string]$OutputDirectory='',[ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version='0.4.0')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$packageRoot=if($PackageDirectory){[IO.Path]::GetFullPath($PackageDirectory)}else{Join-Path $projectRoot ('Artifacts\Windows-'+$Version+'\Windows')}
if(-not $PackageDirectory -and -not(Test-Path -LiteralPath (Join-Path $packageRoot 'ForbiddenCodeSurvivor.exe'))){$packageRoot=Join-Path $projectRoot ('Artifacts\Windows-'+$Version)}
if(-not(Test-Path -LiteralPath (Join-Path $packageRoot 'ForbiddenCodeSurvivor.exe'))){throw 'Package the Windows edition first with Scripts/Package-Windows.ps1.'}
if(-not $CompilerPath){$compilerCommand=Get-Command makensis.exe -ErrorAction SilentlyContinue;$CompilerPath=if($compilerCommand){$compilerCommand.Source}else{Join-Path $projectRoot 'Tools\nsis\nsis-3.13\makensis.exe'}}
if(-not(Test-Path -LiteralPath $CompilerPath)){throw 'Run Scripts/setup_windows_installer.py to fetch the portable official NSIS compiler.'}
$output=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $projectRoot ('Artifacts\Installer-'+$Version)}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$uninstallLines=[Collections.Generic.List[string]]::new()
foreach($file in Get-ChildItem -LiteralPath $packageRoot -File -Recurse | Where-Object {$_.Extension -ne '.pdb' -and $_.Name -notlike 'Manifest_DebugFiles*'}){$relative=$file.FullName.Substring($packageRoot.Length).TrimStart('\');$uninstallLines.Add('Delete "$INSTDIR\'+$relative+'"')}
foreach($directory in Get-ChildItem -LiteralPath $packageRoot -Directory -Recurse | Sort-Object {$_.FullName.Length} -Descending){$relative=$directory.FullName.Substring($packageRoot.Length).TrimStart('\');$uninstallLines.Add('RMDir "$INSTDIR\'+$relative+'"')}
[IO.File]::WriteAllLines((Join-Path $output 'uninstall-files.nsh'),$uninstallLines,[Text.UTF8Encoding]::new($false))
& $CompilerPath ('/DPACKAGE_ROOT='+$packageRoot) ('/DBUILD_VERSION='+$Version) ('/DOUTPUT_DIRECTORY='+$output) ('/DUNINSTALL_INCLUDE='+(Join-Path $output 'uninstall-files.nsh')) (Join-Path $projectRoot 'Packaging\ForbiddenCodeSurvivor.nsi')
if($LASTEXITCODE -ne 0){throw 'NSIS installer compilation failed.'}
Write-Output ('WINDOWS_INSTALLER_READY '+(Join-Path $output ('ForbiddenCode-Survivor-'+$Version+'-Setup.exe')))
