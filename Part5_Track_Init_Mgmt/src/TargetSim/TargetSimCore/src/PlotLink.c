// 플롯 메시지 UDP 송수신 쓰레드. 송신 = 신호처리 (메시지 만들기), 수신 = 데이터 처리 (좌표 변환, R, 트랙 개시 · 관리)
// 소켓은 과제 1 IpcSocket 의 UDP 부분. winsock2.h, windows.h 는 Define.h 보다 먼저 include (INT32 등 #define 충돌)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "PlotLink.h"

#pragma comment(lib, "Ws2_32.lib")

// 헤더에 따라 안 나오는 SDK 가 있어서 직접 정의
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET		_WSAIOW(IOC_VENDOR, 12)
#endif

// 한 스캔의 플롯이 메시지 하나, 트랙 입력 하나에 다 들어가야 함
_Static_assert((TGT_MAX_TARGET_NUM + PCS_MAX_CLUTTER) <= IDD_MAX_PLOT, "IDD_MAX_PLOT is too small");
_Static_assert(IDD_MAX_PLOT <= TRK_MAX_PLOT, "TRK_MAX_PLOT is too small");

// 과제 1 IpcSocket (SocketUtility Winsock2 포팅) 의 UDP 부분
typedef uintptr_t				SOCKET_HANDLE;
#define SOCKET_INVALID_HANDLE	((SOCKET_HANDLE)~(uintptr_t)0)
#define SOCKET_PASS				1
#define SOCKET_FAIL				0
#define SOCKET_UDP_MAX			65507					// UDP 데이터그램 최대 크기 [B]

enum
{
	enum_Socket_Status_Disconnected	= -1,
	enum_Socket_Status_NotDefined	= 0					// UDP 는 늘 이 값
};

enum
{
	enum_Socket_Type_UDP_IPv4Tx		= 1,
	enum_Socket_Type_UDP_IPv4Rx		= 2
};

// 소켓 정보 (과제 1 IpcSocket 의 ST_Socket)
typedef struct
{
	SOCKET_HANDLE			hSockId;
	INT32					iSockStatus;
	INT32					iSockType;
	UINT32					uiSockAddrSize;
	struct sockaddr_in		stSockAddr;						// Tx 는 보낼 곳, Rx 는 자기 bind 주소
	struct timeval			stTimeOutVal;
} ST_Socket;

// 처리한 스캔 기록 + 결과 배열에서 트랙, 플롯 시작 자리
typedef struct
{
	ST_LnkScan				st_Scan;
	INT32					nTrackStart;
	INT32					nPlotStart;
} ST_LnkScanRec;

// 송수신 한 번 (f_Lnk_Start ~ f_Lnk_Stop) 의 상태. st_Lock 으로 보호
typedef struct
{
	CRITICAL_SECTION		st_Lock;
	INT32					isLockReady;
	INT32					isWsaReady;
	HANDLE					hStop;							// 멈춤 (수동 리셋)
	HANDLE					hReady;							// 수신 소켓 bind 끝 (수동 리셋)
	HANDLE					hTxThread;
	HANDLE					hRxThread;
	ST_LnkStatus			st_Status;
	INT32					isTxDone;

	// 송신 입력 (복사본)
	ST_PcsScan				*st_InScan;
	ST_TargetState			*st_InRadar;					// 스캔마다 레이다 (플랫폼) 상태
	STRUCT_PlotMsg			st_TxMsg;

	// 송신 쪽 참값 기록 (플롯 ID 순서마다 0 = 클러터, k = 표적 k)
	INT32					*pt_PlotSource;
	INT32					nPlotSourceNum;
	INT32					nPlotSourceCap;

	// 수신 쪽 데이터 처리
	STRUCT_PlotMsg			st_RxMsg;
	UCHAR8					pt_RxBuf[SOCKET_UDP_MAX];
	ST_TrkTable				st_Table;
	INT32					nLastScan;						// 마지막으로 받은 스캔 번호. -1 이면 아직 없음
	STRUCT_PlatformInfo		st_PrevPlatform;				// 앞 스캔 메시지의 플랫폼 정보
	INT32					isPrevPlatform;

	// 결과
	ST_LnkScanRec			*st_ResScan;					// 스캔 수만큼
	ST_TrkView				*st_ResTrack;
	INT32					nResTrackNum;
	INT32					nResTrackCap;
	ST_LnkPlot				*st_ResPlot;					// 스캔 수 x IDD_MAX_PLOT
	INT32					nResPlotNum;
} ST_LnkContext;

static ST_LnkContext s_Link;

// 첫 소켓 오류 번호를 남김 (이미 있으면 그대로)
static VOID f_Lnk_SetError(INT32 errorCode)
{
	EnterCriticalSection(&s_Link.st_Lock);

	if (s_Link.st_Status.errorCode == 0)
	{
		s_Link.st_Status.errorCode = errorCode;
	}

	LeaveCriticalSection(&s_Link.st_Lock);
}

// 소켓 (과제 1 IpcSocket.c 의 UDP 부분)

// 열지 않은 (무효) 소켓 정보
static ST_Socket f_MakeInvalidSocket(const INT32 iSockType)
{
	ST_Socket st_Socket;

	(VOID)memset(&st_Socket, 0, sizeof(st_Socket));
	st_Socket.hSockId			= SOCKET_INVALID_HANDLE;
	st_Socket.iSockStatus		= enum_Socket_Status_Disconnected;
	st_Socket.iSockType			= iSockType;
	st_Socket.uiSockAddrSize	= (UINT32)sizeof(struct sockaddr_in);

	return st_Socket;
}

