keyzen
======

Permission key manager for Linux

Current state
-------------

This project is in a premature state.

It is a good proof of concept with solid base 
for futur integration.

What does it contain?
---------------------

This project provides:
 - A manager of permissions that is accessed via the filesystem
 - A tool for setting, dropping, querying keys and also for serving 
   authorizations
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

This produces the following binaries:
- `keyzen-fs`, the permission manager via the filesystem;
- `keyzen`, a tool for making tests.

and the following libraries:
- `libkeyzen.a` the C API for accessing permissions;
- `libkeyzen-dbus.a` the C API for accessing permissions of
  a DBUS connection.

Launching the filesystem and testing it
---------------------------------------

To launch it for example to the directory /tmp/keyzen, type
```
$ cd keyzen/src
$ make
$ mkdir /tmp/keyzen
$ ./keyzen-fs /tmp/keyzen
```

To test using `keyzen`, after having launched `keyzen-fs` as
above in a terminal, in an other terminal, type:
```
$ ls /tmp/keyzen
```
This will show you the list of the running process pids
and two special entries: `dial` and `self`.
That is like type `ls /proc`. Even the special entry `self`
has the same meaning that for `/proc/self`: it is a link
to the entry of the current pid.

Each "pid" sub-directory of `/tmp/keyzen` list the possible keys
of the process. For example, type:
```
$ ls /tmp/keyzen/$$
```
It will show you nothing. It is normal because your shell
has no authorized keys.

Let create the authorised key `hello`:
```
$ ./keyzen add /tmp/keyzen/$$/hello
$ ls /tmp/keyzen/$$/*
/tmp/keyzen/26192/hello
$ ls -lZ /tmp/keyzen/$$/*
----------. 1 jb users * 0 apr 22 14:38 /tmp/keyzen/26192/hello
```

The directory `/tmp/keyzen/$$` now contains one entry, the
entry (a pseudo file) of name `hello`, indicating that
`word` is a possible key for the process of pid $$
(your shell).

We are adding the key `hello` now.
```
$ ./keyzen add /tmp/keyzen/$$/word
$ ls /tmp/keyzen/$$
hello  word
$ ls -lZ /tmp/keyzen/$$/*
----------. 1 jb users * 0 apr 22 14:38 /tmp/keyzen/26192/hello
----------. 1 jb users * 0 apr 22 14:38 /tmp/keyzen/26192/word
```


The tool `keyzen` is also able to ask and drop the keys.
```
$ ./keyzen ask /tmp/keyzen/$$/mars
/tmp/keyzen/26192/mars: DENIED
$ ./keyzen ask /tmp/keyzen/$$/word
/tmp/keyzen/26192/word: ALLOWED
$ ./keyzen drop /tmp/keyzen/$$/word
$ ./keyzen ask /tmp/keyzen/$$/word
/tmp/keyzen/26192/word: DENIED
$ ls /tmp/keyzen/$$
hello
```

The possible keys are not the same than the authorized keys
because of the model of validation of keys.

The model of validation is stating that permissions are granted 
to applications (then to the running processes) following the 
5 modes below:
- blanket prompt: the user has to validate at least one time
- session prompt: the user has to validate at least one time per session
- one-shot prompt: The user has to validate each time
- permit: the permission is granted (forever?)
- deny: the permission is denied (forever?)

Keyzen is understanding all this modes. To set the mode for
a key permission, we are using
the following prefix code:
- blanket prompt: !
- session prompt: +
- one-shot prompt: *
- permit: =
- deny: -

Then to set the key 'word' with the mode * (one-shot prompt),
be sure that there is no key `word` by dropping it using
`./keyzen drop /tmp/keyzen/$$/word` if needed and then type:
```
$ ./keyzen add /tmp/keyzen/$$/\*word
$ ls -lZ /tmp/keyzen/$$
hello word
```
The key `world` appears in the set of possible keys without the
given prefix that is removed.

Let ask now if the possible key `word` is granted?
```
$ ./keyzen ask /tmp/keyzen/$$/word
/tmp/keyzen/26192/word: DENIED
```

