# minix
Four small programs written as an introduction to MINIX for a course in Operating Systems.

### task_5
#### Minix File System
Extending mfs server with different mechanisms disabling accidental file removals.
The server's action depends on one of three activated modes:
* **Type A** that simply disables any removal and returns *EPERM*
* **Type B** that requires the file removal to be performed twice. The first attempt to remove returns *EINPROGRESS*, and the second one successfully removes the file.
* **Type C** that instead of removing a file, changes its name by adding '.bak' suffix. From user's perspective the file is gone and a backup is created. Removing a file with '.bak' suffix works as in unmodified server.
*Note that if changing the name is not possible, e.g. due to new filename being too long, removal ends with an error.*

### task_6
Devices