// IPv4 주소 구조체 채우기. 주소가 비어 있으면 INADDR_ANY
static INT32 f_FillSockAddr(struct sockaddr_in *stpAddr, const CHAR *cpIpAddr, const UINT16 usPortNum)
{
	INT32 iResult = SOCKET_PASS;

	(VOID)memset(stpAddr, 0, sizeof(*stpAddr));
	stpAddr->sin_family	= AF_INET;
	stpAddr->sin_port	= htons(usPortNum);

	if ((cpIpAddr == NULL) || (cpIpAddr[0] == '\0'))
	{
		stpAddr->sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else if (inet_pton(AF_INET, cpIpAddr, &stpAddr->sin_addr) != 1)
	{
		iResult = SOCKET_FAIL;
	}
	else
	{
		// 주소 변환 성공
	}

	return iResult;
}

// 상대 포트가 없을 때 recvfrom 이 10054 로 깨지는 Windows 동작 끔
static VOID f_DisableUdpConnReset(const SOCKET hSock)
{
	BOOL	bNewBehavior = FALSE;
	DWORD	dwBytes = 0;

	(VOID)WSAIoctl(hSock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytes, NULL, NULL);
}

// UDP 송신 소켓 열기. 주소는 보낼 곳
static ST_Socket f_SocketInitUDP_IPv4Tx(const CHAR *cpIpAddr, const UINT16 usPortNum)
{
	ST_Socket	st_Socket = f_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Tx);
	SOCKET		hSock = INVALID_SOCKET;

	if (f_FillSockAddr(&st_Socket.stSockAddr, cpIpAddr, usPortNum) == SOCKET_PASS)
	{
		hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

	if (hSock == INVALID_SOCKET)
	{
		f_Lnk_SetError(WSAGetLastError());
	}
	else
	{
		f_DisableUdpConnReset(hSock);
		st_Socket.hSockId		= (SOCKET_HANDLE)hSock;
		st_Socket.iSockStatus	= enum_Socket_Status_NotDefined;
	}

	return st_Socket;
}

// UDP 수신 소켓 열기: bind, 수신 타임아웃 [ms]
static ST_Socket f_SocketInitUDP_IPv4Rx(const CHAR *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
	ST_Socket	st_Socket = f_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Rx);
	SOCKET		hSock = INVALID_SOCKET;
	DWORD		dwTimeOut_ms;

	if (f_FillSockAddr(&st_Socket.stSockAddr, cpIpAddr, usPortNum) == SOCKET_PASS)
	{
		hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

	if (hSock != INVALID_SOCKET)
	{
		f_DisableUdpConnReset(hSock);

		if ((lTimeOut_s > 0) || (lTimeOut_us > 0))
		{
			dwTimeOut_ms = (DWORD)((lTimeOut_s * 1000) + (lTimeOut_us / 1000));
			dwTimeOut_ms = (dwTimeOut_ms == 0U) ? 1U : dwTimeOut_ms;
			(VOID)setsockopt(hSock, SOL_SOCKET, SO_RCVTIMEO, (const CHAR *)&dwTimeOut_ms, (INT32)sizeof(dwTimeOut_ms));
		}

		if (bind(hSock, (const struct sockaddr *)&st_Socket.stSockAddr, (INT32)sizeof(st_Socket.stSockAddr)) == SOCKET_ERROR)
		{
			(VOID)closesocket(hSock);
			hSock = INVALID_SOCKET;
		}
		else
		{
			st_Socket.hSockId				= (SOCKET_HANDLE)hSock;
			st_Socket.iSockStatus			= enum_Socket_Status_NotDefined;
			st_Socket.stTimeOutVal.tv_sec	= (LONG)lTimeOut_s;
			st_Socket.stTimeOutVal.tv_usec	= (LONG)lTimeOut_us;
		}
	}

	return st_Socket;
}

// UDP 송신 (sendto)
static INT64 f_SocketSendUDP_IPv4_Normal(const ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lDataSize)
{
	INT64	lResult = -1;
	INT32	iSent;

	if ((stpSocket->hSockId != SOCKET_INVALID_HANDLE) && (vpDataAddr != NULL) && (lDataSize > 0) && (lDataSize <= SOCKET_UDP_MAX))
	{
		iSent = sendto((SOCKET)stpSocket->hSockId, (const CHAR *)vpDataAddr, (INT32)lDataSize, 0, (const struct sockaddr *)&stpSocket->stSockAddr,
			(INT32)sizeof(stpSocket->stSockAddr));

		if (iSent == SOCKET_ERROR)
		{
			f_Lnk_SetError(WSAGetLastError());
		}
		else
		{
			lResult = (INT64)iSent;
		}
	}

	return lResult;
}

// UDP 수신 (recvfrom). 타임아웃은 오류로 남기지 않음
static INT64 f_SocketRecvUDP_IPv4_Normal(const ST_Socket *stpSocket, VOID *vpDataAddr, const INT64 lMaxSize)
{
	INT64	lResult = -1;
	INT32	iRecv;
	INT32	iError;

	if ((stpSocket->hSockId != SOCKET_INVALID_HANDLE) && (vpDataAddr != NULL) && (lMaxSize > 0))
	{
		iRecv = recvfrom((SOCKET)stpSocket->hSockId, (CHAR *)vpDataAddr, (INT32)lMaxSize, 0, NULL, NULL);

		if (iRecv == SOCKET_ERROR)
		{
			iError = WSAGetLastError();

			if (iError != WSAETIMEDOUT)
			{
				f_Lnk_SetError(iError);
			}
		}
		else
		{
			lResult = (INT64)iRecv;
		}
	}

	return lResult;
}

// 소켓 닫기
static INT32 f_SocketClose(const ST_Socket st_Socket)
{
	INT32 iResult = SOCKET_FAIL;

	if (st_Socket.hSockId != SOCKET_INVALID_HANDLE)
	{
		iResult = (closesocket((SOCKET)st_Socket.hSockId) == 0) ? SOCKET_PASS : SOCKET_FAIL;
	}

	return iResult;
}

// 송신 (신호처리 역할)

// 스캔 하나의 플롯 메시지: 표적 + 클러터를 방위각 순서로 담고 플롯 ID 부여. 안테나 구좌표와 R 표준편차만 채움
// 어느 플롯이 표적인지는 메시지에 넣지 않고 송신 쪽 기록 (화면 확인용) 에만 남김
static UINT32 f_Lnk_BuildMsg(STRUCT_PlotMsg *st_Msg, INT32 nScan, UINT32 *pt_PlotId)
{
	const ST_PcsScan		*st_Scan = &s_Link.st_InScan[nScan];
	const ST_TargetState	*st_Radar = &s_Link.st_InRadar[nScan];
	const INT64				timeNs = llround(st_Scan->time * 1.0e9);
	STRUCT_Coord_Sph		st_Sigma;
	STRUCT_Coord_Sph		st_Sph[IDD_MAX_PLOT];
	INT32					nSource[IDD_MAX_PLOT];
	INT32					nOrder[IDD_MAX_PLOT];
	STRUCT_PlotInfo			*st_Info;
	INT32					nPlotNum = 0;
	INT32					nIndex;
	INT32					nSort;
	INT32					nKey;
	UINT32					msgSize;

	for (nIndex = 0; (nIndex < st_Scan->nTargetNum) && (nPlotNum < IDD_MAX_PLOT); nIndex++)
	{
		st_Sph[nPlotNum]	= st_Scan->st_Plot[nIndex];
		nSource[nPlotNum]	= nIndex + 1;
		nPlotNum++;
	}

	for (nIndex = 0; (nIndex < st_Scan->nClutterNum) && (nPlotNum < IDD_MAX_PLOT); nIndex++)
	{
		st_Sph[nPlotNum]	= st_Scan->st_Clutter[nIndex];
		nSource[nPlotNum]	= 0;
		nPlotNum++;
	}

	// 방위각 오름차순 (삽입 정렬)
	for (nIndex = 0; nIndex < nPlotNum; nIndex++)
	{
		nKey	= nIndex;
		nSort	= nIndex - 1;

		while ((nSort >= 0) && (st_Sph[nOrder[nSort]].az > st_Sph[nKey].az))
		{
			nOrder[nSort + 1] = nOrder[nSort];
			nSort--;
		}

		nOrder[nSort + 1] = nKey;
	}

	msgSize = IDD_MSG_HEAD_SIZE + ((UINT32)nPlotNum * (UINT32)sizeof(STRUCT_PlotInfo));
	(VOID)memset(st_Msg, 0, msgSize);

	st_Msg->MsgId					= IDD_MSG_ID_PLOT;
	st_Msg->MsgSize					= msgSize;
	st_Msg->ScanNum					= (UINT32)nScan;
	st_Msg->TimeStamp_s				= (UINT32)(timeNs / 1000000000LL);
	st_Msg->TimeStamp_ns			= (UINT32)(timeNs % 1000000000LL);
	st_Msg->PlotNum					= (UINT32)nPlotNum;

	// R 표준편차 (과제 값)
	f_Trk_GetMeasSigma(&st_Sigma);
	st_Msg->Sigma_Range				= (FLOAT32)st_Sigma.r;
	st_Msg->Sigma_Azimuth			= (FLOAT32)st_Sigma.az;
	st_Msg->Sigma_Elevation			= (FLOAT32)st_Sigma.el;

	st_Msg->st_Platform.Pf_Lat		= (FLOAT32)st_Radar->st_Lla.Lat;
	st_Msg->st_Platform.Pf_Lon		= (FLOAT32)st_Radar->st_Lla.Lon;
	st_Msg->st_Platform.Pf_Alt		= (FLOAT32)st_Radar->st_Lla.Alt;
	st_Msg->st_Platform.Pf_EcefX	= (FLOAT32)st_Radar->st_PosEcef.x;
	st_Msg->st_Platform.Pf_EcefY	= (FLOAT32)st_Radar->st_PosEcef.y;
	st_Msg->st_Platform.Pf_EcefZ	= (FLOAT32)st_Radar->st_PosEcef.z;
	st_Msg->st_Platform.Pf_Roll		= (FLOAT32)st_Radar->st_Att.Roll;
	st_Msg->st_Platform.Pf_Yaw		= (FLOAT32)st_Radar->st_Att.Yaw;
	st_Msg->st_Platform.Pf_Pitch	= (FLOAT32)st_Radar->st_Att.Pitch;

	for (nIndex = 0; nIndex < nPlotNum; nIndex++)
	{
		st_Info							= &st_Msg->st_Plot[nIndex];
		st_Info->Id						= *pt_PlotId;
		st_Info->EmittedDwellNum		= (UINT32)nScan;		// 스캔당 드웰 하나
		st_Info->EmittedScanNum			= (UINT32)nScan;
		st_Info->EmittedBeamNum			= 0;
		st_Info->EmittedTimeStamp_s		= st_Msg->TimeStamp_s;
		st_Info->EmittedTimeStamp_ns	= st_Msg->TimeStamp_ns;
		st_Info->BeamType				= 0;					// 탐색 빔
		st_Info->Range_Ant				= (FLOAT32)st_Sph[nOrder[nIndex]].r;
		st_Info->Azimuth_Ant			= (FLOAT32)st_Sph[nOrder[nIndex]].az;
		st_Info->Elevation_Ant			= (FLOAT32)st_Sph[nOrder[nIndex]].el;

		EnterCriticalSection(&s_Link.st_Lock);

		if (s_Link.nPlotSourceNum < s_Link.nPlotSourceCap)
		{
			s_Link.pt_PlotSource[s_Link.nPlotSourceNum] = nSource[nOrder[nIndex]];
			s_Link.nPlotSourceNum++;
		}

		LeaveCriticalSection(&s_Link.st_Lock);

		// 플롯 ID 는 10000 ~ 1000000 을 돌아가며 씀
		*pt_PlotId = (*pt_PlotId >= IDD_PLOT_ID_MAX) ? IDD_PLOT_ID_MIN : (*pt_PlotId + 1U);
	}

	return msgSize;
}

// 처리 안 된 메시지가 LNK_MAX_AHEAD 개 이상이면 기다림 (UDP 는 버퍼가 차면 소리 없이 버림)
static INT32 f_Lnk_WaitReceiver(VOID)
{
	const ULONGLONG	endTick = GetTickCount64() + LNK_AHEAD_WAIT_MS;
	INT32			isStop = 0;
	INT32			isBehind = 1;

	while ((isStop == 0) && (isBehind != 0) && (GetTickCount64() < endTick))
	{
		EnterCriticalSection(&s_Link.st_Lock);
		isBehind = ((s_Link.st_Status.nSentNum - (s_Link.st_Status.nProcNum + s_Link.st_Status.nLostNum)) >= LNK_MAX_AHEAD) ? 1 : 0;
		LeaveCriticalSection(&s_Link.st_Lock);

		if ((isBehind != 0) && (WaitForSingleObject(s_Link.hStop, 1) == WAIT_OBJECT_0))
		{
			isStop = 1;
		}
	}

	return isStop;
}

// 송신 쓰레드: 수신이 준비되면 스캔마다 메시지 하나, 다 보내면 끝 메시지
static UINT32 __stdcall f_Lnk_TxThread(VOID *pt_Arg)
{
	const HANDLE	hWait[2] = { s_Link.hStop, s_Link.hReady };
	ST_Socket		st_Socket = f_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Tx);
	STRUCT_PlotMsg	*st_Msg = &s_Link.st_TxMsg;
	UINT32			plotId = IDD_PLOT_ID_MIN;
	UINT32			msgSize;
	UINT16			port;
	INT64			lSent;
	INT32			isStop;
	INT32			nScan;
	ULONGLONG		startTick;
	ULONGLONG		dueTick;
	ULONGLONG		nowTick;

	(VOID)pt_Arg;

	// 수신 쪽이 bind 하기 전에 보내면 버려짐
	isStop		= (WaitForMultipleObjects(2, hWait, FALSE, INFINITE) == WAIT_OBJECT_0) ? 1 : 0;
	startTick	= GetTickCount64();

	EnterCriticalSection(&s_Link.st_Lock);
	port = s_Link.st_Status.port;
	LeaveCriticalSection(&s_Link.st_Lock);

	if ((isStop == 0) && (port != 0U))
	{
		st_Socket = f_SocketInitUDP_IPv4Tx(LNK_IP, port);
	}

	for (nScan = 0; (nScan < s_Link.st_Status.nScanNum) && (isStop == 0) && (st_Socket.hSockId != SOCKET_INVALID_HANDLE); nScan++)
	{
		isStop = f_Lnk_WaitReceiver();

		if (isStop == 0)
		{
			msgSize	= f_Lnk_BuildMsg(st_Msg, nScan, &plotId);

			// 루프백은 sendto 가 끝나기 전에 받을 수 있어 보낸 수를 먼저 셈 (실패하면 되돌림)
			EnterCriticalSection(&s_Link.st_Lock);
			s_Link.st_Status.nSentNum++;
			LeaveCriticalSection(&s_Link.st_Lock);

			lSent	= f_SocketSendUDP_IPv4_Normal(&st_Socket, st_Msg, (INT64)msgSize);

			EnterCriticalSection(&s_Link.st_Lock);

			if (lSent == (INT64)msgSize)
			{
				s_Link.st_Status.sentBytes = s_Link.st_Status.sentBytes + lSent;
			}
			else
			{
				s_Link.st_Status.nSentNum--;
			}

			LeaveCriticalSection(&s_Link.st_Lock);

			// 다음 송신 시각 = 시작 + (스캔 + 1) x 간격 (대기가 길어져도 밀리지 않게)
			if (s_Link.st_Status.sendGapMs > 0)
			{
				dueTick		= startTick + ((ULONGLONG)(nScan + 1) * (ULONGLONG)s_Link.st_Status.sendGapMs);
				nowTick		= GetTickCount64();
				isStop		= ((dueTick > nowTick) && (WaitForSingleObject(s_Link.hStop, (DWORD)(dueTick - nowTick)) == WAIT_OBJECT_0)) ? 1 : 0;
			}
		}
	}

	// 끝 메시지 (플롯 없음)
	if ((isStop == 0) && (st_Socket.hSockId != SOCKET_INVALID_HANDLE))
	{
		(VOID)memset(st_Msg, 0, IDD_MSG_HEAD_SIZE);
		st_Msg->MsgId	= IDD_MSG_ID_END;
		st_Msg->MsgSize	= IDD_MSG_HEAD_SIZE;
		st_Msg->ScanNum	= (UINT32)s_Link.st_Status.nScanNum;
		(VOID)f_SocketSendUDP_IPv4_Normal(&st_Socket, st_Msg, (INT64)IDD_MSG_HEAD_SIZE);
	}

	(VOID)f_SocketClose(st_Socket);

	EnterCriticalSection(&s_Link.st_Lock);
	s_Link.isTxDone = 1;
	LeaveCriticalSection(&s_Link.st_Lock);

	return 0U;
}

