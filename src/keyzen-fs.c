/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include <attr/xattr.h>

#include "procs.h"
#include "keyset.h"

#include "keyzen-constants.h"

#include "itoa.c"

#if !defined(NOTEST)
#define NOTEST 0
#endif

/* protecting accesses to procs and keyset features */
#if defined(_REENTRANT)
#   include <pthread.h>
    static pthread_mutex_t locker = PTHREAD_MUTEX_INITIALIZER;
    static pthread_mutex_t diallocker = PTHREAD_MUTEX_INITIALIZER;
#   define lock() pthread_mutex_lock(&locker)
#   define unlock() pthread_mutex_unlock(&locker)
#   define diallock() pthread_mutex_lock(&diallocker)
#   define dialunlock() pthread_mutex_unlock(&diallocker)
#else
#   define lock() (void)0
#   define unlock() (void)0
#   define diallock() (void)0
#   define dialunlock() (void)0
#endif

#define MASKED(x,m)				((assert((x)==((x)&(m)))), ((x)&(m)))
#define MASK(x)					((((fuse_ino_t)1) << (x)) - 1)
#define MKFIELD(x,b,s)			(MASKED(((fuse_ino_t)x),MASK(b)) << s)
#define FIELD(x,b,s)			((int)(((x) >> s) & MASK(b)))

#define MKINO_KEY(kid)			MKFIELD(kid,13,0)
#define MKINO_PID(pid)			MKFIELD(pid,15,13)
#define MKINO_TYPE(type)		MKFIELD(type,2,28)

#define INOTYP_BIN				0
#define INOTYP_DIR				2
#define INOTYP_KEY				3

#define MK_INO(type,pid,kid)	(MKINO_KEY(kid)|MKINO_PID(pid)|MKINO_TYPE(type))

#define INODE_ROOT				MK_INO(INOTYP_BIN,   0, 1)
#define INODE_SELF				MK_INO(INOTYP_BIN,   0, 2)
#define INODE_DIAL				MK_INO(INOTYP_BIN,   0, 3)
#define MK_INODE_DIR(pid)		MK_INO(INOTYP_DIR, pid, 0)
#define MK_INODE_KEY(pid,kid)	MK_INO(INOTYP_KEY, pid, kid)

#define INODE_KEY(ino)			FIELD(ino,13,0)
#define INODE_PID(ino)			FIELD(ino,15,13)
#define INODE_TYPE(ino)			FIELD(ino,2,28)

#define IS_INODE_DIR(ino)		(INODE_TYPE(ino) == INOTYP_DIR)
#define IS_INODE_KEY(ino)		(INODE_TYPE(ino) == INOTYP_KEY)

time_t  root_time;

#define CHAR_BLANCKET	'!'
#define CHAR_SESSION	'+'
#define CHAR_ONE_SHOT	'*'
#define CHAR_PERMIT		'='
#define CHAR_DENY		'-'

/*====================================================*/
/*===================== PROCESS ======================*/
/*====================================================*/

struct process {
	struct process *next;
	const char *name;
	int keyset;
	int pid;
};

static struct process *first = 0;
static struct process *unused = 0;
static int dirty = 0;
static int should_update = 0;


static int process_check_process_exists_name(const char *name)
{
	char buffer[PATH_MAX];

	strcpy(stpcpy(buffer, "/proc/"), name);
	return access(buffer, F_OK) ? -errno : 0;
}

static int process_check_process_exists_pid(int pid)
{
	char buffer[30];

	itoa(pid, buffer);
	return process_check_process_exists_name(buffer);
}

static void process_init_stat(struct stat *stbuf, struct process *process)
{
	char buffer[PATH_MAX];
	struct stat st;

	memset(stbuf, 0, sizeof(*stbuf));

	strcpy(stpcpy(buffer, "/proc/"), process->name);
	if (!stat(buffer, &st)) {
		stbuf->st_uid = st.st_uid;
		stbuf->st_gid = st.st_gid;
		stbuf->st_atime = st.st_atime;
		stbuf->st_mtime = st.st_mtime;
		stbuf->st_ctime = st.st_ctime;
	}
}

