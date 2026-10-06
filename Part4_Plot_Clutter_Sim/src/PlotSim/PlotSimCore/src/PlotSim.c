//
// @file	PlotSim.c
// @brief	난수 검증, 측정 표준편차, 좌표 변환, 플롯 / 클러터 생성.
//			난수는 참고 소스 NoiseGeneration 의 UNIRAN, GAUSS 를 그대로 씀.
// @author	hwan
// @date	2026.10.06.
//
#include <math.h>
#include <string.h>

#include "PlotSim.h"
#include "NoiseGeneration.h"

// 히스토그램 채우기. isGauss 가 0 이면 UNIRAN, 아니면 GAUSS(0, 1) 로 nSample 개 뽑음.
static INT32 f_Pcs_FillHist(ST_PcsHist *st_Hist, INT32 nSample, INT32 nBin, FLOAT64 binMin, FLOAT64 binMax, INT32 isGauss)
{
	FLOAT64	sum = 0.0;
	FLOAT64	sumSq = 0.0;
	FLOAT64	value;
	INT32	nBinIndex;
	INT32	nIndex;

	if ((st_Hist == NULL) || (nSample < 1) || (nBin < 1) || (nBin > PCS_HIST_MAX_BIN) || !(binMax > binMin))
	{
		return PCS_FAIL;
	}

	(VOID)memset(st_Hist, 0, sizeof(*st_Hist));
	st_Hist->nSample	= nSample;
	st_Hist->nBin		= nBin;
	st_Hist->binMin		= binMin;
	st_Hist->binMax		= binMax;
	st_Hist->binWidth	= (binMax - binMin) / (FLOAT64)nBin;
	st_Hist->minValue	= HUGE_VAL;
	st_Hist->maxValue	= -HUGE_VAL;

	for (nIndex = 0; nIndex < nSample; nIndex++)
	{
		value = (isGauss != 0) ? GAUSS(0.0, 1.0) : UNIRAN();

		sum		= sum + value;
		sumSq	= sumSq + (value * value);

		if (value < st_Hist->minValue)
		{
			st_Hist->minValue = value;
		}

		if (value > st_Hist->maxValue)
		{
			st_Hist->maxValue = value;
		}

		if ((value < binMin) || (value > binMax))
		{
			st_Hist->nOutside++;
		}
		else
		{
			// binMax 와 같은 값은 마지막 bin 에 넣음
			nBinIndex = (INT32)floor((value - binMin) / st_Hist->binWidth);

			if (nBinIndex >= nBin)
			{
				nBinIndex = nBin - 1;
			}

			st_Hist->nCount[nBinIndex]++;
		}
	}

	// PDF = 개수 / (N * bin 폭). 막대 넓이 합이 1 이 되게 맞춤
	for (nIndex = 0; nIndex < nBin; nIndex++)
	{
		st_Hist->pdf[nIndex] = (FLOAT64)st_Hist->nCount[nIndex] / ((FLOAT64)nSample * st_Hist->binWidth);
	}

	st_Hist->mean	= sum / (FLOAT64)nSample;
	st_Hist->std	= sqrt(fmax(0.0, (sumSq / (FLOAT64)nSample) - (st_Hist->mean * st_Hist->mean)));

	return PCS_PASS;
}

//
// @brief	난수 seed 설정
// @param	seed	1 ~ 2147483646. 0 이나 2147483647 의 배수면 UNIRAN 이 0 만 나옴
// @return	없음
// @author	hwan
//
VOID f_Pcs_SetSeed(INT32 seed)
{
	changeSEED((long)seed);
}

//
// @brief	현재 seed (UNIRAN 내부 상태)
// @return	seed
// @author	hwan
//
INT32 f_Pcs_GetSeed(VOID)
{
	return (INT32)valueSEED();
}

//
// @brief	UNIRAN 균등난수 검증. [0, 1] 히스토그램을 PDF 로 정규화
// @param	st_Hist	결과
// @param	nSample	표본 수
// @param	nBin	bin 수
// @return	PCS_PASS / PCS_FAIL (인자 오류)
// @author	hwan
//
INT32 f_Pcs_TestUniform(ST_PcsHist *st_Hist, INT32 nSample, INT32 nBin)
{
	return f_Pcs_FillHist(st_Hist, nSample, nBin, 0.0, 1.0, 0);
}

