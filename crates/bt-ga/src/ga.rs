use crate::features::{Features, confusion, flow_penalty};
use bt_scheme::Rng;
use rayon::prelude::*;

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct GaParams {
    pub seed: u64,
    pub population: usize,
    pub generations: usize,
    pub elite: usize,
    pub swap: f64,
    pub replace: f64,
    pub w_conf: f32,
    pub w_flow: f32,
    pub w_cplx: f32,
}

#[derive(Clone, Debug)]
pub struct GaEnv {
    pub pool_size: usize,
    pub case_count: usize,
    pub lengths: Vec<f32>,
    pub feats: Vec<Features>,
    pub conf_table: Vec<f32>,
    pub flow_table: Vec<f32>,
    pub cooc: Vec<(u32, u32, f32)>,
    pub adj: Vec<(u32, u32, f32)>,
}

#[derive(Clone, PartialEq, Debug)]
pub struct RunOutcome {
    pub best: Vec<u32>,
    pub best_fitness: f32,
    pub best_parts: [f32; 3],
    pub per_gen_best: Vec<f32>,
    pub best_generation: usize,
    pub stagnation_tail: u32,
    pub max_stagnation: u32,
    pub evaluations: u64,
}

pub fn build_env(
    pool_size: usize,
    lengths: Vec<f32>,
    feats: Vec<Features>,
    case_count: usize,
    cooc: Vec<(u32, u32, f32)>,
    adj: Vec<(u32, u32, f32)>,
) -> Result<GaEnv, String> {
    if feats.len() != pool_size || lengths.len() != pool_size {
        return Err("pool feature or length table size mismatch".to_string());
    }
    if pool_size < case_count {
        return Err(format!(
            "pool {pool_size} smaller than case count {case_count}"
        ));
    }
    let mut conf_table = vec![0.0f32; pool_size * pool_size];
    let mut flow_table = vec![0.0f32; pool_size * pool_size];
    for a in 0..pool_size {
        for b in 0..pool_size {
            conf_table[a * pool_size + b] = confusion(&feats[a], &feats[b]);
            flow_table[a * pool_size + b] = flow_penalty(
                &feats[a],
                lengths[a] as usize,
                &feats[b],
                lengths[b] as usize,
            );
        }
    }
    Ok(GaEnv {
        pool_size,
        case_count,
        lengths,
        feats,
        conf_table,
        flow_table,
        cooc,
        adj,
    })
}

pub fn fitness_parts(env: &GaEnv, chrom: &[u32]) -> [f32; 3] {
    let mut conf = 0.0f32;
    for &(i, j, w) in &env.cooc {
        conf += w * env.conf_table
            [chrom[i as usize] as usize * env.pool_size + chrom[j as usize] as usize];
    }
    let mut flow = 0.0f32;
    for &(i, j, w) in &env.adj {
        flow += w * env.flow_table
            [chrom[i as usize] as usize * env.pool_size + chrom[j as usize] as usize];
    }
    let cplx: f32 =
        chrom.iter().map(|&p| env.lengths[p as usize]).sum::<f32>() / env.case_count as f32;
    [conf, flow, cplx]
}

pub fn fitness(params: &GaParams, env: &GaEnv, chrom: &[u32]) -> f32 {
    let [conf, flow, cplx] = fitness_parts(env, chrom);
    params.w_conf * conf + params.w_flow * flow + params.w_cplx * cplx
}

pub fn ox(p1: &[u32], p2: &[u32], cut1: usize, cut2: usize) -> Vec<u32> {
    let n = p1.len();
    let mut child = vec![u32::MAX; n];
    let mut kept = std::collections::HashSet::with_capacity(cut2 - cut1);
    for i in cut1..cut2 {
        child[i] = p1[i];
        kept.insert(p1[i]);
    }
    let fill: Vec<u32> = p2.iter().copied().filter(|g| !kept.contains(g)).collect();
    let free = n - (cut2 - cut1);
    let mut slot = cut2 % n;
    for gene in fill.into_iter().take(free) {
        while child[slot] != u32::MAX {
            slot = (slot + 1) % n;
        }
        child[slot] = gene;
    }
    child
}

