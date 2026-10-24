$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'Xiaoma Archive.exe'
if (!(Test-Path -LiteralPath $exe)) {throw 'Please run this script within the final product directory.'}
function Set-RegistryText([string]$Key,[string]$Name,[string]$Value) {
    if (!$Key.StartsWith('HKCU:\')) {throw 'Only current user registry allowed.'}
    $taskKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($Key.Substring(6))
    try {
        $taskName = if ($Name -eq '(default)') {''} else {$Name}
        $taskKey.SetValue($taskName,$Value,[Microsoft.Win32.RegistryValueKind]::String)
    } finally {$taskKey.Close()}
}
$classes = 'HKCU:\Software\Classes'
$program = "$classes\Xiaoma.Archive"
Set-RegistryText $program '(default)' 'Xiaoma Archive'
Set-RegistryText "$program\DefaultIcon" '(default)' ('"{0}",0' -f $exe)
Set-RegistryText "$program\shell\open\command" '(default)' ('"{0}" --quick-extract "%1"' -f $exe)
Set-RegistryText "$classes\Applications\Xiaoma Archive.exe" 'FriendlyAppName' 'Xiaoma Archive'
Set-RegistryText "$classes\Applications\Xiaoma Archive.exe\shell\open\command" '(default)' ('"{0}" --quick-extract "%1"' -f $exe)
$caps = 'HKCU:\Software\XiaomaArchive\Capabilities'
Set-RegistryText $caps 'ApplicationName' 'Xiaoma Archive'
Set-RegistryText $caps 'ApplicationDescription' 'Lightweight compression, decompression, and verification tool'
Set-RegistryText $caps 'ApplicationIcon' ('"{0}",0' -f $exe)
foreach ($ext in '.zip','.7z','.rar','.tar','.gz','.tgz','.bz2','.xz','.zst','.cab','.iso','.wim','.001') {
    $openWith = "$classes\$ext\OpenWithProgids"
    Set-RegistryText $openWith 'Xiaoma.Archive' ''
    Set-RegistryText "$caps\FileAssociations" $ext 'Xiaoma.Archive'
}
Set-RegistryText 'HKCU:\Software\RegisteredApplications' 'Xiaoma Archive' 'Software\XiaomaArchive\Capabilities'
foreach ($kind in '*','Directory','Directory\Background') {
    $key = "$classes\$kind\shell\XiaomaArchivePack"
    Set-RegistryText $key '(default)' ''
    Set-RegistryText $key 'MUIVerb' 'Xiaoma Archive'
    Set-RegistryText $key 'SubCommands' ''
    Set-RegistryText $key 'Icon' ('"{0}",0' -f $exe)
    Set-RegistryText $key 'MultiSelectModel' 'Single'
    $taskRootKey=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key.Substring(6),$true)
    try {if ($taskRootKey.GetSubKeyNames() -contains 'command') {$taskRootKey.DeleteSubKeyTree('command')}} finally {$taskRootKey.Close()}
    $taskArgument=if($kind -eq 'Directory\Background') {'%V'} else {'%1'}
    foreach ($taskAction in @(@('01ZIP','Create ZIP (Direct completion)','--quick-zip'),@('02Seven','Create 7z (Direct completion)','--quick-7z'),@('03Store','Store only ZIP (Fastest, no compression)','--quick-store'),@('04Options','Password / Split volumes / More compression options…','--quick-options'))) {
        $taskVerb="$key\shell\$($taskAction[0])"
        Set-RegistryText $taskVerb '(default)' $taskAction[1]
        Set-RegistryText $taskVerb 'MUIVerb' $taskAction[1]
        Set-RegistryText $taskVerb 'MultiSelectModel' 'Single'
        Set-RegistryText "$taskVerb\command" '(default)' ('"{0}" {1} "{2}"' -f $exe,$taskAction[2],$taskArgument)
    }
}
foreach ($ext in '.zip','.7z','.rar','.tar','.gz','.tgz','.bz2','.xz','.zst','.cab','.iso','.001') {
    $key = "$classes\SystemFileAssociations\$ext\shell\XiaomaArchiveExtract"
    Set-RegistryText $key '(default)' 'Xiaoma Archive: Extract to named folder'
    Set-RegistryText $key 'MultiSelectModel' 'Single'
    Set-RegistryText $key 'Icon' ('"{0}",0' -f $exe)
    Set-RegistryText "$key\command" '(default)' ('"{0}" --quick-extract "%1"' -f $exe)
    $taskPasswordKey="$classes\SystemFileAssociations\$ext\shell\XiaomaArchivePassword"
    Set-RegistryText $taskPasswordKey '(default)' 'Xiaoma Archive: Extract with password…'
    Set-RegistryText $taskPasswordKey 'Icon' ('"{0}",0' -f $exe)
    Set-RegistryText $taskPasswordKey 'MultiSelectModel' 'Single'
    Set-RegistryText "$taskPasswordKey\command" '(default)' ('"{0}" --quick-extract-password "%1"' -f $exe)
}
Write-Output 'Right-click menu and open handler registered for current user. Default association needs to be selected in Windows Settings.'