static int process_keys_set(struct process *process, const char *key)
{
	int kid;
	char value;

	switch (*key) {
	case CHAR_BLANCKET:	/* '!' */
	case CHAR_SESSION:	/* '+' */
	case CHAR_ONE_SHOT: /* '*' */
	case CHAR_PERMIT:	/* '=' */
		value = *key++;
		break;
	case CHAR_DENY:		/* '-' */
		key++;
		value = 0;
		break;
	default:
		value = CHAR_PERMIT;
		break;
	}

	kid = keyset_keyid(key, 1);
	if (kid < 0)
		return kid;

	keyset_set(process->keyset, kid, value);
	return 0;
}

static int process_keys_drop(struct process *process, const char *key)
{
	int kid;

	switch (*key) {
	case CHAR_BLANCKET:	/* '!' */
	case CHAR_SESSION:	/* '+' */
	case CHAR_ONE_SHOT: /* '*' */
	case CHAR_PERMIT:	/* '=' */
	case CHAR_DENY:		/* '-' */
		key++;
		break;
	default:
		break;
	}

	kid = keyset_keyid(key, 0);
	if (kid < 0)
		return kid;

	keyset_set(process->keyset, kid, 0);
	return 0;
}

static int process_keys_may_have(struct process *process, const char *key)
{
	int kid;

	switch (*key) {
	case CHAR_BLANCKET:	/* '!' */
	case CHAR_SESSION:	/* '+' */
	case CHAR_ONE_SHOT: /* '*' */
	case CHAR_PERMIT:	/* '=' */
	case CHAR_DENY:		/* '-' */
		key++;
		break;
	default:
		break;
	}

	kid = keyset_keyid(key, 0);
	if (kid < 0)
		return kid;

	switch (keyset_get(process->keyset, kid)) {
	case CHAR_BLANCKET:	/* '!' */
	case CHAR_SESSION:	/* '+' */
	case CHAR_ONE_SHOT: /* '*' */
	case CHAR_PERMIT:	/* '=' */
		return kid;
	default:
		return -EPERM;
	}
}

static int process_is_admin(struct process *process)
{
#if NOTEST
	static int kid_admin = -1;

	if (kid_admin < 0) {
		kid_admin = keyset_keyid(KEYZEN_ADMIN_KEY, 1);
		if (kid_admin < 0)
			return 0;
	}

	return keyset_get(process->keyset, kid_admin) == CHAR_PERMIT;
#else
	return 1;
#endif
}

static int process_init_keys(struct process *process)
{
	char buffer[PATH_MAX + 2];
	ssize_t size;
	int i, kid;

	/* get attributes following links */
	strcpy(stpcpy(stpcpy(buffer, "/proc/"), process->name), "/exe");
	size = getxattr(buffer, KEYZEN_XATTR_KEY, buffer + 1, sizeof buffer - 2);
	if (size <= 0)
		return -errno;

	buffer[0] = 0;
	i = (int)size + 1;
	while (i) {
		if (buffer[--i] > ' ') {
			buffer[i+1] = 0;
			while (buffer[--i] > ' ');
			kid = process_keys_set(process, buffer + i + 1);
			if (kid < 0)
				return kid;
		}
	}
	return 0;
}

static void *create_process(const char *pid)
{
	struct process *process;

	/* get a new data */
	process = unused;
	if (process)
		unused = process->next;
	else {
		process = malloc(sizeof * process);
		if (!process)
			return 0;
	}

	process->name = pid;
	process->pid = atoi(pid);
	process->keyset = keyset_new();
	process->next = first;
	first = process;
	process_init_keys(process);
	return process;
}

static void destroy_process(void *data)
{
	if (data) {
		struct process *process = data;
		process->name = 0;
		dirty = 1;
	}
}

static int real_update_processes()
{
	struct process *process, **previous, *next;
	int sts;

	lock();
	sts = procs_update();
	if (dirty) {
		previous = &first;
		process = first;
		while (process) {
			next = process->next;
			if (process->name) {
				previous = &process->next;
			} else {
				*previous = next;
				process->next = unused;
				unused = process;
			}
			process = next;
		}
		dirty = 0;
	}
	should_update = 0;
	unlock();

	return sts;
}

