//
// @file	TrackMgr.c
// @brief	트랙 개시 및 관리 (과제 5)
//			- 2 Point 트랙 개시: 연속 두 스캔 플롯 쌍 + 속도 게이팅 (|z_k,i - z_k-1,j| / T < V_max)
//			- 트랙 스코어 기반 트랙 관리: 임시 -> 확정 -> 메모리 -> 삭제
// @author	hwan
// @date	2026.10.21.
//
#include <math.h>
#include <string.h>

#include "TrackMgr.h"

//
// @brief	트랙 ID 가 트랙 표에서 이미 쓰이는지 확인
// @param st_Table	트랙 표
// @param trackId	확인할 트랙 ID
// @return	1 쓰는 중, 0 비어 있음
// @author	hwan
//
static INT32 f_Trk_IsIdUsed(const ST_TrkTable *st_Table, UINT32 trackId)
{
	INT32	isUsed = 0;
	INT32	nTrack;

	for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
	{
		if (st_Table->st_Track[nTrack].TrackId == trackId)
		{
			isUsed = 1;
		}
	}

	return isUsed;
}

//
// @brief	새 트랙 ID 부여. 100 부터 차례로 주고 10000 다음은 100 으로 돌아감. 표에 남아 있는 ID 는 건너뜀
// @param st_Table	트랙 표 (nextTrackId 갱신)
// @return	새 트랙 ID (100 ~ 10000). 줄 ID 가 없으면 0
// @author	hwan
//
static UINT32 f_Trk_NewId(ST_TrkTable *st_Table)
{
	UINT32	newId = 0;
	UINT32	candidate;
	INT32	nTry;

	for (nTry = 0; (nTry <= (TRK_ID_MAX - TRK_ID_MIN)) && (newId == 0); nTry++)
	{
		candidate				= st_Table->nextTrackId;
		st_Table->nextTrackId	= (candidate >= TRK_ID_MAX) ? TRK_ID_MIN : (candidate + 1);

		if (f_Trk_IsIdUsed(st_Table, candidate) == 0)
		{
			newId = candidate;
		}
	}

	return newId;
}

//
// @brief	앞 스캔에 삭제 상태 (4) 가 된 트랙을 트랙 표에서 뺌 (남은 트랙 순서 유지)
// @param st_Table	트랙 표
// @return	없음
// @author	hwan
//
static VOID f_Trk_RemoveDeleted(ST_TrkTable *st_Table)
{
	INT32	nKeep = 0;
	INT32	nTrack;

	for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
	{
		if (st_Table->st_Track[nTrack].TrackState != TRK_STATE_DELETED)
		{
			if (nKeep != nTrack)
			{
				st_Table->st_Track[nKeep] = st_Table->st_Track[nTrack];
			}
			nKeep++;
		}
	}

	st_Table->nTrackNum = nKeep;
}

//
// @brief	속도 게이팅용: 두 플롯의 위치 변화량 크기 / 시간 차 = |z_k,i - z_k-1,j| / T
// @param st_Prev	앞 스캔 플롯
// @param st_Cur	이번 스캔 플롯
// @param dt		두 스캔의 시간 차 T [s]
// @return	두 플롯 사이 속력 [m/s]
// @author	hwan
//
static FLOAT64 f_Trk_PairSpeed(const ST_TrkPlot *st_Prev, const ST_TrkPlot *st_Cur, FLOAT64 dt)
{
	const FLOAT64	dx = st_Cur->pos[0] - st_Prev->pos[0];
	const FLOAT64	dy = st_Cur->pos[1] - st_Prev->pos[1];
	const FLOAT64	dz = st_Cur->pos[2] - st_Prev->pos[2];

	return sqrt((dx * dx) + (dy * dy) + (dz * dz)) / dt;
}