// 수신 (데이터 처리 역할)

// 머리 검사. 스캔 번호가 범위 밖이면 잘못된 메시지
static INT32 f_Lnk_CheckMsg(const STRUCT_PlotMsg *st_Msg, INT64 lRecv)
{
	const UINT32	scanTotal = (UINT32)s_Link.st_Status.nScanNum;
	INT32			nKind = 0;

	if (lRecv < (INT64)IDD_MSG_HEAD_SIZE)
	{
		// 머리보다 짧음
	}
	else if (st_Msg->MsgSize != (UINT32)lRecv)
	{
		// 크기 불일치
	}
	else if ((st_Msg->MsgId == IDD_MSG_ID_END) && (st_Msg->ScanNum == scanTotal))
	{
		nKind = 2;
	}
	else if ((st_Msg->MsgId == IDD_MSG_ID_PLOT) && (st_Msg->ScanNum < scanTotal) && (st_Msg->PlotNum <= IDD_MAX_PLOT)
		&& (st_Msg->MsgSize == (IDD_MSG_HEAD_SIZE + (st_Msg->PlotNum * (UINT32)sizeof(STRUCT_PlotInfo)))))
	{
		nKind = 1;
	}
	else
	{
		// 알 수 없는 메시지
	}

	return nKind;
}

