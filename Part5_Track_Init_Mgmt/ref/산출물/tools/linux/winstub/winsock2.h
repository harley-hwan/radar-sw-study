#ifndef WINSTUB_WINSOCK2_H
#define WINSTUB_WINSOCK2_H
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
typedef uintptr_t SOCKET;
typedef unsigned long DWORD;
typedef int BOOL;
typedef long LONG;
typedef unsigned int UINT;
typedef void *HANDLE;
typedef unsigned long long ULONGLONG;
typedef struct { int dummy; } CRITICAL_SECTION;
typedef struct { int dummy; } WSADATA;
#define INVALID_SOCKET ((SOCKET)~(uintptr_t)0)
#define SOCKET_ERROR (-1)
#define TRUE 1
#define FALSE 0
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define INFINITE 0xFFFFFFFF
#define WSAETIMEDOUT 10060
#define WSAEWOULDBLOCK 10035
#define MAKEWORD(a,b) 0
#define IOC_VENDOR 0
#define _WSAIOW(x,y) 0
#define __stdcall
static inline int WSAStartup(int a, WSADATA *b) { (void)a; (void)b; return 0; }
static inline int WSACleanup(void) { return 0; }
static inline int WSAGetLastError(void) { return errno; }
static inline int WSAIoctl(SOCKET s, DWORD c, void *a, DWORD b, void *d, DWORD e, DWORD *f, void *g, void *h) { (void)s;(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;(void)g;(void)h; return 0; }
static inline int closesocket(SOCKET s) { return close((int)s); }
static inline int ioctlsocket(SOCKET s, long c, unsigned long *a) { (void)s;(void)c;(void)a; return 0; }
static inline void InitializeCriticalSection(CRITICAL_SECTION *c) { (void)c; }
static inline void DeleteCriticalSection(CRITICAL_SECTION *c) { (void)c; }
static inline void EnterCriticalSection(CRITICAL_SECTION *c) { (void)c; }
static inline void LeaveCriticalSection(CRITICAL_SECTION *c) { (void)c; }
static inline HANDLE CreateEventW(void *a, BOOL b, BOOL c, void *d) { (void)a;(void)b;(void)c;(void)d; return (HANDLE)1; }
static inline BOOL SetEvent(HANDLE h) { (void)h; return 1; }
static inline BOOL ResetEvent(HANDLE h) { (void)h; return 1; }
static inline BOOL CloseHandle(HANDLE h) { (void)h; return 1; }
static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms) { (void)h; (void)ms; return WAIT_TIMEOUT; }
static inline void Sleep(DWORD ms) { usleep(ms * 1000); }
static inline ULONGLONG GetTickCount64(void) { struct timeval tv; gettimeofday(&tv, NULL); return (ULONGLONG)tv.tv_sec * 1000ULL + tv.tv_usec / 1000; }
static inline uintptr_t _beginthreadex(void *a, unsigned b, unsigned (*f)(void *), void *arg, unsigned c, unsigned *d) { (void)a;(void)b;(void)f;(void)arg;(void)c;(void)d; return 0; }

static inline DWORD WaitForMultipleObjects(DWORD n, const HANDLE *h, BOOL all, DWORD ms) { (void)n;(void)h;(void)all;(void)ms; return WAIT_OBJECT_0; }
#endif
