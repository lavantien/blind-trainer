import os
import shutil

print("which:", shutil.which("gh"))
print("path:", os.environ.get("PATH"))