static struct process *real_find_process_pid(int pid)
{
	struct process *process;

	process = first;
	while (process && !(process->name && process->pid == pid))
		process = process->next;

	return process;
}

static struct process *real_find_process_name(const char *name)
{
	struct process *process;

	process = first;
	while (process && !(process->name && !strcmp(process->name, name)))
		process = process->next;

	return process;
}

static int update_processes(int force)
{
	if (!force && !should_update)
		return 0;
	return real_update_processes();
}

static struct process *find_process_pid(int pid)
{
	struct process *process;

	process = real_find_process_pid(pid);
	if (process) {
		if (process_check_process_exists_name(process->name)) {
			should_update = 1;
			process = 0;
		}
	} else {
		if (!process_check_process_exists_pid(pid)) {
			real_update_processes();
			process = real_find_process_pid(pid);
		}
	}

	return process;
}

static struct process *find_process_name(const char *name)
{
	struct process *process;

	process = real_find_process_name(name);
	if (process) {
		if (process_check_process_exists_name(process->name)) {
			should_update = 1;
			process = 0;
		}
	} else {
		if (!process_check_process_exists_name(name)) {
			real_update_processes();
			process = real_find_process_name(name);
		}
	}

	return process;
}

/*====================================================*/
/*===================== STAT =========================*/
/*====================================================*/

static void stat_root(struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_ino = INODE_ROOT;
	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = 2;
	stbuf->st_atime = root_time;
	stbuf->st_mtime = root_time;
	stbuf->st_ctime = root_time;
}

static void stat_self(struct stat *stbuf, pid_t pid)
{
	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_ino = INODE_SELF;
	stbuf->st_mode = S_IFLNK | 0444;
	stbuf->st_nlink = 1;
	stbuf->st_atime = root_time;
	stbuf->st_mtime = root_time;
	stbuf->st_ctime = root_time;
	stbuf->st_size = 1;
	while (pid >= 100) {
		pid /= 100;
		stbuf->st_size += 2;
	}
	if (pid >= 10)
		stbuf->st_size++;
}

static void stat_dial(struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_ino = INODE_DIAL;
	stbuf->st_mode = S_IFREG | 0666;
	stbuf->st_nlink = 1;
	stbuf->st_atime = root_time;
	stbuf->st_mtime = root_time;
	stbuf->st_ctime = root_time;
}

static void stat_process(struct stat *stbuf, struct process *process)
{
	process_init_stat(stbuf, process);
	stbuf->st_ino = MK_INODE_DIR(process->pid);
	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = 2;
}

static void stat_key(struct stat *stbuf, struct process *process, int kid)
{
	process_init_stat(stbuf, process);
	stbuf->st_ino = MK_INODE_KEY(process->pid,kid);
	stbuf->st_mode = S_IFREG | 0000;
	stbuf->st_nlink = 1;
}

/*====================================================*/
/*===================== FDBUF ========================*/
/*====================================================*/

struct fdbuf {
	struct fdbuf *next;
	char *data;
	size_t size;
	size_t alloc;
	int used;
};

static struct fdbuf fdbufs[5];

static int fdbuf_open()
{
	int result;

	for (result = 0 ; result < (sizeof fdbufs / sizeof * fdbufs) ; result++)
		if (!fdbufs[result].used) {
			fdbufs[result].used = 1;
			fdbufs[result].size = 0;
			return result;
		}
	return -ENOMEM;
}

static void fdbuf_close(int fdbuf)
{
	fdbufs[fdbuf].used = 0;
}

static int fdbuf_add(int fdbuf, const char *name, struct stat *stat)
{
	struct fdbuf *b;
	size_t n, a;
	void *p;

	b = &fdbufs[fdbuf];

	n = b->size + fuse_add_direntry(0, 0, 0, name, 0, 0);
	if (n > b->alloc) {
		a = b->alloc ? 2*b->alloc : 4096;
		while (a < n)
			a *= 2;
		p = realloc(b->data, a);
		if (!p)
			return -ENOMEM;
		b->data = p;
		b->alloc = a;
	}
	fuse_add_direntry(0, b->data + b->size, b->alloc - b->size, name, stat, n);
	b->size = n;
	return 0;
}