//
// @brief	트랙 표 초기화. 트랙 없음, 다음 트랙 ID 는 TRK_ID_MIN (100)
// @param st_Table	트랙 표
// @return	없음
// @author	hwan
//
VOID f_Trk_Init(ST_TrkTable *st_Table)
{
	(VOID)memset(st_Table, 0, sizeof(*st_Table));
	st_Table->nextTrackId = TRK_ID_MIN;
}

//
// @brief	VC-NED 원점 이동 보정. VC-NED 원점은 플랫폼을 따라 움직이므로, 이번 스캔 처리 전에
//			앞 스캔 값 (개시 후보 플롯, 트랙 위치) 을 이번 스캔 VC-NED 로 옮김
// @param st_Table	트랙 표
// @param shift		앞 스캔 원점 (플랫폼 위치) 의 이번 스캔 VC-NED 좌표 [m]. 플랫폼이 서 있으면 0
// @remark	두 플롯의 차가 플랫폼 상대 속도가 아니라 표적 속도가 되게 함.
//			두 원점 사이 축 방향 차이 (스캔 사이 수십 m 이동이면 1e-5 rad 이하) 는 무시
// @return	없음
// @author	hwan
//
VOID f_Trk_MoveOrigin(ST_TrkTable *st_Table, const FLOAT64 shift[3])
{
	INT32 nPlot;
	INT32 nTrack;
	INT32 nAxis;

	for (nAxis = 0; nAxis < 3; nAxis++)
	{
		for (nPlot = 0; nPlot < st_Table->nPrevPlotNum; nPlot++)
		{
			st_Table->st_PrevPlot[nPlot].pos[nAxis] = st_Table->st_PrevPlot[nPlot].pos[nAxis] + shift[nAxis];
		}

		for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
		{
			st_Table->st_Track[nTrack].mat_EstX.e[3 * nAxis][0] = st_Table->st_Track[nTrack].mat_EstX.e[3 * nAxis][0] + shift[nAxis];
		}
	}
}

//
// @brief	트랙 스코어 로직 (과제 2.2). Assigned_Flag 는 부르기 전에 채움
//			- 임시: 연관 성공 +1, 실패 -1. TrackScore >= 5 (확정 문턱치) 면 확정, <= 0 (삭제 문턱치) 면 삭제
//			- 확정: 연관 실패면 메모리 트랙 전환, MemoryScore + 1
//			- 메모리: 연관 성공이면 바로 확정 트랙 전환, MemoryScore = 0.
//			  실패면 MemoryScore + 1, MemoryScore >= 5 (메모리 트랙 삭제 문턱치) 면 삭제
// @param st_Track	트랙 (TrackState, TrackScore, MemoryScore 갱신)
// @return	없음
// @author	hwan
//
VOID f_Trk_UpdateState(ST_Track *st_Track)
{
	const INT32 isAssigned = (st_Track->Assigned_Flag != 0) ? 1 : 0;

	switch (st_Track->TrackState)
	{
	case TRK_STATE_TENTATIVE:
		st_Track->TrackScore = st_Track->TrackScore + ((isAssigned != 0) ? 1 : -1);

		if (st_Track->TrackScore >= TRK_SCORE_CONFIRM)
		{
			st_Track->TrackState = TRK_STATE_CONFIRMED;
		}
		else if (st_Track->TrackScore <= TRK_SCORE_DELETE)
		{
			st_Track->TrackState = TRK_STATE_DELETED;
		}
		else
		{
			// 임시 트랙 유지
		}
		break;

	case TRK_STATE_CONFIRMED:
		if (isAssigned == 0)
		{
			st_Track->TrackState	= TRK_STATE_MEMORY;
			st_Track->MemoryScore	= st_Track->MemoryScore + 1;
		}
		break;

	case TRK_STATE_MEMORY:
		if (isAssigned != 0)
		{
			st_Track->TrackState	= TRK_STATE_CONFIRMED;
			st_Track->MemoryScore	= 0;
		}
		else
		{
			st_Track->MemoryScore	= st_Track->MemoryScore + 1;

			if (st_Track->MemoryScore >= TRK_MEMORY_DELETE)
			{
				st_Track->TrackState = TRK_STATE_DELETED;
			}
		}
		break;

	default:
		// 삭제 트랙은 다음 스캔에 표에서 빠짐
		break;
	}
}

