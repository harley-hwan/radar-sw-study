// 플롯 메시지 UDP 송수신 (루프백). 송신 쓰레드 = 신호처리, 수신 쓰레드 = 데이터 처리 (트랙 개시 · 추적 필터 · 관리)
#ifndef PLOT_LINK_H
#define PLOT_LINK_H

#include "Idd.h"
#include "PlotSim.h"
#include "TrackMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LNK_IP					"127.0.0.1"				// 루프백
#define LNK_PORT				50505					// 쓰는 중이면 빈 포트를 받음
#define LNK_RECV_TIMEOUT_MS		100						// 수신 대기 한 번 [ms] (멈춤 확인 간격)
#define LNK_RECV_BUF_SIZE		(8 * 1024 * 1024)		// 수신 소켓 버퍼 [B]
#define LNK_MAX_AHEAD			32						// 처리 안 된 메시지가 이만큼이면 송신이 기다림 (UDP 손실 방지)
#define LNK_AHEAD_WAIT_MS		1000					// 송신이 기다리는 최대 시간 [ms]
#define LNK_END_WAIT_MS			1000					// 송신 끝난 뒤 이만큼 아무것도 안 오면 수신 종료 [ms]

// 받은 플롯 하나 (VC-NED)
typedef struct
{
	UINT32					PlotId;
	FLOAT64					pos[3];							// 북, 동, 아래 [m]
} ST_LnkPlot;

// 처리한 스캔 하나
typedef struct
{
	UINT32					ScanNum;
	FLOAT64					time;							// 스캔 시각 [s]
	UINT32					MsgSize;						// 받은 메시지 크기 [B]
	ST_TrkScanLog			st_Log;
	INT32					nTrackNum;						// 처리 뒤 트랙 수 (이번에 삭제 상태가 된 트랙 포함)
	INT32					nPlotNum;						// 받은 플롯 수
} ST_LnkScan;

// 송수신 진행 상태
typedef struct
{
	INT32					isRunning;						// 1 = 쓰레드 도는 중
	INT32					nScanNum;						// 보낼 스캔 수
	INT32					nSentNum;
	INT32					nRecvNum;
	INT32					nLostNum;						// 건너뛴 스캔 번호 수 (UDP 손실)
	INT32					nBadNum;						// 머리 검사에 걸린 메시지
	INT32					nProcNum;						// 처리를 마친 스캔 수
	INT32					sendGapMs;						// 스캔 사이 송신 간격 [ms]
	UINT16					port;							// 수신 포트
	INT32					errorCode;						// 소켓 오류 (0 = 정상)
	INT64					sentBytes;
	INT64					recvBytes;
	INT32					nCreatedSum;					// 누적: 만든 트랙
	INT32					nDeletedSum;					// 누적: 삭제 상태가 된 트랙
	INT32					nTableFullSum;					// 누적: 표가 차서 못 만든 트랙
	INT32					nDivergedSum;					// 누적: 발산 판정
} ST_LnkStatus;

TSCORE_API INT32	f_Lnk_Start(const ST_PcsScan *st_Scan, const ST_SimSample *st_Sample, INT32 nScanNum, INT32 sendGapMs, const ST_TrkParam *st_Param);
TSCORE_API VOID		f_Lnk_Stop(VOID);
TSCORE_API VOID		f_Lnk_GetStatus(ST_LnkStatus *st_Status);
TSCORE_API INT32	f_Lnk_CopyScan(INT32 nIndex, ST_LnkScan *st_Scan, ST_TrkView *st_Track, ST_LnkPlot *st_Plot);
TSCORE_API INT32	f_Lnk_GetPlotSource(UINT32 plotId);

#ifdef __cplusplus
}
#endif

#endif