static void fdbuf_send(int fdbuf, fuse_req_t req, size_t size, off_t off)
{
	struct fdbuf *b;
	size_t n;

	b = &fdbufs[fdbuf];

	if (off >= b->size)
		fuse_reply_buf(req, NULL, 0);
	else {
		n = b->size - off;
		if (n > size)
			n = size;
		fuse_reply_buf(req, b->data + off, n);
	}
}

/*====================================================*/
/*===================== DIALING ======================*/
/*====================================================*/

/*
the protocol is has follows: 

FS -> SRV:  seq sp pid sp type permission lf
SRV -> FS:  seq sp grant type lf

sp = ' '
lf = '\n'
seq = num
pid = num
num = digit digit*
digit = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
type = '-' | '=' | '*' | '+' | '!'
grant = 'y' | 'n'
*/

struct pending_access_key {
	struct pending_access_key *next;
	fuse_req_t req;
	fuse_ino_t ino;
	int seq;
};

static struct pending_access_key *pak_first_free = 0;
static struct pending_access_key *pak_first_pending = 0;
static struct pending_access_key *pak_last_pending = 0;


static int dialing = 0;
static fuse_req_t  read_req = 0;
static size_t read_size = 0;
static size_t read_pos = 0;
static size_t read_count = 0;
static char read_buffer[2048];

enum writing_states {
	writing_seq,
	writing_grant,
	writing_type,
	writing_eol,
	writing_error
};

static enum writing_states write_state = writing_seq;
static int write_seq = 0;
static char write_grant = 0;
static char write_type = 0;

static int last_seq = 0;

static int dial_is_started()
{
	return dialing;
}

static void dial_start()
{
	assert(!dial_is_started());
	dialing = 1;
}

static void dial_stop()
{
	assert(dial_is_started());
	dialing = 0;
}

static void dial_received(int seq, int granted, char type)
{
	struct pending_access_key *pak, *prv;
	fuse_ino_t ino;
	fuse_req_t req;
	struct process *process;
	int kid;

	assert(seq > 0);

	diallock();

	/* search the pending request of seq */
	prv = 0;
	pak = pak_first_pending;
	while (pak && pak->seq != seq) {
		prv = pak;
		pak = pak->next;
	}

	if (!pak) {
		/* not found ?? */
		dialunlock();
		return;
	}

	/* record data */
	req = pak->req;
	ino = pak->ino;

	/* unlink pak and set it free */
	if (prv)
		prv->next = pak->next;
	else
		pak_first_pending = pak->next;
	if (!pak->next)
		pak_last_pending = prv;
	pak->next = pak_first_free;
	pak_first_free = pak;
	dialunlock();

	/* treat now */
	process = find_process_pid(INODE_PID(ino));
	if (!process)
		fuse_reply_err(req, ENOENT);
	else {
		kid = INODE_KEY(ino);
		assert(keyset_is_valid_keyid(kid));
		keyset_set(process->keyset, kid, type);
		fuse_reply_err(req, granted ? 0 : EPERM);
	}
}

static void send_read()
{
	int sts;
	size_t size;

	assert(read_req);

	size = read_count - read_pos;
	if (size) {
		if (size > read_size)
			size = read_size;
printf("zzzzzzzzzzzz %d {%.*s}\n",(int)size,(int)size,read_buffer + read_pos);
		sts = fuse_reply_buf(read_req, read_buffer + read_pos, size);
		if (!sts)
			read_pos = read_pos + size;
		else {
			fuse_reply_err(read_req, -sts);
			read_pos = read_count;
		}
		read_req = 0;
	}
}

