# minix
Four small programs written as an introduction to MINIX for a course in Operating Systems.

### task_5
#### Minix File System
Extention of mfs server with different mechanisms disabling accidental file removals.
The server's action depends on one of three activated modes:
* **Type A** that simply disables any removal and returns ```EPERM```
* **Type B** that requires the file removal to be performed twice. The first attempt to remove returns ```EINPROGRESS```, and the second one successfully removes the file.
* **Type C** that instead of removing a file, changes its name by adding ```.bak``` suffix. From user's perspective the file is gone and a backup is created. Removing a file with ```.bak``` suffix works as in unmodified server. Note that if changing the name is not possible, e.g. due to new filename being too long, removal ends with an error.

In order to activate one of the above modes, a user needs to create a new file ```A.mode```, ```B.mode``` or ```C.mode```, depending on the chosen mode. The mode is active only for files in the directory.

To deactivate mode, one simply needs to remove ```{A, B, C}.mode``` file. The removal of ```{A, B, C}.mode``` works as in unmodified server.

### task_6
#### Devices
Implementation of new character device that calculates checksum [Adler-32](https://en.wikipedia.org/wiki/Adler-32).

Read operation from device returns the value of checksum of all data written to the device from the last read (or from starting the device, if there were no reads). The state of the device is reset after every read.

Write operation writes data that the value of checksum should be calculated of. Data can be potentially huge.

The device should keep its state in case of ```service update```.