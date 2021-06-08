import os 
import fcntl

def setup(): 
    os.mkdir(".temp") 
    os.chdir(".temp")
    
    with open("Makefile", "w") as f:
        f.write("obj-m := customdevice.o")

    os.system("sudo make -C /lib/modules/`uname -r`/build M=$PWD")
    os.system("sudo insmod customdevice.ko")

def test_ioctl():

    print("Calling ioctl() from a \'python\' process with PID: {}".format(os.getpid()))

    with open("/dev/customdevice", "r") as f: 
        fcntl.ioctl(f, 0, 0) 

    print("\nPrinting the last three lines of dmesg output: \n")

    log = os.popen("sudo dmesg | tail -3").read()
    print(log)

def cleanup(): 
    for file in os.listdir(): 
        os.remove(file) 
    os.chdir("..") 
    os.system("sudo rmmod customdevice")
    os.rmdir(".temp") 
    

if __name__ == '__main__':
    #setup()
    test_ioctl()
    #cleanup() 
    