static void process_pending_paks()
{
	struct process *process;
	int kid, i;
	struct pending_access_key *pak, *p;
	const char *str;
	char type, *b;

	if (!read_req)
		return;

	/* send the rest of the buffer if needed */
	send_read();
	if (!read_req)
		return;

	/* search a pending request */
	diallock();
	pak = pak_first_pending;
	while (pak && pak->seq)
		pak = pak->next;
	if (!pak) {
		dialunlock();
		return;
	}

	/* find the seq */
	do {
		last_seq = (last_seq % 999999) + 1;
		p = pak_first_pending;
		while (p && p->seq != last_seq)
			p = p->next;
	} while(p);
	pak->seq = last_seq;
	dialunlock();

	/* treat now */
	process = find_process_pid(INODE_PID(pak->ino));
	if (!process) {
		dial_received(pak->seq, 0, 0);
		return;
	}

	/* creates the buffer */
	b = read_buffer;
	b += _itoa(pak->seq, b);
	*b++ = ' ';
	str = process->name;
	for (i = 0 ; !!(b[i] = str[i]) ; i++);
	b += i;
	*b++ = ' ';
	kid = INODE_KEY(pak->ino);
	type = keyset_get(process->keyset, kid); 
	*b++ = type ? type : CHAR_DENY; 
	str = keyset_key(kid); 
	for (i = 0 ; !!(b[i] = str[i]) ; i++);
	b += i;
	*b++ = '\n';
	read_count = b - read_buffer;
	read_pos = 0;

	/* send */
	send_read();
}

static void dial_read(fuse_req_t req, size_t size, off_t off)
{
	read_req = req;
	read_size = size;
	process_pending_paks();
}

static void query_access_key(fuse_req_t req, struct process *process, int kid)
{
	struct pending_access_key *pak;

	assert(process);
	assert(keyset_is_valid_keyid(kid));

	/* check if opened dial */
	if (!dial_is_started()) {
		fuse_reply_err(req, EPERM);
		return;
	}
	
	/* enqueue the query */
	diallock();
	pak = pak_first_free;
	if (pak)
		pak_first_free = pak->next;
	else {
		dialunlock();
		pak = malloc(sizeof * pak);
		if (!pak) {
			fuse_reply_err(req, ENOMEM);
			return;
		}
		diallock();
	}
	pak->req = req;
	pak->ino = MK_INODE_KEY(process->pid,kid);
	pak->seq = 0;
	pak->next = 0;

	if (pak_last_pending)
		pak_last_pending->next = pak;
	else
		pak_first_pending = pak;
	pak_last_pending = pak;
	dialunlock();

	/* process the pending queries */
	process_pending_paks();
}

static void access_key(fuse_req_t req, struct process *process, int kid)
{
	assert(process);

	if (!keyset_is_valid_keyid(kid))
		fuse_reply_err(req, ENOENT);
	else {
		switch (keyset_get(process->keyset, kid)) {
		case CHAR_BLANCKET:	/* '!' */
		case CHAR_SESSION:	/* '+' */
		case CHAR_ONE_SHOT: /* '*' */
			query_access_key(req, process, kid);
			break;
		case CHAR_PERMIT:	/* '=' */
			fuse_reply_err(req, 0);
			break;
		default:
			fuse_reply_err(req, EPERM);
			break;
		}
	}
}

static void dial_write(fuse_req_t req, const char *buf, size_t count, off_t off)
{
	size_t pos;
	int state, seq;
	char grant, type, c;

	state = write_state;
	seq = write_seq;
	grant = write_grant;
	type = write_type;
	pos = 0;
	while (pos < count) {
		c = buf[pos++];
		switch (state) {

		case writing_seq:
			if ('0' <= c && c <= '9')
				seq = 10 * seq + (int)(c - '0');
			else if (c != ' ' || !seq)
				state = writing_error;
			else
				state = writing_grant;
			break;

		case writing_grant:
			if (c == 'n') {
				grant = 0;
				state = writing_type;
			} else if (c == 'y') {
				grant = 1;
				state = writing_type;
			} else
				state = writing_error;
			break;

		case writing_type:
			switch (c) {
			case CHAR_DENY:		/* '-' */
				c = 0;
			case CHAR_BLANCKET:	/* '!' */
			case CHAR_SESSION:	/* '+' */
			case CHAR_ONE_SHOT: /* '*' */
			case CHAR_PERMIT:	/* '=' */
				type = c;
				state = writing_eol;
				break;
			default:
				state = writing_error;
				break;
			}
			break;

		case writing_eol:
			if (c != '\n') {
				state = writing_error;
			} else {
				dial_received(seq, grant, type);
				seq = 0;
				grant = 0;
				type = 0;
				state = writing_seq;
			}
			break;
			
		case writing_error:
			seq = 0;
			grant = 0;
			type = 0;
			if (c == '\n')
				state = writing_seq;
			break;
		}
	}
	write_state = state;
	write_seq = seq;
	write_grant = grant;
	write_type = type;

	fuse_reply_write(req, count);
}


