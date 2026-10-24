// 2-Point 트랙 개시, 트랙 스코어 관리 (과제 5) + Singer 모델 CMKF 추적 필터 (과제 6)
#include <math.h>
#include <string.h>

#include "TrackMgr.h"

#define TRK_Q_NUM				6						// q_k 의 서로 다른 원소: q11, q12, q13, q22, q23, q33
#define TRK_SERIES_NUM			5						// 급수 항 수

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

// 파라미터 하나: 범위 밖 (NaN 포함) 이면 기본값
static FLOAT64 f_Trk_CheckParam(FLOAT64 value, FLOAT64 minValue, INT32 isMinIncluded, FLOAT64 defaultValue)
{
	const INT32 isValid = (isMinIncluded != 0) ? ((value >= minValue) ? 1 : 0) : ((value > minValue) ? 1 : 0);

	return ((isValid != 0) && (value < INF)) ? value : defaultValue;
}

// 측정 행렬 H (3 x 9): 상태변수에서 위치 x, y, z 만 꺼냄
static VOID f_Trk_MakeH(matrix *st_H)
{
	Matrix_initialize(st_H, TRK_MEAS_DIM, TRK_STATE_DIM);
	st_H->e[0][0] = 1.0;
	st_H->e[1][3] = 1.0;
	st_H->e[2][6] = 1.0;
}

// 반올림으로 생긴 작은 비대칭 제거
static VOID f_Trk_Symmetrize(matrix *st_P)
{
	FLOAT64	mean;
	INT32	nRow;
	INT32	nCol;

	for (nRow = 0; nRow < st_P->row; nRow++)
	{
		for (nCol = nRow + 1; nCol < st_P->column; nCol++)
		{
			mean				= 0.5 * (st_P->e[nRow][nCol] + st_P->e[nCol][nRow]);
			st_P->e[nRow][nCol]	= mean;
			st_P->e[nCol][nRow]	= mean;
		}
	}
}

// 잔차 v = z - H X, 잔차 공분산 S = H P H^T + R
static VOID f_Trk_Innovation(const ST_Track *st_Track, const ST_TrkPlot *st_Plot, matrix *st_V, matrix *st_S)
{
	matrix	mat_X = st_Track->mat_EstX;
	matrix	mat_P = st_Track->mat_EstP;
	matrix	mat_H;
	matrix	mat_Ht;
	matrix	mat_Z;
	matrix	mat_R;
	matrix	mat_HX;
	matrix	mat_HPHt;
	INT32	nRow;
	INT32	nCol;

	f_Trk_MakeH(&mat_H);
	mat_Ht = Matrix_Transpose(&mat_H);
	Matrix_initialize(&mat_Z, TRK_MEAS_DIM, 1);
	Matrix_initialize(&mat_R, TRK_MEAS_DIM, TRK_MEAS_DIM);

	for (nRow = 0; nRow < TRK_MEAS_DIM; nRow++)
	{
		mat_Z.e[nRow][0] = st_Plot->pos[nRow];

		for (nCol = 0; nCol < TRK_MEAS_DIM; nCol++)
		{
			mat_R.e[nRow][nCol] = st_Plot->cov[nRow][nCol];
		}
	}

	mat_HX		= Matrix_Product2(&mat_H, &mat_X);
	*st_V		= Matrix_Subtract(&mat_Z, &mat_HX);
	mat_HPHt	= Matrix_Product3(&mat_H, &mat_P, &mat_Ht);
	*st_S		= Matrix_Add(&mat_HPHt, &mat_R);
	f_Trk_Symmetrize(st_S);
}

// v^T S^-1 v
static FLOAT64 f_Trk_Mahalanobis(matrix *st_V, matrix *st_Si)
{
	matrix	mat_Vt = Matrix_Transpose(st_V);
	matrix	mat_D = Matrix_Product3(&mat_Vt, st_Si, st_V);

	return mat_D.e[0][0];
}

