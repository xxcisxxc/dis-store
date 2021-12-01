import os, sys, signal, time
from multiprocessing import Process, Value
import socket
import argparse
import psutil

parser = argparse.ArgumentParser(description='Choose server or client')
parser.add_argument('which', help='server or client: [s/c]')
args = parser.parse_args()
which = args.which.strip()

def kill_all(pid):
    parent = psutil.Process(pid)
    children = parent.children(recursive=True)
    children.append(parent)
    for p in children:
        p.send_signal(signal.SIGINT)

server = "192.168.0.11"
port = 8000

nBytes = ['128', '256', '512', '1K', '2K', '4K', '8K', '16K', '32K']
nThreads = ['1', '2', '4', '8', '16', '32', '64', '128', '256']

os.system("make")

if which == 's':
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind(('', 7777))
    serversocket.listen(1)

    pid = -1
    for nB in nBytes:
        os.system("./generate {}".format(nB))
        for nT in nThreads:
            print("bytes: {} threads, {} bytes".format(nT, nB))
            sys.stdout.flush()

            clientsocket, addr = serversocket.accept()
            kill_all(pid) if pid != -1 else pass

            pid = os.fork()
            if pid == 0:
                os.system("./rdma s {}".format(port))
                quit()
            else:
                time.sleep(0.1)
                port += 1
                clientsocket.send("OK")
                clientsocket.close()

if which == 'c':
    clientsocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    for nB in nBytes:
        os.system("./generate {}".format(nB))
        for nT in nThreads:
            print("bytes: {} threads, {} bytes".format(nT, nB))
            sys.stdout.flush()

            clientsocket.connect((server, 7777))
            clientsocket.recv(64)
            os.system("./rdma c {} {}".format(port, server))
            port += 1

os.system("make clean")