/*====================================================*/
/*===================== KYZEN FS =====================*/
/*====================================================*/


/* 
** directory operations
*/

struct opendir_extra {
	struct process *process;
	int fdbuf;
	int error;
};

static void fill_dirbuf_key(const char *key, int kid, char type, void *extra)
{
	struct stat stbuf;
	struct opendir_extra *data;

	assert(extra);

	data = extra;
	if (!data->error && type) {
		stat_key(&stbuf, data->process, kid);
		data->error = fdbuf_add(data->fdbuf, key, &stbuf);
	}
}

static int fill_dirbuf_content(int fdbuf, fuse_ino_t ino, pid_t pid)
{
	int sts;
	struct stat stbuf;
	struct process *process;
	struct opendir_extra extra;

	stat_root(&stbuf);
	sts = fdbuf_add(fdbuf, "..", &stbuf);
	if (sts)
		return sts;

	if (ino == INODE_ROOT) {

		stat_root(&stbuf);
		sts = fdbuf_add(fdbuf, ".", &stbuf);
		if (sts)
			return sts;

		/* add self link */
		stat_self(&stbuf, pid);
		sts = fdbuf_add(fdbuf, KEYZEN_SELF_NAME, &stbuf);
		if (sts)
			return sts;

		/* add dial file */
		stat_dial(&stbuf);
		sts = fdbuf_add(fdbuf, KEYZEN_DIAL_NAME, &stbuf);
		if (sts)
			return sts;

		/* at root directory */
		process = first;
		while (process) {
			stat_process(&stbuf, process);
			sts = fdbuf_add(fdbuf, process->name, &stbuf);
			if (sts)
				return sts;
			process = process->next;
		}

	} else if (IS_INODE_DIR(ino)) {

		process = find_process_pid(INODE_PID(ino));
		if (!process)
			return -ENOENT;

		stat_process(&stbuf, process);
		sts = fdbuf_add(fdbuf, ".", &stbuf);
		if (sts)
			return sts;

		extra.fdbuf = fdbuf;
		extra.process = process;
		extra.error = 0;
		lock();
		keyset_for_all_not_null(process->keyset, fill_dirbuf_key, &extra);
		unlock();

	} else {

		return -ENOTDIR;
	}

	return 0;
}

static int get_dirbuf_of_content(fuse_ino_t ino, pid_t pid)
{
	int sts;
	int fdbuf;

	fdbuf = fdbuf_open();
	if (fdbuf < 0)
		return fdbuf;

	sts = fill_dirbuf_content(fdbuf, ino, pid);
	if (!sts)
		return fdbuf;

	fdbuf_close(fdbuf);
	return sts;
}

static void keyzen_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	int sts;
	int fdbuf;

	fi->fh = 0;
	sts = update_processes(ino == INODE_ROOT);
	if (sts) {
		fuse_reply_err(req, -sts);
		return;
	}

	fdbuf = get_dirbuf_of_content(ino, fuse_req_ctx(req)->pid);
	if (fdbuf < 0) {
		fuse_reply_err(req, -fdbuf);
		return;
	}

	fi->fh = fdbuf;
	fuse_reply_open(req, fi);
}

static void keyzen_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			     off_t off, struct fuse_file_info *fi)
{
	fdbuf_send((int)(fi->fh), req, size, off);
}


static void keyzen_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fdbuf_close((int)fi->fh);
	fuse_reply_err(req, 0);
}