// 추적 필터 기본값 (과제 6: 10 m, 0.3 도, 0.3 도, q 0.5, τ 10)
VOID f_Trk_DefaultParam(ST_TrkParam *st_Param)
{
	st_Param->sigmaRange		= TRK_R_SIGMA_RANGE;
	st_Param->sigmaAzimuth		= TRK_R_SIGMA_AZIMUTH;
	st_Param->sigmaElevation	= TRK_R_SIGMA_ELEVATION;
	st_Param->singerQ			= TRK_SINGER_Q;
	st_Param->singerTau			= TRK_SINGER_TAU;
}

// 트랙 표 초기화. 파라미터가 범위 밖이면 그 값만 기본값 (σ > 0, q >= 0, τ > 0)
VOID f_Trk_Init(ST_TrkTable *st_Table, const ST_TrkParam *st_Param)
{
	ST_TrkParam st_Default;

	f_Trk_DefaultParam(&st_Default);

	(VOID)memset(st_Table, 0, sizeof(*st_Table));
	st_Table->nextTrackId = TRK_ID_MIN;

	if (st_Param == NULL)
	{
		st_Table->st_Param = st_Default;
	}
	else
	{
		st_Table->st_Param.sigmaRange		= f_Trk_CheckParam(st_Param->sigmaRange, 0.0, 0, st_Default.sigmaRange);
		st_Table->st_Param.sigmaAzimuth		= f_Trk_CheckParam(st_Param->sigmaAzimuth, 0.0, 0, st_Default.sigmaAzimuth);
		st_Table->st_Param.sigmaElevation	= f_Trk_CheckParam(st_Param->sigmaElevation, 0.0, 0, st_Default.sigmaElevation);
		st_Table->st_Param.singerQ			= f_Trk_CheckParam(st_Param->singerQ, 0.0, 1, st_Default.singerQ);
		st_Table->st_Param.singerTau		= f_Trk_CheckParam(st_Param->singerTau, 0.0, 0, st_Default.singerTau);
	}
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

// 트랙 스코어 로직 (과제 5). Assigned_Flag 는 부르기 전에 채움
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

	// 필터는 이번 플롯 시각에서 시작
	st_Track->stateTime		= st_Cur->time;
	st_Track->PlotIdLast	= st_Cur->PlotId;
	st_Track->nis			= -1.0;
}

// Singer 상태천이 행렬 F (9 x 9) = blockdiag(f, f, f)
// f = [1  T  τ^2(-1 + T/τ + e^(-T/τ));  0  1  τ(1 - e^(-T/τ));  0  0  e^(-T/τ)]
// e^(-T/τ) - 1 은 expm1 로 계산 (τ 가 크면 1 근처 값끼리 빼서 자리가 빠짐)
VOID f_Trk_SingerF(matrix *st_F, FLOAT64 dt, FLOAT64 tau)
{
	const FLOAT64	x = dt / tau;
	const FLOAT64	em1 = expm1(-x);
	FLOAT64			f[3][3];
	INT32			nBlock;
	INT32			nRow;
	INT32			nCol;

	f[0][0] = 1.0;	f[0][1] = dt;	f[0][2] = tau * tau * (x + em1);
	f[1][0] = 0.0;	f[1][1] = 1.0;	f[1][2] = -tau * em1;
	f[2][0] = 0.0;	f[2][1] = 0.0;	f[2][2] = 1.0 + em1;

	Matrix_initialize(st_F, TRK_STATE_DIM, TRK_STATE_DIM);

	for (nBlock = 0; nBlock < 3; nBlock++)
	{
		for (nRow = 0; nRow < 3; nRow++)
		{
			for (nCol = 0; nCol < 3; nCol++)
			{
				st_F->e[(3 * nBlock) + nRow][(3 * nBlock) + nCol] = f[nRow][nCol];
			}
		}
	}
}

