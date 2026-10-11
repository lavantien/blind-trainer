import os

SRC_MOD = 'crates/bt-app/src/views/mod.rs'
SRC_PRESENT = 'crates/bt-app/src/present.rs'
STAGE = 'artifacts/progress/t12_stage'
os.makedirs(STAGE, exist_ok=True)

mod = open(SRC_MOD, encoding='utf-8').read()


def cut(s, snippets):
    for sn in snippets:
        if isinstance(sn, (list, tuple)):
            s = cut(s, sn)
            continue
        if sn not in s:
            raise SystemExit('missing snippet: %r' % sn[:60])
        s = s.replace(sn, '')
    return s


def span(text, start_marker, end_marker):
    start = text.index(start_marker)
    end = text.index(end_marker)
    return text[start:end]


MEMO_DEFS = span(
    mod,
    '#[derive(Clone, Debug, Default, Resource)]\npub struct SyllableBook',
    'pub fn now_day()',
)
MEMO_IMPORTS = 'use serde::Deserialize;\nuse std::collections::BTreeMap;\n'
MEMO_MODS = 'pub mod images;\npub mod memo;\n'
MEMO_RES = [
    '            .insert_resource(load_syllable_book(&paths))\n',
    '            .insert_resource(memo::MemoSeed(time_seed()))\n',
    '            .insert_resource(memo::MemoSession::default())\n',
]
MEMO_SYS = [
    '                    IntoScheduleConfigs::run_if(memo::intro_keys, in_state(MemoPhase::Intro)),\n',
    '                    IntoScheduleConfigs::run_if(memo::stream, in_state(MemoPhase::Stream)),\n',
    '                    IntoScheduleConfigs::run_if(memo::recall_keys, in_state(MemoPhase::Recall)),\n',
    '                    IntoScheduleConfigs::run_if(memo::review_keys, in_state(MemoPhase::Review)),\n',
]
MEMO_ENTER = [
    '            .add_systems(OnEnter(AppState::Memo), memo::enter)\n',
    '            .add_systems(OnEnter(MemoPhase::Intro), memo::enter_intro)\n',
    '            .add_systems(OnEnter(MemoPhase::Stream), memo::enter_stream)\n',
    '            .add_systems(OnEnter(MemoPhase::Recall), memo::enter_recall)\n',
    '            .add_systems(OnEnter(MemoPhase::Review), memo::enter_review)\n',
]

SOLVE_MS = span(mod, 'pub fn now_unix_ms', 'pub fn time_seed')
SOLVE_RES = [
    '            .insert_resource(solve::SolveSeed(time_seed()))\n',
    '            .insert_resource(solve::SolveScrambleOverride::default())\n',
    '            .insert_resource(solve::SolveSession::default())\n',
]
SOLVE_SYS = [
    '                    IntoScheduleConfigs::run_if(solve::ready_keys, in_state(SolvePhase::Ready)),\n',
    '                    IntoScheduleConfigs::run_if(solve::memo_tick, in_state(SolvePhase::Memo)),\n',
    '                    IntoScheduleConfigs::run_if(solve::memo_keys, in_state(SolvePhase::Memo)),\n',
    '                    IntoScheduleConfigs::run_if(solve::exec_tick, in_state(SolvePhase::Exec)),\n',
    '                    IntoScheduleConfigs::run_if(solve::exec_keys, in_state(SolvePhase::Exec)),\n',
    '                    IntoScheduleConfigs::run_if(solve::review_keys, in_state(SolvePhase::Review)),\n',
]
SOLVE_ENTER = [
    '            .add_systems(OnEnter(AppState::Solve), solve::enter)\n',
    '            .add_systems(OnEnter(SolvePhase::Ready), solve::enter_ready)\n',
    '            .add_systems(OnEnter(SolvePhase::Memo), solve::enter_memo)\n',
    '            .add_systems(OnEnter(SolvePhase::Exec), solve::enter_exec)\n',
    '            .add_systems(OnEnter(SolvePhase::Review), solve::enter_review)\n',
]

ALGS_RES = [
    '            .insert_resource(algs::AlgsQuery::default())\n',
    '            .insert_resource(algs::AlgsBrowser::default())\n',
]
ALGS_SYS = [
    '                    IntoScheduleConfigs::run_if(algs::refresh_list, in_state(AppState::Algs)),\n',
    '                    IntoScheduleConfigs::run_if(algs::list_keys, in_state(AppState::Algs)),\n',
]
ALGS_ENTER = '            .add_systems(OnEnter(AppState::Algs), algs::enter)\n'

STATS_RES = '            .insert_resource(stats::StatsSnapshot::default())\n'
STATS_ENTER = '            .add_systems(OnEnter(AppState::Stats), stats::enter)\n'

SETTINGS_RES = '            .insert_resource(settings::SettingsDraft::default())\n'
SETTINGS_SYS = [
    '                    (\n'
    '                        IntoScheduleConfigs::run_if(\n'
    '                            settings::mode_keys,\n'
    '                            in_state(AppState::Settings),\n'
    '                        ),\n'
    '                        IntoScheduleConfigs::run_if(\n'
    '                            settings::apply_draft,\n'
    '                            in_state(AppState::Settings),\n'
    '                        ),\n'
    '                    )\n'
    '                        .chain(),\n',
]
SETTINGS_ENTER = '            .add_systems(OnEnter(AppState::Settings), settings::enter)'

