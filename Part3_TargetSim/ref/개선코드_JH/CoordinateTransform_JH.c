#include <math.h>

#include "CoordinateTransform_JH.h"

#define COORD_DCM_ELEMENT_COUNT		9

// 원소 9 개를 3 x 3 행렬에 싣는다.
static EN_CoordStatus f_Coord_SetDcm(ST_Matrix *st_Dcm, const FLOAT64 *pt_Element)
{
	EN_CoordStatus enStatus;

	if (f_Mat_Load(st_Dcm, 3, 3, pt_Element, COORD_DCM_ELEMENT_COUNT) == MAT_OK)
	{
		enStatus = COORD_OK;
	}
	else
	{
		enStatus = COORD_ERR_MATRIX;
	}

	return enStatus;
}

// 자세각으로 동체 -> NED 변환 행렬을 만든다. Rz(yaw) Ry(pitch) Rx(roll) 을 전개한 식이다.
EN_CoordStatus f_Coord_Dcm_Body_To_Ned(ST_Matrix *st_Dcm, const ST_CoordAtt *st_Att)
{
	EN_CoordStatus	enStatus;
	FLOAT64			dcm[COORD_DCM_ELEMENT_COUNT];
	FLOAT64			sr;
	FLOAT64			cr;
	FLOAT64			sp;
	FLOAT64			cp;
	FLOAT64			sy;
	FLOAT64			cy;

	if ((st_Dcm == NULL) || (st_Att == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		sr = sin(st_Att->roll);
		cr = cos(st_Att->roll);
		sp = sin(st_Att->pitch);
		cp = cos(st_Att->pitch);
		sy = sin(st_Att->yaw);
		cy = cos(st_Att->yaw);

		// Rz(yaw) * Ry(pitch) * Rx(roll)
		dcm[0] = cy * cp;
		dcm[1] = (cy * sp * sr) - (sy * cr);
		dcm[2] = (cy * sp * cr) + (sy * sr);
		dcm[3] = sy * cp;
		dcm[4] = (sy * sp * sr) + (cy * cr);
		dcm[5] = (sy * sp * cr) - (cy * sr);
		dcm[6] = -sp;
		dcm[7] = cp * sr;
		dcm[8] = cp * cr;

		enStatus = f_Coord_SetDcm(st_Dcm, dcm);
	}

	return enStatus;
}

// 기준 위경도로 NED -> ECEF 변환 행렬을 만든다. 각 열이 그 지점의 북, 동, 아래 단위벡터다.
EN_CoordStatus f_Coord_Dcm_Ned_To_Ecef(ST_Matrix *st_Dcm, FLOAT64 refLat, FLOAT64 refLon)
{
	EN_CoordStatus	enStatus;
	FLOAT64			dcm[COORD_DCM_ELEMENT_COUNT];
	FLOAT64			sLat;
	FLOAT64			cLat;
	FLOAT64			sLon;
	FLOAT64			cLon;

	if (st_Dcm == NULL)
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		sLat = sin(refLat);
		cLat = cos(refLat);
		sLon = sin(refLon);
		cLon = cos(refLon);

		dcm[0] = -sLat * cLon;
		dcm[1] = -sLon;
		dcm[2] = -cLat * cLon;
		dcm[3] = -sLat * sLon;
		dcm[4] = cLon;
		dcm[5] = -cLat * sLon;
		dcm[6] = cLat;
		dcm[7] = 0.0;
		dcm[8] = -sLat;

		enStatus = f_Coord_SetDcm(st_Dcm, dcm);
	}

	return enStatus;
}

// NED 와 ENU 를 오가는 축 치환 행렬.
EN_CoordStatus f_Coord_Dcm_Ned_To_Enu(ST_Matrix *st_Dcm)
{
	// 자기 자신이 역행렬인 치환 행렬이라 ENU -> NED 에도 그대로 쓴다.
	static const FLOAT64 dcm[COORD_DCM_ELEMENT_COUNT] =
	{
		0.0, 1.0,  0.0,
		1.0, 0.0,  0.0,
		0.0, 0.0, -1.0
	};

	EN_CoordStatus enStatus;

	if (st_Dcm == NULL)
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		enStatus = f_Coord_SetDcm(st_Dcm, dcm);
	}

	return enStatus;
}

