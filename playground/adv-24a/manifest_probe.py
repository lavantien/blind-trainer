import json
m = json.load(open("data/vendor/manifest.json", encoding="utf-8"))
def walk(o, path=""):
    if isinstance(o, dict):
        for k, v in o.items():
            walk(v, path + "/" + str(k))
    elif isinstance(o, list):
        print(path, "list len", len(o))
    else:
        print(path, "=", o)
walk(m)
