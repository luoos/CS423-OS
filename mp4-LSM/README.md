# MP4: Linux Security Module

## Tips

The most frustrating part of this MP is that compiling takes a long time. But here are some tips to make it a bit faster.

1. Use `make bindeb-pkg` instead of `make deb-pkg` because `make deb-pkg` calls `make clean`, which deletes all intermediate objects compiled last time.
2. Use `scripts/config --disable DEBUG_INFO` to skip packing debug deb. This is a script that should be called under project root dir. It will save you a lot of time.

## Implementation

The framework was provided for this MP4. We just need to implement each predefined hook function one by one. There are three main steps:

1. init the security blob and set a flag (ssid) for a task based on the file attribute (subject)
2. check against the flag (osid) of the object inode (object)
3. finally determine if the binary can execute the operation it request based on the ssid, osid and object inode mode (file or dir).

One of the annoying things is that there are a lot of null pointer checking. I believe some of them in my code are redundant. But it takes a long time to verify them one by one. I gave up...