//
// @brief	2 Point 트랙 초기화. 초기 상태변수 mat_EstX 와 초기 오차 공분산 mat_EstP 계산
//			- mat_EstX: 위치 = 이번 플롯 z_k, 속도 = (z_k - z_k-1) / T, 가속도 = 0
//			- mat_EstP (축 a, b 마다): P_pp = R_k, P_pv = P_vp = R_k / T, P_vv = (R_k + R_k-1) / T^2, 가속도 행 · 열 = 0
// @param st_Track	만들 트랙 (mat_EstX, mat_EstP 채움)
// @param st_Prev	앞 스캔 플롯 z_k-1 (위치, R)
// @param st_Cur	이번 스캔 플롯 z_k (위치, R)
// @return	없음
// @author	hwan
//
VOID f_Trk_TwoPointInit(ST_Track *st_Track, const ST_TrkPlot *st_Prev, const ST_TrkPlot *st_Cur)
{
	const FLOAT64	dt = st_Cur->time - st_Prev->time;
	matrix			*st_X = &st_Track->mat_EstX;
	matrix			*st_P = &st_Track->mat_EstP;
	INT32			nAxis;
	INT32			nAxisB;

	Matrix_initialize(st_X, TRK_STATE_DIM, 1);
	Matrix_initialize(st_P, TRK_STATE_DIM, TRK_STATE_DIM);

	for (nAxis = 0; nAxis < 3; nAxis++)
	{
		st_X->e[(3 * nAxis)][0]		= st_Cur->pos[nAxis];
		st_X->e[(3 * nAxis) + 1][0]	= (st_Cur->pos[nAxis] - st_Prev->pos[nAxis]) / dt;
		st_X->e[(3 * nAxis) + 2][0]	= 0.0;

		for (nAxisB = 0; nAxisB < 3; nAxisB++)
		{
			st_P->e[(3 * nAxis)][(3 * nAxisB)]			= st_Cur->cov[nAxis][nAxisB];
			st_P->e[(3 * nAxis)][(3 * nAxisB) + 1]		= st_Cur->cov[nAxis][nAxisB] / dt;
			st_P->e[(3 * nAxis) + 1][(3 * nAxisB)]		= st_Cur->cov[nAxis][nAxisB] / dt;
			st_P->e[(3 * nAxis) + 1][(3 * nAxisB) + 1]	= (st_Cur->cov[nAxis][nAxisB] + st_Prev->cov[nAxis][nAxisB]) / (dt * dt);
		}
	}
}

//
// @brief	트랙 요약. 상태변수에서 위치, 속도를, 오차 공분산 대각에서 표준편차를 꺼냄
// @param st_View	출력 요약
// @param st_Track	트랙
// @return	없음
// @author	hwan
//
VOID f_Trk_GetView(ST_TrkView *st_View, const ST_Track *st_Track)
{
	INT32 nAxis;

	st_View->TrackId		= st_Track->TrackId;
	st_View->TrackState		= st_Track->TrackState;
	st_View->TrackScore		= st_Track->TrackScore;
	st_View->MemoryScore	= st_Track->MemoryScore;
	st_View->Assigned_Flag	= st_Track->Assigned_Flag;
	st_View->nInitScan		= st_Track->nInitScan;
	st_View->PlotId[0]		= st_Track->PlotId[0];
	st_View->PlotId[1]		= st_Track->PlotId[1];

	for (nAxis = 0; nAxis < 3; nAxis++)
	{
		st_View->pos[nAxis]		= st_Track->mat_EstX.e[3 * nAxis][0];
		st_View->vel[nAxis]		= st_Track->mat_EstX.e[(3 * nAxis) + 1][0];
		st_View->posStd[nAxis]	= sqrt(st_Track->mat_EstP.e[3 * nAxis][3 * nAxis]);
		st_View->velStd[nAxis]	= sqrt(st_Track->mat_EstP.e[(3 * nAxis) + 1][(3 * nAxis) + 1]);
	}
}

