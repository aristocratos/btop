/*
 * s10compat.c - libc compatibility shims so a binary built on illumos (OpenIndiana) with a
 * modern GCC and a static libstdc++ can run on Solaris 10 x86 (64-bit).
 *
 * WHY: the static libstdc++ (and libgcc's unwinder) built for illumos reference libc functions
 * that Solaris 10 does not have. Defining them in the executable makes the linker bind to these
 * definitions instead of illumos libc, so no ILLUMOS_* version requirement is recorded and
 * nothing is looked up in Solaris 10's libc at run time.
 *
 * Put this file at src/openindiana/s10compat.c and build with:  gmake S10=true
 * Do NOT link it into a binary meant for OpenIndiana itself (__cxa_atexit in particular could
 * recurse with illumos libc's own atexit()).
 *
 * KNOWN LIMITS (deliberate simplifications):
 *  - locale_t is ignored: every *_l function uses the process-global locale (setlocale()).
 *    newlocale() only checks that a named locale exists.
 *  - dl_iterate_phdr() reports only the main executable, so C++ exceptions can be thrown and
 *    caught inside the program, but cannot propagate through frames in shared libraries.
 *  - __cxa_atexit() runs static destructors from one atexit() handler, so their interleaving
 *    with plain atexit() handlers is not exact.
 *  - 64-bit only.
 *
 * Status: written without access to a Solaris 10 or illumos compiler. Expect to fix small
 * prototype mismatches on the first build.
 */
#if !defined(__LP64__) && !defined(_LP64)
#error "s10compat.c supports 64-bit builds only"
#endif

#ifndef __sun
#define _GNU_SOURCE /* only so the file can be syntax-checked on Linux */
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <limits.h>
#include <link.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#ifdef __sun
#include <thread.h>
#endif

#if defined(__has_include)
#if __has_include(<xlocale.h>)
#include <xlocale.h>
#endif
#endif

