# MP3: Virtual Memory Page Fault Profiler

[Report](MP3-report.pdf)

Note:

1. The `node` file in under `/dev` directory, the fullpath is `/dev/node`. The `node` file is created automatically when inserting the module. But its permission needs to be change manually with `sudo chmod 777 /dev/node`

## Run

### Case 1

```
$ nice ./work 1024 R 50000 & nice ./work 1024 R 10000 &
... <after completing the two processes>
$ ./monitor > profile1-1.data
```

```
$ nice ./work 1024 R 50000 & nice ./work 1024 L 10000 &
... <after completing the two processes>
$ ./monitor > profile1-2.data
```

### Case 2

```
$ ./run.sh <N>
... <after completing the two processes>
$ ./monitor > profile2-<N>.data
```

## Plot

```
python3 plot.py
```