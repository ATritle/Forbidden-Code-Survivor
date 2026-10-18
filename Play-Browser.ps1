param([int]$Port=4173,[switch]$Stop)
$ErrorActionPreference='Stop'
$projectRoot=$PSScriptRoot
$serverPidPath=Join-Path $projectRoot 'Saved\BrowserServer.pid'
if($Stop){
    if(Test-Path -LiteralPath $serverPidPath){
        $browserPid=[int](Get-Content -LiteralPath $serverPidPath)
        $browserProcess=Get-CimInstance Win32_Process -Filter ('ProcessId='+$browserPid)
        $serverScript=Join-Path $projectRoot 'Web\serve.mjs'
        if($browserProcess -and $browserProcess.Name -eq 'node.exe' -and $browserProcess.CommandLine.Contains($serverScript)){Stop-Process -Id $browserPid}
        Remove-Item -LiteralPath $serverPidPath
    }
    Write-Output 'Local browser game server stopped.'
    return
}
$nodeCommand=Get-Command node -ErrorAction SilentlyContinue
$nodePath=if($nodeCommand){$nodeCommand.Source}else{Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe'}
if(-not (Test-Path -LiteralPath $nodePath)){throw 'Install Node.js LTS from nodejs.org, then run this launcher again.'}
$browserUri='http://127.0.0.1:'+$Port
$browserAlreadyRunning=$false
try{$response=Invoke-WebRequest -Uri $browserUri -TimeoutSec 2 -UseBasicParsing;$browserAlreadyRunning=$response.Content -match 'Forbidden Code: Survivor'}catch{}
if(-not $browserAlreadyRunning){
    New-Item -ItemType Directory -Path (Join-Path $projectRoot 'Saved') -Force | Out-Null
    $serverProcess=Start-Process -FilePath $nodePath -ArgumentList @(('"'+(Join-Path $projectRoot 'Web\serve.mjs')+'"'),$Port) -WorkingDirectory (Join-Path $projectRoot 'Web') -WindowStyle Hidden -PassThru
    $serverProcess.Id | Set-Content -LiteralPath $serverPidPath
    for($attempt=0;$attempt -lt 30;$attempt++){try{$response=Invoke-WebRequest -Uri $browserUri -TimeoutSec 1 -UseBasicParsing;if($response.StatusCode -eq 200){$browserAlreadyRunning=$true;break}}catch{};Start-Sleep -Milliseconds 200}
    if(-not $browserAlreadyRunning){throw 'The local browser game server did not start. Check whether the port is already in use.'}
}
Start-Process $browserUri
Write-Output ('Forbidden Code: Survivor is running at '+$browserUri)
