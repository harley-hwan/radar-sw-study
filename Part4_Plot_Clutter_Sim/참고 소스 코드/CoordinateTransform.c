#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "CoordinateTransform.h"
#include "matrixCalcLib.h"
 


STRUCT_Coord_Rect f_Trans_Lla_To_Ecef(DOUBLE64 Lon, DOUBLE64 Lat, DOUBLE64 Alt)
{
	STRUCT_Coord_Rect	ST_Result;
	DOUBLE64			Ne;

	Ne = R_1 / sqrt(1. - (e_Coordinate * e_Coordinate * sin(Lat) * sin(Lat)));
    
	ST_Result.x = (Ne + Alt) * cos(Lat) * cos(Lon);
	ST_Result.y = (Ne + Alt) * cos(Lat) * sin(Lon);
	ST_Result.z = ((Ne * (1. - (e_Coordinate * e_Coordinate))) + Alt) * sin(Lat);

	return ST_Result;
}

STRUCT_Coord_Lla f_Trans_Ecef_To_Lla(DOUBLE64 X, DOUBLE64 Y, DOUBLE64 Z)
{
	STRUCT_Coord_Lla	ST_Result;
	DOUBLE64			Ne;
	DOUBLE64 			p;
	DOUBLE64    		temp;
	INT32 				i;

	p = sqrt(pow(X, 2.) + pow(Y, 2.));

    if (X < 0.)
	{
		ST_Result.Lon = atan(Y / X) + PI;
	}
	else
	{
		ST_Result.Lon = atan(Y / X);
	}

	ST_Result.Lat = atan(p / Z);

    for (i = 0; i < 5; i++)
	{
		Ne = R_1 / sqrt(1. - (e_Coordinate * e_Coordinate * sin(ST_Result.Lat) * sin(ST_Result.Lat)));
		ST_Result.Alt = (p / cos(ST_Result.Lat)) - Ne;
		temp = 1. - (e_Coordinate * e_Coordinate * Ne / (Ne + ST_Result.Alt));
		ST_Result.Lat = atan(Z / (p * temp));
	}

	return ST_Result;
}


STRUCT_Coord_Rect f_Trans_Ant_Sph_To_XYZ(DOUBLE64 Range, DOUBLE64 Azimuth, DOUBLE64 Elevation)
{
	STRUCT_Coord_Rect ST_Result;

	ST_Result.x = Range * cos(Elevation) * sin(Azimuth);
	ST_Result.y = Range * sin(Elevation);
	ST_Result.z = Range * cos(Elevation) * cos(Azimuth);

	return ST_Result;
}

STRUCT_Coord_Sph  f_Trans_Ant_XYZ_To_Sph(DOUBLE64 X, DOUBLE64 Y, DOUBLE64 Z)
{
	STRUCT_Coord_Sph ST_Result;

	ST_Result.r = sqrt(pow(X, 2.) + pow(Y, 2.) + pow(Z, 2.));
	ST_Result.az = atan2(X, Z);
	ST_Result.el = atan2(Y, sqrt(pow(X, 2.) + pow(Z, 2.)));

	return ST_Result;
}

STRUCT_Coord_Rect f_Trans_Ant_To_Body(DOUBLE64 X_Ant, DOUBLE64 Y_Ant, DOUBLE64 Z_Ant)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Body;
	matrix 				mat_Ant;
	matrix 				mat_TransformMat_Ant_To_Body;

	Matrix_initialize(&mat_Body, 3, 1);
	Matrix_initialize(&mat_Ant, 3, 1);
	Matrix_initialize(&mat_TransformMat_Ant_To_Body, 3, 3);

    mat_Ant.e[0][0] = X_Ant;
    mat_Ant.e[1][0] = Y_Ant;
    mat_Ant.e[2][0] = Z_Ant;

    mat_TransformMat_Ant_To_Body.e[0][0] = 0.;
    mat_TransformMat_Ant_To_Body.e[0][1] = 0.;
    mat_TransformMat_Ant_To_Body.e[0][2] = 1.;
    mat_TransformMat_Ant_To_Body.e[1][0] = -1.;
    mat_TransformMat_Ant_To_Body.e[1][1] = 0.;
    mat_TransformMat_Ant_To_Body.e[1][2] = 0.;
    mat_TransformMat_Ant_To_Body.e[2][0] = 0.;
    mat_TransformMat_Ant_To_Body.e[2][1] = -1.;
    mat_TransformMat_Ant_To_Body.e[2][2] = 0.;

	mat_Body = Matrix_Product2(&mat_TransformMat_Ant_To_Body, &mat_Ant);

	ST_Result.x = mat_Body.e[0][0];
	ST_Result.y = mat_Body.e[1][0];
	ST_Result.z = mat_Body.e[2][0];

	return ST_Result;
}