pub fn draw_cuts(rng: &mut Rng, n: usize) -> (usize, usize) {
    let cut1 = rng.below(n as u64 - 1) as usize;
    let cut2 = cut1 + 1 + rng.below(n as u64 - cut1 as u64 - 1) as usize;
    (cut1, cut2)
}

pub fn mutate_swap(chrom: &mut [u32], rng: &mut Rng) {
    let n = chrom.len();
    let i = rng.below(n as u64) as usize;
    let mut j = rng.below(n as u64 - 1) as usize;
    if j >= i {
        j += 1;
    }
    chrom.swap(i, j);
}

pub fn mutate_replace(chrom: &mut [u32], rng: &mut Rng, pool_size: usize) {
    let n = chrom.len();
    let mut used = vec![false; pool_size];
    for &gene in chrom.iter() {
        used[gene as usize] = true;
    }
    let pos = rng.below(n as u64) as usize;
    let mut candidate = rng.below(pool_size as u64) as usize;
    while used[candidate] {
        candidate = (candidate + 1) % pool_size;
    }
    chrom[pos] = candidate as u32;
}

pub fn tournament(rng: &mut Rng, fitness: &[f32]) -> usize {
    let a = rng.below(fitness.len() as u64) as usize;
    let b = rng.below(fitness.len() as u64) as usize;
    if fitness[b] < fitness[a] { b } else { a }
}

fn random_individual(rng: &mut Rng, pool_size: usize, case_count: usize) -> Vec<u32> {
    let mut genes: Vec<u32> = (0..pool_size as u32).collect();
    for i in (1..genes.len()).rev() {
        let j = rng.below(i as u64 + 1) as usize;
        genes.swap(i, j);
    }
    genes.truncate(case_count);
    genes
}

fn sorted_order(fitness: &[f32], population: &[Vec<u32>]) -> Vec<usize> {
    let mut order: Vec<usize> = (0..fitness.len()).collect();
    order.sort_by(|&a, &b| {
        fitness[a]
            .partial_cmp(&fitness[b])
            .unwrap_or(std::cmp::Ordering::Equal)
            .then_with(|| population[a].cmp(&population[b]))
    });
    order
}

pub fn run(env: &GaEnv, params: &GaParams) -> RunOutcome {
    assert!(params.population > params.elite);
    assert!(params.population >= 2);
    assert!(params.elite >= 1);
    let mut rng = Rng::new(params.seed);
    let mut population: Vec<Vec<u32>> = (0..params.population)
        .map(|_| random_individual(&mut rng, env.pool_size, env.case_count))
        .collect();
    let mut evaluations = params.population as u64;
    let mut scores: Vec<f32> = population
        .par_iter()
        .map(|chrom| fitness(params, env, chrom))
        .collect();
    let mut order = sorted_order(&scores, &population);
    let mut best = population[order[0]].clone();
    let mut best_fitness = scores[order[0]];
    let mut best_generation = 0usize;
    let mut per_gen_best = vec![best_fitness];
    let mut stagnation_tail = 0u32;
    let mut max_stagnation = 0u32;
    for generation in 1..=params.generations {
        let mut next: Vec<Vec<u32>> = Vec::with_capacity(params.population);
        for &idx in order.iter().take(params.elite) {
            next.push(population[idx].clone());
        }
        let swap_gate = (params.swap * 1_000_000.0) as u64;
        let replace_gate = (params.replace * 1_000_000.0) as u64;
        while next.len() < params.population {
            let p1 = tournament(&mut rng, &scores);
            let p2 = tournament(&mut rng, &scores);
            let (cut1, cut2) = draw_cuts(&mut rng, env.case_count);
            let mut child = ox(&population[p1], &population[p2], cut1, cut2);
            if rng.below(1_000_000) < swap_gate {
                mutate_swap(&mut child, &mut rng);
            }
            if rng.below(1_000_000) < replace_gate {
                mutate_replace(&mut child, &mut rng, env.pool_size);
            }
            next.push(child);
        }
        evaluations += params.population as u64 - params.elite as u64;
        scores = next
            .par_iter()
            .map(|chrom| fitness(params, env, chrom))
            .collect();
        population = next;
        order = sorted_order(&scores, &population);
        let gen_best = scores[order[0]];
        per_gen_best.push(gen_best);
        if gen_best < best_fitness {
            best_fitness = gen_best;
            best = population[order[0]].clone();
            best_generation = generation;
            stagnation_tail = 0;
        } else {
            stagnation_tail += 1;
            max_stagnation = max_stagnation.max(stagnation_tail);
        }
    }
    let parts = fitness_parts(env, &best);
    RunOutcome {
        best,
        best_fitness,
        best_parts: parts,
        per_gen_best,
        best_generation,
        stagnation_tail,
        max_stagnation,
        evaluations,
    }
}

