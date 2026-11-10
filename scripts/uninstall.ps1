#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$class = 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}'

foreach ($adapter in Get-ChildItem $class -ErrorAction SilentlyContinue) {
  foreach ($name in 'OpenGLDriverName', 'OpenGLDriverNameWow') {
    $backup = $adapter.GetValue("Softgpu$name")
    if ($null -eq $backup) {
      continue
    }
    if ("$($adapter.GetValue($name))" -like '*\softgpu.dll') {
      Set-ItemProperty $adapter.PSPath $name $backup -Type $adapter.GetValueKind("Softgpu$name")
      "$($adapter.GetValue('DriverDesc')): $name restored"
    } else {
      "$($adapter.GetValue('DriverDesc')): $name kept, a driver update replaced softgpu"
    }
    Remove-ItemProperty $adapter.PSPath "Softgpu$name"
  }
}
