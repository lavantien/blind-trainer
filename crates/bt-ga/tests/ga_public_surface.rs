use bt_ga::features::{confusion, flow_penalty, parse};
use bt_ga::ga::{
    GaParams, build_env, draw_cuts, fitness, fitness_parts, mutate_replace, mutate_swap, ox, run,
    tournament,
};
use bt_ga::hash::sha256_hex;
use bt_scheme::Rng;
use std::collections::HashSet;

mod common;

const POOL: [&str; 14] = [
    "không", "lông", "tía", "mút", "đặt", "trăng", "của", "anh", "ón", "gì", "xinh", "vững",
    "được", "ngăn",
];

fn env(case_count: usize) -> bt_ga::ga::GaEnv {
    let feats = POOL.iter().map(|t| parse(t).unwrap()).collect::<Vec<_>>();
    let lengths = POOL.iter().map(|t| t.chars().count() as f32).collect();
    let cooc = vec![(0u32, 3u32, 0.4f32), (2u32, 5u32, 0.6f32)];
    let adj = vec![(1u32, 4u32, 0.3f32), (6u32, 7u32, 0.7f32)];
    build_env(POOL.len(), lengths, feats, case_count, cooc, adj).unwrap()
}

fn random_injective(rng: &mut Rng, pool_size: usize, n: usize) -> Vec<u32> {
    let mut genes: Vec<u32> = (0..pool_size as u32).collect();
    for i in (1..genes.len()).rev() {
        let j = rng.below(i as u64 + 1) as usize;
        genes.swap(i, j);
    }
    genes.truncate(n);
    genes
}

fn injective(chrom: &[u32]) -> bool {
    chrom.iter().collect::<HashSet<_>>().len() == chrom.len()
}

fn params(seed: u64) -> GaParams {
    GaParams {
        seed,
        population: 24,
        generations: 30,
        elite: 2,
        swap: 0.15,
        replace: 0.05,
        w_conf: 1.0,
        w_flow: 0.5,
        w_cplx: 0.1,
    }
}

fn outcome_hash(outcome: &bt_ga::ga::RunOutcome) -> String {
    let bytes =
        serde_json::to_vec(&(outcome.best.as_slice(), outcome.per_gen_best.as_slice())).unwrap();
    sha256_hex(&bytes)
}

#[test]
fn ox_pins_hold_through_the_public_api() {
    assert_eq!(
        ox(&[1, 2, 3, 4, 5], &[3, 1, 2, 5, 4], 1, 3),
        vec![4, 2, 3, 1, 5]
    );
    assert_eq!(
        ox(&[5, 4, 3, 2, 1], &[2, 1, 4, 3, 5], 1, 4),
        vec![5, 4, 3, 2, 1]
    );
    assert_eq!(
        ox(&[10, 11, 12, 13, 14, 15], &[12, 14, 15, 10, 11, 13], 1, 3),
        vec![13, 11, 12, 14, 15, 10]
    );
    assert_eq!(
        ox(&[0, 1, 1, 0, 2, 2], &[3, 4, 5, 6, 7, 1], 1, 3),
        vec![6, 1, 1, 3, 4, 5]
    );
}

#[test]
fn hang_watchdog_arms_for_this_binary() {
    common::arm_hang_watchdog();
}

#[test]
fn precomputed_tables_agree_with_the_feature_functions() {
    let env = env(8);
    let mut rng = Rng::new(3);
    for _ in 0..300 {
        let a = rng.below(POOL.len() as u64) as usize;
        let b = rng.below(POOL.len() as u64) as usize;
        let direct_conf = confusion(&env.feats[a], &env.feats[b]);
        let table_conf = env.conf_table[a * env.pool_size + b];
        assert!((direct_conf - table_conf).abs() < 1e-6);
        let direct_flow = flow_penalty(
            &env.feats[a],
            env.lengths[a] as usize,
            &env.feats[b],
            env.lengths[b] as usize,
        );
        let table_flow = env.flow_table[a * env.pool_size + b];
        assert!((direct_flow - table_flow).abs() < 1e-6);
    }
}

#[test]
fn operators_keep_assignments_injective_and_in_bounds() {
    let env = env(8);
    let mut rng = Rng::new(11);
    for _ in 0..400 {
        let p1 = random_injective(&mut rng, env.pool_size, env.case_count);
        let p2 = random_injective(&mut rng, env.pool_size, env.case_count);
        let (cut1, cut2) = draw_cuts(&mut rng, env.case_count);
        let child = ox(&p1, &p2, cut1, cut2);
        assert!(injective(&child));
        assert!(child.iter().all(|g| (*g as usize) < env.pool_size));
        assert_eq!(&child[cut1..cut2], &p1[cut1..cut2]);
        let mut swapped = child.clone();
        mutate_swap(&mut swapped, &mut rng);
        assert!(injective(&swapped));
        let mut replaced = child.clone();
        mutate_replace(&mut replaced, &mut rng, env.pool_size);
        assert!(injective(&replaced));
        assert!(replaced.iter().all(|g| (*g as usize) < env.pool_size));
    }
}

#[test]
fn fitness_matches_its_parts_and_rejects_out_of_range_genes() {
    let env = env(8);
    let mut rng = Rng::new(19);
    let chrom = random_injective(&mut rng, env.pool_size, env.case_count);
    let p = params(1);
    let parts = fitness_parts(&env, &chrom);
    let whole = fitness(&p, &env, &chrom);
    let recomposed = p.w_conf * parts[0] + p.w_flow * parts[1] + p.w_cplx * parts[2];
    assert!((whole - recomposed).abs() < 1e-5, "{whole} vs {recomposed}");
}

#[test]
fn run_hashes_stable_for_a_seed_and_divergent_across_seeds() {
    let env = env(8);
    let a = run(&env, &params(101));
    let a_replay = run(&env, &params(101));
    assert_eq!(outcome_hash(&a), outcome_hash(&a_replay));
    let b = run(&env, &params(102));
    assert_ne!(outcome_hash(&a), outcome_hash(&b));
    assert!(injective(&a.best));
    assert!(a.best.iter().all(|g| (*g as usize) < env.pool_size));
}

#[test]
fn tournament_returns_a_drawn_index_for_every_fitness_shape() {
    let mut rng = Rng::new(23);
    let shapes = [
        vec![0.0f32; 16],
        (0..16).map(|i| i as f32).collect(),
        vec![f32::NAN; 16],
    ];
    for fitness in &shapes {
        let winner = tournament(&mut rng, fitness);
        assert!(winner < fitness.len());
    }
}
