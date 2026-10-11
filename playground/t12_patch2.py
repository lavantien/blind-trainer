def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for old, new in pairs:
        if old not in s:
            print("MISSING in %s: %r" % (path, old[:70]))
        s = s.replace(old, new)
    open(path, 'w', encoding='utf-8', newline='\n').write(s)

drill_pairs = []
for name, phase in [
    ("ready_keys", "Ready"),
    ("input_keys", "Memo"),
    ("verdict_keys", "Exec"),
    ("review_keys", "Review"),
]:
    drill_pairs.append((
        """pub fn %s(
    phase: Res<State<DrillPhase>>,
    mut inputs: MessageReader<KeyboardInput>,""" % name,
        """pub fn %s(
    mut inputs: MessageReader<KeyboardInput>,""" % name,
    ))
drill_pairs.append((
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    if *phase.get() != DrillPhase::Ready {
        return;
    }
    for input in inputs.read() {""",
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {""",
))
drill_pairs.append((
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    if *phase.get() != DrillPhase::Memo {
        return;
    }
    for input in inputs.read() {""",
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {""",
))
drill_pairs.append((
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    if *phase.get() != DrillPhase::Exec {
        return;
    }
    let Some(case) = session.case else {
        return;
    };""",
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    let Some(case) = session.case else {
        return;
    };""",
))
drill_pairs.append((
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    if *phase.get() != DrillPhase::Review {
        return;
    }
    for input in inputs.read() {""",
    """    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {""",
))
drill_pairs.append((
    """pub fn swap_target(
    app_state: Res<State<AppState>>,
    time: Res<Time<Virtual>>,""",
    """pub fn swap_target(
    time: Res<Time<Virtual>>,""",
))
drill_pairs.append((
    """    mut net: ResMut<NetState>,
) {
    if *app_state.get() != AppState::Drill {
        return;
    }
    session.target_swap_clock += time.delta_secs();""",
    """    mut net: ResMut<NetState>,
) {
    session.target_swap_clock += time.delta_secs();""",
))
patch('crates/bt-app/src/views/drill.rs', drill_pairs)

memo_pairs = [
    ("""pub fn intro_keys(
    phase: Res<State<MemoPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Intro {
        return;
    }
    for input in inputs.read() {""",
     """pub fn intro_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {"""),
    ("""pub fn stream(
    phase: Res<State<MemoPhase>>,
    time: Res<Time<Virtual>>,
    stores: Res<Stores>,
    mut session: ResMut<MemoSession>,
    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Stream {
        return;
    }
    let cadence = session.cadence_secs(&stores);""",
     """pub fn stream(
    time: Res<Time<Virtual>>,
    stores: Res<Stores>,
    mut session: ResMut<MemoSession>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    let cadence = session.cadence_secs(&stores);"""),
    ("""pub fn recall_keys(
    phase: Res<State<MemoPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Recall {
        return;
    }
    for input in inputs.read() {""",
     """pub fn recall_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {"""),
    ("""pub fn review_keys(
    phase: Res<State<MemoPhase>>,
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,""",
     """pub fn review_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,"""),
    ("""    mut next: ResMut<NextState<MemoPhase>>,
) {
    if *phase.get() != MemoPhase::Review {
        return;
    }
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {""",
     """    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {"""),
]
patch('crates/bt-app/src/views/memo.rs', memo_pairs)

solve_pairs = [
    ("""pub fn ready_keys(
    phase: Res<State<SolvePhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    if *phase.get() != SolvePhase::Ready {
        return;
    }
    for input in inputs.read() {""",
     """pub fn ready_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut next: ResMut<NextState<SolvePhase>>,
) {
    for input in inputs.read() {"""),
    ("""pub fn review_keys(
    phase: Res<State<SolvePhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,""",
     """pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,"""),
    ("""    mut next: ResMut<NextState<SolvePhase>>,
) {
    if *phase.get() != SolvePhase::Review {
        return;
    }
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            seed.0 = seed.0.wrapping_add(1);""",
     """    mut next: ResMut<NextState<SolvePhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            seed.0 = seed.0.wrapping_add(1);"""),
]
patch('crates/bt-app/src/views/solve.rs', solve_pairs)

patch('crates/bt-app/src/views/mod.rs', [
    ("""                (
                    drill::swap_target,
                    drill::ready_keys,
                    drill::input_keys,
                    drill::verdict_keys,
                    drill::review_keys,
                    memo::intro_keys,
                    memo::stream,
                    memo::recall_keys,
                    memo::review_keys,
                    solve::ready_keys,
                    (solve::memo_tick, solve::memo_keys).run_if(in_state(SolvePhase::Memo)),
                    (solve::exec_tick, solve::exec_keys).run_if(in_state(SolvePhase::Exec)),
                    solve::review_keys,
                    (algs::refresh_list, algs::list_keys).run_if(in_state(AppState::Algs)),
                    (settings::apply_draft, settings::mode_keys)
                        .run_if(in_state(AppState::Settings)),
                )
                    .in_set(ViewsLogic),""",
     """                (
                    IntoScheduleConfigs::run_if(drill::swap_target, in_state(AppState::Drill)),
                    IntoScheduleConfigs::run_if(drill::ready_keys, in_state(DrillPhase::Ready)),
                    IntoScheduleConfigs::run_if(drill::input_keys, in_state(DrillPhase::Memo)),
                    IntoScheduleConfigs::run_if(drill::verdict_keys, in_state(DrillPhase::Exec)),
                    IntoScheduleConfigs::run_if(drill::review_keys, in_state(DrillPhase::Review)),
                    IntoScheduleConfigs::run_if(memo::intro_keys, in_state(MemoPhase::Intro)),
                    IntoScheduleConfigs::run_if(memo::stream, in_state(MemoPhase::Stream)),
                    IntoScheduleConfigs::run_if(memo::recall_keys, in_state(MemoPhase::Recall)),
                    IntoScheduleConfigs::run_if(memo::review_keys, in_state(MemoPhase::Review)),
                    IntoScheduleConfigs::run_if(solve::ready_keys, in_state(SolvePhase::Ready)),
                    IntoScheduleConfigs::run_if(
                        solve::memo_tick,
                        in_state(SolvePhase::Memo),
                    ),
                    IntoScheduleConfigs::run_if(solve::memo_keys, in_state(SolvePhase::Memo)),
                    IntoScheduleConfigs::run_if(solve::exec_tick, in_state(SolvePhase::Exec)),
                    IntoScheduleConfigs::run_if(solve::exec_keys, in_state(SolvePhase::Exec)),
                    IntoScheduleConfigs::run_if(solve::review_keys, in_state(SolvePhase::Review)),
                    IntoScheduleConfigs::run_if(algs::refresh_list, in_state(AppState::Algs)),
                    IntoScheduleConfigs::run_if(algs::list_keys, in_state(AppState::Algs)),
                    IntoScheduleConfigs::run_if(settings::apply_draft, in_state(AppState::Settings)),
                    IntoScheduleConfigs::run_if(settings::mode_keys, in_state(AppState::Settings)),
                )
                    .in_set(ViewsLogic),"""),
])
print("ok")
