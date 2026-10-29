import json
import sys

WANT = ['data_paths_layer_test', 'data_paths_more_test', 'common_config_test']
PLANS = r'C:\Users\lavantien\AppData\Local\Temp\bt-mut-plans.json'
ENV_SRC = r'C:\Users\lavantien\AppData\Local\Temp\bt-mut-shard8.json'
OUT = r'C:\Users\lavantien\AppData\Local\Temp\bt-mut-shard15.json'


def main():
    plans = json.load(open(PLANS, encoding='utf-8'))['plans']
    picked = [p for p in plans if p['suite'] in WANT]
    assert [p['suite'] for p in picked] == WANT, [p['suite'] for p in picked]
    env = json.load(open(ENV_SRC, encoding='utf-8'))['env']
    state = {'shard': 15, 'engine': 'playground/mutcrash/mutate-fixed.exe',
             'plans': picked}
    json.dump(state, open(OUT, 'w', encoding='utf-8'))
    print('shard15 plans:', [p['suite'] for p in picked])


if __name__ == '__main__':
    sys.exit(main())
