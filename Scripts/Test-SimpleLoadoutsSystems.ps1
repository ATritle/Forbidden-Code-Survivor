param([string]$Executable='',[string]$UserDirectory='Saved/QA/SimpleLoadoutsSystems')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'}
$userRoot=[IO.Path]::GetFullPath($UserDirectory)
function Run-Case([string]$testCase,[string]$user,[switch]$Damage){
    $arguments=@('-ForbiddenQA',('-UserDir='+$user),'-nullrhi','-windowed','-ResX=1280','-ResY=720','-unattended','-nosplash')
    $relative='Saved/Review/Profile/'+$testCase+'.json'
    if($Damage){$arguments+='-DamageAudit';$relative='Saved/Review/DamageAudit/report.json'}else{$arguments+=@('-ProfileSmoke',('-ProfileCase='+$testCase))}
    $report=Join-Path $user $relative
    if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
    if($editor){$arguments=@((Join-Path $root 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
    $process=Start-Process -FilePath $Executable -WorkingDirectory $root -WindowStyle Hidden -PassThru -ArgumentList $arguments
    if(-not $process.WaitForExit(90000)){Stop-Process -Id $process.Id -Force;throw ('Timed out: '+$testCase)}
    if(-not(Test-Path -LiteralPath $report)){throw ('No '+$testCase+' report; inspect '+$user)}
    $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
    $expected=if($Damage){'DAMAGE_AUDIT_PASS'}else{'PROFILE_PASS'}
    if($process.ExitCode -ne 0 -or $result.result -ne $expected){throw (Get-Content -LiteralPath $report -Raw)}
    if($Damage){Write-Output ('DAMAGE_AUDIT_PASS '+$result.checks.Count+' numeric checks')}else{Get-Content -LiteralPath $report -Raw}
}
Run-Case Fresh (Join-Path $userRoot 'Fresh')
Run-Case FreshReload (Join-Path $userRoot 'Fresh')
$migration=Join-Path $userRoot 'Migration'
Run-Case PrepareV3 $migration
$old=Join-Path $migration 'Saved/SaveGames/ForbiddenCode_Profile_v3_Automation.sav'
$before=(Get-FileHash -LiteralPath $old -Algorithm SHA256).Hash
foreach($file in @('ForbiddenCode_Profile_v4_Automation.sav','ForbiddenCode_Profile_v4_Automation_Recovery.sav')){
    $fixture=Join-Path $migration ('Saved/SaveGames/'+$file)
    if(Test-Path -LiteralPath $fixture){Remove-Item -LiteralPath $fixture}
}
Run-Case Migration $migration
Run-Case Reload $migration
if((Get-FileHash -LiteralPath $old -Algorithm SHA256).Hash -ne $before){throw 'Migration changed the preserved v3 save'}
# Recovery from an unreadable current v4 file must restore its complete previous revision.
$current=Join-Path $migration 'Saved/SaveGames/ForbiddenCode_Profile_v4_Automation.sav'
[IO.File]::WriteAllText($current,'deliberately invalid isolated QA fixture')
Run-Case Reload $migration
$receipt=@{result='PROFILE_MIGRATION_PASS';previousSlotSHA256=$before;oldSaveUnchanged=$true;recoveryVerified=$true}|ConvertTo-Json
$receipt|Set-Content -LiteralPath (Join-Path $migration 'Saved/Review/Profile/migration-receipt.json') -Encoding UTF8
Run-Case Damage (Join-Path $userRoot 'Damage') -Damage