FULL_STATES = 'use crate::states::{AppState, DrillPhase, MemoPhase, SolvePhase};\n'
DRILL_STATES = 'use crate::states::{AppState, DrillPhase};\n'

v1 = cut(mod, [
    'pub mod algs;\n', MEMO_MODS, 'pub mod solve;\n', 'pub mod stats;\n',
    'pub mod settings;\n',
    MEMO_DEFS, MEMO_IMPORTS, FULL_STATES, SOLVE_MS,
    'use std::path::PathBuf;\n',
    MEMO_RES, SOLVE_RES, STATS_RES, SETTINGS_RES, ALGS_RES,
    MEMO_SYS, SOLVE_SYS, ALGS_SYS, SETTINGS_SYS,
    MEMO_ENTER, SOLVE_ENTER, ALGS_ENTER, STATS_ENTER, SETTINGS_ENTER,
])
v1 = v1.replace('use crate::persist::Stores;\n', 'use crate::persist::Stores;\n' + DRILL_STATES)
open(os.path.join(STAGE, 'mod_v1.rs'), 'w', encoding='utf-8', newline='\n').write(v1)

v2 = cut(mod, [
    'pub mod algs;\n', 'pub mod solve;\n', 'pub mod stats;\n', 'pub mod settings;\n',
    SOLVE_MS, SOLVE_RES, SOLVE_SYS, SOLVE_ENTER,
    ALGS_RES, ALGS_SYS, ALGS_ENTER,
    STATS_RES, STATS_ENTER, SETTINGS_RES, SETTINGS_SYS, SETTINGS_ENTER,
])
v2 = v2.replace(FULL_STATES, 'use crate::states::{AppState, DrillPhase, MemoPhase};\n')
open(os.path.join(STAGE, 'mod_v2.rs'), 'w', encoding='utf-8', newline='\n').write(v2)

v3 = cut(mod, [
    'pub mod algs;\n', 'pub mod stats;\n', 'pub mod settings;\n',
    ALGS_RES, ALGS_SYS, ALGS_ENTER,
    STATS_RES, STATS_ENTER, SETTINGS_RES, SETTINGS_SYS, SETTINGS_ENTER,
])
open(os.path.join(STAGE, 'mod_v3.rs'), 'w', encoding='utf-8', newline='\n').write(v3)

v4 = cut(mod, [
    'pub mod stats;\n', 'pub mod settings;\n',
    STATS_RES, STATS_ENTER, SETTINGS_RES, SETTINGS_SYS, SETTINGS_ENTER,
])
open(os.path.join(STAGE, 'mod_v4.rs'), 'w', encoding='utf-8', newline='\n').write(v4)

present = open(SRC_PRESENT, encoding='utf-8').read()

PRES_MEMO = span(present, '            .add_systems(\n                OnEnter(AppState::Memo)', '            .add_systems(\n                OnEnter(AppState::Solve)')
PRES_SOLVE = span(present, '            .add_systems(\n                OnEnter(AppState::Solve)', '            .add_systems(\n                OnEnter(AppState::Algs)')
PRES_ALGS = span(present, '            .add_systems(\n                OnEnter(AppState::Algs)', '            .add_systems(\n                OnEnter(AppState::Stats)')
PRES_STATS = span(present, '            .add_systems(\n                OnEnter(AppState::Stats)', '            .add_systems(\n                OnEnter(AppState::Settings)')
PRES_SETTINGS = span(present, '            .add_systems(\n                OnEnter(AppState::Settings)', '            .add_systems(OnEnter(AppState::Fatal), enter_fatal)')

BIND_MEMO = '                    views::memo::bind_text,\n'
BIND_SOLVE = '                    views::solve::bind_text,\n'
BIND_ALGS = (
    '                    (\n'
    '                        views::algs::read_query,\n'
    '                        views::algs::sync_query,\n'
    '                        views::algs::bind_detail,\n'
    '                    )\n'
    '                        .chain(),\n'
)
BIND_STATS = '                    views::stats::bind_text,\n'
BIND_SETTINGS = '                    views::settings::bind_text,\n'

p1 = cut(present, [PRES_MEMO, PRES_SOLVE, PRES_ALGS, PRES_STATS, PRES_SETTINGS, BIND_MEMO, BIND_SOLVE, BIND_ALGS, BIND_STATS, BIND_SETTINGS])
open(os.path.join(STAGE, 'present_v1.rs'), 'w', encoding='utf-8', newline='\n').write(p1)

p2 = cut(present, [PRES_SOLVE, PRES_ALGS, PRES_STATS, PRES_SETTINGS, BIND_SOLVE, BIND_ALGS, BIND_STATS, BIND_SETTINGS])
open(os.path.join(STAGE, 'present_v2.rs'), 'w', encoding='utf-8', newline='\n').write(p2)

p3 = cut(present, [PRES_ALGS, PRES_STATS, PRES_SETTINGS, BIND_ALGS, BIND_STATS, BIND_SETTINGS])
open(os.path.join(STAGE, 'present_v3.rs'), 'w', encoding='utf-8', newline='\n').write(p3)

p4 = cut(present, [PRES_STATS, PRES_SETTINGS, BIND_STATS, BIND_SETTINGS])
open(os.path.join(STAGE, 'present_v4.rs'), 'w', encoding='utf-8', newline='\n').write(p4)

print('staged versions written')
