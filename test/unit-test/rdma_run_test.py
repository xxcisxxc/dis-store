import os, sys, signal, time
from multiprocessing import Process, Value
import socket
import argparse

parser = argparse.ArgumentParser(description='Choose server or client')
parser.add_argument('which', help='server or client: [s/c]')
args = parser.parse_args()
which = args.which.strip()

server = "192.168.0.11"
port = 8000

nBytes = ['128', '256', '512', '1K', '2K', '4K', '8K', '16K', '32K']
nThreads = ['1', '2', '4', '8', '16', '32', '64', '128', '256']

os.system("make")

if which == 's':
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind(('', 7777))
    serversocket.listen(1)
    clientsocket, addr = serversocket.accept()

    print("\n===============Test RDMA DRAM=============\n")
    for nB in nBytes:
        os.system("./generate {}".format(nB))
        for nT in nThreads:
            print("bytes: {} threads, {} bytes".format(nT, nB))
            sys.stdout.flush()

            clientsocket.send("OK".encode('ascii'))
            os.system("./rdma_dram s {}".format(port))
            port += 1

    print("\n===============Test RDMA PMEM=============\n")
    for nB in nBytes:
        os.system("./generate {}".format(nB))
        for nT in nThreads:
            print("bytes: {} threads, {} bytes\n\n".format(nT, nB))
            sys.stdout.flush()

            clientsocket.send("OK".encode('ascii'))
            os.system("./rdma_pmem s {}".format(port))
            port += 1

if which == 'c':
    clientsocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    clientsocket.connect((server, 7777))

    print("\n\n\n===============Test RDMA DRAM=============\n\n\n")
    for nB in nBytes:
        os.system("./generate {}".format(nB))
        for nT in nThreads:
            print("bytes: {} threads, {} bytes\n\n".format(nT, nB))
            sys.stdout.flush()

            clientsocket.recv(64)
            time.sleep(0.1)
            os.system("./rdma_dram c {} {} {}".format(port, server, nT))
            port += 1
    
    print("\n\n\n===============Test RDMA PMEM=============\n\n\n")
    for nB in nBytes:
        os.system("./generate {}".format(nB))
        for nT in nThreads:
            print("bytes: {} threads, {} bytes\n\n".format(nT, nB))
            sys.stdout.flush()

            clientsocket.recv(64)
            time.sleep(0.1)
            os.system("./rdma_pmem c {} {} {}".format(port, server, nT))
            port += 1

os.system("make clean")