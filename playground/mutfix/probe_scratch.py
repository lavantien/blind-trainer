import os
import re
import shutil

root = os.getcwd()
dep = open(os.path.join("build", "obj", "70dcf3245464", "src", "scheme", "classifier.d")).read()
tokens = re.split(r"[\s\\]+", dep.replace("\\", "/"))
inputs = os.path.join(os.environ["TEMP"], "bt-probe", "inputs")
copied = []
for t in tokens:
    t = t.rstrip(":")
    if not t.startswith(root.replace("\\", "/") + "/"):
        continue
    rel = t[len(root) + 1 :]
    if rel.startswith("build/"):
        continue
    dst = os.path.join(inputs, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(t, dst)
    copied.append(rel)
print(len(copied), "copied")
for c in copied[:12]:
    print(" ", c)