// 좌표 칸 채우기: 안테나 구좌표 -> 안테나 직교 -> 동체 -> VC-NED (방위각 북에서 동 +, 고각 위 +)
static VOID f_Lnk_FillPlotCoord(STRUCT_PlotInfo *st_Info, const STRUCT_PlatformInfo *st_Platform)
{
	STRUCT_Coord_Rect	st_Ant;
	STRUCT_Coord_Rect	st_Body;
	STRUCT_Coord_Rect	st_Ned;
	FLOAT64				horizontal;

	st_Ant	= f_Trans_Ant_Sph_To_XYZ(st_Info->Range_Ant, st_Info->Azimuth_Ant, st_Info->Elevation_Ant);
	st_Body	= f_Trans_Ant_To_Body(st_Ant.x, st_Ant.y, st_Ant.z);
	st_Ned	= f_Trans_Body_To_Ned(st_Body.x, st_Body.y, st_Body.z, st_Platform->Pf_Roll, st_Platform->Pf_Yaw, st_Platform->Pf_Pitch);

	st_Info->X_Ant		= st_Ant.x;
	st_Info->Y_Ant		= st_Ant.y;
	st_Info->Z_Ant		= st_Ant.z;
	st_Info->X_Body		= st_Body.x;
	st_Info->Y_Body		= st_Body.y;
	st_Info->Z_Body		= st_Body.z;
	st_Info->X_VcNed	= st_Ned.x;
	st_Info->Y_VcNed	= st_Ned.y;
	st_Info->Z_VcNed	= st_Ned.z;

	horizontal					= sqrt((st_Ned.x * st_Ned.x) + (st_Ned.y * st_Ned.y));
	st_Info->Range_VcNed		= (FLOAT32)sqrt((horizontal * horizontal) + (st_Ned.z * st_Ned.z));
	st_Info->Azimuth_VcNed		= (FLOAT32)atan2(st_Ned.y, st_Ned.x);
	st_Info->Elevation_VcNed	= (FLOAT32)atan2(-st_Ned.z, horizontal);
}