STRUCT_Coord_Rect f_Trans_Body_To_Ant(DOUBLE64 X_Body, DOUBLE64 Y_Body, DOUBLE64 Z_Body)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Body;
	matrix 				mat_Ant;
	matrix 				mat_TransformMat_Body_To_Ant;

	Matrix_initialize(&mat_Body, 3, 1);
	Matrix_initialize(&mat_Ant, 3, 1);
	Matrix_initialize(&mat_TransformMat_Body_To_Ant, 3, 3);

    	mat_Body.e[0][0] = X_Body;
	mat_Body.e[1][0] = Y_Body;
	mat_Body.e[2][0] = Z_Body;

	mat_TransformMat_Body_To_Ant.e[0][0] = 0.;
	mat_TransformMat_Body_To_Ant.e[0][1] = -1.;
	mat_TransformMat_Body_To_Ant.e[0][2] = 0.;
	mat_TransformMat_Body_To_Ant.e[1][0] = 0.;
	mat_TransformMat_Body_To_Ant.e[1][1] = 0.;
	mat_TransformMat_Body_To_Ant.e[1][2] = -1.;
    mat_TransformMat_Body_To_Ant.e[2][0] = 1.;
	mat_TransformMat_Body_To_Ant.e[2][1] = 0.;
	mat_TransformMat_Body_To_Ant.e[2][2] = 0.;

	mat_Ant = Matrix_Product2(&mat_TransformMat_Body_To_Ant, &mat_Body);

	ST_Result.x = mat_Ant.e[0][0];
	ST_Result.y = mat_Ant.e[1][0];
	ST_Result.z = mat_Ant.e[2][0];

	return ST_Result;
}

STRUCT_Coord_Rect f_Trans_Body_To_Ned(DOUBLE64 X_Body, DOUBLE64 Y_Body, DOUBLE64 Z_Body, DOUBLE64 Roll, DOUBLE64 Yaw, DOUBLE64 Pitch)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Body;
	matrix 				mat_Ned;
	matrix 				mat_TransformMat_Body_To_Ned;

	Matrix_initialize(&mat_Body, 3, 1);
    Matrix_initialize(&mat_Ned, 3, 1);
    Matrix_initialize(&mat_TransformMat_Body_To_Ned, 3, 3);

    mat_Body.e[0][0] = X_Body;
    mat_Body.e[1][0] = Y_Body;
    mat_Body.e[2][0] = Z_Body;

    mat_TransformMat_Body_To_Ned.e[0][0] = cos(Yaw) * cos(Pitch);
    mat_TransformMat_Body_To_Ned.e[0][1] = (sin(Roll) * sin(Pitch) * cos(Yaw)) - (cos(Roll) * sin(Yaw));
    mat_TransformMat_Body_To_Ned.e[0][2] = (cos(Roll) * sin(Pitch) * cos(Yaw)) + (sin(Roll) * sin(Yaw));
    mat_TransformMat_Body_To_Ned.e[1][0] = cos(Pitch) * sin(Yaw);
    mat_TransformMat_Body_To_Ned.e[1][1] = (sin(Roll) * sin(Pitch) * sin(Yaw)) + (cos(Roll) * cos(Yaw));
    mat_TransformMat_Body_To_Ned.e[1][2] = (cos(Roll) * sin(Pitch) * sin(Yaw)) - (sin(Roll) * cos(Yaw));
    mat_TransformMat_Body_To_Ned.e[2][0] = -1. * sin(Pitch);
    mat_TransformMat_Body_To_Ned.e[2][1] = sin(Roll) * cos(Pitch);
    mat_TransformMat_Body_To_Ned.e[2][2] = cos(Roll) * cos(Pitch);

	mat_Ned = Matrix_Product2(&mat_TransformMat_Body_To_Ned, &mat_Body);

	ST_Result.x = mat_Ned.e[0][0];
	ST_Result.y = mat_Ned.e[1][0];
	ST_Result.z = mat_Ned.e[2][0];

	return ST_Result;
}





