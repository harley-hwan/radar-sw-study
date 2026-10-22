// 2-Point 트랙 개시와 트랙 스코어 기반 트랙 관리 (과제 5)
#include <math.h>
#include <string.h>

#include "TrackMgr.h"

// 트랙 표에서 이미 쓰는 ID 인지
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

// 새 트랙 ID: 100 부터 차례로, 10000 다음은 다시 100. 표에 남은 ID 는 건너뜀
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

// 삭제 상태 (4) 인 트랙을 표에서 뺌
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

// 두 플롯 사이 속력 |z_k - z_k-1| / T
static FLOAT64 f_Trk_PairSpeed(const ST_TrkPlot *st_Prev, const ST_TrkPlot *st_Cur, FLOAT64 dt)
{
	const FLOAT64	dx = st_Cur->pos[0] - st_Prev->pos[0];
	const FLOAT64	dy = st_Cur->pos[1] - st_Prev->pos[1];
	const FLOAT64	dz = st_Cur->pos[2] - st_Prev->pos[2];

	return sqrt((dx * dx) + (dy * dy) + (dz * dz)) / dt;
}

// 트랙 표 초기화
VOID f_Trk_Init(ST_TrkTable *st_Table)
{
	(VOID)memset(st_Table, 0, sizeof(*st_Table));
	st_Table->nextTrackId = TRK_ID_MIN;
}

// VC-NED 원점 (플랫폼) 이 움직인 만큼 앞 스캔 값 (개시 후보, 트랙 위치) 을 옮김
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

// 트랙 스코어 로직 (과제 2.2). Assigned_Flag 는 부르기 전에 채움
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

// 2-Point 초기화. X: 위치 z_k, 속도 (z_k - z_k-1) / T, 가속도 0
// P: 위치 R_k, 위치-속도 R_k / T, 속도 (R_k + R_k-1) / T^2, 가속도 0
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

// 트랙 요약: 위치, 속도, P 대각의 표준편차
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

// R 표준편차 (과제 조건 10 m, 0.2 도, 0.2 도)
VOID f_Trk_GetMeasSigma(STRUCT_Coord_Sph *st_Sigma)
{
	st_Sigma->r		= TRK_R_SIGMA_RANGE;
	st_Sigma->az	= f_Deg_To_Rad(TRK_R_SIGMA_AZIMUTH);
	st_Sigma->el	= f_Deg_To_Rad(TRK_R_SIGMA_ELEVATION);
}

// 측정잡음 공분산 R (VC-NED) = M J diag(σr^2, σaz^2, σel^2) J^T M^T
// J: 구좌표 -> 안테나 직교 기울기, M: 안테나 -> 동체 -> NED 회전
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

	// 반올림으로 생긴 작은 비대칭 제거
	for (nRow = 0; nRow < 3; nRow++)
	{
		for (nCol = 0; nCol < 3; nCol++)
		{
			cov[nRow][nCol] = 0.5 * (mat_R.e[nRow][nCol] + mat_R.e[nCol][nRow]);
		}
	}
}

// 스캔 하나 처리 (플롯 메시지마다 부름)
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

	// 1. 삭제 트랙 빼기
	f_Trk_RemoveDeleted(st_Table);

	// 2. 트랙 관리: 연관 기능이 없어 Assigned_Flag = 0
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

	// 3. 2-Point 개시: 게이트 통과 쌍을 속력이 작은 쌍부터, 한 플롯은 한 쌍에만
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

	// 4. 이번 플롯을 다음 스캔 개시 후보로 보관
	for (nCur = 0; nCur < nCurNum; nCur++)
	{
		st_Table->st_PrevPlot[nCur] = st_Cur[nCur];
	}

	st_Table->nPrevPlotNum	= nCurNum;
	st_Table->prevTime		= time;
	st_Table->isPrevValid	= 1;
}
