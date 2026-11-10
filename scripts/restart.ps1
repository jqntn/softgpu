#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

foreach ($device in Get-PnpDevice -Class Display -PresentOnly) {
  $driver = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName DEVPKEY_Device_Driver).Data
  if (-not (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Class\$driver" -Name OpenGLDriverName -ErrorAction SilentlyContinue)) {
    continue
  }
  pnputil /restart-device $device.InstanceId | Out-Null
  if ($LASTEXITCODE -eq 3010) {
    "$($device.FriendlyName): restarts at the next reboot"
  } elseif ($LASTEXITCODE -ne 0) {
    throw "pnputil could not restart $($device.FriendlyName)"
  } else {
    "$($device.FriendlyName): restarted"
  }
}
