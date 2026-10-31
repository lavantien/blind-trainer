import sys

sys.path.insert(0, r"C:\Users\lavantien\dev\github\blind-trainer")
import bt


def tgt_fake(args):
    pass


try:
    bt.target_functions_from([tgt_fake])
    print("UNEXPECTED: undocumented target accepted")
except bt.BtError as e:
    print(f"OK: undocumented target rejected: {e}")