//
// @brief	트랙 초기화 측정잡음 공분산 R 을 구성하는 구좌표 표준편차 (과제 조건)
//			σ range = 10 m, σ azimuth = 0.2 도, σ elevation = 0.2 도
// @param st_Sigma	출력 표준편차 (r [m], az, el [rad])
// @return	없음
// @author	hwan
//
VOID f_Trk_GetMeasSigma(STRUCT_Coord_Sph *st_Sigma)
{
	st_Sigma->r		= TRK_R_SIGMA_RANGE;
	st_Sigma->az	= f_Deg_To_Rad(TRK_R_SIGMA_AZIMUTH);
	st_Sigma->el	= f_Deg_To_Rad(TRK_R_SIGMA_ELEVATION);
}

//
// @brief	측정잡음 공분산 R 계산. 안테나 구좌표 (거리, 방위각, 고각) 표준편차를 VC-NED 위치 공분산으로 바꿈
//			R_ant = J diag(σr^2, σaz^2, σel^2) J^T (J: 구좌표 -> 안테나 직교 좌표 야코비안)
//			R_ned = M R_ant M^T (M: 안테나 -> 동체 -> NED 회전)
// @param cov		출력 R (VC-NED) [m^2]
// @param st_Meas	측정 (거리 [m], 방위각, 고각 [rad])
// @param st_Sigma	구좌표 표준편차 (σr [m], σaz, σel [rad]). f_Trk_GetMeasSigma 값 (과제 조건)
// @param st_Att	플랫폼 자세 [rad]
// @return	없음
// @author	hwan
//
VOID f_Trk_MeasCov(FLOAT64 cov[3][3], const STRUCT_Coord_Sph *st_Meas, const STRUCT_Coord_Sph *st_Sigma, const STRUCT_Coord_Attitude *st_Att)
{
	const FLOAT64		r = st_Meas->r;
	const FLOAT64		cosAz = cos(st_Meas->az);
	const FLOAT64		sinAz = sin(st_Meas->az);
	const FLOAT64		cosEl = cos(st_Meas->el);
	const FLOAT64		sinEl = sin(st_Meas->el);
	STRUCT_Coord_Rect	st_Body;
	STRUCT_Coord_Rect	st_Ned;
	matrix				mat_J;
	matrix				mat_Jt;
	matrix				mat_S;
	matrix				mat_Ra;
	matrix				mat_M;
	matrix				mat_Mt;
	matrix				mat_R;
	FLOAT64				axis[3];
	INT32				nRow;
	INT32				nCol;

	// x = r cos(el) sin(az), y = r sin(el), z = r cos(el) cos(az) 를 r, az, el 로 미분
	Matrix_initialize(&mat_J, 3, 3);
	mat_J.e[0][0] = cosEl * sinAz;	mat_J.e[0][1] = r * cosEl * cosAz;		mat_J.e[0][2] = -r * sinEl * sinAz;
	mat_J.e[1][0] = sinEl;			mat_J.e[1][1] = 0.0;					mat_J.e[1][2] = r * cosEl;
	mat_J.e[2][0] = cosEl * cosAz;	mat_J.e[2][1] = -r * cosEl * sinAz;	mat_J.e[2][2] = -r * sinEl * cosAz;

	Matrix_initialize(&mat_S, 3, 3);
	mat_S.e[0][0] = st_Sigma->r * st_Sigma->r;
	mat_S.e[1][1] = st_Sigma->az * st_Sigma->az;
	mat_S.e[2][2] = st_Sigma->el * st_Sigma->el;

	mat_Jt = Matrix_Transpose(&mat_J);
	mat_Ra = Matrix_Product3(&mat_J, &mat_S, &mat_Jt);

	// 회전 M 의 열 = 안테나 축 단위 벡터를 동체, NED 로 옮긴 것
	Matrix_initialize(&mat_M, 3, 3);

	for (nCol = 0; nCol < 3; nCol++)
	{
		axis[0]	= (nCol == 0) ? 1.0 : 0.0;
		axis[1]	= (nCol == 1) ? 1.0 : 0.0;
		axis[2]	= (nCol == 2) ? 1.0 : 0.0;
		st_Body	= f_Trans_Ant_To_Body(axis[0], axis[1], axis[2]);
		st_Ned	= f_Trans_Body_To_Ned(st_Body.x, st_Body.y, st_Body.z, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);
		mat_M.e[0][nCol] = st_Ned.x;
		mat_M.e[1][nCol] = st_Ned.y;
		mat_M.e[2][nCol] = st_Ned.z;
	}

	mat_Mt	= Matrix_Transpose(&mat_M);
	mat_R	= Matrix_Product3(&mat_M, &mat_Ra, &mat_Mt);

	// 곱셈 반올림으로 생기는 아주 작은 비대칭을 없앰 (공분산은 대칭이어야 함)
	for (nRow = 0; nRow < 3; nRow++)
	{
		for (nCol = 0; nCol < 3; nCol++)
		{
			cov[nRow][nCol] = 0.5 * (mat_R.e[nRow][nCol] + mat_R.e[nCol][nRow]);
		}
	}
}

