

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#include "procs.h"

#define PID_LENGTH	32

struct proc {
	struct proc *children[10];
	char pid[PID_LENGTH];
	void *data;
	int epoch;
	ino_t inode;
};

static int epoch = 0;
static struct proc *free_proc = 0;
static struct proc *root_proc = 0;

static void *(*cb_new)(const char *pid) = 0;
static void (*cb_del)(void *data) = 0;



/* create a new proc */
static struct proc *get_new_proc()
{
	struct proc *result;

	result = free_proc;
	if (result) {
		free_proc = result->children[0];
		memset(result, 0, sizeof * result);
	} else
		result = calloc(1, sizeof * result);
	return result;
}

/* purge unsed sub tree */
static int purge_proc(struct proc *proc)
{
	struct proc *child;
	int i, result;

	assert(proc);
	assert(cb_del);

	result = proc->epoch == epoch;
	for (i = 0 ; i < 10 ; i++) {
		child = proc->children[i];
		if (child) {
			if (purge_proc(child))
				result = 1;
			else
				proc->children[i] = 0;
		}
	}
	if (!result) {
		if (proc->pid[0])
			cb_del(proc->data);
		proc->children[0] = free_proc;
		free_proc = proc;
	}
	return result;
}

/* purge all */
static void purge()
{
	if (root_proc && !purge_proc(root_proc))
		root_proc = 0;
}

/* creates the root if needed */
static int make_root()
{
	if (!root_proc) {
		root_proc = get_new_proc();
		if (!root_proc)
			return -ENOMEM;
	}
	return 0;
}

static struct proc *lookup(const char *pid, int create)
{
	struct proc *base, *child;
	int i, n;

	assert(pid);
	assert(*pid);
	assert(root_proc);

	base = root_proc;
	i = 0;
	while ('0' <= pid[i] && pid[i] <= '9') {
		n = pid[i] - '0';
		child = base->children[n];
		if (!child) {
			if (!create)
				return 0;
			child = get_new_proc();
			if (!child)
				return 0;
			base->children[n] = child;
		}
		base = child;
		i++;
	}
	return 0 < i && i < PID_LENGTH && !pid[i] ? base : 0;
}

/* compute 	a new epoch */
static int new_epoch()
{
	DIR *dir;
	struct dirent *ent;
	struct proc *proc;

	/* new epoch */
	epoch = (epoch + 1) & 255;

	/* reads the proc pid */
	dir = opendir("/proc");
	if (!dir)
		return -errno;

	ent = readdir(dir);
	while (ent) {
		if (ent->d_type == DT_DIR) {
			proc = lookup(ent->d_name, 1);
			if (proc) {
				proc->epoch = epoch;
				if (!proc->pid[0]) {
					/* new proc */
					strcpy(proc->pid, ent->d_name);
					proc->inode = ent->d_ino;
					proc->data = cb_new(proc->pid);
				} else if (proc->inode != ent->d_ino) {
					/* existing proc but changed */
					assert(!strcmp(proc->pid, ent->d_name));
					cb_del(proc->data);
					proc->inode = ent->d_ino;
					proc->data = cb_new(proc->pid);
				}
			}
		}
		ent = readdir(dir);
	}
	closedir(dir);

	return 0;
}

static void for_all(struct proc *base, void (*cb)(void *, void *), void *extra)
{
	int i;
	struct proc *child;

	assert(cb);
	assert(base);

	if (base->pid[0])
		cb(base->data, extra);
	for (i = 0 ; i < 10 ; i++) {
		child = base->children[i];
		if (child)
			for_all(child, cb, extra);
	}
}

int procs_update()
{
	int sts;

	assert(cb_del);
	assert(cb_new);

	sts = make_root();
	if (!sts)
		sts = new_epoch();
	purge();

	return sts;
}

void *procs_lookup(const char *pid)
{
	struct proc *proc;

	assert(cb_del);
	assert(cb_new);
	assert(pid);
	assert(*pid);

	if (!root_proc)
		return 0;

	proc = lookup(pid, 0);
	return proc && proc->pid[0] ? proc : 0;
}

void procs_init(void *(*newcb)(const char *), void (*delcb)(void *))
{
	cb_new = newcb;
	cb_del = delcb;
}


void procs_for_all(void (*cb)(void *, void *), void *extra)
{
	assert(cb);
	if (root_proc)
		for_all(root_proc, cb, extra);
}


