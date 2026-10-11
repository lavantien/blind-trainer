import io

def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for old, new in pairs:
        if old not in s:
            print("MISSING in %s: %r" % (path, old[:70]))
        s = s.replace(old, new)
    open(path, 'w', encoding='utf-8', newline='\n').write(s)

patch('crates/bt-app/src/views/drill.rs', [
    ("""pub fn ready_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut next: NextState<DrillPhase>,
) {
    for input in inputs.read() {""",
     """pub fn ready_keys(
    phase: Res<State<DrillPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut next: NextState<DrillPhase>,
) {
    if *phase.get() != DrillPhase::Ready {
        return;
    }
    for input in inputs.read() {"""),
    ("""pub fn input_keys(
    mut inputs: MessageReader<KeyboardInput>,
    draft: Res<DrillDraft>,""",
     """pub fn input_keys(
    phase: Res<State<DrillPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    draft: Res<DrillDraft>,"""),
    ("""    mut next: NextState<DrillPhase>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Enter {""",
     """    mut next: NextState<DrillPhase>,
) {
    if *phase.get() != DrillPhase::Memo {
        return;
    }
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Enter {"""),
    ("""pub fn verdict_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,""",
     """pub fn verdict_keys(
    phase: Res<State<DrillPhase>>,
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,"""),
    ("""    mut next: NextState<DrillPhase>,
) {
    let Some(case) = session.case else {
        return;
    };""",
     """    mut next: NextState<DrillPhase>,
) {
    if *phase.get() != DrillPhase::Exec {
        return;
    }
    let Some(case) = session.case else {
        return;
    };"""),
    ("""pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,""",
     """pub fn review_keys(
    phase: Res<State<DrillPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,"""),
    ("""    mut next: NextState<DrillPhase>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let mut rng = Rng::new(seed.0);""",
     """    mut next: NextState<DrillPhase>,
) {
    if *phase.get() != DrillPhase::Review {
        return;
    }
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let mut rng = Rng::new(seed.0);"""),
    ("""pub fn swap_target(
    time: Res<Time<Virtual>>,""",
     """pub fn swap_target(
    app_state: Res<State<AppState>>,
    time: Res<Time<Virtual>>,"""),
    ("""    mut net: ResMut<NetState>,
) {
    session.target_swap_clock += time.delta_secs();""",
     """    mut net: ResMut<NetState>,
) {
    if *app_state.get() != AppState::Drill {
        return;
    }
    session.target_swap_clock += time.delta_secs();"""),
])

patch('crates/bt-app/src/views/memo.rs', [
    ("""pub fn intro_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    mut next: NextState<MemoPhase>,
) {
    for input in inputs.read() {""",
     """pub fn intro_keys(
    phase: Res<State<MemoPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Intro {
        return;
    }
    for input in inputs.read() {"""),
    ("""pub fn stream(
    time: Res<Time<Virtual>>,
    stores: Res<Stores>,
    mut session: ResMut<MemoSession>,
    mut next: NextState<MemoPhase>,
) {
    let cadence = session.cadence_secs(&stores);""",
     """pub fn stream(
    phase: Res<State<MemoPhase>>,
    time: Res<Time<Virtual>>,
    stores: Res<Stores>,
    mut session: ResMut<MemoSession>,
    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Stream {
        return;
    }
    let cadence = session.cadence_secs(&stores);"""),
    ("""pub fn recall_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut next: NextState<MemoPhase>,
) {
    for input in inputs.read() {""",
     """pub fn recall_keys(
    phase: Res<State<MemoPhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Recall {
        return;
    }
    for input in inputs.read() {"""),
    ("""pub fn review_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,""",
     """pub fn review_keys(
    phase: Res<State<MemoPhase>>,
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,"""),
    ("""    mut next: NextState<MemoPhase>,
) {
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {""",
     """    mut next: NextState<MemoPhase>,
) {
    if *phase.get() != MemoPhase::Review {
        return;
    }
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {"""),
])

patch('crates/bt-app/src/views/solve.rs', [
    ("""pub fn ready_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut next: NextState<SolvePhase>,
) {
    for input in inputs.read() {""",
     """pub fn ready_keys(
    phase: Res<State<SolvePhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,
    mut next: NextState<SolvePhase>,
) {
    if *phase.get() != SolvePhase::Ready {
        return;
    }
    for input in inputs.read() {"""),
    ("""pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,""",
     """pub fn review_keys(
    phase: Res<State<SolvePhase>>,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<SolveSession>,"""),
    ("""    mut next: NextState<SolvePhase>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            seed.0 = seed.0.wrapping_add(1);""",
     """    mut next: NextState<SolvePhase>,
) {
    if *phase.get() != SolvePhase::Review {
        return;
    }
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            seed.0 = seed.0.wrapping_add(1);"""),
])

patch('crates/bt-app/src/views/mod.rs', [
    ("""                (
                    (drill::swap_target,).run_if(in_state(AppState::Drill)),
                    (drill::ready_keys,).run_if(in_state(DrillPhase::Ready)),
                    (drill::input_keys,).run_if(in_state(DrillPhase::Memo)),
                    (drill::verdict_keys,).run_if(in_state(DrillPhase::Exec)),
                    (drill::review_keys,).run_if(in_state(DrillPhase::Review)),
                    (memo::intro_keys,).run_if(in_state(MemoPhase::Intro)),
                    (memo::stream,).run_if(in_state(MemoPhase::Stream)),
                    (memo::recall_keys,).run_if(in_state(MemoPhase::Recall)),
                    (memo::review_keys,).run_if(in_state(MemoPhase::Review)),
                    (solve::ready_keys,).run_if(in_state(SolvePhase::Ready)),
                    (solve::memo_tick, solve::memo_keys).run_if(in_state(SolvePhase::Memo)),
                    (solve::exec_tick, solve::exec_keys).run_if(in_state(SolvePhase::Exec)),
                    (solve::review_keys,).run_if(in_state(SolvePhase::Review)),
                    (algs::refresh_list, algs::list_keys).run_if(in_state(AppState::Algs)),
                    (settings::apply_draft, settings::mode_keys)
                        .run_if(in_state(AppState::Settings)),
                )
                    .in_set(ViewsLogic),""",
     """                (
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
                    .in_set(ViewsLogic),"""),
])
print("ok")