// Singer 공정잡음 공분산 Q (9 x 9) = (2 q^2 / τ) blockdiag(q_k, q_k, q_k)
// q_k 원소는 과제 자료 식 (q12 는 표준식 2(T/τ)e^(-T/τ). 자료의 가운데 T 는 오타)
// T/τ 가 작으면 괄호 안 항들이 거의 지워져 자리가 빠지므로 같은 식을 급수로 계산 (앞 5 항)
VOID f_Trk_SingerQ(matrix *st_Q, FLOAT64 dt, FLOAT64 q, FLOAT64 tau)
{
	// 괄호 안 값 = Σ c_n (T/τ)^n. 첫 항 차수와 계수 (q11, q12, q13, q22, q23, q33 순서)
	static const INT32		s_Power[TRK_Q_NUM] = { 5, 4, 3, 3, 2, 1 };
	static const FLOAT64	s_Coef[TRK_Q_NUM][TRK_SERIES_NUM] =
	{
		{ 1.0 / 10.0,	-1.0 / 18.0,	5.0 / 252.0,	-1.0 / 180.0,	17.0 / 12960.0 },
		{ 1.0 / 4.0,	-1.0 / 6.0,		5.0 / 72.0,		-1.0 / 45.0,	17.0 / 2880.0 },
		{ 1.0 / 3.0,	-1.0 / 3.0,		11.0 / 60.0,	-13.0 / 180.0,	19.0 / 840.0 },
		{ 2.0 / 3.0,	-1.0 / 2.0,		7.0 / 30.0,		-1.0 / 12.0,	31.0 / 1260.0 },
		{ 1.0,			-1.0,			7.0 / 12.0,		-1.0 / 4.0,		31.0 / 360.0 },
		{ 2.0,			-2.0,			4.0 / 3.0,		-2.0 / 3.0,		4.0 / 15.0 }
	};
	const FLOAT64	x = dt / tau;
	const FLOAT64	e1 = exp(-x);
	const FLOAT64	e2 = exp(-2.0 * x);
	const FLOAT64	scale = (2.0 * q * q) / tau;
	FLOAT64			bracket[TRK_Q_NUM];
	FLOAT64			qk[3][3];
	FLOAT64			power;
	INT32			nIndex;
	INT32			nTerm;
	INT32			nBlock;
	INT32			nRow;
	INT32			nCol;

	if (x < TRK_SERIES_X)
	{
		for (nIndex = 0; nIndex < TRK_Q_NUM; nIndex++)
		{
			power			= pow(x, (FLOAT64)s_Power[nIndex]);
			bracket[nIndex]	= 0.0;

			for (nTerm = 0; nTerm < TRK_SERIES_NUM; nTerm++)
			{
				bracket[nIndex]	= bracket[nIndex] + (s_Coef[nIndex][nTerm] * power);
				power			= power * x;
			}
		}
	}
	else
	{
		bracket[0] = 1.0 - e2 + (2.0 * x) + ((2.0 * x * x * x) / 3.0) - (2.0 * x * x) - (4.0 * x * e1);
		bracket[1] = 1.0 + e2 - (2.0 * e1) + (2.0 * x * e1) - (2.0 * x) + (x * x);
		bracket[2] = 1.0 - e2 - (2.0 * x * e1);
		bracket[3] = (4.0 * e1) - 3.0 - e2 + (2.0 * x);
		bracket[4] = e2 + 1.0 - (2.0 * e1);
		bracket[5] = 1.0 - e2;
	}

	qk[0][0] = (pow(tau, 5.0) / 2.0) * bracket[0];
	qk[0][1] = (pow(tau, 4.0) / 2.0) * bracket[1];
	qk[0][2] = (pow(tau, 3.0) / 2.0) * bracket[2];
	qk[1][1] = (pow(tau, 3.0) / 2.0) * bracket[3];
	qk[1][2] = (pow(tau, 2.0) / 2.0) * bracket[4];
	qk[2][2] = (tau / 2.0) * bracket[5];
	qk[1][0] = qk[0][1];
	qk[2][0] = qk[0][2];
	qk[2][1] = qk[1][2];

	Matrix_initialize(st_Q, TRK_STATE_DIM, TRK_STATE_DIM);

	for (nBlock = 0; nBlock < 3; nBlock++)
	{
		for (nRow = 0; nRow < 3; nRow++)
		{
			for (nCol = 0; nCol < 3; nCol++)
			{
				st_Q->e[(3 * nBlock) + nRow][(3 * nBlock) + nCol] = scale * qk[nRow][nCol];
			}
		}
	}
}