STRUCT_Coord_Rect f_Trans_Ned_To_Body(DOUBLE64 X_Ned, DOUBLE64 Y_Ned, DOUBLE64 Z_Ned, DOUBLE64 Roll, DOUBLE64 Yaw, DOUBLE64 Pitch)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Body;
	matrix 				mat_Ned;
	matrix 				mat_TransformMat_Ned_To_Body;

	Matrix_initialize(&mat_Body, 3, 1);
	Matrix_initialize(&mat_Ned, 3, 1);
    Matrix_initialize(&mat_TransformMat_Ned_To_Body, 3, 3);

    mat_Ned.e[0][0] = X_Ned;
    mat_Ned.e[1][0] = Y_Ned;
    mat_Ned.e[2][0] = Z_Ned;

    mat_TransformMat_Ned_To_Body.e[0][0] = cos(Yaw) * cos(Pitch);
    mat_TransformMat_Ned_To_Body.e[0][1] = sin(Yaw) * cos(Pitch);
    mat_TransformMat_Ned_To_Body.e[0][2] = -1. * sin(Pitch);
    mat_TransformMat_Ned_To_Body.e[1][0] = (-1. * sin(Yaw) * cos(Roll)) + (cos(Yaw) * sin(Pitch) * sin(Roll));
    mat_TransformMat_Ned_To_Body.e[1][1] = (cos(Yaw) * cos(Roll)) + (sin(Yaw) * sin(Pitch) * sin(Roll));
	mat_TransformMat_Ned_To_Body.e[1][2] = cos(Pitch) * sin(Roll);
	mat_TransformMat_Ned_To_Body.e[2][0] = (sin(Yaw) * sin(Roll)) + (cos(Yaw) * sin(Pitch) * cos(Roll));
	mat_TransformMat_Ned_To_Body.e[2][1] = (-1. * cos(Yaw) * sin(Roll)) + (sin(Yaw) * sin(Pitch) * cos(Roll));
	mat_TransformMat_Ned_To_Body.e[2][2] = cos(Pitch) * cos(Roll);

	mat_Body = Matrix_Product2(&mat_TransformMat_Ned_To_Body, &mat_Ned);

	ST_Result.x = mat_Body.e[0][0];
	ST_Result.y = mat_Body.e[1][0];
	ST_Result.z = mat_Body.e[2][0];

	return ST_Result;
}

STRUCT_Coord_Rect f_Trans_Ned_To_Enu(DOUBLE64 X_Ned, DOUBLE64 Y_Ned, DOUBLE64 Z_Ned)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Ned;
	matrix 				mat_Enu;
	matrix 				mat_TransformMat_Ned_To_Enu;

	Matrix_initialize(&mat_Ned, 3, 1);
	Matrix_initialize(&mat_Enu, 3, 1);
	Matrix_initialize(&mat_TransformMat_Ned_To_Enu, 3, 3);

    mat_Ned.e[0][0] = X_Ned;
    mat_Ned.e[1][0] = Y_Ned;
    mat_Ned.e[2][0] = Z_Ned;

    mat_TransformMat_Ned_To_Enu.e[0][0] = 0.;
    mat_TransformMat_Ned_To_Enu.e[0][1] = 1.;
    mat_TransformMat_Ned_To_Enu.e[0][2] = 0.;
    mat_TransformMat_Ned_To_Enu.e[1][0] = 1.;
    mat_TransformMat_Ned_To_Enu.e[1][1] = 0.;
    mat_TransformMat_Ned_To_Enu.e[1][2] = 0.;
    mat_TransformMat_Ned_To_Enu.e[2][0] = 0.;
    mat_TransformMat_Ned_To_Enu.e[2][1] = 0.;
    mat_TransformMat_Ned_To_Enu.e[2][2] = -1.;

    mat_Enu = Matrix_Product2(&mat_TransformMat_Ned_To_Enu, &mat_Ned);

	ST_Result.x = mat_Enu.e[0][0];
	ST_Result.y = mat_Enu.e[1][0];
	ST_Result.z = mat_Enu.e[2][0];

	return ST_Result;
}


