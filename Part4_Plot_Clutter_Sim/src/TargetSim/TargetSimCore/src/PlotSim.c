#include <math.h>
#include <string.h>

#include "PlotSim.h"
#include "NoiseGeneration.h"

// isGauss 가 0 이면 UNIRAN, 아니면 GAUSS(0, 1) 히스토그램.
static VOID f_Pcs_FillHist(ST_PcsHist *st_Hist, INT32 nBin, FLOAT64 binMin, FLOAT64 binMax, INT32 isGauss)
{
	INT32	nCount[PCS_GAUSS_BIN] = { 0 };
	FLOAT64	sum = 0.0;
	FLOAT64	sumSq = 0.0;
	FLOAT64	value;
	INT32	nBinIndex;
	INT32	nIndex;

	(VOID)memset(st_Hist, 0, sizeof(*st_Hist));
	st_Hist->nBin		= nBin;
	st_Hist->binMin		= binMin;
	st_Hist->binWidth	= (binMax - binMin) / (FLOAT64)nBin;

	for (nIndex = 0; nIndex < PCS_RAND_NUM; nIndex++)
	{
		value	= (isGauss != 0) ? GAUSS(0.0, 1.0) : UNIRAN();
		sum		= sum + value;
		sumSq	= sumSq + (value * value);

		// 구간 밖 값은 세지 않음 (GAUSS 의 |x| > 4)
		nBinIndex = (INT32)floor((value - binMin) / st_Hist->binWidth);

		if ((nBinIndex >= 0) && (nBinIndex < nBin))
		{
			nCount[nBinIndex]++;
		}
	}

	for (nIndex = 0; nIndex < nBin; nIndex++)
	{
		st_Hist->pdf[nIndex] = (FLOAT64)nCount[nIndex] / ((FLOAT64)PCS_RAND_NUM * st_Hist->binWidth);
	}

	st_Hist->mean	= sum / (FLOAT64)PCS_RAND_NUM;
	st_Hist->std	= sqrt((sumSq / (FLOAT64)PCS_RAND_NUM) - (st_Hist->mean * st_Hist->mean));
}

// ECEF -> NED -> 동체 -> 안테나 -> 구좌표.
static STRUCT_Coord_Sph f_Pcs_EcefToSph(const STRUCT_Coord_Rect *st_Ecef, const ST_TargetState *st_Radar)
{
	const STRUCT_Coord_Attitude	*st_Att = &st_Radar->st_Att;
	STRUCT_Coord_Rect			st_Ned;
	STRUCT_Coord_Rect			st_Body;
	STRUCT_Coord_Rect			st_Ant;

	st_Ned	= f_Trans_Ecef_To_Ned(st_Ecef->x, st_Ecef->y, st_Ecef->z, st_Radar->st_PosEcef.x, st_Radar->st_PosEcef.y, st_Radar->st_PosEcef.z,
		st_Radar->st_Lla.Lat, st_Radar->st_Lla.Lon);
	st_Body	= f_Trans_Ned_To_Body(st_Ned.x, st_Ned.y, st_Ned.z, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);
	st_Ant	= f_Trans_Body_To_Ant(st_Body.x, st_Body.y, st_Body.z);

	return f_Trans_Ant_XYZ_To_Sph(st_Ant.x, st_Ant.y, st_Ant.z);
}

// 구좌표 -> 안테나 -> 동체 -> NED -> ECEF -> LLA.
static STRUCT_Coord_Lla f_Pcs_SphToLla(const STRUCT_Coord_Sph *st_Sph, const ST_TargetState *st_Radar)
{
	const STRUCT_Coord_Attitude	*st_Att = &st_Radar->st_Att;
	STRUCT_Coord_Rect			st_Ant;
	STRUCT_Coord_Rect			st_Body;
	STRUCT_Coord_Rect			st_Ned;
	STRUCT_Coord_Rect			st_Ecef;

	st_Ant	= f_Trans_Ant_Sph_To_XYZ(st_Sph->r, st_Sph->az, st_Sph->el);
	st_Body	= f_Trans_Ant_To_Body(st_Ant.x, st_Ant.y, st_Ant.z);
	st_Ned	= f_Trans_Body_To_Ned(st_Body.x, st_Body.y, st_Body.z, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);
	st_Ecef	= f_Trans_Ned_To_Ecef(st_Ned.x, st_Ned.y, st_Ned.z, st_Radar->st_PosEcef.x, st_Radar->st_PosEcef.y, st_Radar->st_PosEcef.z,
		st_Radar->st_Lla.Lat, st_Radar->st_Lla.Lon);

	return f_Trans_Ecef_To_Lla(st_Ecef.x, st_Ecef.y, st_Ecef.z);
}

