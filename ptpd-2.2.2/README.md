# NOTE 1:
In order to debug in sudo mode:

I did the following:

create a script called "gdb" in e.g. my home directory, containing: `pkexec /usr/bin/gdb "$@"`
make it executable
modify the launch.json in VSCode to call the script (obviously change username accordingly) by adding "miDebuggerPath":
```
            "externalConsole": false,
            "miDebuggerPath": "/home/<username>/gdb",
            "MIMode": "gdb",
```
whilst debugging, use top or such like to verify the process is running as root.
That should be enough.



# NOTE 2:
For testptp.c program, if getting compile errors, this may be due to outdated linux headers.
Solution is to import `ptp_clock.h` from elsewhere:
Ex: #include "/usr/src/linux-hwe-5.11-headers-5.11.0-38/include/uapi/linux/ptp_clock.h" 

