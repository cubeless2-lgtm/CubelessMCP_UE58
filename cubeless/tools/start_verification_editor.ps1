<# Start one local Editor after the existing-editor check. The caller verifies MCP separately. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EditorExe,
    [Parameter(Mandatory)][string]$Project,
    [Parameter(Mandatory)][ValidateRange(1, 65535)][int]$Port,
    [Parameter(Mandatory)][string]$Receipt
)

$ErrorActionPreference = 'Stop'
$mutex = $null
$lockHeld = $false
$stream = $null
$state = [ordered]@{
    status = 'failed'; stage = 'validate_paths'; launch_attempted = $false
    editor = $null; project = $null; port = $Port; pid = $null; start_time_utc = $null
    mcp_checked = $false
}

function Write-LaunchReceipt {
    $bytes = [Text.Encoding]::UTF8.GetBytes(($state | ConvertTo-Json -Compress) + [Environment]::NewLine)
    $stream.Position = 0
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.SetLength($bytes.Length)
    $stream.Flush($true)
}

try {
    $root = (Get-Item -LiteralPath (Join-Path $PSScriptRoot '../..')).FullName
    $projects = @(Get-ChildItem -LiteralPath $root -File -Force -Filter '*.uproject')
    $selected = Get-Item -LiteralPath $Project -Force
    if ($projects.Count -ne 1 -or $selected.FullName -ne $projects[0].FullName -or
        ($selected.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Expected the single regular .uproject directly above this cubeless bundle.'
    }
    $exe = Get-Item -LiteralPath $EditorExe -Force
    if ($exe.PSIsContainer -or $exe.FullName -notmatch '\\Engine\\Binaries\\Win64\\UnrealEditor\.exe$') {
        throw 'Specify the installed Engine\Binaries\Win64\UnrealEditor.exe.'
    }
    $state.editor = $exe.FullName
    $state.project = $selected.FullName

    $state.stage = 'validate_engine'
    $versionFile = Join-Path $exe.Directory.Parent.Parent.FullName 'Build/Build.version'
    $version = Get-Content -LiteralPath $versionFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($version -isnot [pscustomobject] -or
        ($version.MajorVersion -isnot [int] -and $version.MajorVersion -isnot [long]) -or
        ($version.MinorVersion -isnot [int] -and $version.MinorVersion -isnot [long]) -or
        $version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8) {
        throw 'The selected engine must declare integer MajorVersion 5 and MinorVersion 8 in Engine/Build/Build.version.'
    }
    $state.stage = 'validate_paths'

    $path = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Receipt)
    $path = [IO.Path]::GetFullPath($path)
    $verification = [IO.Path]::Combine($root, 'cubeless', 'records', 'verification') + '\'
    $leaf = [IO.Path]::GetFileName($path)
    if (-not $path.StartsWith($verification, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetExtension($leaf) -ne '.json' -or $leaf.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
        throw 'Receipt must be a new .json inside cubeless/records/verification.'
    }
    $parent = Get-Item -LiteralPath ([IO.Path]::GetDirectoryName($path)) -Force
    if (-not $parent.PSIsContainer) { throw 'Receipt directory does not exist.' }
    for ($dir = $parent; $null -ne $dir; $dir = $dir.Parent) {
        if ($dir.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Linked receipt directories are not supported.' }
    }

    $state.stage = 'check_existing'
    $mutex = [Threading.Mutex]::new($false, 'Local\CubelessMCPUE58VerificationEditorStart')
    try { $lockHeld = $mutex.WaitOne(0) }
    catch [Threading.AbandonedMutexException] { $lockHeld = $true; throw 'Previous launcher was interrupted; reconcile its receipt and process.' }
    if (-not $lockHeld) { throw 'Another verification launcher is active.' }
    $editors = @(Get-Process | Where-Object {
        $_.ProcessName -match '^UnrealEditor(?:-Win64-(?:Debug|DebugGame|Development|Shipping|Test))?(?:-Cmd)?$'
    })
    if ($editors.Count) { throw "An Editor already exists (PID $($editors.Id -join ', ')); check its project and connection." }
    $listeners = [Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
    if ($listeners | Where-Object Port -eq $Port) { throw "TCP port $Port is occupied." }

    $state.stage = 'reserve_receipt'
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    $state.status = 'launch_pending'
    # An interruption after this durable marker may leave a live process.
    $state.launch_attempted = $null
    Write-LaunchReceipt

    $state.stage = 'launch'
    $arguments = @(('"' + $selected.FullName + '"'), '/Engine/Maps/Entry', '-NoSplash', '-unattended',
                   '-ModelContextProtocolStartServer', "-ModelContextProtocolPort=$Port")
    $state.launch_attempted = $true
    $process = Start-Process -FilePath $exe.FullName -WorkingDirectory $root -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $state.pid = $process.Id
    $state.start_time_utc = $process.StartTime.ToUniversalTime().ToString('o')
    $state.status = 'launched'
    $state.stage = 'write_launch_receipt'
    Write-LaunchReceipt
    $state | ConvertTo-Json -Compress
} catch {
    $state.error = $_.Exception.Message
    if ($state.launch_attempted) {
        $state.status = 'launch_state_unknown'
        if ($null -ne $state.pid) { $state.status = 'launched_receipt_incomplete' }
    } else { $state.status = 'failed'; $state.launch_attempted = $false }
    if ($state.stage -eq 'launch' -and $null -ne $stream) {
        try { Write-LaunchReceipt } catch { $state.receipt_error = $_.Exception.Message }
    }
    $state | ConvertTo-Json -Compress
    exit 1
} finally {
    if ($null -ne $stream) { $stream.Dispose() }
    if ($lockHeld) { $mutex.ReleaseMutex() }
    if ($null -ne $mutex) { $mutex.Dispose() }
}
