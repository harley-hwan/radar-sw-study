#ifndef MATRIX_CALC_LIB_JH_H
#define MATRIX_CALC_LIB_JH_H

#include "Define_JH.h"
#include "TargetSimCoreApi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAT_MAX_DIM				9
#define MAT_SINGULAR_EPSILON	1.0e-12

typedef enum
{
	MAT_OK = 0,
	MAT_ERR_NULL,
	MAT_ERR_DIM,
	MAT_ERR_NOT_SQUARE,
	MAT_ERR_SINGULAR
} EN_MatStatus;

typedef struct
{
	FLOAT64		e[MAT_MAX_DIM][MAT_MAX_DIM];
	INT32		row;
	INT32		column;
} ST_Matrix;

TSCORE_API EN_MatStatus		f_Mat_Init(ST_Matrix *st_Mat, INT32 row, INT32 column);
TSCORE_API EN_MatStatus		f_Mat_Identity(ST_Matrix *st_Mat, INT32 dim);
TSCORE_API EN_MatStatus		f_Mat_Load(ST_Matrix *st_Mat, INT32 row, INT32 column, const FLOAT64 *pt_Element, INT32 elementCount);
TSCORE_API EN_MatStatus		f_Mat_Copy(ST_Matrix *st_Out, const ST_Matrix *st_Src);

TSCORE_API EN_MatStatus		f_Mat_Add(ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs);
TSCORE_API EN_MatStatus		f_Mat_Sub(ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs);
TSCORE_API EN_MatStatus		f_Mat_Scale(ST_Matrix *st_Out, const ST_Matrix *st_Src, FLOAT64 scalar);
TSCORE_API EN_MatStatus		f_Mat_Mul(ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs);
TSCORE_API EN_MatStatus		f_Mat_Mul3(ST_Matrix *st_Out, const ST_Matrix *st_A, const ST_Matrix *st_B, const ST_Matrix *st_C);
TSCORE_API EN_MatStatus		f_Mat_Transpose(ST_Matrix *st_Out, const ST_Matrix *st_Src);
TSCORE_API EN_MatStatus		f_Mat_Inverse(ST_Matrix *st_Out, const ST_Matrix *st_Src);
TSCORE_API EN_MatStatus		f_Mat_Solve(ST_Matrix *st_Out, const ST_Matrix *st_A, const ST_Matrix *st_B);
TSCORE_API EN_MatStatus		f_Mat_Det(const ST_Matrix *st_Src, FLOAT64 *pt_Det);

TSCORE_API INT32			f_Mat_IsEqual(const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs, FLOAT64 tolerance);
TSCORE_API const CHAR *		f_Mat_StatusStr(EN_MatStatus status);

#ifdef __cplusplus
}
#endif

#endif
