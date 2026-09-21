#ifndef COORDINATE_TRANSFORM_JH_H
#define COORDINATE_TRANSFORM_JH_H

#include "Define_JH.h"
#include "TargetSimCoreApi.h"
#include "matrixCalcLib_JH.h"

#ifdef __cplusplus
extern "C" {
#endif

// 좌표계 규약 (각도 rad, 길이 m)
//   Body    : x 전방,       y 우현,  z 하방
//   NED     : x 북,         y 동,    z 하
//   ENU     : x 동,         y 북,    z 상
//   Antenna : x 보어사이트, y 우측,  z 하방
//   az 는 보어사이트 기준 우측 회전이 (+), el 은 수평면 기준 상승이 (+)
//   자세각 회전 순서는 ZYX (yaw -> pitch -> roll)
//   모든 DCM 은 정규직교이므로 역변환은 전치와 같다

#define WGS84_A					6378137.0
#define WGS84_F_INV				298.257223563
#define WGS84_F					(1.0 / WGS84_F_INV)
#define WGS84_B					(WGS84_A * (1.0 - WGS84_F))
#define WGS84_E_SQ				(WGS84_F * (2.0 - WGS84_F))
#define WGS84_EP_SQ				(WGS84_E_SQ / (1.0 - WGS84_E_SQ))

#define COORD_POLAR_EPSILON		1.0e-9
#define ANT_DEFAULT_TILT_DEG	22.0

typedef enum
{
	COORD_OK = 0,
	COORD_ERR_NULL,
	COORD_ERR_RANGE,
	COORD_ERR_MATRIX
} EN_CoordStatus;

typedef struct
{
	FLOAT64		x;
	FLOAT64		y;
	FLOAT64		z;
} ST_CoordRect;

typedef struct
{
	FLOAT64		r;
	FLOAT64		az;
	FLOAT64		el;
} ST_CoordSph;

typedef struct
{
	FLOAT64		r;
	FLOAT64		u;
	FLOAT64		v;
} ST_CoordUv;

typedef struct
{
	FLOAT64		lat;
	FLOAT64		lon;
	FLOAT64		alt;
} ST_CoordLla;

typedef struct
{
	FLOAT64		roll;
	FLOAT64		pitch;
	FLOAT64		yaw;
} ST_CoordAtt;

TSCORE_API EN_CoordStatus	f_Coord_Dcm_Body_To_Ned(ST_Matrix *st_Dcm, const ST_CoordAtt *st_Att);
TSCORE_API EN_CoordStatus	f_Coord_Dcm_Ned_To_Ecef(ST_Matrix *st_Dcm, FLOAT64 refLat, FLOAT64 refLon);
TSCORE_API EN_CoordStatus	f_Coord_Dcm_Ned_To_Enu(ST_Matrix *st_Dcm);
TSCORE_API EN_CoordStatus	f_Coord_Dcm_Ant_To_Body(ST_Matrix *st_Dcm, FLOAT64 mountYaw, FLOAT64 mountTilt);

TSCORE_API EN_CoordStatus	f_Coord_RotateVec(ST_CoordRect *st_Out, const ST_Matrix *st_Dcm, const ST_CoordRect *st_In);
TSCORE_API EN_CoordStatus	f_Coord_RotateVecInv(ST_CoordRect *st_Out, const ST_Matrix *st_Dcm, const ST_CoordRect *st_In);

TSCORE_API EN_CoordStatus	f_Trans_Lla_To_Ecef(ST_CoordRect *st_Out, const ST_CoordLla *st_Lla);
TSCORE_API EN_CoordStatus	f_Trans_Ecef_To_Lla(ST_CoordLla *st_Out, const ST_CoordRect *st_Ecef);

TSCORE_API EN_CoordStatus	f_Trans_Sph_To_Rect(ST_CoordRect *st_Out, const ST_CoordSph *st_Sph);
TSCORE_API EN_CoordStatus	f_Trans_Rect_To_Sph(ST_CoordSph *st_Out, const ST_CoordRect *st_Rect);
TSCORE_API EN_CoordStatus	f_Trans_Sph_To_Uv(ST_CoordUv *st_Out, const ST_CoordSph *st_Sph);
TSCORE_API EN_CoordStatus	f_Trans_Uv_To_Sph(ST_CoordSph *st_Out, const ST_CoordUv *st_Uv);

TSCORE_API EN_CoordStatus	f_Trans_Ant_To_Body(ST_CoordRect *st_Out, const ST_CoordRect *st_Ant, FLOAT64 mountYaw, FLOAT64 mountTilt);
TSCORE_API EN_CoordStatus	f_Trans_Body_To_Ant(ST_CoordRect *st_Out, const ST_CoordRect *st_Body, FLOAT64 mountYaw, FLOAT64 mountTilt);

TSCORE_API EN_CoordStatus	f_Trans_Body_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Body, const ST_CoordAtt *st_Att);
TSCORE_API EN_CoordStatus	f_Trans_Ned_To_Body(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned, const ST_CoordAtt *st_Att);

TSCORE_API EN_CoordStatus	f_Trans_Ned_To_Enu(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned);
TSCORE_API EN_CoordStatus	f_Trans_Enu_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Enu);

TSCORE_API EN_CoordStatus	f_Trans_NedVec_To_Ecef(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned, FLOAT64 refLat, FLOAT64 refLon);
TSCORE_API EN_CoordStatus	f_Trans_EcefVec_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Ecef, FLOAT64 refLat, FLOAT64 refLon);

TSCORE_API EN_CoordStatus	f_Trans_Ned_To_Ecef(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned, const ST_CoordRect *st_RefEcef, FLOAT64 refLat, FLOAT64 refLon);
TSCORE_API EN_CoordStatus	f_Trans_Ecef_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Ecef, const ST_CoordRect *st_RefEcef, FLOAT64 refLat, FLOAT64 refLon);

TSCORE_API EN_CoordStatus	f_Coord_LeverArm_Body_To_Ecef(ST_CoordRect *st_Out, const ST_CoordRect *st_BodyOffset, const ST_CoordAtt *st_Att, FLOAT64 refLat, FLOAT64 refLon);

TSCORE_API EN_CoordStatus	f_Coord_VecAdd(ST_CoordRect *st_Out, const ST_CoordRect *st_Lhs, const ST_CoordRect *st_Rhs);
TSCORE_API EN_CoordStatus	f_Coord_VecSub(ST_CoordRect *st_Out, const ST_CoordRect *st_Lhs, const ST_CoordRect *st_Rhs);
TSCORE_API EN_CoordStatus	f_Coord_VecScale(ST_CoordRect *st_Out, const ST_CoordRect *st_Src, FLOAT64 scalar);
TSCORE_API FLOAT64			f_Coord_VecNorm(const ST_CoordRect *st_Vec);
TSCORE_API FLOAT64			f_Coord_WrapAngle(FLOAT64 angle);
TSCORE_API const CHAR *		f_Coord_StatusStr(EN_CoordStatus status);

#ifdef __cplusplus
}
#endif

#endif