// 결과 트랙 배열이 모자라면 두 배로 늘림. 잠금 안에서 부름
static INT32 f_Lnk_ReserveTrack(INT32 nAddNum)
{
	ST_TrkView	*st_New;
	INT32		nNewCap = s_Link.nResTrackCap;
	INT32		isOk = 1;

	while ((s_Link.nResTrackNum + nAddNum) > nNewCap)
	{
		nNewCap = nNewCap * 2;
	}

	if (nNewCap != s_Link.nResTrackCap)
	{
		st_New = (ST_TrkView *)realloc(s_Link.st_ResTrack, (size_t)nNewCap * sizeof(ST_TrkView));

		if (st_New == NULL)
		{
			isOk = 0;
		}
		else
		{
			s_Link.st_ResTrack	= st_New;
			s_Link.nResTrackCap	= nNewCap;
		}
	}

	return isOk;
}

// 플롯 메시지 하나 처리: 좌표 변환, R 계산, 트랙 개시 · 관리, 결과 쌓기
static VOID f_Lnk_ProcessMsg(STRUCT_PlotMsg *st_Msg)
{
	const FLOAT64			time = (FLOAT64)st_Msg->TimeStamp_s + ((FLOAT64)st_Msg->TimeStamp_ns * 1.0e-9);
	STRUCT_Coord_Sph		st_Sigma;
	STRUCT_Coord_Sph		st_Meas;
	STRUCT_Coord_Attitude	st_Att;
	ST_TrkPlot				st_Plot[IDD_MAX_PLOT];
	ST_TrkScanLog			st_Log;
	ST_LnkScanRec			*st_Rec;
	STRUCT_PlotInfo			*st_Info;
	STRUCT_Coord_Rect		st_Shift;
	FLOAT64					shift[3];
	const INT32				nPlotNum = (INT32)st_Msg->PlotNum;
	INT32					nScanIndex;
	INT32					nPlot;
	INT32					nTrack;

	// R 표준편차 (메시지에 실려 온 과제 값)
	st_Sigma.r	= st_Msg->Sigma_Range;
	st_Sigma.az	= st_Msg->Sigma_Azimuth;
	st_Sigma.el	= st_Msg->Sigma_Elevation;
	st_Att.Roll		= st_Msg->st_Platform.Pf_Roll;
	st_Att.Pitch	= st_Msg->st_Platform.Pf_Pitch;
	st_Att.Yaw		= st_Msg->st_Platform.Pf_Yaw;

	for (nPlot = 0; nPlot < nPlotNum; nPlot++)
	{
		st_Info = &st_Msg->st_Plot[nPlot];
		f_Lnk_FillPlotCoord(st_Info, &st_Msg->st_Platform);

		st_Meas.r	= st_Info->Range_Ant;
		st_Meas.az	= st_Info->Azimuth_Ant;
		st_Meas.el	= st_Info->Elevation_Ant;

		st_Plot[nPlot].PlotId	= st_Info->Id;
		st_Plot[nPlot].time		= time;
		st_Plot[nPlot].pos[0]	= st_Info->X_VcNed;
		st_Plot[nPlot].pos[1]	= st_Info->Y_VcNed;
		st_Plot[nPlot].pos[2]	= st_Info->Z_VcNed;
		st_Plot[nPlot].isUsed	= 0;
		f_Trk_MeasCov(st_Plot[nPlot].cov, &st_Meas, &st_Sigma, &st_Att);
	}

	// 플랫폼이 움직인 만큼 앞 스캔 값을 이번 VC-NED 로 옮김
	if (s_Link.isPrevPlatform != 0)
	{
		st_Shift = f_Trans_Ecef_To_Ned(s_Link.st_PrevPlatform.Pf_EcefX, s_Link.st_PrevPlatform.Pf_EcefY, s_Link.st_PrevPlatform.Pf_EcefZ,
			st_Msg->st_Platform.Pf_EcefX, st_Msg->st_Platform.Pf_EcefY, st_Msg->st_Platform.Pf_EcefZ, st_Msg->st_Platform.Pf_Lat, st_Msg->st_Platform.Pf_Lon);
		shift[0] = st_Shift.x;
		shift[1] = st_Shift.y;
		shift[2] = st_Shift.z;
		f_Trk_MoveOrigin(&s_Link.st_Table, shift);
	}

	s_Link.st_PrevPlatform	= st_Msg->st_Platform;
	s_Link.isPrevPlatform	= 1;

	f_Trk_ProcessScan(&s_Link.st_Table, st_Plot, nPlotNum, time, (INT32)st_Msg->ScanNum, &st_Log);

	// 결과 쌓기
	EnterCriticalSection(&s_Link.st_Lock);

	nScanIndex = s_Link.st_Status.nProcNum;

	if ((nScanIndex < s_Link.st_Status.nScanNum) && (f_Lnk_ReserveTrack(s_Link.st_Table.nTrackNum) != 0))
	{
		st_Rec						= &s_Link.st_ResScan[nScanIndex];
		st_Rec->st_Scan.ScanNum		= st_Msg->ScanNum;
		st_Rec->st_Scan.time		= time;
		st_Rec->st_Scan.MsgSize		= st_Msg->MsgSize;
		st_Rec->st_Scan.st_Log		= st_Log;
		st_Rec->st_Scan.nTrackNum	= s_Link.st_Table.nTrackNum;
		st_Rec->st_Scan.nPlotNum	= nPlotNum;
		st_Rec->nTrackStart			= s_Link.nResTrackNum;
		st_Rec->nPlotStart			= s_Link.nResPlotNum;

		for (nTrack = 0; nTrack < s_Link.st_Table.nTrackNum; nTrack++)
		{
			f_Trk_GetView(&s_Link.st_ResTrack[s_Link.nResTrackNum], &s_Link.st_Table.st_Track[nTrack]);
			s_Link.nResTrackNum++;
		}

		for (nPlot = 0; nPlot < nPlotNum; nPlot++)
		{
			s_Link.st_ResPlot[s_Link.nResPlotNum].PlotId	= st_Plot[nPlot].PlotId;
			s_Link.st_ResPlot[s_Link.nResPlotNum].pos[0]	= st_Plot[nPlot].pos[0];
			s_Link.st_ResPlot[s_Link.nResPlotNum].pos[1]	= st_Plot[nPlot].pos[1];
			s_Link.st_ResPlot[s_Link.nResPlotNum].pos[2]	= st_Plot[nPlot].pos[2];
			s_Link.nResPlotNum++;
		}

		s_Link.st_Status.nProcNum++;
	}

	s_Link.st_Status.nCreatedSum	= s_Link.st_Table.nCreatedSum;
	s_Link.st_Status.nDeletedSum	= s_Link.st_Table.nDeletedSum;
	s_Link.st_Status.nTableFullSum	= s_Link.st_Table.nTableFullSum;

	LeaveCriticalSection(&s_Link.st_Lock);
}

