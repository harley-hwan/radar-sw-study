/* Linux stand-in for the Win32 / Winsock2 API surface used by PlotLink.c (validation only, not part of the deliverable). */
#ifndef WINSTUB_WINSOCK2_H
#define WINSTUB_WINSOCK2_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef uintptr_t SOCKET;
typedef uint32_t DWORD;
typedef int BOOL;
typedef long LONG;
typedef void *HANDLE;
typedef unsigned long long ULONGLONG;
typedef pthread_mutex_t CRITICAL_SECTION;
typedef struct { int dummy; } WSADATA;

#define INVALID_SOCKET ((SOCKET)~(uintptr_t)0)
#define SOCKET_ERROR (-1)
#define TRUE 1
#define FALSE 0
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define INFINITE 0xFFFFFFFFu
#define WSAETIMEDOUT 10060
#define MAKEWORD(a, b) ((a) | ((b) << 8))
#define IOC_VENDOR 0
#define _WSAIOW(x, y) 0
#define __stdcall

static inline ULONGLONG GetTickCount64(void) { struct timeval tv; gettimeofday(&tv, NULL); return (ULONGLONG)tv.tv_sec * 1000ULL + (ULONGLONG)(tv.tv_usec / 1000); }
static inline void Sleep(DWORD ms) { usleep((useconds_t)ms * 1000u); }

static inline int WSAStartup(int v, WSADATA *d) { (void)v; (void)d; return 0; }
static inline int WSACleanup(void) { return 0; }
static inline int WSAGetLastError(void) { return ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ? WSAETIMEDOUT : errno; }
static inline int WSAIoctl(SOCKET s, DWORD c, void *a, DWORD b, void *d, DWORD e, DWORD *f, void *g, void *h)
{ (void)s; (void)c; (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; (void)h; return 0; }
static inline int closesocket(SOCKET s) { return close((int)s); }

/* SO_RCVTIMEO takes a DWORD [ms] on Windows and a struct timeval on Linux */
static inline int stub_setsockopt(SOCKET s, int level, int opt, const void *val, int len)
{
	if ((level == SOL_SOCKET) && (opt == SO_RCVTIMEO) && (len == (int)sizeof(DWORD)))
	{
		DWORD ms = *(const DWORD *)val;
		struct timeval tv;
		tv.tv_sec = ms / 1000u;
		tv.tv_usec = (ms % 1000u) * 1000u;
		return setsockopt((int)s, level, opt, &tv, sizeof(tv));
	}
	return setsockopt((int)s, level, opt, val, (socklen_t)len);
}
#define setsockopt(s, l, o, v, n) stub_setsockopt((SOCKET)(s), (l), (o), (v), (int)(n))
#define getsockname(s, a, n) getsockname((int)(s), (a), (socklen_t *)(n))

static inline void InitializeCriticalSection(CRITICAL_SECTION *c)
{ pthread_mutexattr_t a; pthread_mutexattr_init(&a); pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE); pthread_mutex_init(c, &a); }
static inline void DeleteCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_destroy(c); }
static inline void EnterCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_lock(c); }
static inline void LeaveCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_unlock(c); }

/* events and threads share one handle object; signaled = event set / thread finished */
typedef struct { int isThread; int manual; volatile int signaled; pthread_mutex_t m; pthread_t t; unsigned (*fn)(void *); void *arg; } STUB_OBJ;

static inline HANDLE CreateEventW(void *a, BOOL manual, BOOL initial, void *n)
{
	STUB_OBJ *o = (STUB_OBJ *)calloc(1, sizeof(STUB_OBJ)); (void)a; (void)n;
	o->manual = manual; o->signaled = initial; pthread_mutex_init(&o->m, NULL); return o;
}
static inline BOOL SetEvent(HANDLE h) { STUB_OBJ *o = (STUB_OBJ *)h; pthread_mutex_lock(&o->m); o->signaled = 1; pthread_mutex_unlock(&o->m); return 1; }
static inline BOOL ResetEvent(HANDLE h) { STUB_OBJ *o = (STUB_OBJ *)h; pthread_mutex_lock(&o->m); o->signaled = 0; pthread_mutex_unlock(&o->m); return 1; }
static inline int stub_try(STUB_OBJ *o)
{
	int r; pthread_mutex_lock(&o->m); r = o->signaled; if (r && !o->isThread && !o->manual) o->signaled = 0; pthread_mutex_unlock(&o->m); return r;
}
static inline DWORD WaitForMultipleObjects(DWORD n, const HANDLE *h, BOOL all, DWORD ms)
{
	ULONGLONG end = GetTickCount64() + ms; DWORD i;
	for (;;)
	{
		if (all) { int ok = 1; for (i = 0; i < n; i++) { if (!stub_try((STUB_OBJ *)h[i])) ok = 0; } if (ok) return WAIT_OBJECT_0; }
		else { for (i = 0; i < n; i++) { if (stub_try((STUB_OBJ *)h[i])) return WAIT_OBJECT_0 + i; } }
		if ((ms != INFINITE) && (GetTickCount64() >= end)) return WAIT_TIMEOUT;
		usleep(200);
	}
}
static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms) { return WaitForMultipleObjects(1, &h, FALSE, ms); }
static void *stub_thread_main(void *p)
{
	STUB_OBJ *o = (STUB_OBJ *)p; o->fn(o->arg); pthread_mutex_lock(&o->m); o->signaled = 1; pthread_mutex_unlock(&o->m); return NULL;
}
static inline uintptr_t _beginthreadex(void *sec, unsigned stack, unsigned (*fn)(void *), void *arg, unsigned flag, unsigned *id)
{
	STUB_OBJ *o = (STUB_OBJ *)calloc(1, sizeof(STUB_OBJ)); (void)sec; (void)stack; (void)flag; (void)id;
	o->isThread = 1; o->fn = fn; o->arg = arg; pthread_mutex_init(&o->m, NULL);
	if (pthread_create(&o->t, NULL, stub_thread_main, o) != 0) { free(o); return 0; }
	return (uintptr_t)o;
}
static inline BOOL CloseHandle(HANDLE h) { STUB_OBJ *o = (STUB_OBJ *)h; if (o->isThread) pthread_join(o->t, NULL); pthread_mutex_destroy(&o->m); free(o); return 1; }
#endif
