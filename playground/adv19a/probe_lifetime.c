#include "app/views/replay.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect(int cond, const char *what) {
    if (!cond) {
        failures++;
        printf("FAIL %s\n", what);
    } else {
        printf("ok %s\n", what);
    }
}

static bt_json_value *parse(const char *text) {
    bt_json_value *root = NULL;
    bt_json_error error;
    if (bt_json_parse(text, strlen(text), &root, &error) != 0) {
        printf("fixture rejected: %s\n", error.message);
        exit(2);
    }
    return root;
}

int main(void) {
    bt_view_replay_scene scene;
    bt_view_replay_scene_default(&scene);
    expect(scene.moves.items == NULL && bt_alg_len(&scene.moves) == 0, "default holds an empty alg");

    bt_alg alg;
    bt_alg_init(&alg);
    char err_token[64];
    if (!bt_alg_parse("R U R' U'", &alg, err_token, sizeof err_token)) {
        printf("parse failed at %s\n", err_token);
        return 2;
    }
    bt_cube start = bt_cube_solved();
    bt_cube_apply_alg(&start, &alg);
    bt_alg inverse = bt_alg_inverse(&alg);
    bt_view_replay_scene_session(&scene, start, inverse.items, bt_alg_len(&inverse));
    expect(scene.playback.moves_len == bt_alg_len(&inverse), "session copies the moves");
    expect(scene.playback.moves == scene.moves.items, "playback points at the owned alg");
    bt_replay_playback_toggle(&scene.playback);
    for (int i = 0; i < 16; i++) {
        bt_replay_playback_tick(&scene.playback, 0.5, 0.5);
    }
    expect(bt_cube_is_solved(&scene.playback.cube), "session playback solves");

    bt_view_replay_scene_session(&scene, bt_cube_solved(), NULL, 0);
    expect(scene.moves.items == NULL || bt_alg_len(&scene.moves) == 0, "re-session clears moves");
    expect(bt_replay_playback_finished(&scene.playback), "re-session playback finished");

    bt_json_value *hub = parse("{\"app\":{\"replay\":{\"distance\":7.0,\"yaw_deg\":\"broken\"}}}");
    bt_replay_cfg cfg;
    bt_replay_cfg_from(hub, &cfg);
    bt_json_free(hub);
    bt_replay_cfg defaults;
    bt_replay_cfg_defaults(&defaults);
    expect(cfg.distance == defaults.distance && cfg.yaw_deg == defaults.yaw_deg,
           "one wrong-typed field resets the whole cfg");

    hub = parse("{\"app\":{\"replay\":{\"pitch_clamp_deg\":[\"a\",70]}}}");
    bt_replay_cfg_from(hub, &cfg);
    bt_json_free(hub);
    expect(cfg.pitch_clamp_deg[0] == defaults.pitch_clamp_deg[0] &&
               cfg.pitch_clamp_deg[1] == defaults.pitch_clamp_deg[1],
           "malformed clamp resets the whole cfg");

    hub = parse("{\"app\":{\"replay\":{\"pitch_clamp_deg\":[-70,70,90]}}}");
    bt_replay_cfg_from(hub, &cfg);
    bt_json_free(hub);
    expect(cfg.pitch_clamp_deg[0] == defaults.pitch_clamp_deg[0],
           "three-element clamp resets the whole cfg");

    expect(bt_replay_wrap_yaw(-540.0F) == 180.0F, "wrap -540 lands on 180");
    expect(bt_replay_wrap_yaw(-180.0F) == 180.0F, "wrap -180 lands on 180");
    expect(bt_replay_wrap_yaw(180.0F) == 180.0F, "wrap 180 stays 180");
    expect(bt_replay_wrap_yaw(-181.0F) == 179.0F, "wrap -181 lands on 179");

    bt_view_replay_app_free();
    bt_view_replay_app_free();
    expect(1, "double free survives");

    printf(failures == 0 ? "PROBE-OK\n" : "PROBE-FAILED %d\n", failures);
    return failures == 0 ? 0 : 1;
}
