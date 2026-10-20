#ifndef IDD_H
#define IDD_H

#include "matrixCalcLib.h"
#include "Define.h"

#pragma pack(1)

typedef struct STRUCT_PlatformInfo
{
	FLOAT32					Pf_Lat;
	FLOAT32					Pf_Lon;
	FLOAT32					Pf_Alt;
    FLOAT32					Pf_EcefX;
	FLOAT32					Pf_EcefY;
	FLOAT32					Pf_EcefZ;
	FLOAT32					Pf_Roll;
	FLOAT32					Pf_Yaw;
	FLOAT32					Pf_Pitch;
}STRUCT_PlatformInfo;


typedef struct STRUCT_PlotInfo
{
	UINT32					Id;
	UINT32					EmittedDwellNum;
	UINT32					EmittedScanNum;
	UINT32					EmittedBeamNum;
	UINT32					EmittedTimeStamp_s;
	UINT32					EmittedTimeStamp_ns;
	UCHAR8					BeamType;
	FLOAT32					Range_Ant;
	FLOAT32					Azimuth_Ant;
	FLOAT32					Elevation_Ant;
	FLOAT32					Range_VcNed;
	FLOAT32					Azimuth_VcNed;
	FLOAT32					Elevation_VcNed;
	DOUBLE64				X_Ant;
	DOUBLE64				Y_Ant;
	DOUBLE64				Z_Ant;
	DOUBLE64				X_Body;
	DOUBLE64				Y_Body;
