param([string]$OutputDirectory='', [string]$EngineDirectory='')
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if (!$EngineDirectory) { $EngineDirectory = Join-Path $taskRoot 'vendor' }
foreach ($name in '7z.exe','7z.dll','7-Zip-LICENSE.txt') {
    if (!(Test-Path -LiteralPath (Join-Path $EngineDirectory $name))) { throw "Missing 7-Zip dependency: $name. See README.md." }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Required Visual Studio C++ Build Tools.' }
$vc = Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$kitRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdk = Get-ChildItem -LiteralPath (Join-Path $kitRoot 'Include') -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h') } | Sort-Object Name -Descending | Select-Object -First 1
$env:INCLUDE = @((Join-Path $vc.FullName 'include'), (Join-Path $sdk.FullName 'ucrt'), (Join-Path $sdk.FullName 'shared'), (Join-Path $sdk.FullName 'um')) -join ';'
$env:LIB = @((Join-Path $vc.FullName 'lib\x64'), (Join-Path $kitRoot "Lib\$($sdk.Name)\ucrt\x64"), (Join-Path $kitRoot "Lib\$($sdk.Name)\um\x64")) -join ';'
$bin = Join-Path $vc.FullName 'bin\Hostx64\x64'
$env:PATH = "$bin;$env:PATH"
$out = if ($OutputDirectory) {[IO.Path]::GetFullPath($OutputDirectory)} else {Join-Path $taskRoot 'dist'}
$obj = Join-Path $taskRoot 'build'
New-Item -ItemType Directory -Path $out,$obj,(Join-Path $out 'engine') -Force | Out-Null
Push-Location $taskRoot
try {
    & python (Join-Path $taskRoot 'tools\make_icon.py')
    if ($LASTEXITCODE -ne 0) {throw 'Icon asset export failed (requires Python and Pillow)'}
    & (Join-Path $kitRoot "bin\$($sdk.Name)\x64\rc.exe") /nologo /fo (Join-Path $obj 'app.res') 'src\app.rc'
    if ($LASTEXITCODE -ne 0) {throw 'Resource compilation failed'}
    $compile = @('/nologo','/std:c++17','/utf-8','/W4','/O1','/MT','/EHsc','/GS','/GL','/DNDEBUG','/DUNICODE','/D_UNICODE','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX',"/Fo$obj\",'src\main.cpp','src\core.cpp','src\tests.cpp','src\quick.cpp',(Join-Path $obj 'app.res'),"/Fe$out\Xiaoma Archive.exe",'/link','/SUBSYSTEM:WINDOWS','/LTCG','/OPT:REF','/OPT:ICF','/DYNAMICBASE','/NXCOMPAT','/HIGHENTROPYVA','user32.lib','gdi32.lib','shell32.lib','comctl32.lib','dwmapi.lib','uxtheme.lib','ole32.lib','advapi32.lib','uuid.lib','psapi.lib','shlwapi.lib')
    & (Join-Path $bin 'cl.exe') @compile
    if ($LASTEXITCODE -ne 0) {throw 'Program compilation failed'}
    Copy-Item -LiteralPath (Join-Path $EngineDirectory '7z.exe'),(Join-Path $EngineDirectory '7z.dll'),(Join-Path $EngineDirectory '7-Zip-LICENSE.txt') -Destination (Join-Path $out 'engine') -Force
    Copy-Item -LiteralPath (Join-Path $taskRoot 'USER_GUIDE.txt') -Destination $out -Force
    Copy-Item -LiteralPath (Join-Path $taskRoot 'Install-ContextMenu.ps1'),(Join-Path $taskRoot 'Uninstall-ContextMenu.ps1') -Destination $out -Force
    Get-ChildItem -LiteralPath $out -Recurse -File | Measure-Object Length -Sum
} finally {Pop-Location}
