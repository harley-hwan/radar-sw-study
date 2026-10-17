#ifndef IDD_H
#define IDD_H

// 참고 소스 코드의 Idd.h. STRUCT_PlatformInfo 와 STRUCT_PlotInfo 의 Y_Body 까지는 그대로 두고,
// 파일이 끊긴 뒤 (Z_Body 부터) 와 플롯 메시지 STRUCT_PlotMsg 를 이어 씀.
// 거리는 m, 각도는 rad. 메시지 안 값은 모두 송신 쪽 기준 (레이다 = 플랫폼).

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
	DOUBLE64				Z_Body;
	DOUBLE64				X_VcNed;
	DOUBLE64				Y_VcNed;
	DOUBLE64				Z_VcNed;
}STRUCT_PlotInfo;

#define IDD_MSG_ID_PLOT			0x5001				// 스캔 하나의 플롯 메시지
#define IDD_MSG_ID_END			0x50FF				// 보낼 스캔이 끝났다는 메시지 (플롯 없음)
#define IDD_MAX_PLOT			64					// 메시지 하나에 담는 플롯 최대 개수
#define IDD_PLOT_ID_MIN			10000
#define IDD_PLOT_ID_MAX			1000000

// 플롯 메시지. 송신 쪽은 안테나 구좌표 성분 (Range_Ant, Azimuth_Ant, Elevation_Ant) 과 머리 정보만 채움.
// 나머지 좌표 성분 (VcNed, 안테나 · 동체 직교) 은 받은 쪽 데이터 처리가 계산해 채움.
// 보낼 때는 머리 + 플롯 PlotNum 개만큼만 보냄 (MsgSize).
typedef struct STRUCT_PlotMsg
{
	UINT32					MsgId;
	UINT32					MsgSize;			// 보내는 바이트 수
	UINT32					ScanNum;
	UINT32					TimeStamp_s;		// 스캔 시각
	UINT32					TimeStamp_ns;
	UINT32					PlotNum;			// 표적 플롯 + 클러터
	FLOAT32					Sigma_Range;		// 측정 표준편차 [m]
	FLOAT32					Sigma_Azimuth;		// [rad]
	FLOAT32					Sigma_Elevation;	// [rad]
	STRUCT_PlatformInfo		st_Platform;
	STRUCT_PlotInfo			st_Plot[IDD_MAX_PLOT];
}STRUCT_PlotMsg;

#pragma pack()

#define IDD_MSG_HEAD_SIZE		((UINT32)(sizeof(STRUCT_PlotMsg) - (sizeof(STRUCT_PlotInfo) * IDD_MAX_PLOT)))

#endif