static void keyzen_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int sts, kid;
	struct fuse_entry_param e;
	struct process *process;

	sts = update_processes(0);
	if (sts) {
		sts = -sts;

	} else if (!strcmp(name, "..")) {
		stat_root(&e.attr);

	} else if (parent == INODE_ROOT) {

		if (!strcmp(name, ".")) {
			stat_root(&e.attr);

		} else if (!strcmp(name, KEYZEN_SELF_NAME)) {
			stat_self(&e.attr, fuse_req_ctx(req)->pid);

		} else if (!strcmp(name, KEYZEN_DIAL_NAME)) {
			stat_dial(&e.attr);

		} else {
			process = find_process_name(name);
			if (process) {
				stat_process(&e.attr, process);

			} else {
				sts = ENOENT;
			}
		}
	} else if (IS_INODE_DIR(parent)) {

		process = find_process_pid(INODE_PID(parent));
		if (!process) {
			sts = ENOENT;

		} else if (!strcmp(name, ".")) {
			stat_process(&e.attr, process);

		} else {
			lock();
			kid = process_keys_may_have(process, name);
			if (kid >= 0) {
				stat_key(&e.attr, process, kid);
			} else {
				sts = ENOENT;
			}
			unlock();
		}

	} else {

		sts = ENOENT;
	}

	if (sts)
		fuse_reply_err(req, sts);
	else {
		e.ino = e.attr.st_ino;
		e.generation = 0;
		e.attr_timeout = 0.125;
		e.entry_timeout = 0.125;
		fuse_reply_entry(req, &e);
	}
}

static void keyzen_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	int sts;
	struct stat stbuf;
	struct process *process;

	sts = update_processes(0);
	if (sts)
		sts = -sts;
	else if (ino == INODE_ROOT)
		stat_root(&stbuf);
	else if (ino == INODE_SELF)
		stat_self(&stbuf, fuse_req_ctx(req)->pid);
	else if (ino == INODE_DIAL)
		stat_dial(&stbuf);
	else if (IS_INODE_DIR(ino)) {
		process = find_process_pid(INODE_PID(ino));
		if (process)
			stat_process(&stbuf, process);
		else
			sts = ENOENT;
	} else if (IS_INODE_KEY(ino)) {
		process = find_process_pid(INODE_PID(ino));
		if (process)
			stat_key(&stbuf, process, INODE_KEY(ino));
		else
			sts = ENOENT;
	} else
		sts = ENOENT;

	if (sts)
		fuse_reply_err(req, sts);
	else
		fuse_reply_attr(req, &stbuf, 0);
}

static void keyzen_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	int sts;
	struct process *process;
#define CHECK(x) (((mask & (x))==mask) ? 0 : EACCES)
	sts = update_processes(0);
	if (sts)
		sts = -sts;
	else if (ino == INODE_ROOT)
		sts = CHECK(F_OK|R_OK|X_OK);
	else if (ino == INODE_SELF)
		sts = CHECK(F_OK|R_OK);
	else if (ino == INODE_DIAL)
		sts = CHECK(F_OK|R_OK|W_OK);/*TODO restrict usage?*/
	else if (IS_INODE_DIR(ino)) {
		process = find_process_pid(INODE_PID(ino));
		if (process)
			sts = CHECK(F_OK|R_OK|X_OK);
		else
			sts = ENOENT;
	} else if (IS_INODE_KEY(ino)) {
		process = find_process_pid(INODE_PID(ino));
		if (process) {
			access_key(req, process, INODE_KEY(ino));
			return;
		}
		else
			sts = ENOENT;
	} else
		sts = ENOENT;
#undef CHECK
	fuse_reply_err(req, sts);
}

static void keyzen_readlink(fuse_req_t req, fuse_ino_t ino)
{
	if (ino != INODE_SELF)
		fuse_reply_err(req, EINVAL);
	else {
		char buffer[10];
		sprintf(buffer, "%d", (int)fuse_req_ctx(req)->pid);
		fuse_reply_readlink(req, buffer);
	}
}

