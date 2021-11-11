import os, signal

user = "Zhejian"
name = "test"
config = "{}.config".format(name)

nBytes = ['1M', '2M', '4M', '8M', '16M', '32M','64M', '128M', '256M', '512M', '1G', '2G', '4G', '8G']

def changeMem(nB):
    lines = "lxc.network.type = none\nlxc.cgroup.memory.limit_in_bytes = {}\n".format(nB)
    with open(config, 'w') as cf:
        cf.write(lines)

def handler(signum, other):
    print("Next Bytes: ", end='')

signal.signal(signal.SIGINT, handler)

print("Next Bytes: ", end='')
for nB in nBytes:
    os.system("sudo lxc-execute -n {} -f {} -- memcached -u {}".format(name, config, user))
    changeMem(nB)