// KF 예측 (시각 time 까지): X = F X, P = F P F^T + Q
VOID f_Trk_Predict(ST_Track *st_Track, FLOAT64 time, const ST_TrkParam *st_Param)
{
	const FLOAT64	dt = time - st_Track->stateTime;
	matrix			mat_F;
	matrix			mat_Ft;
	matrix			mat_Q;
	matrix			mat_FPFt;

	if (dt > 0.0)
	{
		f_Trk_SingerF(&mat_F, dt, st_Param->singerTau);
		f_Trk_SingerQ(&mat_Q, dt, st_Param->singerQ, st_Param->singerTau);
		mat_Ft = Matrix_Transpose(&mat_F);

		st_Track->mat_EstX	= Matrix_Product2(&mat_F, &st_Track->mat_EstX);
		mat_FPFt			= Matrix_Product3(&mat_F, &st_Track->mat_EstP, &mat_Ft);
		st_Track->mat_EstP	= Matrix_Add(&mat_FPFt, &mat_Q);
		f_Trk_Symmetrize(&st_Track->mat_EstP);
		st_Track->stateTime	= time;
	}
}

// 플롯까지 통계적 거리 d^2 = v^T S^-1 v (연관 가정에서 가장 가까운 플롯을 고를 때. 문턱 없음)
FLOAT64 f_Trk_Distance(const ST_Track *st_Track, const ST_TrkPlot *st_Plot)
{
	matrix mat_V;
	matrix mat_S;
	matrix mat_Si;

	f_Trk_Innovation(st_Track, st_Plot, &mat_V, &mat_S);
	mat_Si = Matrix_Inverse(&mat_S);

	return f_Trk_Mahalanobis(&mat_V, &mat_Si);
}

// CMKF 갱신: K = P H^T (H P H^T + R)^-1, X = X + K (z - H X), P = (I - K H) P
// 그때의 잔차, S, NIS 로 게이트 밖 (NIS > TRK_GATE_CHI2) 횟수를 세고, TRK_DIVERGE_NUM 번 이어지면 발산 판정 1 회
VOID f_Trk_Update(ST_Track *st_Track, const ST_TrkPlot *st_Plot)
{
	matrix	mat_H;
	matrix	mat_Ht;
	matrix	mat_V;
	matrix	mat_S;
	matrix	mat_Si;
	matrix	mat_PHt;
	matrix	mat_K;
	matrix	mat_KV;
	matrix	mat_KH;
	matrix	mat_I;
	matrix	mat_IKH;
	INT32	nRow;
	INT32	nCol;

	f_Trk_Innovation(st_Track, st_Plot, &mat_V, &mat_S);
	f_Trk_MakeH(&mat_H);
	mat_Ht	= Matrix_Transpose(&mat_H);
	mat_Si	= Matrix_Inverse(&mat_S);

	// 칼만 게인
	mat_PHt	= Matrix_Product2(&st_Track->mat_EstP, &mat_Ht);
	mat_K	= Matrix_Product2(&mat_PHt, &mat_Si);

	// 추정치
	mat_KV				= Matrix_Product2(&mat_K, &mat_V);
	st_Track->mat_EstX	= Matrix_Add(&st_Track->mat_EstX, &mat_KV);

	// 추정오차공분산
	Matrix_Identity(&mat_I);
	mat_KH				= Matrix_Product2(&mat_K, &mat_H);
	mat_IKH				= Matrix_Subtract(&mat_I, &mat_KH);
	st_Track->mat_EstP	= Matrix_Product2(&mat_IKH, &st_Track->mat_EstP);
	f_Trk_Symmetrize(&st_Track->mat_EstP);

	// 잔차 기록, 게이트 밖 횟수
	for (nRow = 0; nRow < TRK_MEAS_DIM; nRow++)
	{
		st_Track->innov[nRow] = mat_V.e[nRow][0];

		for (nCol = 0; nCol < TRK_MEAS_DIM; nCol++)
		{
			st_Track->innovCov[nRow][nCol] = mat_S.e[nRow][nCol];
		}
	}

	st_Track->nis			= f_Trk_Mahalanobis(&mat_V, &mat_Si);
	st_Track->PlotIdLast	= st_Plot->PlotId;
	st_Track->nUpdateNum	= st_Track->nUpdateNum + 1;

	if (st_Track->nis > TRK_GATE_CHI2)
	{
		st_Track->nGateOutRun = st_Track->nGateOutRun + 1;
		st_Track->nGateOutSum = st_Track->nGateOutSum + 1;

		if (st_Track->nGateOutRun == TRK_DIVERGE_NUM)
		{
			st_Track->nDivergeNum = st_Track->nDivergeNum + 1;
		}
	}
	else
	{
		st_Track->nGateOutRun = 0;
	}
}