No! Why?

Keyzen is able to handle it but it needs an authorisation
server to be running and collaborating to achieve that function.
That is the role of the special file `/tmp/keyzen/dial`. 
To try it, you need to launch a third terminal where you type:
```
$ ./keyzen server /tmp/keyzen/dial
keyzen authorization server started
waiting...
```

Then come back to the terminal where issuing commands and re-type
```
$ ./keyzen ask /tmp/keyzen/$$/word
```
The command doesn't ends but in the terminal of the keyzen
authorization server, a prompt appeared. Grant access by 
typing y:
```
keyzen authorization server started
waiting...
For pid=26192, key word of type *: one-shot
 .. do you grant (y/n)? y

```

Then the command asking for the key `word` returns and prompt:
```
/tmp/keyzen/26192/word: ALLOWED
```

Because it is a one-shot prompt mode, the server will be asked each
time. For other modes, the server is replies to the filesystem key 
manager keyzen-fs to definitely allow or deny the key. Other policies 
are possibles.

Security
--------

The above examples are only possible because, currently, by default,
the security is disabled to make it easier to test.

In the Makefile, line 16:
```
OPTFS += -DNOSECURITY=1
```
Remove that line or replace 1 by 0 will add the security to
keyzen.

In that case, the security apply the following rules.

Dropping permission keys is easy:
- Any process can drop any of its permission key at any time;
- No process can drop a permission key of an other process.

Adding permission keys is subject to security restrictions:
- To add itself a permission key, a process must have
the permission key `keyzen.admin`;
- No process can add a permission key to an other process.

At startup, the processes are gaining the permission keys
set in the extended security attribute of name 
`security.keyzen`.

For exemple, issuing `setfattr -n security.keyzen -v application.kill appkiller`
will make the command `appkiller` start with the only permission key 'application.kill'.
And issuing `setfattr -n security.keyzen -v "application.kill application.read" appkiller`
will set 2 permission keys.

The library 
-----------
The libraries are only delivered as static. 
It detects automatically the keyzen filesystem. 
It offers various verbs to query, add, drop and list keys.

It is, currently, not multi-thread ready.

The heder file is `keyzen.h`.

Advantages
----------

Any program can interact with the filesystem
easily: there is no need to open a socket (and its buffer) and to
manage the dialog state.

The current implementation rely only on existence of files. 
It is working with the functions `access`, `mknod`, `unlink` 
that are stateless and fast because there is only a kernel switch
(if the implementation were made in the kernel what is the 
final goal).

The model of validation of the keys is currently implemented.
This model states that permissions are granted to applications
(then to the running processes) following the 5 modes below:
 - blanket prompt: the user has to validate at least one time
 - session prompt: the user has to validate at least one time per session
 - one-shot prompt: The user has to validate each time
 - permit: the permission is granted (forever?)
 - deny: the permission is denied (forever?)

Using the companion projet `smaunch` (https://github.com/jobol/smaunch)
it provides a strong security system for native applications.

Drawbacks
---------

The FUSE system is good for rapid development but is slow.
That will be corrected by using a kernel module integration
and even better an integration to SMACK as an added facility.

The keys are not following forks/execs. That will be changed
when integrated into the kernel and even better an integration
to SMACK as an added facility.

The current implementation of the model of validation is blocking
the `access` system call when necessary until the permission is
permitted or forbidden. This behaviour may be problematic for
client that would prefer to have a non-blocking interface. This
can be acheived in a futur by allowing to query permission by
reading 0/1/Y/N from the virtual permission file. Having a not
blocking opened file descriptor would allow to `poll` it until
the answer.

The interrupts are not handled currently. Then typing ctrl+C in the
server doesn't seems to have any effect.

Links to Cynara
---------------
The current implementation of keyzen is only taking care of PIDs.
Cynara is taking care of applications (application id).

A link can be made between the two models. For memory,
I list below two possible ways:
- The filesystem principle of keyzen can be modified to
replace the pids with a couple uid/appid.
- Keyzen could include a mechanism fo identifying the application-id
and the user.

