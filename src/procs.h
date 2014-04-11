/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

void procs_init(void *(*newcb)(const char *), void (*delcb)(void *));
int procs_update();
void *procs_lookup(const char *pid);
void procs_for_all(void (*cb)(void *, void *), void *extra);

