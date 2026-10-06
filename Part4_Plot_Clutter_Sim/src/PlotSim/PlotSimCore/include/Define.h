#ifndef DEFINE_H
#define DEFINE_H

#include <stdint.h>
#include <stddef.h>

#define VOID 		void
#define CHAR8 		signed char
#define UCHAR8 		unsigned char
#define INT16 		signed short
#define UINT16 		unsigned short
#define INT32 		signed int
#define UINT32 		unsigned int
#define FLOAT32 	float
#define DOUBLE64 	double
#define INT64		long long
#define TICK 		unsigned long

// 코딩룰이 요구하지만 위에 없는 형. BOOL 은 MFC 의 typedef(int) 와 충돌해 두지 않는다.
typedef char		CHAR;
typedef double		FLOAT64;

#define LIGHT_SPEED							299792458.0
#define PI									3.14159265358979
#define INF									9999999999
#define G_FORCE								9.80665
#define EARTH_RADIUS						6371.2		//[km]
#define EARTH_GRAVITIONAL_PARAMETER			398600.4418  	//[km^3/s^2]

static inline DOUBLE64 f_Deg_To_Rad(DOUBLE64 deg)
{
	return deg * (PI / 180.0);
}

static inline DOUBLE64 f_Rad_To_Deg(DOUBLE64 rad)
{
	return rad * (180.0 / PI);
}

#endif
