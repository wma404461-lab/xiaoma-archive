$ErrorActionPreference = 'Stop'
$classes = 'HKCU:\Software\Classes'
foreach ($key in "$classes\Xiaoma.Archive","$classes\Applications\小马压缩.exe","$classes\*\shell\XiaomaArchivePack","$classes\Directory\shell\XiaomaArchivePack","$classes\Directory\Background\shell\XiaomaArchivePack",'HKCU:\Software\XiaomaArchive') {
    if (Test-Path -LiteralPath $key) {Remove-Item -LiteralPath $key -Recurse -Force}
}
Remove-ItemProperty -LiteralPath 'HKCU:\Software\RegisteredApplications' -Name '小马压缩' -ErrorAction SilentlyContinue
foreach ($ext in '.zip','.7z','.rar','.tar','.gz','.tgz','.bz2','.xz','.zst','.cab','.iso','.wim','.001') {
    Remove-ItemProperty -LiteralPath "$classes\$ext\OpenWithProgids" -Name 'Xiaoma.Archive' -ErrorAction SilentlyContinue
    $taskPasswordKey="$classes\SystemFileAssociations\$ext\shell\XiaomaArchivePassword"
    if (Test-Path -LiteralPath $taskPasswordKey) {Remove-Item -LiteralPath $taskPasswordKey -Recurse -Force}
    $key = "$classes\SystemFileAssociations\$ext\shell\XiaomaArchiveExtract"
    if (Test-Path -LiteralPath $key) {Remove-Item -LiteralPath $key -Recurse -Force}
}
Write-Output '小马压缩的右键菜单和打开方式注册已移除。成品和原文件已保留。'
