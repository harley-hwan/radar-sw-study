#ifndef DEFINE_JH_H
#define DEFINE_JH_H

#include <stddef.h>

// MISRA C:2023 deviation
//   Rule 2.3 / 2.5 (Advisory) : 아래 형과 상수는 codingRule.md 1절이 프로젝트 표준으로
//   규정한 집합이라 현재 미사용분도 유지한다.

// Windows SDK 가 VOID 를 매크로로 먼저 정의하는 경우가 있다.
#ifndef VOID
typedef void					VOID;
#endif

typedef unsigned char			UCHAR;
typedef unsigned char			UINT8;
typedef unsigned short			USHORT;
typedef unsigned short			UINT16;
typedef unsigned short int		USHORTINT;
typedef unsigned int			UINT32;
typedef unsigned long			ULONG;
typedef unsigned long int		ULONGINT;
typedef unsigned long long		UINT64;

typedef char					CHAR;
typedef signed char				INT8;
typedef short					SHORT;
typedef signed short			INT16;
typedef signed short int		SSHORT_INT;
typedef signed int				INT32;
typedef signed long				SLONG;
typedef signed long int			SLONGINT;
typedef signed long long		INT64;

typedef float					FLOAT32;
typedef double					FLOAT64;

#define PI							3.14159265358979323846
#define TWO_PI						(2.0 * PI)
#define HALF_PI						(0.5 * PI)

#define LIGHT_SPEED					299792458.0
#define G_FORCE						9.80665

#define EARTH_MEAN_RADIUS			6371200.0
#define EARTH_GRAVITATIONAL_PARAM	3.986004418e14

static inline FLOAT64 f_Deg_To_Rad(FLOAT64 deg)
{
	return deg * (PI / 180.0);
}

static inline FLOAT64 f_Rad_To_Deg(FLOAT64 rad)
{
	return rad * (180.0 / PI);
}

#endif
