// 플롯 메시지 ICD (송신 쓰레드 -> 수신 쓰레드). 거리 m, 각도 rad
// 참고 소스 코드의 Idd.h 에서 끊긴 뒤 (Z_Body 부터) 와 STRUCT_PlotMsg 를 이어 씀
#ifndef IDD_H
#define IDD_H

#include "matrixCalcLib.h"
#include "Define.h"

#pragma pack(1)

// 플랫폼 (레이다) 위치, 자세 (참고 소스 코드 그대로)
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


// 플롯 하나 (참고 소스 코드 그대로). 송신 쪽은 *_Ant 구좌표와 머리 정보만 채움
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
	DOUBLE64				Z_Body;
	DOUBLE64				X_VcNed;
	DOUBLE64				Y_VcNed;
	DOUBLE64				Z_VcNed;
}STRUCT_PlotInfo;

#define IDD_MSG_ID_PLOT			0x5001					// 플롯 메시지
#define IDD_MSG_ID_END			0x50FF					// 송신 끝 (플롯 없음)
#define IDD_MAX_PLOT			64						// 메시지당 최대 플롯 수
#define IDD_PLOT_ID_MIN			10000					// 플롯 ID 범위 (과제 조건)
#define IDD_PLOT_ID_MAX			1000000

// 플롯 메시지: 스캔 하나에 하나, 표적 플롯과 클러터가 섞임. 머리 + 플롯 PlotNum 개만 보냄
typedef struct STRUCT_PlotMsg
{
	UINT32					MsgId;				// IDD_MSG_ID_PLOT 또는 IDD_MSG_ID_END
	UINT32					MsgSize;			// 보내는 바이트 수
	UINT32					ScanNum;			// 스캔 번호
	UINT32					TimeStamp_s;		// 스캔 시각 (초)
	UINT32					TimeStamp_ns;		// 스캔 시각 (나노초)
	UINT32					PlotNum;			// 표적 플롯 + 클러터 수
	FLOAT32					Sigma_Range;		// R 표준편차: 거리 [m]
	FLOAT32					Sigma_Azimuth;		// 방위각 [rad]
	FLOAT32					Sigma_Elevation;	// 고각 [rad]
	STRUCT_PlatformInfo		st_Platform;		// 레이다 (플랫폼) 위치 · 자세
	STRUCT_PlotInfo			st_Plot[IDD_MAX_PLOT];	// PlotNum 개만 보냄
}STRUCT_PlotMsg;

#pragma pack()

#define IDD_MSG_HEAD_SIZE		((UINT32)(sizeof(STRUCT_PlotMsg) - (sizeof(STRUCT_PlotInfo) * IDD_MAX_PLOT)))	// 머리 크기 [B]

#endif