// 받은 데이터그램 하나 처리. 스캔 번호가 건너뛰면 그만큼 손실로 셈
static INT32 f_Lnk_OnRecv(INT64 lRecv)
{
	STRUCT_PlotMsg	*st_Msg = &s_Link.st_RxMsg;
	INT32			isEnd = 0;
	INT32			nKind;
	INT32			nGap;

	(VOID)memcpy(st_Msg, s_Link.pt_RxBuf, ((size_t)lRecv < sizeof(STRUCT_PlotMsg)) ? (size_t)lRecv : sizeof(STRUCT_PlotMsg));
	nKind = f_Lnk_CheckMsg(st_Msg, lRecv);

	EnterCriticalSection(&s_Link.st_Lock);
	s_Link.st_Status.recvBytes = s_Link.st_Status.recvBytes + lRecv;

	if (nKind == 1)
	{
		// 스캔 번호가 건너뛰면 그만큼 잃은 것
		nGap = (INT32)st_Msg->ScanNum - (s_Link.nLastScan + 1);

		if (nGap < 0)
		{
			nKind = 0;
		}
		else
		{
			s_Link.st_Status.nLostNum	= s_Link.st_Status.nLostNum + nGap;
			s_Link.st_Status.nRecvNum++;
			s_Link.nLastScan			= (INT32)st_Msg->ScanNum;
		}
	}
	else if (nKind == 2)
	{
		// 끝 메시지 앞에서 빠진 스캔
		nGap = (INT32)st_Msg->ScanNum - (s_Link.nLastScan + 1);
		s_Link.st_Status.nLostNum = s_Link.st_Status.nLostNum + ((nGap > 0) ? nGap : 0);
		isEnd = 1;
	}
	else
	{
		// 잘못된 메시지는 아래에서 셈
	}

	if (nKind == 0)
	{
		s_Link.st_Status.nBadNum++;
	}

	LeaveCriticalSection(&s_Link.st_Lock);

	if (nKind == 1)
	{
		f_Lnk_ProcessMsg(st_Msg);
	}

	return isEnd;
}