static void keyzen_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int sts;
	struct process *process;

	if (!IS_INODE_DIR(parent)) {
		fuse_reply_err(req, EPERM);
	} else {
		process = find_process_pid(INODE_PID(parent));
		if (!process) {
			fuse_reply_err(req, ENOENT);
#if NOTEST
		} else if (process->pid != (int)fuse_req_ctx(req)->pid) {
			fuse_reply_err(req, EPERM);
#endif
		} else {
			lock();
			sts = process_keys_drop(process, name);
			unlock();
			fuse_reply_err(req, -sts);
		}
	}
}

static void keyzen_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev)
{
	int sts;
	struct process *process;

	if (!IS_INODE_DIR(parent)) {
		fuse_reply_err(req, EPERM);
	} else if (!S_ISREG(mode)) {
		fuse_reply_err(req, EPERM);
	} else {
		process = find_process_pid(INODE_PID(parent));
		if (!process) {
			fuse_reply_err(req, ENOENT);
#if NOTEST
		} else if (process->pid != (int)fuse_req_ctx(req)->pid) {
			fuse_reply_err(req, EPERM);
#endif
		} else {
			lock();
			if (!process_is_admin(process)) {
				unlock();
				fuse_reply_err(req, EPERM);
			} else {
				sts = process_keys_set(process, name);
				unlock();
				if (sts)
					fuse_reply_err(req, -sts);
				else
					keyzen_lookup(req, parent, name);
			}
		}
	}
}

static void keyzen_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
{
	(void)name;
	if (size == 0)
		fuse_reply_xattr(req, 1);
	else
		fuse_reply_buf(req, "*", 1);
}

static void keyzen_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	int sts;

printf("yyyyyyyyyyyyyyyy\n");
	if (ino != INODE_DIAL || dial_is_started() || (fi->flags & O_ACCMODE) != O_RDWR)
		fuse_reply_err(req, EACCES);
	else {
		dial_start();
		fi->direct_io = 1;
		sts = fuse_reply_open(req, fi);
		if (sts)
			dial_stop();
printf("mmmmmmmmmmmmmmmm %d\n", sts);
	}
}

static void keyzen_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	if (ino != INODE_DIAL || !dial_is_started())
		fuse_reply_err(req, EBADF);
	else
		dial_read(req, size, off);
}


static void keyzen_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	if (ino != INODE_DIAL || !dial_is_started())
		fuse_reply_err(req, EBADF);
	else
		dial_write(req, buf, size, off);
}

static void keyzen_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	if (!dial_is_started())
		fuse_reply_err(req, EBADF);
	else {
		dial_stop();
		fuse_reply_err(req, 0);
	}
}


/*
** definition struct
*/

static struct fuse_lowlevel_ops keyzen_oper = {
	.lookup		= keyzen_lookup,
	.access		= keyzen_access,
	.getattr	= keyzen_getattr,
	.readdir	= keyzen_readdir,
	.readlink	= keyzen_readlink,
	.opendir	= keyzen_opendir,
	.releasedir	= keyzen_releasedir,
	.unlink		= keyzen_unlink,
	.mknod		= keyzen_mknod,
	.getxattr	= keyzen_getxattr,
	.open		= keyzen_open,
	.read		= keyzen_read,
	.write		= keyzen_write,
	.release	= keyzen_release,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	char *mountpoint;
	int err = -1;

	root_time = time(NULL);
	procs_init(create_process, destroy_process);

	if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
	    (ch = fuse_mount(mountpoint, &args)) != NULL) {
		struct fuse_session *se;

		se = fuse_lowlevel_new(&args, &keyzen_oper,
				       sizeof(keyzen_oper), NULL);
		if (se != NULL) {
			if (fuse_set_signal_handlers(se) != -1) {
				fuse_session_add_chan(se, ch);
				err = fuse_session_loop(se);
				fuse_remove_signal_handlers(se);
				fuse_session_remove_chan(ch);
			}
			fuse_session_destroy(se);
		}
		fuse_unmount(mountpoint, ch);
	}
	fuse_opt_free_args(&args);

	return err ? 1 : 0;
}
