def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for old, new in pairs:
        if old not in s:
            print("MISSING in %s: %r" % (path, old[:70]))
        s = s.replace(old, new)
    open(path, 'w', encoding='utf-8', newline='\n').write(s)


patch('crates/bt-app/src/views/drill.rs', [
    ("""fn install_case(session: &mut DrillSession, net: &mut NetState, case: CaseId) {
    session.case = Some(case);
    session.position = case_position(&case);
    session.buffer_sticker = buffer_of(case.orbit);
    session.verdict = None;
    session.target_pair = Some((case.t1, case.t2));
    session.target_swap_clock = 0.0;
    net.cube = Cube::solved();
    net.buffer_sticker = Some(session.buffer_sticker);
    net.target_sticker = Some(case.t1);
}""",
     """fn install_case(session: &mut DrillSession, case: CaseId) {
    session.case = Some(case);
    session.position = case_position(&case);
    session.buffer_sticker = buffer_of(case.orbit);
    session.verdict = None;
    session.target_pair = Some((case.t1, case.t2));
    session.target_swap_clock = 0.0;
}

pub fn sync_net(session: Res<DrillSession>, mut net: ResMut<NetState>) {
    let Some(case) = session.case else {
        return;
    };
    net.cube = Cube::solved();
    net.buffer_sticker = Some(session.buffer_sticker);
    net.target_sticker = Some(case.t1);
}"""),
    ("""pub fn enter(
    mut pending: ResMut<PendingDrill>,
    mut session: ResMut<DrillSession>,
    mut net: ResMut<NetState>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    seed: Res<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    let case = pending.0.take().unwrap_or_else(|| {
        let mut rng = Rng::new(seed.0);
        pick_case(&corpus.0, &stores, now_day.0, &mut rng)
    });
    install_case(&mut session, &mut net, case);
    next.set(DrillPhase::Ready);
}""",
     """pub fn enter(
    mut pending: ResMut<PendingDrill>,
    mut session: ResMut<DrillSession>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    seed: Res<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    let case = pending.0.take().unwrap_or_else(|| {
        let mut rng = Rng::new(seed.0);
        pick_case(&corpus.0, &stores, now_day.0, &mut rng)
    });
    install_case(&mut session, case);
    next.set(DrillPhase::Ready);
}"""),
    ("""pub fn enter_ready(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut draft: ResMut<DrillDraft>,
    mut session: ResMut<DrillSession>,
) {
    messages.clear();
    draft.0.clear();
    session.verdict = None;
    session.target_swap_clock = 0.0;
}""",
     """pub fn enter_ready(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut draft: ResMut<DrillDraft>,
    mut session: ResMut<DrillSession>,
    mut net: ResMut<NetState>,
) {
    messages.clear();
    draft.0.clear();
    session.verdict = None;
    session.target_swap_clock = 0.0;
    sync_net(session.into_inner(), &mut net);
}"""),
    ("""pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,
    mut net: ResMut<NetState>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    mut seed: ResMut<DrillSeed>,
    mut pending: ResMut<PendingDrill>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let mut rng = Rng::new(seed.0);
            let case = pick_case(&corpus.0, &stores, now_day.0, &mut rng);
            seed.0 = seed.0.wrapping_add(1);
            install_case(&mut session, &mut net, case);
            pending.0 = None;
            next.set(DrillPhase::Ready);
            return;
        }
    }
}""",
     """pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    mut seed: ResMut<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let mut rng = Rng::new(seed.0);
            let case = pick_case(&corpus.0, &stores, now_day.0, &mut rng);
            seed.0 = seed.0.wrapping_add(1);
            install_case(&mut session, case);
            next.set(DrillPhase::Ready);
            return;
        }
    }
}"""),
    ("""    for children in &boxes {
        for child in children.iter() {
            if let Ok(mut editable) = inputs.get_mut(child) {
                if editable.value().to_string() != draft.0 {
                    editable.editor_mut().set_text(&draft.0);
                }
            }
        }
    }
}""",
     """    for children in &boxes {
        for child in children.iter() {
            if let Ok(mut editable) = inputs.get_mut(child)
                && editable.value().to_string() != draft.0
            {
                editable.editor_mut().set_text(&draft.0);
            }
        }
    }
}"""),
])

patch('crates/bt-app/src/views/memo.rs', [
    ("""pub fn intro_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            session.reset_stream();
            next.set(MemoPhase::Stream);
            return;
        }
    }
}""",
     """pub fn intro_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    book: Res<SyllableBook>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            session.seed = session.seed.wrapping_add(1);
            rebuild(&mut session, session.seed, &book);
            next.set(MemoPhase::Stream);
            return;
        }
    }
}"""),
    ("""pub fn enter_stream(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}""",
     """pub fn enter_stream(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut session: ResMut<MemoSession>,
) {
    messages.clear();
    session.reset_stream();
}"""),
    ("""pub fn review_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    mut seed: ResMut<MemoSeed>,
    book: Res<SyllableBook>,
    mut stores: ResMut<Stores>,
    sm2: Res<Sm2>,
    now_day: Res<NowDay>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::KeyR => {
                session.reset_stream();
                next.set(MemoPhase::Stream);
                return;
            }
            KeyCode::Space => {
                seed.0 = seed.0.wrapping_add(1);
                rebuild(&mut session, seed.0, &book);
                next.set(MemoPhase::Intro);
                return;
            }
            key => {
                let Some(rating) = rating_of(key) else {
                    continue;
                };
                rate_pairs(&mut stores, &sm2.0, &session, rating, now_day.0);
                commands.write_message(SaveStores);
                seed.0 = seed.0.wrapping_add(1);
                rebuild(&mut session, seed.0, &book);
                next.set(MemoPhase::Intro);
                return;
            }
        }
    }
}""",
     """pub fn review_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    session: Res<MemoSession>,
    mut stores: ResMut<Stores>,
    sm2: Res<Sm2>,
    now_day: Res<NowDay>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::KeyR => {
                next.set(MemoPhase::Stream);
                return;
            }
            KeyCode::Space => {
                next.set(MemoPhase::Intro);
                return;
            }
            key => {
                let Some(rating) = rating_of(key) else {
                    continue;
                };
                rate_pairs(&mut stores, &sm2.0, &session, rating, now_day.0);
                commands.write_message(SaveStores);
                next.set(MemoPhase::Intro);
                return;
            }
        }
    }
}"""),
])

patch('crates/bt-app/src/views/mod.rs', [
    ("""            .add_systems(OnEnter(AppState::Drill), drill::enter)""",
     """            .add_systems(OnEnter(AppState::Drill), (drill::enter, drill::sync_net).chain())"""),
])
print('ok')