//
// @brief	GAUSS(0, 1) 정규난수 검증. [binMin, binMax] 히스토그램을 PDF 로 정규화
// @param	st_Hist	결과
// @param	nSample	표본 수
// @param	nBin	bin 수
// @param	binMin	구간 하한
// @param	binMax	구간 상한
// @return	PCS_PASS / PCS_FAIL (인자 오류)
// @author	hwan
//
INT32 f_Pcs_TestGauss(ST_PcsHist *st_Hist, INT32 nSample, INT32 nBin, FLOAT64 binMin, FLOAT64 binMax)
{
	return f_Pcs_FillHist(st_Hist, nSample, nBin, binMin, binMax, 1);
}

//
// @brief	거리 / 방위각 / 고각 측정 표준편차
//			p_rng = c / (2 * B * sqrt(2 * SNR)), p_azi = 빔폭 / (K_M * sqrt(2 * SNR)), p_ele 도 같은 식
//			과제 원문은 square(2 * SNR) 이지만 제곱근이 맞음 (제곱이면 p_rng 가 0.019 m 로 잡음이 거의 없어짐)
// @param	st_Sigma	결과
// @param	st_Radar	레이다 파라미터
// @return	PCS_PASS / PCS_FAIL (SNR, 대역폭, K_M 이 0 이하)
// @author	hwan
//
INT32 f_Pcs_CalcSigma(ST_PcsSigma *st_Sigma, const ST_PcsRadar *st_Radar)
{
	FLOAT64 snr;
	FLOAT64 root;

	if ((st_Sigma == NULL) || (st_Radar == NULL))
	{
		return PCS_FAIL;
	}

	snr = (st_Radar->enSnrUnit == PCS_SNR_DB) ? pow(10.0, st_Radar->snr / 10.0) : st_Radar->snr;

	if ((snr <= 0.0) || (st_Radar->bandwidth <= 0.0) || (st_Radar->kmAzi <= 0.0) || (st_Radar->kmEle <= 0.0))
	{
		return PCS_FAIL;
	}

	root				= sqrt(2.0 * snr);
	st_Sigma->snrLinear	= snr;
	st_Sigma->rng		= st_Radar->lightSpeed / (2.0 * st_Radar->bandwidth * root);
	st_Sigma->azi		= st_Radar->beamAzi / (st_Radar->kmAzi * root);
	st_Sigma->ele		= st_Radar->beamEle / (st_Radar->kmEle * root);

	return PCS_PASS;
}

//
// @brief	표적 ECEF 위치 -> 레이다 기준 거리 / 방위각 / 고각 (참값 x)
//			ECEF -> NED (레이다 위치 기준) 로 바꾼 뒤 방위각 = atan2(동, 북), 고각 = atan2(위, 수평거리)
// @param	st_TgtEcef	표적 ECEF 위치 [m]
// @param	st_Radar	레이다(플랫폼) 상태
// @return	구좌표
// @author	hwan
//
ST_PcsSph f_Pcs_EcefToSph(const STRUCT_Coord_Rect *st_TgtEcef, const ST_TargetState *st_Radar)
{
	STRUCT_Coord_Rect	st_Ned;
	ST_PcsSph			st_Sph;
	FLOAT64				horizontal;

	st_Ned = f_Trans_Ecef_To_Ned(st_TgtEcef->x, st_TgtEcef->y, st_TgtEcef->z, st_Radar->st_PosEcef.x, st_Radar->st_PosEcef.y,
		st_Radar->st_PosEcef.z, st_Radar->st_Lla.Lat, st_Radar->st_Lla.Lon);

	horizontal	= sqrt((st_Ned.x * st_Ned.x) + (st_Ned.y * st_Ned.y));
	st_Sph.rng	= sqrt((horizontal * horizontal) + (st_Ned.z * st_Ned.z));
	st_Sph.azi	= f_Rad_To_Deg(atan2(st_Ned.y, st_Ned.x));
	st_Sph.ele	= f_Rad_To_Deg(atan2(-st_Ned.z, horizontal));

	return st_Sph;
}

