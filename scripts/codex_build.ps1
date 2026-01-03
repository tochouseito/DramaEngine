# scripts/codex_build.ps1
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

param(
    # 1) .slnx を明示。必要なら -Solution で上書き可。
    [string] $Solution = "DramaEngine.slnx",

    # 2) 固定したい構成
    [string] $Configuration = "Debug",
    [string] $Platform = "x64",

    # 3) 必要なときだけ復元（NuGet 等）
    [switch] $Restore,

    # 4) C++ は VsDevCmd 経由が安定
    [switch] $UseVsDevCmd = $true
)

function Get-RepoRoot()
{
    # 1) scripts/ の親を repo ルートとみなす
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-SolutionPath([string] $repoRoot, [string] $solutionArg)
{
    # 1) 引数が空なら自動検出（.slnx 優先）
    if ([string]::IsNullOrWhiteSpace($solutionArg))
    {
        $candidates = @()
        $candidates += Get-ChildItem -Path $repoRoot -Filter "*.slnx" -File -ErrorAction SilentlyContinue
        $candidates += Get-ChildItem -Path $repoRoot -Filter "*.sln"  -File -ErrorAction SilentlyContinue

        if ($candidates.Count -eq 0)
        {
            throw "repo ルートに .slnx / .sln が見つかりません。-Solution で明示してください。"
        }
        if ($candidates.Count -gt 1)
        {
            $names = ($candidates | ForEach-Object { $_.Name }) -join ", "
            throw "repo ルートに .slnx / .sln が複数あります ($names)。-Solution で明示してください。"
        }

        return $candidates[0].FullName
    }

    # 2) 引数指定がある場合（相対/絶対どちらでも）
    $full = $solutionArg
    if (-not (Test-Path $full))
    {
        $full = Join-Path $repoRoot $solutionArg
    }

    if (-not (Test-Path $full))
    {
        throw "Solution が見つかりません: $solutionArg"
    }

    return (Resolve-Path $full).Path
}

function Find-VsWhere()
{
    # 1) 典型パス
    $candidates = @(
        "$env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    foreach ($p in $candidates)
    {
        if (Test-Path $p)
        {
            return $p
        }
    }

    return ""
}

function Find-VisualStudio([string] $vswherePath)
{
    # 1) vswhere が無ければ諦める
    if ([string]::IsNullOrWhiteSpace($vswherePath))
    {
        return ""
    }

    # 2) MSBuild を含む最新の VS を拾う
    $installPath = & $vswherePath -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($LASTEXITCODE -ne 0)
    {
        return ""
    }

    if ([string]::IsNullOrWhiteSpace($installPath))
    {
        return ""
    }

    return $installPath.Trim()
}

function Find-MSBuild([string] $vsInstallPath)
{
    # 1) VS から MSBuild.exe を探す
    if (-not [string]::IsNullOrWhiteSpace($vsInstallPath))
    {
        $candidates = @(
            (Join-Path $vsInstallPath "MSBuild\Current\Bin\MSBuild.exe"),
            (Join-Path $vsInstallPath "MSBuild\17.0\Bin\MSBuild.exe")
        )

        foreach ($p in $candidates)
        {
            if (Test-Path $p)
            {
                return $p
            }
        }
    }

    # 2) PATH の msbuild にフォールバック
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($null -ne $cmd)
    {
        return $cmd.Source
    }

    return ""
}

function Find-VsDevCmd([string] $vsInstallPath)
{
    if ([string]::IsNullOrWhiteSpace($vsInstallPath))
    {
        return ""
    }

    $p = Join-Path $vsInstallPath "Common7\Tools\VsDevCmd.bat"
    if (Test-Path $p)
    {
        return $p
    }

    return ""
}

function Ensure-WorkspaceCaches([string] $repoRoot)
{
    # 1) 生成物・キャッシュを repo 内に寄せる（Codex sandbox/workspace-write 対策）
    $tmpDir   = Join-Path $repoRoot ".tmp"
    $nugetDir = Join-Path $repoRoot ".cache\nuget"

    New-Item -ItemType Directory -Force -Path $tmpDir   | Out-Null
    New-Item -ItemType Directory -Force -Path $nugetDir | Out-Null

    # 2) 外部書き込み（ユーザープロファイル等）を減らす
    $env:TEMP = $tmpDir
    $env:TMP  = $tmpDir
    $env:NUGET_PACKAGES = $nugetDir
}

function Get-MSBuildVersion([string] $msbuildPath)
{
    # 1) msbuild -version をパース（失敗したら空）
    try
    {
        $out = & $msbuildPath -version -nologo 2>$null
        if ($LASTEXITCODE -ne 0)
        {
            return ""
        }

        # 2) 最初に見つかった “数字.数字” を採用（例: 17.12.6）
        foreach ($line in $out)
        {
            $m = [regex]::Match($line, "(\d+)\.(\d+)(\.\d+)?")
            if ($m.Success)
            {
                return $m.Value
            }
        }

        return ""
    }
    catch
    {
        return ""
    }
}

function Assert-SlnxSupportIfNeeded([string] $solutionPath, [string] $msbuildPath)
{
    # 1) .slnx の場合は MSBuild 17.12+ が必要（古いと失敗する）
    if ($solutionPath.EndsWith(".slnx", [System.StringComparison]::OrdinalIgnoreCase))
    {
        $ver = Get-MSBuildVersion -msbuildPath $msbuildPath
        if (-not [string]::IsNullOrWhiteSpace($ver))
        {
            $parts = $ver.Split(".")
            $major = [int]$parts[0]
            $minor = [int]$parts[1]

            if (($major -lt 17) -or (($major -eq 17) -and ($minor -lt 12)))
            {
                throw ".slnx は MSBuild 17.12+ が必要です。検出: $ver。Visual Studio / Build Tools を更新するか、.sln を用意してください。"
            }
        }
        else
        {
            Write-Host "[codex_build] WARN: MSBuild version could not be detected. .slnx requires MSBuild 17.12+." -ForegroundColor Yellow
        }
    }
}

function Invoke-Build(
    [string] $msbuildPath,
    [string] $vsDevCmdPath,
    [string] $solutionPath,
    [string] $configuration,
    [string] $platform,
    [bool] $restore,
    [bool] $useVsDevCmd
)
{
    # 1) msbuild 引数（Debug|x64 固定）
    $args = @(
        "`"$solutionPath`"",
        "/m",
        "/nologo",
        "/v:minimal",
        "/p:Configuration=$configuration",
        "/p:Platform=$platform"
    )

    # 2) 必要なら復元
    if ($restore)
    {
        $args += "/restore"
    }

    # 3) VsDevCmd 経由で環境を整える（cl.exe 等）
    if ($useVsDevCmd -and (-not [string]::IsNullOrWhiteSpace($vsDevCmdPath)))
    {
        $cmdLine = "`"$vsDevCmdPath`" -no_logo -arch=$platform -host_arch=$platform && `"$msbuildPath`" " + ($args -join " ")
        Write-Host "[codex_build] cmd.exe /c $cmdLine"
        & cmd.exe /c $cmdLine
        return $LASTEXITCODE
    }

    # 4) 直接実行（環境が整っている場合のみ）
    Write-Host "[codex_build] $msbuildPath $($args -join ' ')"
    & $msbuildPath @args
    return $LASTEXITCODE
}

# 1) repo ルートへ
$repoRoot = Get-RepoRoot
Set-Location $repoRoot

# 2) Solution パス確定
$solutionPath = Resolve-SolutionPath -repoRoot $repoRoot -solutionArg $Solution

# 3) キャッシュを repo 内へ寄せる
Ensure-WorkspaceCaches -repoRoot $repoRoot

# 4) VS / MSBuild / VsDevCmd を特定
$vswhere    = Find-VsWhere
$vsInstall  = Find-VisualStudio -vswherePath $vswhere
$msbuild    = Find-MSBuild -vsInstallPath $vsInstall
$vsDevCmd   = Find-VsDevCmd -vsInstallPath $vsInstall

if ([string]::IsNullOrWhiteSpace($msbuild))
{
    throw "MSBuild が見つかりません。Visual Studio Build Tools を入れて msbuild を使える状態にしてください。"
}

# 5) .slnx の場合、MSBuild 17.12+ をチェック
Assert-SlnxSupportIfNeeded -solutionPath $solutionPath -msbuildPath $msbuild

Write-Host "[codex_build] repoRoot     : $repoRoot"
Write-Host "[codex_build] solution     : $solutionPath"
Write-Host "[codex_build] configuration: $Configuration"
Write-Host "[codex_build] platform     : $Platform"
Write-Host "[codex_build] msbuild      : $msbuild"
Write-Host "[codex_build] vsDevCmd     : $vsDevCmd"
Write-Host "[codex_build] restore      : $($Restore.IsPresent)"
Write-Host "[codex_build] useVsDevCmd  : $UseVsDevCmd"

# 6) ビルド実行
$exitCode = Invoke-Build `
    -msbuildPath $msbuild `
    -vsDevCmdPath $vsDevCmd `
    -solutionPath $solutionPath `
    -configuration $Configuration `
    -platform $Platform `
    -restore $Restore.IsPresent `
    -useVsDevCmd $UseVsDevCmd

# 7) 結果
if ($exitCode -ne 0)
{
    Write-Host "[codex_build] Build FAILED. ExitCode=$exitCode"
    exit $exitCode
}

Write-Host "[codex_build] Build SUCCEEDED."
exit 0