// 트랙 요약: 위치, 속도, 가속도, P 대각의 표준편차, 마지막 잔차
VOID f_Trk_GetView(ST_TrkView *st_View, const ST_Track *st_Track)
{
	INT32 nAxis;
	INT32 nAxisB;

	st_View->TrackId		= st_Track->TrackId;
	st_View->TrackState		= st_Track->TrackState;
	st_View->TrackScore		= st_Track->TrackScore;
	st_View->MemoryScore	= st_Track->MemoryScore;
	st_View->Assigned_Flag	= st_Track->Assigned_Flag;
	st_View->nInitScan		= st_Track->nInitScan;
	st_View->PlotId[0]		= st_Track->PlotId[0];
	st_View->PlotId[1]		= st_Track->PlotId[1];
	st_View->PlotIdLast		= st_Track->PlotIdLast;
	st_View->nis			= st_Track->nis;
	st_View->nUpdateNum		= st_Track->nUpdateNum;
	st_View->nGateOutRun	= st_Track->nGateOutRun;
	st_View->nGateOutSum	= st_Track->nGateOutSum;
	st_View->nDivergeNum	= st_Track->nDivergeNum;

	for (nAxis = 0; nAxis < 3; nAxis++)
	{
		st_View->pos[nAxis]		= st_Track->mat_EstX.e[3 * nAxis][0];
		st_View->vel[nAxis]		= st_Track->mat_EstX.e[(3 * nAxis) + 1][0];
		st_View->acc[nAxis]		= st_Track->mat_EstX.e[(3 * nAxis) + 2][0];
		st_View->posStd[nAxis]	= sqrt(st_Track->mat_EstP.e[3 * nAxis][3 * nAxis]);
		st_View->velStd[nAxis]	= sqrt(st_Track->mat_EstP.e[(3 * nAxis) + 1][(3 * nAxis) + 1]);
		st_View->accStd[nAxis]	= sqrt(st_Track->mat_EstP.e[(3 * nAxis) + 2][(3 * nAxis) + 2]);
		st_View->innov[nAxis]	= st_Track->innov[nAxis];

		for (nAxisB = 0; nAxisB < 3; nAxisB++)
		{
			st_View->posCov[nAxis][nAxisB] = st_Track->mat_EstP.e[3 * nAxis][3 * nAxisB];
		}
	}
}

