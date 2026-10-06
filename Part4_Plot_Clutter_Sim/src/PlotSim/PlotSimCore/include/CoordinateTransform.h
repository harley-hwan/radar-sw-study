#ifndef	COORDINATE_TRANSFORM_H
#define COORDINATE_TRANSFORM_H

#include "Define.h"

#define R_1							6378137.0
#define R_2							6356752.3142
#define FirstEccentricitySquared	0.006694
#define e_Coordinate				0.08181919
#define AntTiltAngle				22.000000

typedef struct STRUCT_Coord_Rect
{
	DOUBLE64	x;		// meter
	DOUBLE64	y;		// meter
	DOUBLE64	z;		// meter
}STRUCT_Coord_Rect;

typedef struct STRUCT_Coord_Ant
{
	DOUBLE64	r;
	DOUBLE64	u;
	DOUBLE64	v;
}STRUCT_Coord_Ant;

typedef struct STRUCT_Coord_Sph
{
	DOUBLE64	r;		// meter
	DOUBLE64	az;		// radian : sin(targetAz) - sin(beamAz)
	DOUBLE64	el;		// radian : sin(targetEl) - sin(beamEl)
}STRUCT_Coord_Sph;

typedef struct STRUCT_Coord_Lla
{
	DOUBLE64 	Lat;
	DOUBLE64 	Lon;
	DOUBLE64 	Alt;
}STRUCT_Coord_Lla;

typedef struct STRUCT_Coord_Attitude
{
	DOUBLE64 	Roll;
	DOUBLE64 	Pitch;
	DOUBLE64 	Yaw;
}STRUCT_Coord_Attitude;


STRUCT_Coord_Rect f_Trans_Lla_To_Ecef(DOUBLE64 Lon, DOUBLE64 Lat, DOUBLE64 Alt);
STRUCT_Coord_Lla  f_Trans_Ecef_To_Lla(DOUBLE64 X, DOUBLE64 Y, DOUBLE64 Z);

STRUCT_Coord_Rect f_Trans_Ant_Sph_To_XYZ(DOUBLE64 Range, DOUBLE64 Azimuth, DOUBLE64 Elevation);
STRUCT_Coord_Sph  f_Trans_Ant_XYZ_To_Sph(DOUBLE64 X, DOUBLE64 Y, DOUBLE64 Z);

STRUCT_Coord_Rect f_Trans_Ant_To_Body(DOUBLE64 X_Ant, DOUBLE64 Y_Ant, DOUBLE64 Z_Ant);
STRUCT_Coord_Rect f_Trans_Body_To_Ant(DOUBLE64 X_Body, DOUBLE64 Y_Body, DOUBLE64 Z_Body);

STRUCT_Coord_Rect f_Trans_Body_To_Ned(DOUBLE64 X_Body, DOUBLE64 Y_Body, DOUBLE64 Z_Body, DOUBLE64 Roll, DOUBLE64 Yaw, DOUBLE64 Pitch);
STRUCT_Coord_Rect f_Trans_Ned_To_Body(DOUBLE64 X_Ned, DOUBLE64 Y_Ned, DOUBLE64 Z_Ned, DOUBLE64 Roll, DOUBLE64 Yaw, DOUBLE64 Pitch);

STRUCT_Coord_Rect f_Trans_Ned_To_Enu(DOUBLE64 X_Ned, DOUBLE64 Y_Ned, DOUBLE64 Z_Ned);
STRUCT_Coord_Rect f_Trans_Enu_To_Ned(DOUBLE64 X_Enu, DOUBLE64 Y_Enu, DOUBLE64 Z_Enu);

STRUCT_Coord_Rect f_Trans_Ned_To_Ecef(DOUBLE64 Target_X_Ned, DOUBLE64 Target_Y_Ned, DOUBLE64 Target_Z_Ned, DOUBLE64 Pf_X, DOUBLE64 Pf_Y, DOUBLE64 Pf_Z, DOUBLE64 Pf_Lat, DOUBLE64 Pf_Lon);
STRUCT_Coord_Rect f_Trans_Ecef_To_Ned(DOUBLE64 Target_X_Ecef, DOUBLE64 Target_Y_Ecef, DOUBLE64 Target_Z_Ecef, DOUBLE64 Pf_X, DOUBLE64 Pf_Y, DOUBLE64 Pf_Z, DOUBLE64 Pf_Lat, DOUBLE64 Pf_Lon);
STRUCT_Coord_Rect f_Get_PfCompensationVal(DOUBLE64 Pf_X, DOUBLE64 Pf_Y, DOUBLE64 Pf_Z, DOUBLE64 Diff_X, DOUBLE64 Diff_Y, DOUBLE64 Diff_Z);

#endif
