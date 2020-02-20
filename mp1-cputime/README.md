# MP1: CPU Usage

The task of this MP is to implement a linux module that monitor CPU usage per process via proc filesystem.

## Demo

```shell class:"lineNo"
make  # compile

sudo rmmod mp1  # if necessary
sudo insmod mp1.ko

echo "<pid>" > /proc/mp1/status  # register pid that we want to monitor
echo "1" > /proc/mp1/status  # example

cat /proc/mp1/status  # show CPU usage per process. Format - pid: usage

./userapp <n> &  # run in background
# Two things in user app
# 1. Get its pid and write the pid to /proc/mp1/status
# 2. compute the n-th fibonacci number. 

./userapp 45 &  # example

cat /proc/mp1/status  # check the cpu usage
```

Note:
1. This linux module will remove finished PID from its watch list
