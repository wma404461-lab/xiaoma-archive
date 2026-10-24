$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot '小马压缩.exe'
if (!(Test-Path -LiteralPath $exe)) {throw '请在成品目录内运行此脚本。'}
function Set-RegistryText([string]$Key,[string]$Name,[string]$Value) {
    if (!$Key.StartsWith('HKCU:\')) {throw '仅允许当前用户注册表。'}
    $taskKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($Key.Substring(6))
    try {
        $taskName = if ($Name -eq '(default)') {''} else {$Name}
        $taskKey.SetValue($taskName,$Value,[Microsoft.Win32.RegistryValueKind]::String)
    } finally {$taskKey.Close()}
}
$classes = 'HKCU:\Software\Classes'
$program = "$classes\Xiaoma.Archive"
Set-RegistryText $program '(default)' '小马压缩'
Set-RegistryText "$program\DefaultIcon" '(default)' ('"{0}",0' -f $exe)
Set-RegistryText "$program\shell\open\command" '(default)' ('"{0}" --quick-extract "%1"' -f $exe)
Set-RegistryText "$classes\Applications\小马压缩.exe" 'FriendlyAppName' '小马压缩'
Set-RegistryText "$classes\Applications\小马压缩.exe\shell\open\command" '(default)' ('"{0}" --quick-extract "%1"' -f $exe)
$caps = 'HKCU:\Software\XiaomaArchive\Capabilities'
Set-RegistryText $caps 'ApplicationName' '小马压缩'
Set-RegistryText $caps 'ApplicationDescription' '轻量压缩、解压与校验工具'
Set-RegistryText $caps 'ApplicationIcon' ('"{0}",0' -f $exe)
foreach ($ext in '.zip','.7z','.rar','.tar','.gz','.tgz','.bz2','.xz','.zst','.cab','.iso','.wim','.001') {
    $openWith = "$classes\$ext\OpenWithProgids"
    Set-RegistryText $openWith 'Xiaoma.Archive' ''
    Set-RegistryText "$caps\FileAssociations" $ext 'Xiaoma.Archive'
}
Set-RegistryText 'HKCU:\Software\RegisteredApplications' '小马压缩' 'Software\XiaomaArchive\Capabilities'
foreach ($kind in '*','Directory','Directory\Background') {
    $key = "$classes\$kind\shell\XiaomaArchivePack"
    Set-RegistryText $key '(default)' ''
    Set-RegistryText $key 'MUIVerb' '小马压缩'
    Set-RegistryText $key 'SubCommands' ''
    Set-RegistryText $key 'Icon' ('"{0}",0' -f $exe)
    Set-RegistryText $key 'MultiSelectModel' 'Single'
    $taskRootKey=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($key.Substring(6),$true)
    try {if ($taskRootKey.GetSubKeyNames() -contains 'command') {$taskRootKey.DeleteSubKeyTree('command')}} finally {$taskRootKey.Close()}
    $taskArgument=if($kind -eq 'Directory\Background') {'%V'} else {'%1'}
    foreach ($taskAction in @(@('01ZIP','压成 ZIP（直接完成）','--quick-zip'),@('02Seven','压成 7z（直接完成）','--quick-7z'),@('03Store','仅打包 ZIP（最快，不压缩）','--quick-store'),@('04Options','密码 / 分卷 / 更多压缩选项…','--quick-options'))) {
        $taskVerb="$key\shell\$($taskAction[0])"
        Set-RegistryText $taskVerb '(default)' $taskAction[1]
        Set-RegistryText $taskVerb 'MUIVerb' $taskAction[1]
        Set-RegistryText $taskVerb 'MultiSelectModel' 'Single'
        Set-RegistryText "$taskVerb\command" '(default)' ('"{0}" {1} "{2}"' -f $exe,$taskAction[2],$taskArgument)
    }
}
foreach ($ext in '.zip','.7z','.rar','.tar','.gz','.tgz','.bz2','.xz','.zst','.cab','.iso','.001') {
    $key = "$classes\SystemFileAssociations\$ext\shell\XiaomaArchiveExtract"
    Set-RegistryText $key '(default)' '小马压缩：解压到同名文件夹'
    Set-RegistryText $key 'MultiSelectModel' 'Single'
    Set-RegistryText $key 'Icon' ('"{0}",0' -f $exe)
    Set-RegistryText "$key\command" '(default)' ('"{0}" --quick-extract "%1"' -f $exe)
    $taskPasswordKey="$classes\SystemFileAssociations\$ext\shell\XiaomaArchivePassword"
    Set-RegistryText $taskPasswordKey '(default)' '小马压缩：密码解压…'
    Set-RegistryText $taskPasswordKey 'Icon' ('"{0}",0' -f $exe)
    Set-RegistryText $taskPasswordKey 'MultiSelectModel' 'Single'
    Set-RegistryText "$taskPasswordKey\command" '(default)' ('"{0}" --quick-extract-password "%1"' -f $exe)
}
Write-Output '已注册当前用户的右键菜单和打开方式。默认关联需在 Windows 设置中选择。'