//
// @brief	레이다 기준 거리 / 방위각 / 고각 -> LLA (전시용)
//			동 = r cos(고각) sin(방위각), 북 = r cos(고각) cos(방위각), 위 = r sin(고각)
//			NED (북, 동, -위) -> ECEF -> LLA
// @param	st_Sph		구좌표
// @param	st_Radar	레이다(플랫폼) 상태
// @return	위도, 경도 [rad], 고도 [m]
// @author	hwan
//
STRUCT_Coord_Lla f_Pcs_SphToLla(const ST_PcsSph *st_Sph, const ST_TargetState *st_Radar)
{
	STRUCT_Coord_Rect	st_Ecef;
	FLOAT64				azi;
	FLOAT64				ele;
	FLOAT64				east;
	FLOAT64				north;
	FLOAT64				up;

	azi		= f_Deg_To_Rad(st_Sph->azi);
	ele		= f_Deg_To_Rad(st_Sph->ele);
	east	= st_Sph->rng * cos(ele) * sin(azi);
	north	= st_Sph->rng * cos(ele) * cos(azi);
	up		= st_Sph->rng * sin(ele);

	st_Ecef = f_Trans_Ned_To_Ecef(north, east, -up, st_Radar->st_PosEcef.x, st_Radar->st_PosEcef.y, st_Radar->st_PosEcef.z,
		st_Radar->st_Lla.Lat, st_Radar->st_Lla.Lon);

	return f_Trans_Ecef_To_Lla(st_Ecef.x, st_Ecef.y, st_Ecef.z);
}

//
// @brief	플롯 z = x + v, v ~ N(0, p^2). 성분마다 GAUSS 를 따로 부름 (거리, 방위각, 고각 순서)
// @param	st_True		참값 x
// @param	st_Sigma	측정 표준편차 p
// @return	플롯 z. 방위각은 -180 ~ 180 으로 맞춤
// @author	hwan
//
ST_PcsSph f_Pcs_MakePlot(const ST_PcsSph *st_True, const ST_PcsSigma *st_Sigma)
{
	ST_PcsSph st_Plot;

	st_Plot.rng = st_True->rng + GAUSS(0.0, st_Sigma->rng);
	st_Plot.azi = st_True->azi + GAUSS(0.0, st_Sigma->azi);
	st_Plot.ele = st_True->ele + GAUSS(0.0, st_Sigma->ele);

	if (st_Plot.azi > 180.0)
	{
		st_Plot.azi = st_Plot.azi - 360.0;
	}
	else if (st_Plot.azi <= -180.0)
	{
		st_Plot.azi = st_Plot.azi + 360.0;
	}
	else
	{
		// 범위 안
	}

	return st_Plot;
}

//
// @brief	스캔 하나의 클러터. FOV 안에서 성분마다 c = min + (max - min) * UNIRAN()
// @param	st_Clutter	결과 배열 (PCS_MAX_CLUTTER 개)
// @param	st_Cfg		클러터 설정
// @return	만든 개수
// @author	hwan
//
INT32 f_Pcs_MakeClutter(ST_PcsSph *st_Clutter, const ST_PcsClutterCfg *st_Cfg)
{
	INT32 nClutterNum = st_Cfg->nClutterNum;
	INT32 nClutter;

	// RANDOM: 0 ~ N 중 하나. UNIRAN 은 1 이 안 나오므로 N + 1 을 곱하고 버림
	if (st_Cfg->enMode == PCS_CLUTTER_RANDOM)
	{
		nClutterNum = (INT32)(UNIRAN() * (FLOAT64)(st_Cfg->nClutterNum + 1));
	}

	if (nClutterNum > PCS_MAX_CLUTTER)
	{
		nClutterNum = PCS_MAX_CLUTTER;
	}
	else if (nClutterNum < 0)
	{
		nClutterNum = 0;
	}
	else
	{
		// 범위 안
	}

	for (nClutter = 0; nClutter < nClutterNum; nClutter++)
	{
		st_Clutter[nClutter].rng = st_Cfg->rngMin + ((st_Cfg->rngMax - st_Cfg->rngMin) * UNIRAN());
		st_Clutter[nClutter].azi = st_Cfg->aziMin + ((st_Cfg->aziMax - st_Cfg->aziMin) * UNIRAN());
		st_Clutter[nClutter].ele = st_Cfg->eleMin + ((st_Cfg->eleMax - st_Cfg->eleMin) * UNIRAN());
	}

	return nClutterNum;
}
