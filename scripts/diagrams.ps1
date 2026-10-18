$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$version = '1.2026.8'
$sha256 = '5e1ecfa8ecd32c90b03bbf3b1eb6f020943f98ab0fcf4032be31a0002ee2c462'
$url = "https://github.com/plantuml/plantuml/releases/download/v$version/plantuml-$version.jar"

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'docs/diagrams'
$tools = Join-Path $root 'artifacts/tools'
$jar = Join-Path $tools "plantuml-$version.jar"

New-Item -ItemType Directory -Force $out, $tools | Out-Null
if (-not (Test-Path $jar) -or (Get-FileHash $jar -Algorithm SHA256).Hash.ToLower() -ne $sha256) {
    Write-Output "fetching plantuml $version jar"
    curl.exe -fsSL $url -o $jar
    if ($LASTEXITCODE -ne 0) { throw "plantuml jar download failed with exit $LASTEXITCODE" }
}
$actual = (Get-FileHash $jar -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $sha256) { throw "plantuml jar hash mismatch: expected $sha256 got $actual" }
if (-not (Get-Command java -ErrorAction SilentlyContinue)) {
    $rootSlash = $root -replace '\\', '/'
    Write-Output "java not found; docker fallback: docker run --rm -v ${rootSlash}:/w -w /w/docs/diagrams plantuml/plantuml:$version -tpng '*.puml'"
    exit 1
}
$sources = @(Get-ChildItem -Path $out -Filter '*.puml' | Sort-Object Name)
if ($sources.Count -eq 0) { throw "no puml sources found in $out" }
java -jar $jar -failfast2 -overwrite -charset UTF-8 -tpng @($sources.FullName)
if ($LASTEXITCODE -ne 0) { throw "plantuml render failed with exit $LASTEXITCODE" }
foreach ($source in $sources) {
    $png = [IO.Path]::ChangeExtension($source.FullName, '.png')
    if (-not (Test-Path $png)) { throw "missing rendered png for $($source.Name)" }
    Write-Output "rendered $($source.BaseName).png"
}
