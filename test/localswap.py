import os, signal, psutil, time

user = "XXC"
name = "test"
config = "test.config"
run = "../../libmemcached-1.0.18/clients/memaslap -s 127.0.0.1:11211 -t 10s"

nBytes = ['4M', '8M', '16M', '32M','64M', '128M', '256M', '512M', '1G', '2G', '4G']

def changeMem(nB):
    lines = "lxc.network.type = none\nlxc.cgroup.memory.limit_in_bytes = {}\n".format(nB)
    with open(config, 'w') as cf:
        cf.write(lines)

def kill_all(pid):
    parent = psutil.Process(pid)
    process = parent.children(recursive=True)
    process.append(parent)
    for p in process:
        p.kill()

for nB in nBytes:
    print("Next Bytes: {}".format(nB))
    changeMem(nB)
    pid = os.fork()
    if pid == 0:
        os.system("lxc-execute -n {} -f {} -- memcached -u {}".format(name, config, user))
        quit()
    else:
        time.sleep(0.1)
        os.system("{}".format(run))
        kill_all(pid)