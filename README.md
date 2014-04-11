keyzen
======

Permission key manager for Linux

Current state
-------------

This project is in a premature state.
It still have to be fully documented.

It is a good proof of concept with solid base 
for futur integration.

What does it contain?
---------------------

This project provides:
 - A manager of permissions that is accessed via the filesystem
 - a C/DBUS/GLIB library for interacting with the manager through
   the filesystem

Installation
------------

```
# install and compile
git clone https://github.com/jobol/keyzen
cd keyzen/src
make
```

Launching the filesystem
------------------------

To launch it for example to the directory /tmp/keyzen, type
```
cd keyzen/src
make
mkdir /tmp/keyzen
./keyzen-fs /tmp/keyzen
```

Examples of use
---------------

In these examples, the filesystem is mounted on the directory
/sys/fs/keyzen

To know what are the permission keys of the process of pid 25
just type `ls /sys/fs/keyzen/25`, it will return the list of
permission keys for the process 25.

To know what are the permission keys of the current process,
type `ls /sys/fs/keyzen/self`. If your pid is 2398, typing
`ls -l /sys/fs/keyzen/self` will prompt:

 lr--r--r--. 1 root root 4 apr 11 01:53 /sys/fs/keyzen/self -> 1278

Dropping permission keys is easy, it follows the
following rules:
 - Any process can drop any of its permission key at any time;
 - No process can drop a permission key of an other process.

For example, to drop the permission key 'application.read',
the idea is to issue the command 
`rm /sys/fs/keyzen/self/application.read`.

But that would not work because `rm` is an other process, 
it is not the command interpreter.
Using C, a program will use the system call 
`unlink("/sys/fs/keyzen/self/application.read")` for dropping 
it's permission key 'application.read'.

Adding permission keys is also easy but is subject to
security restrictions:
 - To add itself a permission key, a process must have the permission key 'keyzen.admin';
 - No process can add a permission key to an other process.

For example, if a process has the permission key 'keyzen.admin', 
the idea to add itself the permission 'application.launch' is
to issue the command `touch /sys/fs/keyzen/self/application.launch`. 
Here again, it wouldn't work because 'touch' is run as an other procces.

For C programs, it will be `mknod("/sys/fs/keyzen/self/application.launch", S_IFREG, 0)`.

At startup, the processes are gaining the permission keys
set in the extended security attribute of name 
"security.keyzen".

For exemple, issuing `setfattr -n security.keyzen -v application.kill appkiller`
will make the command `appkiller` start with the only permission key 'application.kill'.
And issuing `setfattr -n security.keyzen -v "application.kill application.read" appkiller`
will set 2 permission keys.

Advantages
----------

Any program can interact with the filesystem
easily: there is no need to open a socket (and its buffer) and to
manage the dialog state.

The current implementation rely only on existence of files. 
Then the functions `access`, `mknod`, `unlink` are useable
what is a good point because they are fast and STATELESS.

Drawbacks
---------

The FUSE system is good for rapid development but is slow.
That will be corrected by using a kernel module integration
and even better an integration to SMACK as an added facility.

The keys are not following forks/execs. That will be changed
when integrated into the kernel and even better an integration
to SMACK as an added facility.

The model of validation of the keys isn't currently implemented.
This model states that permissions are granted to applications
(then to the running processes) following the 5 modes below:
 - blanket prompt: the user has to validate at least one time
 - session prompt: the user has to validate at least one time per session
 - one-shot prompt: The user has to validate each time
 - permit: the permission is granted (forever?)
 - deny: the permission is denied (forever?)

To implement it, a minor change has to be done: the files representing
permissions should be read and the reading process (non blocking) will
interact with a daemon that will ask the user.

