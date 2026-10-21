//
// @file	Idd.h
// @brief	플롯 메시지 ICD (신호처리 역할 송신 쓰레드 -> 데이터 처리 역할 수신 쓰레드)
//			참고 소스 코드의 Idd.h. STRUCT_PlatformInfo 와 STRUCT_PlotInfo 의 Y_Body 까지는 그대로 두고,
//			파일이 끊긴 뒤 (Z_Body 부터) 와 플롯 메시지 STRUCT_PlotMsg 를 이어 씀.
//			거리는 m, 각도는 rad. 메시지 안 값은 모두 송신 쪽 기준 (레이다 = 플랫폼).
// @author	hwan
// @date	2026.10.21.
//
#ifndef IDD_H
#define IDD_H

#include "matrixCalcLib.h"
#include "Define.h"

#pragma pack(1)

//
// @struct STRUCT_PlatformInfo
// @brief	플랫폼 (레이다) 위치, 자세 (참고 소스 코드 그대로)
//
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


//
// @struct STRUCT_PlotInfo
// @brief	플롯 하나 (참고 소스 코드 그대로). 송신 쪽은 안테나 구좌표 (Range_Ant, Azimuth_Ant, Elevation_Ant) 와 머리 정보만 채움
//
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

// @def IDD_MSG_ID_PLOT
// @brief 스캔 하나의 플롯 메시지 ID
#define IDD_MSG_ID_PLOT			0x5001

// @def IDD_MSG_ID_END
// @brief 보낼 스캔이 끝났다는 메시지 ID (플롯 없음)
#define IDD_MSG_ID_END			0x50FF

// @def IDD_MAX_PLOT
// @brief 메시지 하나에 담는 플롯 최대 개수
#define IDD_MAX_PLOT			64

// @def IDD_PLOT_ID_MIN
// @brief 플롯 ID 최소값 (과제 조건 10000 ~ 1000000)
#define IDD_PLOT_ID_MIN			10000

// @def IDD_PLOT_ID_MAX
// @brief 플롯 ID 최대값
#define IDD_PLOT_ID_MAX			1000000

//
// @struct STRUCT_PlotMsg
// @brief	플롯 메시지 (스캔 하나 = 메시지 하나). 표적 플롯과 클러터가 섞여 들어감 (어느 것이 표적인지는 싣지 않음).
//			송신 쪽은 안테나 구좌표 성분 (Range_Ant, Azimuth_Ant, Elevation_Ant) 과 머리 정보만 채움.
//			나머지 좌표 성분 (VcNed, 안테나 · 동체 직교) 은 받은 쪽 데이터 처리가 계산해 채움.
//			보낼 때는 머리 + 플롯 PlotNum 개만큼만 보냄 (MsgSize).
//
typedef struct STRUCT_PlotMsg
{
	UINT32					MsgId;				// @var IDD_MSG_ID_PLOT 또는 IDD_MSG_ID_END
	UINT32					MsgSize;			// @var 보내는 바이트 수
	UINT32					ScanNum;			// @var 스캔 번호
	UINT32					TimeStamp_s;		// @var 스캔 시각 (초)
	UINT32					TimeStamp_ns;		// @var 스캔 시각 (나노초)
	UINT32					PlotNum;			// @var 표적 플롯 + 클러터 수
	FLOAT32					Sigma_Range;		// @var 측정잡음 거리 표준편차 [m]. 트랙 초기화 R 구성용 (과제 조건 10 m)
	FLOAT32					Sigma_Azimuth;		// @var 측정잡음 방위각 표준편차 [rad] (과제 조건 0.2 도)
	FLOAT32					Sigma_Elevation;	// @var 측정잡음 고각 표준편차 [rad] (과제 조건 0.2 도)
	STRUCT_PlatformInfo		st_Platform;		// @var 스캔 시각의 플랫폼 (레이다) 정보
	STRUCT_PlotInfo			st_Plot[IDD_MAX_PLOT];	// @var 플롯 (PlotNum 개만 보냄)
}STRUCT_PlotMsg;

#pragma pack()

// @def IDD_MSG_HEAD_SIZE
// @brief 플롯 메시지 머리 크기 [B] (플롯 배열 앞까지)
#define IDD_MSG_HEAD_SIZE		((UINT32)(sizeof(STRUCT_PlotMsg) - (sizeof(STRUCT_PlotInfo) * IDD_MAX_PLOT)))

#endif