//
// @brief	스캔 하나 처리 (데이터 처리 쓰레드가 플롯 메시지마다 부름)
//			1. 앞 스캔에 삭제 상태가 된 트랙을 표에서 뺌
//			2. 트랙 연관 (아직 없음): 모든 트랙의 Assigned_Flag 를 실패 (0) 로 두고 스코어 로직을 돌림
//			3. 2 Point 개시: 앞 스캔에 남은 플롯과 이번 스캔 플롯의 쌍 가운데 속도 게이트 (|z_k - z_k-1| / T < V_max) 를
//			   넘는 쌍을 속력이 작은 쌍부터 하나씩 골라 트랙을 만듦. 한 플롯은 한 쌍에만 씀
//			4. 이번 스캔 플롯을 다음 스캔의 개시 후보로 보관 (개시에 쓴 플롯은 표시해 둠)
// @param st_Table	트랙 표
// @param st_Plot	이번 스캔 플롯 (VC-NED 위치, R)
// @param nPlotNum	플롯 수
// @param time		스캔 시각 [s]
// @param nScan		스캔 번호
// @param st_Log	출력 처리 기록
// @return	없음
// @author	hwan
//
VOID f_Trk_ProcessScan(ST_TrkTable *st_Table, const ST_TrkPlot *st_Plot, INT32 nPlotNum, FLOAT64 time, INT32 nScan, ST_TrkScanLog *st_Log)
{
	ST_TrkPlot	st_Cur[TRK_MAX_PLOT];
	ST_Track	*st_Track;
	FLOAT64		dt;
	FLOAT64		speed;
	FLOAT64		bestSpeed;
	INT32		nCurNum;
	INT32		nTrack;
	INT32		oldState;
	INT32		nPrev;
	INT32		nCur;
	INT32		bestPrev;
	INT32		bestCur;
	INT32		isSearching;
	UINT32		newId;

	(VOID)memset(st_Log, 0, sizeof(*st_Log));
	nCurNum = (nPlotNum < 0) ? 0 : ((nPlotNum > TRK_MAX_PLOT) ? TRK_MAX_PLOT : nPlotNum);
	st_Log->nPlotNum = nCurNum;

	// 1
	f_Trk_RemoveDeleted(st_Table);

	// 2
	for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
	{
		st_Track				= &st_Table->st_Track[nTrack];
		oldState				= st_Track->TrackState;
		st_Track->Assigned_Flag	= 0;
		f_Trk_UpdateState(st_Track);

		if (st_Track->TrackState != oldState)
		{
			if (st_Track->TrackState == TRK_STATE_CONFIRMED)
			{
				st_Log->nConfirmedNum++;
			}
			else if (st_Track->TrackState == TRK_STATE_MEMORY)
			{
				st_Log->nMemoryNum++;
			}
			else if (st_Track->TrackState == TRK_STATE_DELETED)
			{
				st_Log->nDeletedNum++;
				st_Table->nDeletedSum++;
			}
			else
			{
				// 다른 전이는 없음
			}
		}
	}

	// 3
	for (nCur = 0; nCur < nCurNum; nCur++)
	{
		st_Cur[nCur]		= st_Plot[nCur];
		st_Cur[nCur].isUsed	= 0;
	}

	dt = time - st_Table->prevTime;

	if ((st_Table->isPrevValid != 0) && (dt > 0.0))
	{
		for (nPrev = 0; nPrev < st_Table->nPrevPlotNum; nPrev++)
		{
			if (st_Table->st_PrevPlot[nPrev].isUsed == 0)
			{
				st_Log->nCandidateNum++;

				for (nCur = 0; nCur < nCurNum; nCur++)
				{
					st_Log->nPairNum++;

					if (f_Trk_PairSpeed(&st_Table->st_PrevPlot[nPrev], &st_Cur[nCur], dt) < TRK_VMAX)
					{
						st_Log->nGatePassNum++;
					}
				}
			}
		}

		isSearching = 1;

		while (isSearching != 0)
		{
			bestSpeed	= TRK_VMAX;
			bestPrev	= -1;
			bestCur		= -1;

			for (nPrev = 0; nPrev < st_Table->nPrevPlotNum; nPrev++)
			{
				for (nCur = 0; nCur < nCurNum; nCur++)
				{
					if ((st_Table->st_PrevPlot[nPrev].isUsed == 0) && (st_Cur[nCur].isUsed == 0))
					{
						speed = f_Trk_PairSpeed(&st_Table->st_PrevPlot[nPrev], &st_Cur[nCur], dt);

						if (speed < bestSpeed)
						{
							bestSpeed	= speed;
							bestPrev	= nPrev;
							bestCur		= nCur;
						}
					}
				}
			}

			if (bestPrev < 0)
			{
				isSearching = 0;
			}
			else
			{
				st_Table->st_PrevPlot[bestPrev].isUsed	= 1;
				st_Cur[bestCur].isUsed					= 1;

				newId = (st_Table->nTrackNum < TRK_MAX_TRACK) ? f_Trk_NewId(st_Table) : 0;

				if (newId == 0)
				{
					st_Table->nTableFullSum++;
				}
				else
				{
					st_Track = &st_Table->st_Track[st_Table->nTrackNum];
					(VOID)memset(st_Track, 0, sizeof(*st_Track));
					f_Trk_TwoPointInit(st_Track, &st_Table->st_PrevPlot[bestPrev], &st_Cur[bestCur]);
					st_Track->TrackId		= newId;
					st_Track->TrackState	= TRK_STATE_TENTATIVE;
					st_Track->TrackScore	= TRK_SCORE_INIT;
					st_Track->MemoryScore	= 0;
					st_Track->Assigned_Flag	= 0;
					st_Track->initTime		= time;
					st_Track->nInitScan		= nScan;
					st_Track->PlotId[0]		= st_Table->st_PrevPlot[bestPrev].PlotId;
					st_Track->PlotId[1]		= st_Cur[bestCur].PlotId;
					st_Table->nTrackNum++;
					st_Table->nCreatedSum++;
					st_Log->nCreatedNum++;
				}
			}
		}
	}

	// 4
	for (nCur = 0; nCur < nCurNum; nCur++)
	{
		st_Table->st_PrevPlot[nCur] = st_Cur[nCur];
	}

	st_Table->nPrevPlotNum	= nCurNum;
	st_Table->prevTime		= time;
	st_Table->isPrevValid	= 1;
}
