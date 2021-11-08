import os
from multiprocessing import Process, Value
from time import sleep
import argparse

parser = argparse.ArgumentParser(description='Choose emulated pmem setting')
parser.add_argument('start', help='Start of the reserved address')
parser.add_argument('size', help='Size of the reserved memory')

args = parser.parse_args()

start = args.start.strip()
size = args.size.strip()

def ok_inc(is_ok):
    with is_ok.get_lock():
        is_ok.value += 1

def ok_wait(is_ok, val):
    while is_ok.value != val:
        sleep(1)

def apt_install(is_ok):
    os.system('apt update -y --force-yes')
    os.system('apt full-upgrade -y --force-yes')
    os.system('apt autoremove -y --force-yes')
    ok_inc(is_ok)
    os.system('apt install make cmake build-essential pkg-config autoconf libndctl-dev libdaxctl-dev pandoc libfabric-dev -y --force-yes')
    ok_inc(is_ok)

def pmem_install(is_ok):
    revised_lines = []
    
    with open("/etc/default/grub", 'r') as grub:
        line = grub.readline()
        while line != '':
            line = line.strip()
            
            if line == '' or line[0] == '#':
                line = grub.readline()
                revised_lines.append(line)
                continue
            
            option = line.split('=')
            if option[0].strip() == "GRUB_CMDLINE_LINUX":
                value = option[1].strip('" ')
                value += " memmap={}!{}".format(size, start)
                line = '{}="{}"'.format(option[0].strip(), value)
            
            revised_lines.append(line)
            line = grub.readline()
    
    with open("/etc/default/grub", 'w') as grub:
        for line in revised_lines:
            grub.write(line + '\n')
    
    os.system('update-grub2')

def pmdk_install(is_ok):
    ok_wait(is_ok, 1)
    os.system('apt install git-all -y --force-yes')
    os.system('git clone https://github.com/pmem/pmdk.git')
    os.chdir('pmdk')
    ok_wait(is_ok, 2)
    os.system('make install')
    os.chdir('..')
    os.system('touch /etc/ld.so.conf.d/libpmemobj.conf')
    os.system('echo "/usr/local/lib/" > /etc/ld.so.conf.d/libpmemobj.conf')
    os.system('ldconfig')

if __name__ == '__main__':
    exec =  []
    is_ok = Value('i', 0)
    os.chdir('..')
    exec.append(Process(target = apt_install, args=(is_ok,)))
    exec.append(Process(target = pmem_install, args=(is_ok,)))
    exec.append(Process(target = pmdk_install, args=(is_ok,)))
    for e in exec:
        e.start()
    for e in exec:
        e.join()