// 수신 쓰레드: 끝 메시지가 오거나 멈출 때까지 받아 처리 (대기 시간 제한으로 멈춤 신호 확인)
static UINT32 __stdcall f_Lnk_RxThread(VOID *pt_Arg)
{
	const INT32				rcvBufSize = LNK_RECV_BUF_SIZE;
	ST_Socket				st_Socket;
	struct sockaddr_in		st_Addr;
	INT32					addrSize = (INT32)sizeof(st_Addr);
	INT64					lRecv;
	INT32					isEnd = 0;
	INT32					isTxDone;
	INT32					idleMs = 0;

	(VOID)pt_Arg;

	st_Socket = f_SocketInitUDP_IPv4Rx(LNK_IP, LNK_PORT, 0, (INT64)LNK_RECV_TIMEOUT_MS * 1000);

	// 포트를 다른 프로그램이 쓰고 있으면 빈 포트로
	if (st_Socket.hSockId == SOCKET_INVALID_HANDLE)
	{
		st_Socket = f_SocketInitUDP_IPv4Rx(LNK_IP, 0, 0, (INT64)LNK_RECV_TIMEOUT_MS * 1000);
	}

	// 오류 번호가 남아 있지 않으면 (bind 실패 뒤 closesocket 이 지움) -1
	if (st_Socket.hSockId == SOCKET_INVALID_HANDLE)
	{
		f_Lnk_SetError((WSAGetLastError() != 0) ? WSAGetLastError() : -1);
		isEnd = 1;
	}
	else
	{
		(VOID)setsockopt((SOCKET)st_Socket.hSockId, SOL_SOCKET, SO_RCVBUF, (const CHAR *)&rcvBufSize, (INT32)sizeof(rcvBufSize));
		(VOID)getsockname((SOCKET)st_Socket.hSockId, (struct sockaddr *)&st_Addr, &addrSize);

		EnterCriticalSection(&s_Link.st_Lock);
		s_Link.st_Status.port = ntohs(st_Addr.sin_port);
		LeaveCriticalSection(&s_Link.st_Lock);
	}

	// 실패해도 알림 (송신 쪽은 포트 0 을 보고 끝냄)
	(VOID)SetEvent(s_Link.hReady);

	while (isEnd == 0)
	{
		if (WaitForSingleObject(s_Link.hStop, 0) == WAIT_OBJECT_0)
		{
			isEnd = 1;
		}
		else
		{
			lRecv = f_SocketRecvUDP_IPv4_Normal(&st_Socket, s_Link.pt_RxBuf, (INT64)sizeof(s_Link.pt_RxBuf));

			if (lRecv > 0)
			{
				idleMs	= 0;
				isEnd	= f_Lnk_OnRecv(lRecv);
			}
			else
			{
				// 시간 초과: 송신이 끝났는데 한동안 안 오면 끝 (남은 스캔은 손실로 셈)
				EnterCriticalSection(&s_Link.st_Lock);
				isTxDone	= s_Link.isTxDone;
				idleMs		= idleMs + LNK_RECV_TIMEOUT_MS;

				if ((isTxDone != 0) && (idleMs >= LNK_END_WAIT_MS))
				{
					s_Link.st_Status.nLostNum	= s_Link.st_Status.nLostNum + (s_Link.st_Status.nScanNum - (s_Link.nLastScan + 1));
					isEnd						= 1;
				}

				LeaveCriticalSection(&s_Link.st_Lock);
			}
		}
	}

	(VOID)f_SocketClose(st_Socket);

	return 0U;
}

// 지난 결과와 입력 복사본을 비움
static VOID f_Lnk_FreeBuffers(VOID)
{
	free(s_Link.st_InScan);
	free(s_Link.st_InRadar);
	free(s_Link.pt_PlotSource);
	free(s_Link.st_ResScan);
	free(s_Link.st_ResTrack);
	free(s_Link.st_ResPlot);

	s_Link.st_InScan		= NULL;
	s_Link.st_InRadar		= NULL;
	s_Link.pt_PlotSource	= NULL;
	s_Link.st_ResScan		= NULL;
	s_Link.st_ResTrack		= NULL;
	s_Link.st_ResPlot		= NULL;
	s_Link.nPlotSourceNum	= 0;
	s_Link.nPlotSourceCap	= 0;
	s_Link.nResTrackNum		= 0;
	s_Link.nResTrackCap		= 0;
	s_Link.nResPlotNum		= 0;
}

// 송수신 시작: 입력을 복사해 두고 수신, 송신 쓰레드를 띄움 (돌던 송수신은 먼저 멈춤)
INT32 f_Lnk_Start(const ST_PcsScan *st_Scan, const ST_SimSample *st_Sample, INT32 nScanNum, INT32 sendGapMs)
{
	WSADATA	st_WsaData;
	INT32	isOk = 1;
	INT32	nScan;

	if (s_Link.isLockReady == 0)
	{
		InitializeCriticalSection(&s_Link.st_Lock);
		s_Link.isLockReady = 1;
	}

	f_Lnk_Stop();
	f_Lnk_FreeBuffers();

	EnterCriticalSection(&s_Link.st_Lock);
	(VOID)memset(&s_Link.st_Status, 0, sizeof(s_Link.st_Status));
	s_Link.st_Status.nScanNum	= (nScanNum > 0) ? nScanNum : 0;
	s_Link.st_Status.sendGapMs	= (sendGapMs > 0) ? sendGapMs : 0;
	s_Link.isTxDone				= 0;
	s_Link.nLastScan			= -1;
	s_Link.isPrevPlatform		= 0;
	LeaveCriticalSection(&s_Link.st_Lock);

	f_Trk_Init(&s_Link.st_Table);

	if (nScanNum > 0)
	{
		s_Link.st_InScan		= (ST_PcsScan *)malloc((size_t)nScanNum * sizeof(ST_PcsScan));
		s_Link.st_InRadar		= (ST_TargetState *)malloc((size_t)nScanNum * sizeof(ST_TargetState));
		s_Link.nPlotSourceCap	= nScanNum * IDD_MAX_PLOT;
		s_Link.pt_PlotSource	= (INT32 *)malloc((size_t)s_Link.nPlotSourceCap * sizeof(INT32));
		s_Link.st_ResScan		= (ST_LnkScanRec *)calloc((size_t)nScanNum, sizeof(ST_LnkScanRec));
		s_Link.nResTrackCap		= nScanNum * 8;
		s_Link.st_ResTrack		= (ST_TrkView *)malloc((size_t)s_Link.nResTrackCap * sizeof(ST_TrkView));
		s_Link.st_ResPlot		= (ST_LnkPlot *)malloc((size_t)nScanNum * IDD_MAX_PLOT * sizeof(ST_LnkPlot));
	}

	if ((s_Link.st_InScan == NULL) || (s_Link.st_InRadar == NULL) || (s_Link.pt_PlotSource == NULL) || (s_Link.st_ResScan == NULL)
		|| (s_Link.st_ResTrack == NULL) || (s_Link.st_ResPlot == NULL))
	{
		f_Lnk_FreeBuffers();
		isOk = 0;
	}
	else
	{
		(VOID)memcpy(s_Link.st_InScan, st_Scan, (size_t)nScanNum * sizeof(ST_PcsScan));

		for (nScan = 0; nScan < nScanNum; nScan++)
		{
			s_Link.st_InRadar[nScan] = st_Sample[nScan].st_Platform;
		}
	}

	if (isOk != 0)
	{
		isOk = (WSAStartup(MAKEWORD(2, 2), &st_WsaData) == 0) ? 1 : 0;
		s_Link.isWsaReady = isOk;
	}

	if (isOk != 0)
	{
		s_Link.hStop	= CreateEventW(NULL, TRUE, FALSE, NULL);
		s_Link.hReady	= CreateEventW(NULL, TRUE, FALSE, NULL);
		isOk			= ((s_Link.hStop != NULL) && (s_Link.hReady != NULL)) ? 1 : 0;
	}

	if (isOk != 0)
	{
		s_Link.st_Status.isRunning	= 1;
		s_Link.hRxThread			= (HANDLE)_beginthreadex(NULL, 0, f_Lnk_RxThread, NULL, 0, NULL);
		s_Link.hTxThread			= (HANDLE)_beginthreadex(NULL, 0, f_Lnk_TxThread, NULL, 0, NULL);
		isOk						= ((s_Link.hRxThread != NULL) && (s_Link.hTxThread != NULL)) ? 1 : 0;
	}

	if (isOk == 0)
	{
		f_Lnk_Stop();
	}

	return isOk;
}