// 안테나 장착 방위와 기울기로 안테나 -> 동체 변환 행렬을 만든다.
EN_CoordStatus f_Coord_Dcm_Ant_To_Body(ST_Matrix *st_Dcm, FLOAT64 mountYaw, FLOAT64 mountTilt)
{
	EN_CoordStatus	enStatus;
	FLOAT64			dcm[COORD_DCM_ELEMENT_COUNT];
	FLOAT64			sy;
	FLOAT64			cy;
	FLOAT64			st;
	FLOAT64			ct;

	if (st_Dcm == NULL)
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		sy = sin(mountYaw);
		cy = cos(mountYaw);
		st = sin(mountTilt);
		ct = cos(mountTilt);

		// Rz(mountYaw) * Ry(mountTilt), mountTilt 는 보어사이트 상방 기울기가 (+)
		dcm[0] = cy * ct;
		dcm[1] = -sy;
		dcm[2] = cy * st;
		dcm[3] = sy * ct;
		dcm[4] = cy;
		dcm[5] = sy * st;
		dcm[6] = -st;
		dcm[7] = 0.0;
		dcm[8] = ct;

		enStatus = f_Coord_SetDcm(st_Dcm, dcm);
	}

	return enStatus;
}

// 벡터에 행렬을 곱한다. 입출력이 같은 객체여도 된다.
EN_CoordStatus f_Coord_RotateVec(ST_CoordRect *st_Out, const ST_Matrix *st_Dcm, const ST_CoordRect *st_In)
{
	EN_CoordStatus	enStatus;
	FLOAT64			x;
	FLOAT64			y;
	FLOAT64			z;

	if ((st_Out == NULL) || (st_Dcm == NULL) || (st_In == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else if ((st_Dcm->row != 3) || (st_Dcm->column != 3))
	{
		enStatus = COORD_ERR_MATRIX;
	}
	else
	{
		// 입출력이 같은 객체여도 되도록 먼저 복사해 둔다.
		x = st_In->x;
		y = st_In->y;
		z = st_In->z;

		st_Out->x = (st_Dcm->e[0][0] * x) + (st_Dcm->e[0][1] * y) + (st_Dcm->e[0][2] * z);
		st_Out->y = (st_Dcm->e[1][0] * x) + (st_Dcm->e[1][1] * y) + (st_Dcm->e[1][2] * z);
		st_Out->z = (st_Dcm->e[2][0] * x) + (st_Dcm->e[2][1] * y) + (st_Dcm->e[2][2] * z);

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 벡터에 전치 행렬을 곱한다. 정규직교 행렬이라 이것이 역변환이다.
EN_CoordStatus f_Coord_RotateVecInv(ST_CoordRect *st_Out, const ST_Matrix *st_Dcm, const ST_CoordRect *st_In)
{
	EN_CoordStatus	enStatus;
	FLOAT64			x;
	FLOAT64			y;
	FLOAT64			z;

	if ((st_Out == NULL) || (st_Dcm == NULL) || (st_In == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else if ((st_Dcm->row != 3) || (st_Dcm->column != 3))
	{
		enStatus = COORD_ERR_MATRIX;
	}
	else
	{
		x = st_In->x;
		y = st_In->y;
		z = st_In->z;

		// 정규직교 행렬이므로 전치를 곱하는 것이 역변환이다.
		st_Out->x = (st_Dcm->e[0][0] * x) + (st_Dcm->e[1][0] * y) + (st_Dcm->e[2][0] * z);
		st_Out->y = (st_Dcm->e[0][1] * x) + (st_Dcm->e[1][1] * y) + (st_Dcm->e[2][1] * z);
		st_Out->z = (st_Dcm->e[0][2] * x) + (st_Dcm->e[1][2] * y) + (st_Dcm->e[2][2] * z);

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 위경고도를 ECEF 직교좌표로 바꾼다.
EN_CoordStatus f_Trans_Lla_To_Ecef(ST_CoordRect *st_Out, const ST_CoordLla *st_Lla)
{
	EN_CoordStatus	enStatus;
	FLOAT64			sLat;
	FLOAT64			cLat;
	FLOAT64			sLon;
	FLOAT64			cLon;
	FLOAT64			primeVertical;

	if ((st_Out == NULL) || (st_Lla == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		sLat = sin(st_Lla->lat);
		cLat = cos(st_Lla->lat);
		sLon = sin(st_Lla->lon);
		cLon = cos(st_Lla->lon);

		// 이심률 제곱이 1 보다 작으므로 근호 안은 항상 양수다.
		primeVertical = WGS84_A / sqrt(1.0 - (WGS84_E_SQ * sLat * sLat));

		st_Out->x = (primeVertical + st_Lla->alt) * cLat * cLon;
		st_Out->y = (primeVertical + st_Lla->alt) * cLat * sLon;
		st_Out->z = ((primeVertical * (1.0 - WGS84_E_SQ)) + st_Lla->alt) * sLat;

		enStatus = COORD_OK;
	}

	return enStatus;
}

// Bowring 폐형식 해. 보조 위도로 위도를 한 번에 구하므로 반복이 없고 지구 근방에서 mm 수준이다.
// 고도는 1/cos(lat) 대신 p*cos + z*sin 형태를 써서 고위도에서도 발산하지 않는다.
EN_CoordStatus f_Trans_Ecef_To_Lla(ST_CoordLla *st_Out, const ST_CoordRect *st_Ecef)
{
	EN_CoordStatus	enStatus;
	FLOAT64			p;
	FLOAT64			theta;
	FLOAT64			sTheta;
	FLOAT64			cTheta;
	FLOAT64			sLat;
	FLOAT64			cLat;
	FLOAT64			primeVertical;

	if ((st_Out == NULL) || (st_Ecef == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		p = sqrt((st_Ecef->x * st_Ecef->x) + (st_Ecef->y * st_Ecef->y));

		if (p < COORD_POLAR_EPSILON)
		{
			st_Out->lon = 0.0;
			st_Out->lat = (st_Ecef->z >= 0.0) ? HALF_PI : -HALF_PI;
			st_Out->alt = fabs(st_Ecef->z) - WGS84_B;
		}
		else
		{
			st_Out->lon = atan2(st_Ecef->y, st_Ecef->x);

			theta	= atan2(st_Ecef->z * WGS84_A, p * WGS84_B);
			sTheta	= sin(theta);
			cTheta	= cos(theta);

			st_Out->lat = atan2(st_Ecef->z + (WGS84_EP_SQ * WGS84_B * sTheta * sTheta * sTheta),
								p - (WGS84_E_SQ * WGS84_A * cTheta * cTheta * cTheta));

			sLat			= sin(st_Out->lat);
			cLat			= cos(st_Out->lat);
			primeVertical	= WGS84_A / sqrt(1.0 - (WGS84_E_SQ * sLat * sLat));

			st_Out->alt = (p * cLat) + (st_Ecef->z * sLat) - (primeVertical * (1.0 - (WGS84_E_SQ * sLat * sLat)));
		}

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 구면 좌표(거리, 방위, 고각)를 직교좌표로 바꾼다.
EN_CoordStatus f_Trans_Sph_To_Rect(ST_CoordRect *st_Out, const ST_CoordSph *st_Sph)
{
	EN_CoordStatus	enStatus;
	FLOAT64			cEl;

	if ((st_Out == NULL) || (st_Sph == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		cEl = cos(st_Sph->el);

		st_Out->x = st_Sph->r * cEl * cos(st_Sph->az);
		st_Out->y = st_Sph->r * cEl * sin(st_Sph->az);
		st_Out->z = -st_Sph->r * sin(st_Sph->el);

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 직교좌표를 구면 좌표로 바꾼다.
EN_CoordStatus f_Trans_Rect_To_Sph(ST_CoordSph *st_Out, const ST_CoordRect *st_Rect)
{
	EN_CoordStatus	enStatus;
	FLOAT64			groundRange;

	if ((st_Out == NULL) || (st_Rect == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		groundRange = sqrt((st_Rect->x * st_Rect->x) + (st_Rect->y * st_Rect->y));

		st_Out->r	= sqrt((groundRange * groundRange) + (st_Rect->z * st_Rect->z));
		st_Out->az	= atan2(st_Rect->y, st_Rect->x);
		st_Out->el	= atan2(-st_Rect->z, groundRange);

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 구면 좌표를 방향여현 u, v 로 바꾼다.
EN_CoordStatus f_Trans_Sph_To_Uv(ST_CoordUv *st_Out, const ST_CoordSph *st_Sph)
{
	EN_CoordStatus enStatus;

	if ((st_Out == NULL) || (st_Sph == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		st_Out->r = st_Sph->r;
		st_Out->u = cos(st_Sph->el) * sin(st_Sph->az);
		st_Out->v = sin(st_Sph->el);

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 방향여현 u, v 를 구면 좌표로 바꾼다. u^2 + v^2 가 1 을 넘으면 거부한다.
EN_CoordStatus f_Trans_Uv_To_Sph(ST_CoordSph *st_Out, const ST_CoordUv *st_Uv)
{
	EN_CoordStatus	enStatus;
	FLOAT64			wSquared;

	if ((st_Out == NULL) || (st_Uv == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		wSquared = 1.0 - (st_Uv->u * st_Uv->u) - (st_Uv->v * st_Uv->v);

		// wSquared 가 0 이상이면 |v| <= 1 이 보장되어 asin 정의역을 벗어나지 않는다.
		if (wSquared < 0.0)
		{
			enStatus = COORD_ERR_RANGE;
		}
		else
		{
			st_Out->r	= st_Uv->r;
			st_Out->el	= asin(st_Uv->v);
			st_Out->az	= atan2(st_Uv->u, sqrt(wSquared));

			enStatus = COORD_OK;
		}
	}

	return enStatus;
}

// 안테나 좌표의 벡터를 동체 좌표로 바꾼다.
EN_CoordStatus f_Trans_Ant_To_Body(ST_CoordRect *st_Out, const ST_CoordRect *st_Ant, FLOAT64 mountYaw, FLOAT64 mountTilt)
{
	EN_CoordStatus	enStatus;
	ST_Matrix		st_Dcm;

	enStatus = f_Coord_Dcm_Ant_To_Body(&st_Dcm, mountYaw, mountTilt);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Coord_RotateVec(st_Out, &st_Dcm, st_Ant);
	}

	return enStatus;
}

// 동체 좌표의 벡터를 안테나 좌표로 바꾼다.
EN_CoordStatus f_Trans_Body_To_Ant(ST_CoordRect *st_Out, const ST_CoordRect *st_Body, FLOAT64 mountYaw, FLOAT64 mountTilt)
{
	EN_CoordStatus	enStatus;
	ST_Matrix		st_Dcm;

	enStatus = f_Coord_Dcm_Ant_To_Body(&st_Dcm, mountYaw, mountTilt);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Coord_RotateVecInv(st_Out, &st_Dcm, st_Body);
	}

	return enStatus;
}

// 동체 좌표의 벡터를 NED 로 바꾼다. 동체 속도를 ECEF 로 옮기는 1 단계다.
EN_CoordStatus f_Trans_Body_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Body, const ST_CoordAtt *st_Att)
{
	EN_CoordStatus	enStatus;
	ST_Matrix		st_Dcm;

	enStatus = f_Coord_Dcm_Body_To_Ned(&st_Dcm, st_Att);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Coord_RotateVec(st_Out, &st_Dcm, st_Body);
	}

	return enStatus;
}

// NED 벡터를 동체 좌표로 바꾼다.
EN_CoordStatus f_Trans_Ned_To_Body(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned, const ST_CoordAtt *st_Att)
{
	EN_CoordStatus	enStatus;
	ST_Matrix		st_Dcm;

	enStatus = f_Coord_Dcm_Body_To_Ned(&st_Dcm, st_Att);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Coord_RotateVecInv(st_Out, &st_Dcm, st_Ned);
	}

	return enStatus;
}

// NED 벡터를 ENU 로 바꾼다 (축 치환).
EN_CoordStatus f_Trans_Ned_To_Enu(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned)
{
	EN_CoordStatus	enStatus;
	FLOAT64			north;
	FLOAT64			east;
	FLOAT64			down;

	if ((st_Out == NULL) || (st_Ned == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		north	= st_Ned->x;
		east	= st_Ned->y;
		down	= st_Ned->z;

		st_Out->x = east;
		st_Out->y = north;
		st_Out->z = -down;

		enStatus = COORD_OK;
	}

	return enStatus;
}

// ENU 벡터를 NED 로 바꾼다 (축 치환).
EN_CoordStatus f_Trans_Enu_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Enu)
{
	EN_CoordStatus	enStatus;
	FLOAT64			east;
	FLOAT64			north;
	FLOAT64			up;

	if ((st_Out == NULL) || (st_Enu == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		east	= st_Enu->x;
		north	= st_Enu->y;
		up		= st_Enu->z;

		st_Out->x = north;
		st_Out->y = east;
		st_Out->z = -up;

		enStatus = COORD_OK;
	}

	return enStatus;
}

// NED 벡터를 ECEF 로 회전한다. 기준점을 더하지 않으므로 속도 같은 벡터에 쓴다.
EN_CoordStatus f_Trans_NedVec_To_Ecef(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned, FLOAT64 refLat, FLOAT64 refLon)
{
	EN_CoordStatus	enStatus;
	ST_Matrix		st_Dcm;

	enStatus = f_Coord_Dcm_Ned_To_Ecef(&st_Dcm, refLat, refLon);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Coord_RotateVec(st_Out, &st_Dcm, st_Ned);
	}

	return enStatus;
}

// ECEF 벡터를 NED 로 회전한다. 기준점을 빼지 않는다.
EN_CoordStatus f_Trans_EcefVec_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Ecef, FLOAT64 refLat, FLOAT64 refLon)
{
	EN_CoordStatus	enStatus;
	ST_Matrix		st_Dcm;

	enStatus = f_Coord_Dcm_Ned_To_Ecef(&st_Dcm, refLat, refLon);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Coord_RotateVecInv(st_Out, &st_Dcm, st_Ecef);
	}

	return enStatus;
}

// NED 점을 ECEF 점으로 바꾼다. 회전한 뒤 기준점을 더한다.
EN_CoordStatus f_Trans_Ned_To_Ecef(ST_CoordRect *st_Out, const ST_CoordRect *st_Ned, const ST_CoordRect *st_RefEcef, FLOAT64 refLat, FLOAT64 refLon)
{
	EN_CoordStatus	enStatus;
	ST_CoordRect	st_Rotated;

	if (st_RefEcef == NULL)
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		enStatus = f_Trans_NedVec_To_Ecef(&st_Rotated, st_Ned, refLat, refLon);

		if (enStatus == COORD_OK)
		{
			enStatus = f_Coord_VecAdd(st_Out, &st_Rotated, st_RefEcef);
		}
	}

	return enStatus;
}

// ECEF 점을 NED 점으로 바꾼다. 기준점을 뺀 뒤 역회전한다.
EN_CoordStatus f_Trans_Ecef_To_Ned(ST_CoordRect *st_Out, const ST_CoordRect *st_Ecef, const ST_CoordRect *st_RefEcef, FLOAT64 refLat, FLOAT64 refLon)
{
	EN_CoordStatus	enStatus;
	ST_CoordRect	st_Delta;

	enStatus = f_Coord_VecSub(&st_Delta, st_Ecef, st_RefEcef);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Trans_EcefVec_To_Ned(st_Out, &st_Delta, refLat, refLon);
	}

	return enStatus;
}

// 동체 기준 장착 오프셋을 ECEF 변위로 바꾼다.
EN_CoordStatus f_Coord_LeverArm_Body_To_Ecef(ST_CoordRect *st_Out, const ST_CoordRect *st_BodyOffset, const ST_CoordAtt *st_Att, FLOAT64 refLat, FLOAT64 refLon)
{
	EN_CoordStatus	enStatus;
	ST_CoordRect	st_Ned;

	enStatus = f_Trans_Body_To_Ned(&st_Ned, st_BodyOffset, st_Att);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Trans_NedVec_To_Ecef(st_Out, &st_Ned, refLat, refLon);
	}

	return enStatus;
}

// 벡터 덧셈.
EN_CoordStatus f_Coord_VecAdd(ST_CoordRect *st_Out, const ST_CoordRect *st_Lhs, const ST_CoordRect *st_Rhs)
{
	EN_CoordStatus enStatus;

	if ((st_Out == NULL) || (st_Lhs == NULL) || (st_Rhs == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		st_Out->x = st_Lhs->x + st_Rhs->x;
		st_Out->y = st_Lhs->y + st_Rhs->y;
		st_Out->z = st_Lhs->z + st_Rhs->z;

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 벡터 뺄셈.
EN_CoordStatus f_Coord_VecSub(ST_CoordRect *st_Out, const ST_CoordRect *st_Lhs, const ST_CoordRect *st_Rhs)
{
	EN_CoordStatus enStatus;

	if ((st_Out == NULL) || (st_Lhs == NULL) || (st_Rhs == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		st_Out->x = st_Lhs->x - st_Rhs->x;
		st_Out->y = st_Lhs->y - st_Rhs->y;
		st_Out->z = st_Lhs->z - st_Rhs->z;

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 벡터 상수배.
EN_CoordStatus f_Coord_VecScale(ST_CoordRect *st_Out, const ST_CoordRect *st_Src, FLOAT64 scalar)
{
	EN_CoordStatus enStatus;

	if ((st_Out == NULL) || (st_Src == NULL))
	{
		enStatus = COORD_ERR_NULL;
	}
	else
	{
		st_Out->x = st_Src->x * scalar;
		st_Out->y = st_Src->y * scalar;
		st_Out->z = st_Src->z * scalar;

		enStatus = COORD_OK;
	}

	return enStatus;
}

// 벡터 크기. 인자가 NULL 이면 0 이다.
FLOAT64 f_Coord_VecNorm(const ST_CoordRect *st_Vec)
{
	FLOAT64 norm;

	if (st_Vec == NULL)
	{
		norm = 0.0;
	}
	else
	{
		norm = sqrt((st_Vec->x * st_Vec->x) + (st_Vec->y * st_Vec->y) + (st_Vec->z * st_Vec->z));
	}

	return norm;
}

// 각도를 (-PI, PI] 로 접는다. 자세각이 무한정 커지지 않게 한다.
FLOAT64 f_Coord_WrapAngle(FLOAT64 angle)
{
	FLOAT64 wrapped;

	wrapped = fmod(angle + PI, TWO_PI);

	if (wrapped < 0.0)
	{
		wrapped += TWO_PI;
	}

	return wrapped - PI;
}

// 좌표 변환 상태 코드의 이름 문자열.
const CHAR *f_Coord_StatusStr(EN_CoordStatus status)
{
	const CHAR *text;

	switch (status)
	{
	case COORD_OK:
		text = "COORD_OK";
		break;

	case COORD_ERR_NULL:
		text = "COORD_ERR_NULL";
		break;

	case COORD_ERR_RANGE:
		text = "COORD_ERR_RANGE";
		break;

	case COORD_ERR_MATRIX:
		text = "COORD_ERR_MATRIX";
		break;

	default:
		text = "COORD_ERR_UNKNOWN";
		break;
	}

	return text;
}
