#Requires -RunAsAdministrator
param(
  [string]$X64 = "$PSScriptRoot\..\build\softgpu.dll",
  [string]$X86 = "$PSScriptRoot\..\build-x86\softgpu.dll"
)
$ErrorActionPreference = 'Stop'
$class = 'HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}'

function Resolve-Dll($path, $machine) {
  $full = (Resolve-Path $path).Path
  $bytes = [IO.File]::ReadAllBytes($full)
  $pe = [BitConverter]::ToInt32($bytes, 0x3C)
  if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne $machine) {
    throw "$full is built for the wrong architecture"
  }
  $full
}

$dlls = [ordered]@{
  OpenGLDriverName    = Resolve-Dll $X64 0x8664
  OpenGLDriverNameWow = Resolve-Dll $X86 0x14C
}

$adapters = Get-ChildItem $class -ErrorAction SilentlyContinue |
  Where-Object { $_.GetValue('OpenGLDriverName') }
if (-not $adapters) {
  throw 'No display adapter has an OpenGL driver'
}
foreach ($adapter in $adapters) {
  foreach ($name in $dlls.Keys) {
    if ($null -eq $adapter.GetValue($name)) {
      continue
    }
    $kind = $adapter.GetValueKind($name)
    if ($null -eq $adapter.GetValue("Softgpu$name")) {
      Set-ItemProperty $adapter.PSPath "Softgpu$name" $adapter.GetValue($name) -Type $kind
    }
    Set-ItemProperty $adapter.PSPath $name $dlls[$name] -Type $kind
    "$($adapter.GetValue('DriverDesc')): $name -> $($dlls[$name])"
  }
}

& "$PSScriptRoot\restart.ps1"
