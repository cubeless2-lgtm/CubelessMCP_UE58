param(
 [Parameter(Mandatory=$true)][string]$Project,
 [string]$Engine = 'C:/Program Files/Epic Games/UE_5.8',
 [switch]$Plan, [switch]$SkipPlugin, [switch]$KeepRuntime, [switch]$WithoutHands, [switch]$WithoutManny, [switch]$NoEnable
)
$ErrorActionPreference = 'Stop'
$transferPython = Join-Path $Engine 'Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
if (-not (Test-Path -LiteralPath $transferPython)) { throw 'UE bundled Python not found. Check -Engine.' }
$transferArgs = @('-B', (Join-Path $PSScriptRoot 'install_transfer.py'), '--release', $PSScriptRoot, '--project', (Resolve-Path -LiteralPath $Project).Path, '--engine', (Resolve-Path -LiteralPath $Engine).Path)
if ($Plan) { $transferArgs += '--plan' }
if ($SkipPlugin) { $transferArgs += '--skip-plugin' }
# KeepRuntime remains accepted for older commands. Python environments are always prepared locally.
if ($WithoutHands) { $transferArgs += '--without-hands' }
if ($WithoutManny) { $transferArgs += '--without-manny' }
if ($NoEnable) { $transferArgs += '--no-enable' }
& $transferPython @transferArgs
if ($LASTEXITCODE -ne 0) { throw "Installation stopped, exit code $LASTEXITCODE. Existing differing files were preserved; inspect the reported stage." }
