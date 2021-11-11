import os
import sys

DIR = "/dev/shm"

COMMAND1 = "./write {} 1".format(DIR)
COMMAND2 = "./read {} 1".format(DIR)

os.system('make')

def printBytes(s):
    print(s)

nBytes = ['4K', '8K', '16K', '32K', '64K', '128K', '256K', '512K', '1M', '2M', '4M', '8M', '16M', '32M','64M', '128M', '256M', '512M', '1G']

for nB in nBytes:
    print("bytes: {}".format(nB))
    sys.stdout.flush()
    os.system("./generate {}".format(nB))
    os.system(COMMAND1)
    os.system(COMMAND2)
    os.system("rm -rf {}/write*.test".format(DIR))