// R 표준편차 (파라미터의 도 -> 라디안)
VOID f_Trk_GetMeasSigma(STRUCT_Coord_Sph *st_Sigma, const ST_TrkParam *st_Param)
{
	st_Sigma->r		= st_Param->sigmaRange;
	st_Sigma->az	= f_Deg_To_Rad(st_Param->sigmaAzimuth);
	st_Sigma->el	= f_Deg_To_Rad(st_Param->sigmaElevation);
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
// 1 삭제 트랙 빼기, 2 예측, 3 연관 (가정) · CMKF 갱신, 4 트랙 관리, 5 남은 플롯으로 2-Point 개시, 6 개시 후보 보관
VOID f_Trk_ProcessScan(ST_TrkTable *st_Table, const ST_TrkPlot *st_Plot, INT32 nPlotNum, FLOAT64 time, INT32 nScan, ST_TrkScanLog *st_Log)
{
	ST_TrkPlot	st_Cur[TRK_MAX_PLOT];
	ST_Track	*st_Track;
	FLOAT64		dt;
	FLOAT64		speed;
	FLOAT64		bestSpeed;
	FLOAT64		bestDistance;
	INT32		nCurNum;
	INT32		nTrack;
	INT32		oldState;
	INT32		nPrev;
	INT32		nCur;
	INT32		bestTrack;
	INT32		bestPrev;
	INT32		bestCur;
	INT32		isSearching;
	UINT32		newId;

	(VOID)memset(st_Log, 0, sizeof(*st_Log));
	nCurNum = (nPlotNum < 0) ? 0 : ((nPlotNum > TRK_MAX_PLOT) ? TRK_MAX_PLOT : nPlotNum);
	st_Log->nPlotNum = nCurNum;

	for (nCur = 0; nCur < nCurNum; nCur++)
	{
		st_Cur[nCur]		= st_Plot[nCur];
		st_Cur[nCur].isUsed	= 0;
	}

	// 1. 삭제 트랙 빼기
	f_Trk_RemoveDeleted(st_Table);

	// 2. 예측 (이번 스캔 시각까지)
	for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
	{
		st_Track				= &st_Table->st_Track[nTrack];
		st_Track->nis			= -1.0;
		st_Track->Assigned_Flag	= 0;
		f_Trk_Predict(st_Track, time, &st_Table->st_Param);

		for (nCur = 0; nCur < nCurNum; nCur++)
		{
			st_Table->distance[nTrack][nCur] = f_Trk_Distance(st_Track, &st_Cur[nCur]);
		}
	}

	// 3. 연관 (가정) 과 갱신: 연관 기법이 없어 플롯은 무조건 연관된 것으로 봄 (게이트 문턱 없음)
	// 플롯이 여럿이면 통계적 거리가 가장 가까운 트랙-플롯 짝부터 하나씩 묶음
	isSearching = 1;

	while (isSearching != 0)
	{
		bestTrack		= -1;
		bestCur			= -1;
		bestDistance	= 0.0;

		for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
		{
			for (nCur = 0; nCur < nCurNum; nCur++)
			{
				if ((st_Table->st_Track[nTrack].Assigned_Flag == 0) && (st_Cur[nCur].isUsed == 0)
					&& ((bestTrack < 0) || (st_Table->distance[nTrack][nCur] < bestDistance)))
				{
					bestDistance	= st_Table->distance[nTrack][nCur];
					bestTrack		= nTrack;
					bestCur			= nCur;
				}
			}
		}

		if (bestTrack < 0)
		{
			isSearching = 0;
		}
		else
		{
			st_Track = &st_Table->st_Track[bestTrack];
			f_Trk_Update(st_Track, &st_Cur[bestCur]);
			st_Cur[bestCur].isUsed	= 1;
			st_Track->Assigned_Flag	= 1;
			st_Log->nUpdatedNum++;

			if (st_Track->nis > TRK_GATE_CHI2)
			{
				st_Log->nGateOutNum++;
			}

			if (st_Track->nGateOutRun == TRK_DIVERGE_NUM)
			{
				st_Log->nDivergedNum++;
				st_Table->nDivergedSum++;
			}
		}
	}

	// 4. 트랙 관리 (스코어 로직)
	for (nTrack = 0; nTrack < st_Table->nTrackNum; nTrack++)
	{
		st_Track = &st_Table->st_Track[nTrack];
		oldState = st_Track->TrackState;
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

	// 5. 2-Point 개시: 트랙이 가져가지 않은 플롯끼리. 게이트 통과 쌍을 속력이 작은 쌍부터, 한 플롯은 한 쌍에만
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
					if (st_Cur[nCur].isUsed == 0)
					{
						st_Log->nPairNum++;

						if (f_Trk_PairSpeed(&st_Table->st_PrevPlot[nPrev], &st_Cur[nCur], dt) < TRK_VMAX)
						{
							st_Log->nGatePassNum++;
						}
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

	// 6. 이번 플롯을 다음 스캔 개시 후보로 보관 (트랙 갱신, 개시에 쓴 플롯은 isUsed = 1 로 빠짐)
	for (nCur = 0; nCur < nCurNum; nCur++)
	{
		st_Table->st_PrevPlot[nCur] = st_Cur[nCur];
	}

	st_Table->nPrevPlotNum	= nCurNum;
	st_Table->prevTime		= time;
	st_Table->isPrevValid	= 1;
}