pub fn config_ga_params(config: &serde_json::Value) -> Result<GaParams, String> {
    let number = |pointer: &str| -> Result<f64, String> {
        config
            .pointer(pointer)
            .and_then(|v| v.as_f64())
            .ok_or_else(|| format!("config section missing: {pointer}"))
    };
    let tournament = number("/ga/tournament")? as u64;
    if tournament != 2 {
        return Err(format!(
            "ga.tournament {tournament} unsupported, engine implements tournament-of-2 only"
        ));
    }
    Ok(GaParams {
        seed: number("/ga/seed")? as u64,
        population: number("/ga/population")? as usize,
        generations: number("/ga/generations")? as usize,
        elite: number("/ga/elite")? as usize,
        swap: number("/ga/mutation/swap")?,
        replace: number("/ga/mutation/replace")?,
        w_conf: number("/ga/weights/confusion")? as f32,
        w_flow: number("/ga/weights/flow")? as f32,
        w_cplx: number("/ga/weights/complexity")? as f32,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::features::parse;
    use crate::inventory::CASE_COUNT;

    fn env_fixture(case_count: usize, pool_texts: &[&str]) -> GaEnv {
        let feats: Vec<Features> = pool_texts.iter().map(|t| parse(t).unwrap()).collect();
        let lengths: Vec<f32> = pool_texts
            .iter()
            .map(|t| t.chars().count() as f32)
            .collect();
        let cooc = vec![(0u32, 1u32, 0.5f32), (1u32, 2u32, 0.25f32)];
        let adj = vec![(0u32, 1u32, 0.5f32)];
        build_env(pool_texts.len(), lengths, feats, case_count, cooc, adj).unwrap()
    }

    #[test]
    fn ox_reproduces_the_chapter_pins() {
        assert_eq!(
            ox(&[1, 2, 3, 4, 5], &[3, 1, 2, 5, 4], 1, 3),
            vec![4, 2, 3, 1, 5]
        );
        assert_eq!(
            ox(&[5, 4, 3, 2, 1], &[2, 1, 4, 3, 5], 1, 4),
            vec![5, 4, 3, 2, 1]
        );
    }

    #[test]
    fn ox_keeps_kept_segment_and_permutation_shape() {
        let p1 = vec![1, 2, 3, 4, 5];
        let p2 = vec![3, 1, 2, 5, 4];
        let child = ox(&p1, &p2, 1, 3);
        assert_eq!(&child[1..3], &p1[1..3]);
        let mut sorted = child.clone();
        sorted.sort_unstable();
        assert_eq!(sorted, vec![1, 2, 3, 4, 5]);
        let child = ox(&p1, &p2, 0, 5);
        assert_eq!(child, p1);
    }

    #[test]
    fn ox_terminates_on_disjoint_parent_universes() {
        let p1 = vec![10u32, 11, 12, 13];
        let p2 = vec![20u32, 21, 22, 23];
        let child = ox(&p1, &p2, 1, 3);
        assert_eq!(child, vec![21, 11, 12, 20]);
        assert!(is_injective(&child));
    }

    #[test]
    fn ox_is_closed_over_injective_parents() {
        let mut rng = Rng::new(99);
        for _ in 0..200 {
            let n = 12usize;
            let mut base: Vec<u32> = (0..40u32).collect();
            for i in (1..base.len()).rev() {
                let j = rng.below(i as u64 + 1) as usize;
                base.swap(i, j);
            }
            let p1: Vec<u32> = base[..n].to_vec();
            let mut base2: Vec<u32> = (0..40u32).collect();
            for i in (1..base2.len()).rev() {
                let j = rng.below(i as u64 + 1) as usize;
                base2.swap(i, j);
            }
            let p2: Vec<u32> = base2[..n].to_vec();
            let (cut1, cut2) = draw_cuts(&mut rng, n);
            let child = ox(&p1, &p2, cut1, cut2);
            assert_eq!(child.len(), n);
            let mut seen = std::collections::HashSet::new();
            for gene in &child {
                assert!(seen.insert(*gene));
            }
            assert_eq!(&child[cut1..cut2], &p1[cut1..cut2]);
        }
    }

    #[test]
    fn mutations_preserve_injectivity() {
        let mut rng = Rng::new(7);
        for _ in 0..200 {
            let mut chrom: Vec<u32> = (0..30u32).collect();
            for i in (1..chrom.len()).rev() {
                let j = rng.below(i as u64 + 1) as usize;
                chrom.swap(i, j);
            }
            chrom.truncate(10);
            let mut swapped = chrom.clone();
            mutate_swap(&mut swapped, &mut rng);
            assert_ne!(swapped, chrom);
            assert!(is_injective(&swapped));
            let mut replaced = chrom.clone();
            mutate_replace(&mut replaced, &mut rng, 30);
            assert!(is_injective(&replaced));
            assert!(replaced.iter().all(|g| (*g as usize) < 30));
            let changed = replaced
                .iter()
                .zip(chrom.iter())
                .filter(|(a, b)| a != b)
                .count();
            assert_eq!(changed, 1);
            let fresh = replaced
                .iter()
                .find(|g| !chrom.contains(g))
                .expect("a new gene entered the pool draw");
            assert!((*fresh as usize) < 30);
        }
    }

    fn is_injective(chrom: &[u32]) -> bool {
        chrom.iter().collect::<std::collections::HashSet<_>>().len() == chrom.len()
    }

    #[test]
    fn tournament_is_deterministic_and_picks_the_lower_fitness() {
        let fitness = vec![5.0, 3.0, 8.0, 1.0, 9.0, 2.0, 7.0, 4.0];
        let mut rng = Rng::new(1);
        let winner = tournament(&mut rng, &fitness);
        let mut replay = Rng::new(1);
        assert_eq!(tournament(&mut replay, &fitness), winner);
        let mut all_equal = Rng::new(2);
        let flat = vec![0.0f32; 8];
        for _ in 0..50 {
            let winner = tournament(&mut all_equal, &flat);
            assert!(winner < flat.len());
        }
    }

    #[test]
    fn tournament_wins_track_the_drawn_pair() {
        let fitness = vec![5.0, 3.0, 8.0, 1.0, 9.0, 2.0, 7.0, 4.0];
        for seed in 0..64u64 {
            let mut rng = Rng::new(seed);
            let a = rng.below(8) as usize;
            let mut replay = Rng::new(seed);
            replay.next_u64();
            let b = replay.below(8) as usize;
            let mut draw = Rng::new(seed);
            let winner = tournament(&mut draw, &fitness);
            let expected = if fitness[b] < fitness[a] { b } else { a };
            assert_eq!(winner, expected, "seed {seed} draws ({a},{b})");
        }
    }

    #[test]
    fn fitness_parts_pin_the_documented_constants() {
        let env = env_fixture(3, &["ả", "ã", "mút"]);
        let chrom = vec![0u32, 1u32, 2u32];
        let [conf, _flow, cplx] = fitness_parts(&env, &chrom);
        let hỏi_ngã = 0.70f32;
        let expected = 0.5 * hỏi_ngã + 0.25 * confusion(&env.feats[1], &env.feats[2]);
        assert!((conf - expected).abs() < 1e-6, "{conf} vs {expected}");
        assert!((cplx - 5.0 / 3.0).abs() < 1e-6, "{cplx}");
    }

    #[test]
    fn flow_part_matches_the_feature_function() {
        let env = env_fixture(3, &["không", "lông", "tía"]);
        let chrom = vec![0u32, 1u32, 2u32];
        let [_conf, flow, _cplx] = fitness_parts(&env, &chrom);
        let expected = 0.5
            * flow_penalty(
                &env.feats[0],
                env.lengths[0] as usize,
                &env.feats[1],
                env.lengths[1] as usize,
            );
        assert!((flow - expected).abs() < 1e-6, "{flow} vs {expected}");
    }

    fn small_params(seed: u64) -> GaParams {
        GaParams {
            seed,
            population: 24,
            generations: 40,
            elite: 2,
            swap: 0.15,
            replace: 0.05,
            w_conf: 1.0,
            w_flow: 0.5,
            w_cplx: 0.1,
        }
    }

    #[test]
    fn run_is_deterministic_for_a_fixed_seed() {
        let env = env_fixture(6, &["ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"]);
        let params = small_params(20261012);
        let a = run(&env, &params);
        let b = run(&env, &params);
        assert_eq!(a.best, b.best);
        assert_eq!(a.per_gen_best, b.per_gen_best);
        assert_eq!(a.best_fitness, b.best_fitness);
        assert_eq!(a.evaluations, b.evaluations);
        let bytes_a = serde_json::to_vec(&a.per_gen_best).unwrap();
        let bytes_b = serde_json::to_vec(&b.per_gen_best).unwrap();
        assert_eq!(
            crate::hash::sha256_hex(&bytes_a),
            crate::hash::sha256_hex(&bytes_b)
        );
    }

    #[test]
    fn run_keeps_elitism_monotone_and_valid_chromosomes() {
        let env = env_fixture(6, &["ả", "ã", "không", "lông", "tía", "mút", "án", "đặt"]);
        let params = small_params(5);
        let outcome = run(&env, &params);
        assert_eq!(outcome.per_gen_best.len(), params.generations + 1);
        for pair in outcome.per_gen_best.windows(2) {
            assert!(
                pair[1] <= pair[0] + f32::EPSILON,
                "{:?}",
                outcome.per_gen_best
            );
        }
        assert_eq!(outcome.best.len(), env.case_count);
        assert!(is_injective(&outcome.best));
        assert!(outcome.best.iter().all(|g| (*g as usize) < env.pool_size));
        assert_eq!(
            outcome.evaluations,
            params.population as u64
                + params.generations as u64 * (params.population as u64 - params.elite as u64)
        );
        assert!(outcome.stagnation_tail <= outcome.max_stagnation);
    }

    #[test]
    fn config_params_read_the_hub() {
        let config = bt_data::paths::load_config().expect("config loads");
        let params = config_ga_params(&config).expect("ga section");
        assert_eq!(params.seed, 20261012);
        assert_eq!(params.population, 200);
        assert_eq!(params.generations, 500);
        assert_eq!(params.elite, 2);
        assert!((params.swap - 0.15).abs() < 1e-9);
        assert!((params.replace - 0.05).abs() < 1e-9);
        assert!((params.w_conf - 1.0).abs() < 1e-6);
        assert!((params.w_flow - 0.5).abs() < 1e-6);
        assert!((params.w_cplx - 0.1).abs() < 1e-6);
    }

    #[test]
    fn build_env_rejects_undersized_pools() {
        let feats: Vec<Features> = ["ma", "má"].iter().map(|t| parse(t).unwrap()).collect();
        let lengths = vec![2.0f32; 2];
        let err = build_env(2, lengths, feats, CASE_COUNT, vec![], vec![]);
        assert!(err.is_err());
    }
}
