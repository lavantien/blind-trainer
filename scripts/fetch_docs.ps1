$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$refDoc = Join-Path $root 'ref_doc'
$retrieved = (Get-Date -AsUTC).ToString('yyyy-MM-dd')
$ua = 'blind-trainer-docs-fetch/1.0 (offline grounding snapshot)'

$entries = @(
    [pscustomobject]@{ Path = 'bevy/release-notes-v0.20.0.html'; Url = 'https://github.com/bevyengine/bevy/releases/tag/v0.20.0'; License = 'MIT OR Apache-2.0, bevyengine/bevy release notes'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/release-blog-0-20.html'; Url = 'https://bevy.org/news/bevy-0-20/'; License = 'CC-BY-4.0, bevy.org news post'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/migration-guide-0-19-to-0-20.html'; Url = 'https://bevy.org/learn/migration-guides/0-19-to-0-20/'; License = 'MIT OR Apache-2.0, bevy.org learn docs'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/migration-guide-text-input.md'; Url = 'https://raw.githubusercontent.com/bevyengine/bevy/v0.20.0/_release-content/migration-guides/text_input.md'; License = 'MIT OR Apache-2.0, bevyengine/bevy at v0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/migration-guide-escape-in-text-input.md'; Url = 'https://raw.githubusercontent.com/bevyengine/bevy/v0.20.0/_release-content/migration-guides/escape_in_a_text_input_releases_focus_and_propagates.md'; License = 'MIT OR Apache-2.0, bevyengine/bevy at v0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/quick-start-getting-started.html'; Url = 'https://bevy.org/learn/quick-start/getting-started/'; License = 'MIT OR Apache-2.0, bevy.org learn docs'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy.html'; Url = 'https://docs.rs/bevy/0.20.0/bevy/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-ui.html'; Url = 'https://docs.rs/bevy_ui/0.20.0/bevy_ui/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_ui 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-state.html'; Url = 'https://docs.rs/bevy_state/0.20.0/bevy_state/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_state 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-time.html'; Url = 'https://docs.rs/bevy_time/0.20.0/bevy_time/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_time 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-input.html'; Url = 'https://docs.rs/bevy_input/0.20.0/bevy_input/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_input 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-text.html'; Url = 'https://docs.rs/bevy_text/0.20.0/bevy_text/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_text 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-asset.html'; Url = 'https://docs.rs/bevy_asset/0.20.0/bevy_asset/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_asset 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-window.html'; Url = 'https://docs.rs/bevy_window/0.20.0/bevy_window/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_window 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-render.html'; Url = 'https://docs.rs/bevy_render/0.20.0/bevy_render/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_render 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-feathers.html'; Url = 'https://docs.rs/bevy_feathers/0.20.0/bevy_feathers/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_feathers 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-feathers-controls.html'; Url = 'https://docs.rs/bevy_feathers/0.20.0/bevy_feathers/controls/index.html'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_feathers 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-feathers-textinput.html'; Url = 'https://docs.rs/bevy_feathers/0.20.0/bevy_feathers/controls/struct.FeathersTextInput.html'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_feathers 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-text-editabletext.html'; Url = 'https://docs.rs/bevy_text/0.20.0/bevy_text/struct.EditableText.html'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_text 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/docsrs-bevy-ui-widgets.html'; Url = 'https://docs.rs/bevy_ui_widgets/0.20.0/bevy_ui_widgets/'; License = 'MIT OR Apache-2.0, docs.rs render of bevy_ui_widgets 0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bevy/feathers-src-text-input.rs'; Url = 'https://raw.githubusercontent.com/bevyengine/bevy/v0.20.0/crates/bevy_feathers/src/controls/text_input.rs'; License = 'MIT OR Apache-2.0, bevyengine/bevy at v0.20.0'; Binary = $false }
    [pscustomobject]@{ Path = 'bld/speedsolving-speffz.wiki'; Url = 'https://www.speedsolving.com/wiki/index.php/Speffz?action=raw'; License = 'speedsolving.com wiki, no explicit license, local reference only'; Binary = $false }
    [pscustomobject]@{ Path = 'bld/speedsolving-letter-pairs.wiki'; Url = 'https://www.speedsolving.com/wiki/index.php/List_of_letter_pairs?action=raw'; License = 'speedsolving.com wiki, no explicit license, local reference only'; Binary = $false }
    [pscustomobject]@{ Path = 'bld/speedsolving-bld-memorization.wiki'; Url = 'https://www.speedsolving.com/wiki/index.php/BLD_Memorization?action=raw'; License = 'speedsolving.com wiki, no explicit license, local reference only'; Binary = $false }
    [pscustomobject]@{ Path = 'bld/blddb-readme-v2.md'; Url = 'https://raw.githubusercontent.com/nbwzx/blddb/v2/README.md'; License = 'GPL-3.0, nbwzx/blddb v2 branch'; Binary = $false }
    [pscustomobject]@{ Path = 'bld/blddb-readme-main.md'; Url = 'https://raw.githubusercontent.com/nbwzx/blddb/main/README.md'; License = 'GPL-3.0, nbwzx/blddb main branch'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/wikipedia-phonology.wiki'; Url = 'https://en.wikipedia.org/wiki/Vietnamese_phonology?action=raw'; License = 'CC BY-SA 4.0, Wikipedia'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/kirby-2011-hanoi-vietnamese-jipa.pdf'; Url = 'https://web.archive.org/web/20231126004115im_/https://www.pure.ed.ac.uk/ws/portalfiles/portal/589949/S0025100311000181a.pdf'; License = 'Kirby 2011, JIPA 41(3):381-392, open access, canonical at research.ed.ac.uk/files/589949/S0025100311000181a.pdf which 403s non-browser clients, fetched from Wayback Machine snapshot 2023-11-26'; Binary = $true }
    [pscustomobject]@{ Path = 'vietnamese/syllable-readme.md'; Url = 'https://raw.githubusercontent.com/vietnameselanguage/syllable/master/README.md'; License = 'no license file in repo, vietnameselanguage/syllable'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/syllables-current.txt'; Url = 'https://raw.githubusercontent.com/vietnameselanguage/syllable/master/vietnamesesyllable.txt'; License = 'no license file in repo, vietnameselanguage/syllable'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/syllables-7184-abc.txt'; Url = 'https://raw.githubusercontent.com/vietnameselanguage/syllable/master/vietnamesesyllable_7184_sorted_abc.txt'; License = 'no license file in repo, vietnameselanguage/syllable'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/vn-freqs-readme.md'; Url = 'https://raw.githubusercontent.com/tabidots/vn-freqs/main/README.md'; License = 'no license file in repo, tabidots/vn-freqs'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/vn-word-frequencies.tsv'; Url = 'https://raw.githubusercontent.com/tabidots/vn-freqs/main/vn_word_frequencies.tsv'; License = 'no license file in repo, tabidots/vn-freqs'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/bigram-lm-readme.md'; Url = 'https://raw.githubusercontent.com/KienTranOMRE/vietnamese-bigram-lm/main/README.md'; License = 'no license file in repo, KienTranOMRE/vietnamese-bigram-lm'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/bigram-lm-result.txt'; Url = 'https://raw.githubusercontent.com/KienTranOMRE/vietnamese-bigram-lm/main/result.txt'; License = 'no license file in repo, KienTranOMRE/vietnamese-bigram-lm'; Binary = $false }
    [pscustomobject]@{ Path = 'vietnamese/bigram-lm-model.py'; Url = 'https://raw.githubusercontent.com/KienTranOMRE/vietnamese-bigram-lm/main/bigram_model.py'; License = 'no license file in repo, KienTranOMRE/vietnamese-bigram-lm'; Binary = $false }
)

$results = $entries | ForEach-Object -Parallel {
    $retrieved = $using:retrieved
    $ua = $using:ua
    $Entry = $_
    $target = Join-Path $using:refDoc $Entry.Path
    $dir = Split-Path -Parent $target
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $tmp = "$target.download"
    $stderr = (& curl.exe --fail --location --silent --show-error --compressed --max-time 300 --retry 3 --retry-delay 2 -A $ua -o $tmp $Entry.Url) 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        return [pscustomobject]@{ Path = $Entry.Path; Url = $Entry.Url; Ok = $false; Error = "curl exit ${LASTEXITCODE}: $stderr" }
    }
    if (-not (Test-Path $tmp)) {
        return [pscustomobject]@{ Path = $Entry.Path; Url = $Entry.Url; Ok = $false; Error = 'no output file produced' }
    }
    $bytes = [IO.File]::ReadAllBytes($tmp)
    if ($bytes.Length -eq 0) {
        Remove-Item -Force $tmp
        return [pscustomobject]@{ Path = $Entry.Path; Url = $Entry.Url; Ok = $false; Error = 'empty body' }
    }
    $header = "# source: $($Entry.Url)`n# retrieved: $retrieved`n# license: $($Entry.License)`n"
    if ($Entry.Binary) {
        [IO.File]::WriteAllBytes($target, $bytes)
        [IO.File]::WriteAllText("$target.meta.txt", $header)
    } else {
        $headerBytes = [Text.Encoding]::UTF8.GetBytes($header)
        $stream = [IO.MemoryStream]::new($headerBytes.Length + $bytes.Length)
        $stream.Write($headerBytes, 0, $headerBytes.Length)
        $stream.Write($bytes, 0, $bytes.Length)
        [IO.File]::WriteAllBytes($target, $stream.ToArray())
        $stream.Dispose()
    }
    Remove-Item -Force $tmp
    return [pscustomobject]@{ Path = $Entry.Path; Url = $Entry.Url; Ok = $true; Error = '' }
} -ThrottleLimit 6

$failed = @($results | Where-Object { -not $_.Ok })
if ($failed.Count -gt 0) {
    $lines = $failed | ForEach-Object { "FAILED $($_.Path) $($_.Url) $($_.Error)" }
    throw "docs fetch failed:`n$($lines -join "`n")"
}

$preamble = @(
    '# ref_doc manifest'
    ''
    "generated: $retrieved via make docs-fetch (scripts/fetch_docs.ps1)"
    'bevy line: 0.20.0 is current as of 2026-10-12 per crates.io max_version, released 2026-10-08, no 0.20.x patch and no 0.21 announced'
    'crate versions pinned: bevy 0.20.0, bevy_ui 0.20.0, bevy_state 0.20.0, bevy_time 0.20.0, bevy_input 0.20.0, bevy_text 0.20.0, bevy_asset 0.20.0, bevy_window 0.20.0, bevy_render 0.20.0, bevy_feathers 0.20.0, bevy_ui_widgets 0.20.0'
    'blddb branches: v2 (default, deployed at v2.blddb.net, dev rewrite) and main (v1 stable at blddb.net)'
    'dataset corrections vs plan: vn-freqs ships word-level frequencies (vn_word_frequencies.tsv), not syllable-level, reduce by whitespace split; vietnamese-bigram-lm keeps only top-20 bigrams plus corpus stats in result.txt, the full bigram table is not in the repo; Kirby is JIPA 41(3) 2011 (published online 2011-11-11), commonly cited as 2010, fetched via a pinned Wayback Machine snapshot because research.ed.ac.uk 403s non-browser clients'
    'file format: text files carry a 3-line header prefixed with "# " (source, retrieved, license), consumers skip lines starting with "# "; the pdf is saved bare with a sidecar .meta.txt'
    'format: sha256  bytes  path  url'
    ''
)

$manifestLines = New-Object System.Collections.Generic.List[string]
foreach ($entry in $entries) {
    $target = Join-Path $refDoc $entry.Path
    $hash = (Get-FileHash -Algorithm SHA256 $target).Hash.ToLower()
    $len = (Get-Item $target).Length
    $manifestLines.Add("$hash  $len  $($entry.Path)  $($entry.Url)")
    if ($entry.Binary) {
        $meta = "$target.meta.txt"
        $metaHash = (Get-FileHash -Algorithm SHA256 $meta).Hash.ToLower()
        $metaLen = (Get-Item $meta).Length
        $manifestLines.Add("$metaHash  $metaLen  $($entry.Path).meta.txt  $($entry.Url)")
    }
}

$manifestPath = Join-Path $refDoc 'MANIFEST.md'
[IO.File]::WriteAllLines($manifestPath, ($preamble + $manifestLines))
Write-Host "fetched $($entries.Count) files into ref_doc, manifest at $manifestPath"