// 난수 검증. 검증마다 seed 를 처음 값으로 맞춤.
VOID f_Pcs_TestRand(ST_PcsHist *st_Uniform, ST_PcsHist *st_Gauss)
{
	changeSEED(PCS_SEED);
	f_Pcs_FillHist(st_Uniform, PCS_UNIFORM_BIN, 0.0, 1.0, 0);

	changeSEED(PCS_SEED);
	f_Pcs_FillHist(st_Gauss, PCS_GAUSS_BIN, PCS_GAUSS_MIN, PCS_GAUSS_MAX, 1);
}

// 측정 표준편차 p. snr 은 선형값.
// p_rng = c / (2 * B * sqrt(2 * SNR)), p_azi = 빔폭 / (K_M * sqrt(2 * SNR)), p_ele 도 같은 식.
STRUCT_Coord_Sph f_Pcs_CalcSigma(FLOAT64 snr)
{
	const FLOAT64		root = sqrt(2.0 * snr);
	STRUCT_Coord_Sph	st_Sigma;

	st_Sigma.r	= LIGHT_SPEED / (2.0 * PCS_BANDWIDTH * root);
	st_Sigma.az	= f_Deg_To_Rad(PCS_BEAM_AZI) / (PCS_KM * root);
	st_Sigma.el	= f_Deg_To_Rad(PCS_BEAM_ELE) / (PCS_KM * root);

	return st_Sigma;
}

// Part 3 결과 표본 nScanNum 개를 스캔마다 잼. 레이다는 그 표본의 플랫폼. 클러터 수는 0 ~ PCS_MAX_CLUTTER 로 맞춤.
// 난수 순서: seed 를 처음 값으로 맞춘 뒤 스캔마다 표적 1 ~ N 플롯 (거리, 방위각, 고각), 그다음 클러터.
VOID f_Pcs_Measure(ST_PcsScan *st_Scan, const ST_SimSample *st_Sample, INT32 nScanNum, FLOAT64 snr, INT32 nClutterNum)
{
	const STRUCT_Coord_Sph	st_Sigma = f_Pcs_CalcSigma(snr);
	const ST_SimSample		*st_In;
	const ST_TargetState	*st_Radar;
	ST_PcsScan				*st_Out;
	STRUCT_Coord_Sph		*st_Clutter;
	INT32					nScan;
	INT32					nTarget;
	INT32					nClutter;

	nClutterNum = (nClutterNum < 0) ? 0 : ((nClutterNum > PCS_MAX_CLUTTER) ? PCS_MAX_CLUTTER : nClutterNum);
	changeSEED(PCS_SEED);

	for (nScan = 0; nScan < nScanNum; nScan++)
	{
		st_In		= &st_Sample[nScan];
		st_Radar	= &st_In->st_Platform;
		st_Out		= &st_Scan[nScan];

		st_Out->time		= st_In->simTime;
		st_Out->nTargetNum	= st_In->nTargetNum;

		for (nTarget = 0; nTarget < st_In->nTargetNum; nTarget++)
		{
			// 참값 x
			st_Out->st_True[nTarget] = f_Pcs_EcefToSph(&st_In->st_Target[nTarget].st_PosEcef, st_Radar);

			// 플롯 z = x + v, v ~ N(0, p^2)
			st_Out->st_Plot[nTarget].r	= st_Out->st_True[nTarget].r + GAUSS(0.0, st_Sigma.r);
			st_Out->st_Plot[nTarget].az	= st_Out->st_True[nTarget].az + GAUSS(0.0, st_Sigma.az);
			st_Out->st_Plot[nTarget].el	= st_Out->st_True[nTarget].el + GAUSS(0.0, st_Sigma.el);
			st_Out->st_PlotLla[nTarget]	= f_Pcs_SphToLla(&st_Out->st_Plot[nTarget], st_Radar);
		}

		// 클러터: FOV 안 균등분포, 매 스캔 새로 뽑음
		st_Out->nClutterNum = nClutterNum;

		for (nClutter = 0; nClutter < nClutterNum; nClutter++)
		{
			st_Clutter		= &st_Out->st_Clutter[nClutter];
			st_Clutter->r	= PCS_CLT_RNG_MIN + ((PCS_CLT_RNG_MAX - PCS_CLT_RNG_MIN) * UNIRAN());
			st_Clutter->az	= f_Deg_To_Rad(PCS_CLT_AZI_MIN + ((PCS_CLT_AZI_MAX - PCS_CLT_AZI_MIN) * UNIRAN()));
			st_Clutter->el	= f_Deg_To_Rad(PCS_CLT_ELE_MIN + ((PCS_CLT_ELE_MAX - PCS_CLT_ELE_MIN) * UNIRAN()));
			st_Out->st_ClutterLla[nClutter] = f_Pcs_SphToLla(st_Clutter, st_Radar);
		}
	}
}