STRUCT_Coord_Rect f_Trans_Enu_To_Ned(DOUBLE64 X_Enu, DOUBLE64 Y_Enu, DOUBLE64 Z_Enu)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Ned;
	matrix 				mat_Enu;
	matrix 				mat_TransformMat_Enu_To_Ned;

	Matrix_initialize(&mat_Ned, 3, 1);
	Matrix_initialize(&mat_Enu, 3, 1);
	Matrix_initialize(&mat_TransformMat_Enu_To_Ned, 3, 3);

    mat_Enu.e[0][0] = X_Enu;
    mat_Enu.e[1][0] = Y_Enu;
    mat_Enu.e[2][0] = Z_Enu;

    mat_TransformMat_Enu_To_Ned.e[0][0] = 0.;
    mat_TransformMat_Enu_To_Ned.e[0][1] = 1.;
    mat_TransformMat_Enu_To_Ned.e[0][2] = 0.;
    mat_TransformMat_Enu_To_Ned.e[1][0] = 1.;
    mat_TransformMat_Enu_To_Ned.e[1][1] = 0.;
    mat_TransformMat_Enu_To_Ned.e[1][2] = 0.;
    mat_TransformMat_Enu_To_Ned.e[2][0] = 0.;
    mat_Enu.e[0][0] = X_Enu;
    mat_Enu.e[1][0] = Y_Enu;
    mat_Enu.e[2][0] = Z_Enu;

    mat_TransformMat_Enu_To_Ned.e[0][0] = 0.;
    mat_TransformMat_Enu_To_Ned.e[0][1] = 1.;
    mat_TransformMat_Enu_To_Ned.e[0][2] = 0.;
    mat_TransformMat_Enu_To_Ned.e[1][0] = 1.;
    mat_TransformMat_Enu_To_Ned.e[1][1] = 0.;
    mat_TransformMat_Enu_To_Ned.e[1][2] = 0.;
    mat_TransformMat_Enu_To_Ned.e[2][0] = 0.;
	mat_TransformMat_Enu_To_Ned.e[2][1] = 0.;
	mat_TransformMat_Enu_To_Ned.e[2][2] = -1.;

	mat_Ned = Matrix_Product2(&mat_TransformMat_Enu_To_Ned, &mat_Enu);

	ST_Result.x = mat_Ned.e[0][0];
	ST_Result.y = mat_Ned.e[1][0];
	ST_Result.z = mat_Ned.e[2][0];

	return ST_Result;
}


STRUCT_Coord_Rect f_Trans_Ned_To_Ecef(DOUBLE64 Target_X_Ned, DOUBLE64 Target_Y_Ned, DOUBLE64 Target_Z_Ned, DOUBLE64 Pf_X, DOUBLE64 Pf_Y, DOUBLE64 Pf_Z, DOUBLE64 Pf_Lat, DOUBLE64 Pf_Lon)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Platform;
	matrix 				mat_Target_Ned;
	matrix 				mat_Target_Ecef;
	matrix 				mat_Ned2Ecef;

    Matrix_initialize(&mat_Platform, 3, 1);
    Matrix_initialize(&mat_Target_Ned, 3, 1);
    Matrix_initialize(&mat_Target_Ecef, 3, 1);
    Matrix_initialize(&mat_Ned2Ecef, 3, 3);

    mat_Platform.e[0][0] = Pf_X;
    mat_Platform.e[1][0] = Pf_Y;
    mat_Platform.e[2][0] = Pf_Z;

    mat_Target_Ned.e[0][0] = Target_X_Ned;
    mat_Target_Ned.e[1][0] = Target_Y_Ned;
    mat_Target_Ned.e[2][0] = Target_Z_Ned;

	mat_Ned2Ecef.e[0][0] = -1. * sin(Pf_Lat) * cos(Pf_Lon);
	mat_Ned2Ecef.e[0][1] = -1. * sin(Pf_Lon);
	mat_Ned2Ecef.e[0][2] = -1. * cos(Pf_Lat) * cos(Pf_Lon);
	mat_Ned2Ecef.e[1][0] = -1. * sin(Pf_Lat) * sin(Pf_Lon);
	mat_Ned2Ecef.e[1][1] = cos(Pf_Lon);
	mat_Ned2Ecef.e[1][2] = -1. * cos(Pf_Lat) * sin(Pf_Lon);
	mat_Ned2Ecef.e[2][0] = cos(Pf_Lat);
	mat_Ned2Ecef.e[2][1] = 0;
    mat_Ned2Ecef.e[2][2] = -1. * sin(Pf_Lat);

	mat_Target_Ecef = Matrix_Product2(&mat_Ned2Ecef, &mat_Target_Ned);
	mat_Target_Ecef = Matrix_Add(&mat_Target_Ecef, &mat_Platform);

	ST_Result.x = mat_Target_Ecef.e[0][0];
	ST_Result.y = mat_Target_Ecef.e[1][0];
	ST_Result.z = mat_Target_Ecef.e[2][0];

	return ST_Result;
}

