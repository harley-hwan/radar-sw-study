#ifndef PLOT_LINK_H
#define PLOT_LINK_H

#include "Idd.h"
#include "PlotSim.h"
#include "TrackMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

// 플롯 메시지 UDP 송수신 (같은 PC 안 루프백).
// 송신 쓰레드 (신호처리 역할): 측정 모의 결과를 스캔마다 STRUCT_PlotMsg 하나로 만들어 보냄.
// 수신 쓰레드 (데이터 처리 역할): 메시지를 받아 VC-NED 로 바꾸고 트랙 개시 · 관리를 돌림. 결과는 스캔마다 쌓아 둠.
#define LNK_IP					"127.0.0.1"
#define LNK_PORT				50505					// 쓰는 중이면 빈 포트를 받음
#define LNK_RECV_TIMEOUT_MS		100						// 수신 대기 한 번의 최대 시간. 멈춤 확인 간격
#define LNK_RECV_BUF_SIZE		(8 * 1024 * 1024)		// 수신 소켓 버퍼 [B]. 메시지 600 개가 다 들어가는 크기
#define LNK_MAX_AHEAD			200						// 처리가 끝나지 않은 메시지가 이만큼이면 송신이 기다림
#define LNK_AHEAD_WAIT_MS		1000					// 기다리는 최대 시간 [ms]
#define LNK_END_WAIT_MS			1000					// 송신이 끝난 뒤 이만큼 아무것도 안 오면 수신 종료

// 받은 플롯 하나 (수신 쓰레드가 바꾼 값)
typedef struct
{
	UINT32					PlotId;
	FLOAT64					pos[3];							// VC-NED (북, 동, 아래) [m]
} ST_LnkPlot;

// 처리한 스캔 하나
typedef struct
{
	UINT32					ScanNum;
	FLOAT64					time;							// [s]
	UINT32					MsgSize;						// 받은 메시지 크기 [B]
	ST_TrkScanLog			st_Log;
	INT32					nTrackNum;						// 처리 뒤 트랙 표의 트랙 수 (이번에 삭제 상태가 된 트랙 포함)
	INT32					nPlotNum;
} ST_LnkScan;

// 진행 상태
typedef struct
{
	INT32					isRunning;						// 1 이면 쓰레드가 도는 중
	INT32					nScanNum;						// 보낼 스캔 수
	INT32					nSentNum;						// 보낸 메시지 수
	INT32					nRecvNum;						// 받은 플롯 메시지 수
	INT32					nLostNum;						// 스캔 번호가 건너뛴 수 (UDP 손실)
	INT32					nBadNum;						// 머리 검사에 걸린 메시지 수
	INT32					nProcNum;						// 처리를 마친 스캔 수 (결과 수)
	INT32					sendGapMs;						// 스캔 사이 송신 간격 [ms]
	UINT16					port;							// 수신 포트
	INT32					errorCode;						// 소켓 오류 번호. 0 이면 정상
	INT64					sentBytes;
	INT64					recvBytes;
	INT32					nCreatedSum;					// 만든 트랙 수
	INT32					nDeletedSum;					// 삭제 상태가 된 트랙 수
	INT32					nTableFullSum;					// 트랙 표가 차서 못 만든 수
} ST_LnkStatus;

TSCORE_API INT32	f_Lnk_Start(const ST_PcsScan *st_Scan, const ST_SimSample *st_Sample, INT32 nScanNum, const STRUCT_Coord_Sph *st_Sigma, INT32 sendGapMs);
TSCORE_API VOID		f_Lnk_Stop(VOID);
TSCORE_API VOID		f_Lnk_GetStatus(ST_LnkStatus *st_Status);
TSCORE_API INT32	f_Lnk_CopyScan(INT32 nIndex, ST_LnkScan *st_Scan, ST_TrkView *st_Track, ST_LnkPlot *st_Plot);
TSCORE_API INT32	f_Lnk_GetPlotSource(UINT32 plotId);

#ifdef __cplusplus
}
#endif

#endif
