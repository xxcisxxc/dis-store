import os
import sys

os.system("make")

#nBytes = ['512', '1K', '2K', '4K', '8K', '16K', '32K']
nBytes = ['1K', '1K', '1K', '1K']

for nB in nBytes:
    print("bytes: {}".format(nB))
    sys.stdout.flush()
    os.system("./generate {}".format(nB))
    os.system("./local")
    os.system("make clean_test")

os.system("make clean")