STRUCT_Coord_Rect f_Trans_Ecef_To_Ned(DOUBLE64 Target_X_Ecef, DOUBLE64 Target_Y_Ecef, DOUBLE64 Target_Z_Ecef, DOUBLE64 Pf_X, DOUBLE64 Pf_Y, DOUBLE64 Pf_Z, DOUBLE64 Pf_Lat, DOUBLE64 Pf_Lon)
{
	STRUCT_Coord_Rect 	ST_Result;
	matrix 				mat_Platform;
	matrix 				mat_Target_Ned;
	matrix 				mat_Target_Ecef;
	matrix 				mat_Ecef2Ned;

    Matrix_initialize(&mat_Platform, 3, 1);
    Matrix_initialize(&mat_Target_Ned, 3, 1);
    Matrix_initialize(&mat_Target_Ecef, 3, 1);
    Matrix_initialize(&mat_Ecef2Ned, 3, 3);

    mat_Platform.e[0][0] = Pf_X;
    mat_Platform.e[1][0] = Pf_Y;
    mat_Platform.e[2][0] = Pf_Z;

    mat_Target_Ecef.e[0][0] = Target_X_Ecef;
    mat_Target_Ecef.e[1][0] = Target_Y_Ecef;
    mat_Target_Ecef.e[2][0] = Target_Z_Ecef;

    mat_Ecef2Ned.e[0][0] = -1. * sin(Pf_Lat) * cos(Pf_Lon);
    mat_Ecef2Ned.e[0][1] = -1. * sin(Pf_Lat) * sin(Pf_Lon);
    mat_Ecef2Ned.e[0][2] = cos(Pf_Lat);
    mat_Ecef2Ned.e[1][0] = -1. * sin(Pf_Lon);
	mat_Ecef2Ned.e[1][1] = cos(Pf_Lon);
	mat_Ecef2Ned.e[1][2] = 0;
	mat_Ecef2Ned.e[2][0] = -1. * cos(Pf_Lat) * cos(Pf_Lon);
	mat_Ecef2Ned.e[2][1] = -1. * cos(Pf_Lat) * sin(Pf_Lon);
	mat_Ecef2Ned.e[2][2] = -1. * sin(Pf_Lat);

	mat_Target_Ned = Matrix_Subtract(&mat_Target_Ecef, &mat_Platform);
	mat_Target_Ned = Matrix_Product2(&mat_Ecef2Ned, &mat_Target_Ned);

	ST_Result.x = mat_Target_Ned.e[0][0];
	ST_Result.y = mat_Target_Ned.e[1][0];
	ST_Result.z = mat_Target_Ned.e[2][0];

	return ST_Result;
}

STRUCT_Coord_Rect f_Get_PfCompensationVal(DOUBLE64 Pf_X, DOUBLE64 Pf_Y, DOUBLE64 Pf_Z, DOUBLE64 Diff_X, DOUBLE64 Diff_Y, DOUBLE64 Diff_Z)
{
	STRUCT_Coord_Rect	ST_Result;
	matrix      		mat_Diff_C;
	matrix				mat_Result;
	matrix				mat_TransMat;

	DOUBLE64			R_1_Square;
	DOUBLE64			R_2_Square;
	DOUBLE64			Alpha;
	DOUBLE64			R_xy;
	Matrix_initialize(&mat_Diff_C, 3, 1);
	Matrix_initialize(&mat_Result, 3, 1);
	Matrix_initialize(&mat_TransMat, 3, 3);

	R_1_Square = pow(R_1, 2.);
	R_2_Square = pow(R_2, 2.);

	R_xy = sqrt(pow(Pf_X, 2.) + pow(Pf_Y, 2.));
	Alpha = atan2(Pf_Z * R_1_Square, R_xy * R_2_Square);

	mat_TransMat.e[0][0] = -1. * Pf_X / R_xy * sin(Alpha);
	mat_TransMat.e[0][1] = Pf_Y / R_xy * sin(Alpha);
	mat_TransMat.e[0][2] = -1. * cos(Alpha);
	mat_TransMat.e[1][0] = -1. * Pf_Y / R_xy;
	mat_TransMat.e[1][1] = -1. * Pf_X / R_xy;
	mat_TransMat.e[1][2] = 0;
	mat_TransMat.e[2][0] = -1. * Pf_X / R_xy * cos(Alpha);
	mat_TransMat.e[2][1] = Pf_Y / R_xy * cos(Alpha);
	mat_TransMat.e[2][2] = sin(Alpha);

	mat_Diff_C.e[0][0] = Diff_X;
	mat_Diff_C.e[1][0] = -1. * Diff_Y;
	mat_Diff_C.e[2][0] = -1. * Diff_Z;

    mat_Result = Matrix_Product2(&mat_TransMat, &mat_Diff_C);

	ST_Result.x = mat_Result.e[0][0];
	ST_Result.y = mat_Result.e[1][0];
	ST_Result.z = mat_Result.e[2][0];

	return ST_Result;
}
