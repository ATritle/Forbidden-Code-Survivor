param([string]$TargetName='InstallerFinalSmoke-0.3.5',[ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version='0.3.5')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$artifacts=[IO.Path]::GetFullPath((Join-Path $projectRoot 'Artifacts'))+'\'
$target=[IO.Path]::GetFullPath((Join-Path $artifacts $TargetName))
if(-not $target.StartsWith($artifacts,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $target)){throw 'Use a new verification directory within Artifacts.'}
$registration='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\ForbiddenCodeSurvivor-'+$Version
if(Test-Path $registration){throw ('Preserve the existing '+$Version+' installation; use a separate machine or uninstall it before running this temporary installer test.')}
$setup=Start-Process -FilePath (Join-Path $artifacts ('Installer-'+$Version+'\ForbiddenCode-Survivor-'+$Version+'-Setup.exe')) -WindowStyle Hidden -PassThru -ArgumentList @('/S',('/D='+$target))
$setup.WaitForExit()
if($setup.ExitCode -ne 0){throw 'Installer failed.'}
$package=Join-Path $artifacts ('Windows-'+$Version);$count=0
foreach($file in Get-ChildItem -LiteralPath $package -File -Recurse | Where-Object {$_.Extension -ne '.pdb' -and $_.Name -notlike 'Manifest_DebugFiles*'}){
    $relative=$file.FullName.Substring($package.Length).TrimStart('\');$installed=Join-Path $target $relative
    if(-not(Test-Path -LiteralPath $installed) -or (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash){throw ('Installed file differs: '+$relative)}
    ++$count
}
@{result='INSTALLER_EXTRACTION_HASH_PASS';files=$count;installation=$target;version=$Version}|ConvertTo-Json|Set-Content (Join-Path $projectRoot ('Saved\Review\InstallerExtraction-'+$Version+'.json'))
& (Join-Path $PSScriptRoot 'Test-PackagedWindows.ps1') -PackageDirectory $target -ReportDirectory (Join-Path $projectRoot 'Saved\Review\InstalledSmoke')
$uninstaller=[IO.Path]::GetFullPath((Join-Path $target 'Uninstall.exe'))
if(-not $uninstaller.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected uninstall path.'}
$cleanup=Start-Process -FilePath $uninstaller -WindowStyle Hidden -PassThru -ArgumentList '/S';$cleanup.WaitForExit();Start-Sleep -Seconds 2
if((Test-Path -LiteralPath (Join-Path $target 'ForbiddenCodeSurvivor.exe')) -or (Test-Path $registration)){throw 'Temporary installer cleanup failed.'}
@{result='INSTALLER_ROUNDTRIP_PASS';extractionHashes=('Saved/Review/InstallerExtraction-'+$Version+'.json');installedLaunch='Saved/Review/InstalledSmoke/report.json';temporaryInstallRemoved=$true;originalBuildPreserved=$true}|ConvertTo-Json|Set-Content (Join-Path $projectRoot ('Saved\Review\InstallerRoundtrip-'+$Version+'.json'))
Write-Output 'INSTALLER_ROUNDTRIP_PASS'
