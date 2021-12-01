import os
import sys

os.system("make")

"""
# Test different Bytes
nBytes = ['128', '256', '512', '1K', '2K', '4K', '8K', '16K', '32K']
for nB in nBytes:
    print("bytes: {}".format(nB))
    sys.stdout.flush()
    os.system("./generate {}".format(nB))
    os.system("./local")
    os.system("make clean_test")

# Test different threads
nThreads = ['1', '2', '4', '8', '16', '32', '64', '128', '256']
nB = '4K'
os.system("./generate {}".format(nB))
for nT in nThreads:
    print("bytes: {} threads, {} bytes".format(nT, nB))
    sys.stdout.flush()
    os.system("./local {}".format(nT))
"""

#Combine
nBytes = ['128', '256', '512', '1K', '2K', '4K', '8K', '16K', '32K']
nThreads = ['1', '2', '4', '8', '16', '32', '64', '128', '256']

for nB in nBytes:
    os.system("./generate {}".format(nB))
    for nT in nThreads:
        print("bytes: {} threads, {} bytes".format(nT, nB))
        sys.stdout.flush()
        os.system("./local {}".format(nT))

os.system("make clean")