$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
foreach ($f in 'src/app/boot.c', 'src/app/smoke.c', 'src/app/usage.c', 'src/app/views/fatal.c', 'src/app/views/replay_scene.c') {
    $log = git log -1 --format='%h %ci %s' -- $f
    "$f => $log"
}
"df82fee df82feed331a716b62f86ff955cfdb3164e1b418"
"modified vs HEAD:"
git status --porcelain -- src/app | Out-String