// 송수신 멈춤 (결과는 다음 시작까지 남김)
VOID f_Lnk_Stop(VOID)
{
	HANDLE	hThread[2];
	INT32	nThreadNum = 0;

	if (s_Link.hStop != NULL)
	{
		(VOID)SetEvent(s_Link.hStop);
	}

	if (s_Link.hRxThread != NULL)
	{
		hThread[nThreadNum] = s_Link.hRxThread;
		nThreadNum++;
	}

	if (s_Link.hTxThread != NULL)
	{
		hThread[nThreadNum] = s_Link.hTxThread;
		nThreadNum++;
	}

	if (nThreadNum > 0)
	{
		(VOID)WaitForMultipleObjects((DWORD)nThreadNum, hThread, TRUE, INFINITE);
	}

	if (s_Link.hRxThread != NULL)
	{
		(VOID)CloseHandle(s_Link.hRxThread);
		s_Link.hRxThread = NULL;
	}

	if (s_Link.hTxThread != NULL)
	{
		(VOID)CloseHandle(s_Link.hTxThread);
		s_Link.hTxThread = NULL;
	}

	if (s_Link.hStop != NULL)
	{
		(VOID)CloseHandle(s_Link.hStop);
		s_Link.hStop = NULL;
	}

	if (s_Link.hReady != NULL)
	{
		(VOID)CloseHandle(s_Link.hReady);
		s_Link.hReady = NULL;
	}

	if (s_Link.isWsaReady != 0)
	{
		(VOID)WSACleanup();
		s_Link.isWsaReady = 0;
	}

	if (s_Link.isLockReady != 0)
	{
		EnterCriticalSection(&s_Link.st_Lock);
		s_Link.st_Status.isRunning = 0;
		LeaveCriticalSection(&s_Link.st_Lock);
	}
}

// 진행 상태 복사 (쓰레드가 둘 다 끝났으면 isRunning = 0)
VOID f_Lnk_GetStatus(ST_LnkStatus *st_Status)
{
	INT32 isTxEnd;
	INT32 isRxEnd;

	if (s_Link.isLockReady == 0)
	{
		(VOID)memset(st_Status, 0, sizeof(*st_Status));
	}
	else
	{
		isTxEnd = ((s_Link.hTxThread == NULL) || (WaitForSingleObject(s_Link.hTxThread, 0) == WAIT_OBJECT_0)) ? 1 : 0;
		isRxEnd = ((s_Link.hRxThread == NULL) || (WaitForSingleObject(s_Link.hRxThread, 0) == WAIT_OBJECT_0)) ? 1 : 0;

		EnterCriticalSection(&s_Link.st_Lock);
		*st_Status = s_Link.st_Status;
		LeaveCriticalSection(&s_Link.st_Lock);

		st_Status->isRunning = ((isTxEnd != 0) && (isRxEnd != 0)) ? 0 : st_Status->isRunning;
	}
}

// 처리한 스캔 하나의 결과 복사
INT32 f_Lnk_CopyScan(INT32 nIndex, ST_LnkScan *st_Scan, ST_TrkView *st_Track, ST_LnkPlot *st_Plot)
{
	const ST_LnkScanRec	*st_Rec;
	INT32				isOk = 0;

	if (s_Link.isLockReady != 0)
	{
		EnterCriticalSection(&s_Link.st_Lock);

		if ((nIndex >= 0) && (nIndex < s_Link.st_Status.nProcNum))
		{
			st_Rec		= &s_Link.st_ResScan[nIndex];
			*st_Scan	= st_Rec->st_Scan;
			(VOID)memcpy(st_Track, &s_Link.st_ResTrack[st_Rec->nTrackStart], (size_t)st_Rec->st_Scan.nTrackNum * sizeof(ST_TrkView));
			(VOID)memcpy(st_Plot, &s_Link.st_ResPlot[st_Rec->nPlotStart], (size_t)st_Rec->st_Scan.nPlotNum * sizeof(ST_LnkPlot));
			isOk		= 1;
		}

		LeaveCriticalSection(&s_Link.st_Lock);
	}

	return isOk;
}

// 플롯 ID 의 참값 (송신 쪽 기록, 화면 확인용)
INT32 f_Lnk_GetPlotSource(UINT32 plotId)
{
	const INT64	nSeq = (INT64)plotId - (INT64)IDD_PLOT_ID_MIN;
	INT32		nSource = -1;

	if (s_Link.isLockReady != 0)
	{
		EnterCriticalSection(&s_Link.st_Lock);

		if ((nSeq >= 0) && (nSeq < (INT64)s_Link.nPlotSourceNum))
		{
			nSource = s_Link.pt_PlotSource[nSeq];
		}

		LeaveCriticalSection(&s_Link.st_Lock);
	}

	return nSource;
}