#ifdef __sun
#define S10_HRTIME() ((long long)gethrtime())
#else
#include <sys/syscall.h>
static long long s10_hrtime_stub(void) /* stand-in for gethrtime() so the logic can be tested on Linux */
{
	struct timespec ts;
	syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
#define S10_HRTIME() s10_hrtime_stub()
#endif

/* ------------------------------------------------------------------------------------------ */
/* __cxa_atexit                                                                                */
/* ------------------------------------------------------------------------------------------ */

struct s10_atexit_ent {
	void (*fn)(void *);
	void *arg;
	struct s10_atexit_ent *next;
};
static struct s10_atexit_ent *s10_atexit_head;
static int s10_atexit_registered;
static pthread_mutex_t s10_atexit_lock = PTHREAD_MUTEX_INITIALIZER;

static void s10_run_cxa_atexit(void)
{
	for (;;) {
		pthread_mutex_lock(&s10_atexit_lock);
		struct s10_atexit_ent *e = s10_atexit_head;
		if (e)
			s10_atexit_head = e->next;
		pthread_mutex_unlock(&s10_atexit_lock);
		if (!e)
			break;
		e->fn(e->arg);
		free(e);
	}
}

int __cxa_atexit(void (*fn)(void *), void *arg, void *dso)
{
	(void)dso;
	struct s10_atexit_ent *e = malloc(sizeof(*e));
	if (!e)
		return -1;
	e->fn = fn;
	e->arg = arg;
	pthread_mutex_lock(&s10_atexit_lock);
	e->next = s10_atexit_head;
	s10_atexit_head = e;
	int need_register = !s10_atexit_registered;
	s10_atexit_registered = 1;
	pthread_mutex_unlock(&s10_atexit_lock);
	if (need_register && atexit(s10_run_cxa_atexit) != 0)
		return -1;
	return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* clock_gettime / nanosleep / pthread_cond_clockwait                                          */
/* (on Solaris 10 the first two live in librt, not libc)                                       */
/* ------------------------------------------------------------------------------------------ */

int clock_gettime(clockid_t clk, struct timespec *ts)
{
	if (!ts) {
		errno = EFAULT;
		return -1;
	}
	if (clk == CLOCK_REALTIME) {
		struct timeval tv;
		if (gettimeofday(&tv, NULL) != 0)
			return -1;
		ts->tv_sec = tv.tv_sec;
		ts->tv_nsec = (long)tv.tv_usec * 1000L;
		return 0;
	}
	if (clk == CLOCK_MONOTONIC) {
		long long ns = S10_HRTIME();
		ts->tv_sec = (time_t)(ns / 1000000000LL);
		ts->tv_nsec = (long)(ns % 1000000000LL);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

int nanosleep(const struct timespec *rq, struct timespec *rem)
{
	if (!rq || rq->tv_sec < 0 || rq->tv_nsec < 0 || rq->tv_nsec >= 1000000000L) {
		errno = EINVAL;
		return -1;
	}
	const long long start = S10_HRTIME();
	const long long want = (long long)rq->tv_sec * 1000000000LL + rq->tv_nsec;
	long long ms = (want + 999999LL) / 1000000LL;
	while (ms > 0) {
		const int chunk = ms > 1000000000LL ? 1000000000 : (int)ms;
		if (poll(NULL, 0, chunk) < 0) {
			if (errno == EINTR) {
				if (rem) {
					long long left = want - (S10_HRTIME() - start);
					if (left < 0)
						left = 0;
					rem->tv_sec = (time_t)(left / 1000000000LL);
					rem->tv_nsec = (long)(left % 1000000000LL);
				}
				return -1;
			}
			return -1;
		}
		ms -= chunk;
	}
	return 0;
}

int pthread_cond_clockwait(pthread_cond_t *cond, pthread_mutex_t *mtx, clockid_t clk,
                           const struct timespec *abstime)
{
	if (clk == CLOCK_REALTIME)
		return pthread_cond_timedwait(cond, mtx, abstime);

	/* pthread_cond_timedwait() only understands CLOCK_REALTIME: turn the deadline into a
	 * realtime deadline by adding the remaining time on the requested clock to "now". */
	struct timespec now_clk, now_rt, target;
	if (clock_gettime(clk, &now_clk) != 0 || clock_gettime(CLOCK_REALTIME, &now_rt) != 0)
		return EINVAL;
	long long delta = ((long long)abstime->tv_sec - now_clk.tv_sec) * 1000000000LL +
	                  ((long long)abstime->tv_nsec - now_clk.tv_nsec);
	if (delta < 0)
		delta = 0;
	long long ns = (long long)now_rt.tv_nsec + delta % 1000000000LL;
	target.tv_sec = now_rt.tv_sec + (time_t)(delta / 1000000000LL) + (time_t)(ns / 1000000000LL);
	target.tv_nsec = (long)(ns % 1000000000LL);
	return pthread_cond_timedwait(cond, mtx, &target);
}

/* sched_yield() lives in librt on Solaris 10 (libc on illumos). thr_yield() is in Solaris 10's libc. */
int sched_yield(void)
{
#ifdef __sun
	thr_yield();
#endif
	return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* Randomness and environment                                                                  */
/* ------------------------------------------------------------------------------------------ */

int getentropy(void *buf, size_t len)
{
	if (len > 256) {
		errno = EIO;
		return -1;
	}
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -1;
	unsigned char *p = buf;
	size_t got = 0;
	while (got < len) {
		ssize_t r = read(fd, p + got, len - got);
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0) {
			close(fd);
			errno = EIO;
			return -1;
		}
		got += (size_t)r;
	}
	close(fd);
	return 0;
}

uint32_t arc4random(void)
{
	uint32_t v = 0;
	if (getentropy(&v, sizeof(v)) != 0)
		abort();
	return v;
}

char *secure_getenv(const char *name)
{
#ifdef __sun
	if (issetugid())
		return NULL;
#endif
	return getenv(name);
}

/* ------------------------------------------------------------------------------------------ */
/* dirfd / fchmodat                                                                            */
/* ------------------------------------------------------------------------------------------ */

/* Solaris 10's DIR starts with "int dd_fd", so the descriptor is the first int. */
int dirfd(DIR *d)
{
	if (!d) {
		errno = EINVAL;
		return -1;
	}
	return *(int *)(void *)d;
}

int fchmodat(int fd, const char *path, mode_t mode, int flag)
{
	if ((unsigned int)fd == (unsigned int)AT_FDCWD && !(flag & AT_SYMLINK_NOFOLLOW))
		return chmod(path, mode);
	int oflags = O_RDONLY | O_NONBLOCK;
#ifdef O_NOFOLLOW
	if (flag & AT_SYMLINK_NOFOLLOW)
		oflags |= O_NOFOLLOW;
#endif
	int f = openat(fd, path, oflags);
	if (f < 0)
		return -1;
	int r = fchmod(f, mode);
	int saved = errno;
	close(f);
	errno = saved;
	return r;
}

/* ------------------------------------------------------------------------------------------ */
/* POSIX 2008 locale API: locale_t is ignored, the global locale is used                       */
/* ------------------------------------------------------------------------------------------ */

/* illumos's LC_GLOBAL_LOCALE is not a compile-time constant and may refer to symbols Solaris 10 lacks,
 * so it is deliberately not used. Every locale_t is ignored anyway; one dummy object stands in for
 * "the global locale" and for every locale newlocale() hands out. */
static int s10_dummy_locale;
static locale_t s10_current_locale; /* NULL until uselocale() sets something */
#define S10_LOCALE ((locale_t)(void *)&s10_dummy_locale)

locale_t newlocale(int mask, const char *name, locale_t base)
{
	(void)mask;
	(void)base;
	if (name && *name && strcmp(name, "C") != 0 && strcmp(name, "POSIX") != 0) {
		/* Check the locale exists so std::locale("bogus") still throws. */
		char *saved = NULL;
		const char *cur = setlocale(LC_ALL, NULL);
		if (cur)
			saved = strdup(cur);
		const char *r = setlocale(LC_ALL, name);
		if (saved) {
			setlocale(LC_ALL, saved);
			free(saved);
		}
		if (!r) {
			errno = ENOENT;
			return (locale_t)0;
		}
	}
	return S10_LOCALE;
}

void freelocale(locale_t loc)
{
	(void)loc;
}

locale_t duplocale(locale_t loc)
{
	(void)loc;
	return S10_LOCALE;
}

locale_t uselocale(locale_t loc)
{
	locale_t old = s10_current_locale ? s10_current_locale : S10_LOCALE;
	if (loc)
		s10_current_locale = loc;
	return old;
}

struct lconv *localeconv_l(locale_t loc)
{
	(void)loc;
	return localeconv();
}

wctype_t wctype_l(const char *name, locale_t loc)
{
	(void)loc;
	return wctype(name);
}

wint_t towupper_l(wint_t wc, locale_t loc)
{
	(void)loc;
	return towupper(wc);
}

wint_t towlower_l(wint_t wc, locale_t loc)
{
	(void)loc;
	return towlower(wc);
}

float strtof_l(const char *s, char **end, locale_t loc)
{
	(void)loc;
	return strtof(s, end);
}

double strtod_l(const char *s, char **end, locale_t loc)
{
	(void)loc;
	return strtod(s, end);
}

long double strtold_l(const char *s, char **end, locale_t loc)
{
	(void)loc;
	return strtold(s, end);
}

int strcoll_l(const char *a, const char *b, locale_t loc)
{
	(void)loc;
	return strcoll(a, b);
}

size_t strxfrm_l(char *dst, const char *src, size_t n, locale_t loc)
{
	(void)loc;
	return strxfrm(dst, src, n);
}

int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t loc)
{
	(void)loc;
	return wcscoll(a, b);
}

size_t wcsxfrm_l(wchar_t *dst, const wchar_t *src, size_t n, locale_t loc)
{
	(void)loc;
	return wcsxfrm(dst, src, n);
}

int mbtowc_l(wchar_t *pwc, const char *s, size_t n, locale_t loc)
{
	(void)loc;
	return mbtowc(pwc, s, n);
}

size_t mbstowcs_l(wchar_t *dst, const char *src, size_t n, locale_t loc)
{
	(void)loc;
	return mbstowcs(dst, src, n);
}

size_t strftime_l(char *s, size_t max, const char *fmt, const struct tm *tm, locale_t loc)
{
	(void)loc;
	return strftime(s, max, fmt, tm);
}

size_t wcsftime_l(wchar_t *s, size_t max, const wchar_t *fmt, const struct tm *tm, locale_t loc)
{
	(void)loc;
	return wcsftime(s, max, fmt, tm);
}

char *nl_langinfo_l(nl_item item, locale_t loc)
{
	(void)loc;
	return nl_langinfo(item);
}

/* ------------------------------------------------------------------------------------------ */
/* mbsnrtowcs / wcsnrtombs (POSIX 2008), built on the restartable single-character functions   */
/* ------------------------------------------------------------------------------------------ */

size_t mbsnrtowcs(wchar_t *dst, const char **src, size_t nms, size_t len, mbstate_t *ps)
{
	static mbstate_t internal;
	if (!ps)
		ps = &internal;
	const char *s = *src;
	size_t n = 0;
	while (!dst || n < len) {
		if (nms == 0)
			break;
		wchar_t wc;
		size_t r = mbrtowc(&wc, s, nms, ps);
		if (r == (size_t)-1) {
			if (dst)
				*src = s;
			return (size_t)-1;
		}
		if (r == (size_t)-2) { /* incomplete character at the end of the input */
			s += nms;
			nms = 0;
			break;
		}
		if (r == 0) { /* terminating NUL: stored, not counted */
			if (dst) {
				dst[n] = L'\0';
				*src = NULL;
			}
			return n;
		}
		if (dst)
			dst[n] = wc;
		n++;
		s += r;
		nms -= r;
	}
	if (dst)
		*src = s;
	return n;
}

size_t wcsnrtombs(char *dst, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps)
{
	static mbstate_t internal;
	if (!ps)
		ps = &internal;
	const wchar_t *w = *src;
	size_t n = 0;
	char buf[32];
	while (nwc > 0) {
		size_t r = wcrtomb(buf, *w, ps);
		if (r == (size_t)-1) {
			if (dst)
				*src = w;
			return (size_t)-1;
		}
		if (dst) {
			if (n + r > len)
				break;
			memcpy(dst + n, buf, r);
		}
		if (*w == L'\0') { /* terminating NUL: stored, not counted */
			if (dst)
				*src = NULL;
			return n;
		}
		n += r;
		w++;
		nwc--;
	}
	if (dst)
		*src = w;
	return n;
}

/* ------------------------------------------------------------------------------------------ */
/* dl_iterate_phdr - needed by the statically linked libgcc unwinder to find .eh_frame          */
/*                                                                                             */
/* Solaris 10 has no dl_iterate_phdr. Only the main executable is described: its program        */
/* headers are found through the ELF auxiliary vector, which sits right after envp[].          */
/* ------------------------------------------------------------------------------------------ */

struct s10_auxv {
	int a_type;
	long a_val;
};
#define S10_AT_NULL 0
#define S10_AT_PHDR 3
#define S10_AT_PHENT 4
#define S10_AT_PHNUM 5

extern char **environ;
static const Elf64_Phdr *s10_phdr;
static size_t s10_phnum;

__attribute__((constructor)) static void s10_capture_auxv(void)
{
	char **p = environ;
	if (!p)
		return;
	while (*p)
		p++;
	p++; /* skip the NULL that terminates envp[] */
	for (const struct s10_auxv *a = (const struct s10_auxv *)(const void *)p; a->a_type != S10_AT_NULL; a++) {
		if (a->a_type == S10_AT_PHDR)
			s10_phdr = (const Elf64_Phdr *)(uintptr_t)a->a_val;
		else if (a->a_type == S10_AT_PHNUM)
			s10_phnum = (size_t)a->a_val;
	}
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data)
{
	if (!s10_phdr || s10_phnum == 0)
		return 0;

	/* Load bias: 0 for a normal (ET_EXEC) executable, non-zero for a PIE. */
	uintptr_t bias = 0;
	for (size_t i = 0; i < s10_phnum; i++) {
		if (s10_phdr[i].p_type == PT_PHDR) {
			bias = (uintptr_t)s10_phdr - (uintptr_t)s10_phdr[i].p_vaddr;
			break;
		}
	}

	struct dl_phdr_info info;
	memset(&info, 0, sizeof(info));
	info.dlpi_addr = bias;
	info.dlpi_name = "";
	info.dlpi_phdr = s10_phdr;
	info.dlpi_phnum = (unsigned short)s10_phnum;
	info.dlpi_adds = 1; /* the executable never comes or goes */
	info.dlpi_subs = 0;
	return callback(&info, sizeof(info), data);
}

/* ------------------------------------------------------------------------------------------ */
/* __mb_cur_max: on illumos MB_CUR_MAX expands to a call to __mb_cur_max() (a function), and    */
/* __mb_cur_max_l(locale_t) is its per-locale sibling.                                          */
/*                                                                                             */
/* Solaris 10 keeps this value elsewhere, so derive it from the current locale's codeset. An    */
/* over-estimate is harmless (it only sizes conversion buffers), so anything that is not a      */
/* known single-byte codeset is reported as 6, the UTF-8 maximum.                               */
/* ------------------------------------------------------------------------------------------ */

unsigned char __mb_cur_max(void)
{
	const char *cs = nl_langinfo(CODESET);
	if (!cs || !*cs)
		return 1;
	if (strcmp(cs, "646") == 0 || strcasecmp(cs, "ASCII") == 0 || strcasecmp(cs, "US-ASCII") == 0 ||
	    strcasecmp(cs, "ANSI_X3.4-1968") == 0 || strncasecmp(cs, "ISO8859-", 8) == 0 ||
	    strncasecmp(cs, "ISO-8859-", 9) == 0 || strncasecmp(cs, "KOI8", 4) == 0 ||
	    strncasecmp(cs, "CP125", 5) == 0)
		return 1;
	return 6;
}

unsigned char __mb_cur_max_l(locale_t loc)
{
	(void)loc;
	return __mb_cur_max();
}
