SHELL := pwsh.exe
.SHELLFLAGS := -NoProfile -ExecutionPolicy Bypass -Command

.PHONY: all curdir loop guard envv slashpath crossline exit7 guardfail dq splitloop abspath relpath envrel

all: curdir loop guard envv slashpath crossline

curdir:
	@Write-Output "curdir=[$(CURDIR)]"
	@Write-Output "pscwd=[$$(Get-Location)]"

loop:
	@foreach ($$t in 'a','b') { Write-Output "t=$$t" }

guard:
	@if ([string]::IsNullOrEmpty('x')) { Write-Output 'guard-unexpected'; exit 2 } else { Write-Output 'guard-pass-ok' }

envv:
	@$$env:PROBE = 'zzz'; Write-Output "env=[$$env:PROBE]"

slashpath:
	@$$env:WINDIR/System32/where.exe pwsh

crossline:
	@$$env:P1LEAK = 'yes'
	@if ($$env:P1LEAK) { Write-Output 'crossline=leaked' } else { Write-Output 'crossline=isolated' }

exit7:
	@exit 7

guardfail:
	@if ([string]::IsNullOrEmpty('')) { Write-Output 'usage: probe needs v=<x>'; exit 2 }

dq:
	@Write-Output "blind trainer v1"

splitloop:
	@foreach ($$t in 'x y z'.Split(' ')) { Write-Output fuzz-$$t }

abspath:
	@C:/Windows/System32/where.exe pwsh

relpath:
	@sub/probe2.ps1

envrel:
	@$$env:PROBE2 = 'y'; sub/probe2.ps1
