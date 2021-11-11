import os, sys, signal

os.system("make")

nBytes = ['4K', '8K', '16K', '32K', '64K', '128K', '256K', '512K', '1M', '2M', '4M', '8M', '16M', '32M','64M', '128M', '256M', '512M', '1G']

def handler(signum, other):
    print("Next Bytes: ", end='')

signal.signal(signal.SIGINT, handler)

port = 5000

for nB in nBytes:
    print("bytes: {}".format(nB))
    sys.stdout.flush()
    os.system("./generate {}".format(nB))
    os.system("./rdma c {} 192.168.0.11 ".format(port))
    os.system("make clean_test")
    port += 1

os.system("make clean")