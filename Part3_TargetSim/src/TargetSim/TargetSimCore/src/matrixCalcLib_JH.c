#include <math.h>

#include "matrixCalcLib_JH.h"

static EN_MatStatus f_Mat_Validate(const ST_Matrix *st_Mat)
{
	EN_MatStatus enStatus;

	if (st_Mat == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else if ((st_Mat->row < 1) || (st_Mat->row > MAT_MAX_DIM) ||
			 (st_Mat->column < 1) || (st_Mat->column > MAT_MAX_DIM))
	{
		enStatus = MAT_ERR_DIM;
	}
	else
	{
		enStatus = MAT_OK;
	}

	return enStatus;
}

static EN_MatStatus f_Mat_ValidateOperands(const ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs)
{
	EN_MatStatus enStatus;

	if (st_Out == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else
	{
		enStatus = f_Mat_Validate(st_Lhs);

		if (enStatus == MAT_OK)
		{
			enStatus = f_Mat_Validate(st_Rhs);
		}
	}

	return enStatus;
}

// 특이 판정 임계값을 원소 최대 절댓값에 비례시켜 행렬 스케일에 둔감하도록 한다.
static FLOAT64 f_Mat_GetTolerance(const ST_Matrix *st_Mat)
{
	FLOAT64		maxAbs = 0.0;
	FLOAT64		absVal;
	FLOAT64		tolerance;
	INT32		i;
	INT32		j;

	for (i = 0; i < st_Mat->row; i++)
	{
		for (j = 0; j < st_Mat->column; j++)
		{
			absVal = fabs(st_Mat->e[i][j]);

			if (absVal > maxAbs)
			{
				maxAbs = absVal;
			}
		}
	}

	if (maxAbs > 1.0)
	{
		tolerance = MAT_SINGULAR_EPSILON * maxAbs;
	}
	else
	{
		tolerance = MAT_SINGULAR_EPSILON;
	}

	return tolerance;
}

// 증강행렬 [A | B] (n x (n+m)) 에 대한 Gauss-Jordan 소거.
// 매 단계에서 절댓값이 가장 큰 행을 피벗으로 교환해 작은 피벗으로 나눌 때의 오차 증폭을 막는다.
// 정상 종료 시 좌측 n x n 은 단위행렬, 우측 n x m 이 해가 된다.
static EN_MatStatus f_Mat_GaussJordan(FLOAT64 aug[][2 * MAT_MAX_DIM], INT32 n, INT32 m, FLOAT64 tolerance)
{
	EN_MatStatus	enStatus = MAT_OK;
	INT32			total = n + m;
	INT32			col;
	INT32			row;
	INT32			k;
	INT32			pivotRow;
	FLOAT64			pivotVal;
	FLOAT64			factor;
	FLOAT64			swapVal;

	for (col = 0; col < n; col++)
	{
		if (enStatus == MAT_OK)
		{
			pivotRow = col;

			for (row = col + 1; row < n; row++)
			{
				if (fabs(aug[row][col]) > fabs(aug[pivotRow][col]))
				{
					pivotRow = row;
				}
			}

			if (fabs(aug[pivotRow][col]) <= tolerance)
			{
				enStatus = MAT_ERR_SINGULAR;
			}
			else
			{
				if (pivotRow != col)
				{
					for (k = col; k < total; k++)
					{
						swapVal				= aug[col][k];
						aug[col][k]			= aug[pivotRow][k];
						aug[pivotRow][k]	= swapVal;
					}
				}

				pivotVal = aug[col][col];

				for (k = col; k < total; k++)
				{
					aug[col][k] /= pivotVal;
				}

				for (row = 0; row < n; row++)
				{
					if (row != col)
					{
						factor = aug[row][col];

						for (k = col; k < total; k++)
						{
							aug[row][k] -= factor * aug[col][k];
						}
					}
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Init(ST_Matrix *st_Mat, INT32 row, INT32 column)
{
	EN_MatStatus	enStatus;
	INT32			i;
	INT32			j;

	if (st_Mat == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else if ((row < 1) || (row > MAT_MAX_DIM) || (column < 1) || (column > MAT_MAX_DIM))
	{
		enStatus = MAT_ERR_DIM;
	}
	else
	{
		for (i = 0; i < MAT_MAX_DIM; i++)
		{
			for (j = 0; j < MAT_MAX_DIM; j++)
			{
				st_Mat->e[i][j] = 0.0;
			}
		}

		st_Mat->row		= row;
		st_Mat->column	= column;
		enStatus		= MAT_OK;
	}

	return enStatus;
}

EN_MatStatus f_Mat_Identity(ST_Matrix *st_Mat, INT32 dim)
{
	EN_MatStatus	enStatus;
	INT32			i;

	enStatus = f_Mat_Init(st_Mat, dim, dim);

	if (enStatus == MAT_OK)
	{
		for (i = 0; i < dim; i++)
		{
			st_Mat->e[i][i] = 1.0;
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Load(ST_Matrix *st_Mat, INT32 row, INT32 column, const FLOAT64 *pt_Element, INT32 elementCount)
{
	EN_MatStatus	enStatus;
	INT32			i;
	INT32			j;

	if (pt_Element == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else if (elementCount != (row * column))
	{
		enStatus = MAT_ERR_DIM;
	}
	else
	{
		enStatus = f_Mat_Init(st_Mat, row, column);

		if (enStatus == MAT_OK)
		{
			for (i = 0; i < row; i++)
			{
				for (j = 0; j < column; j++)
				{
					st_Mat->e[i][j] = pt_Element[(i * column) + j];
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Copy(ST_Matrix *st_Out, const ST_Matrix *st_Src)
{
	EN_MatStatus	enStatus;
	INT32			i;
	INT32			j;

	if (st_Out == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else
	{
		enStatus = f_Mat_Validate(st_Src);

		if ((enStatus == MAT_OK) && (st_Out != st_Src))
		{
			enStatus = f_Mat_Init(st_Out, st_Src->row, st_Src->column);

			if (enStatus == MAT_OK)
			{
				for (i = 0; i < st_Src->row; i++)
				{
					for (j = 0; j < st_Src->column; j++)
					{
						st_Out->e[i][j] = st_Src->e[i][j];
					}
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Add(ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs)
{
	EN_MatStatus	enStatus;
	INT32			i;
	INT32			j;

	enStatus = f_Mat_ValidateOperands(st_Out, st_Lhs, st_Rhs);

	if (enStatus == MAT_OK)
	{
		if ((st_Lhs->row != st_Rhs->row) || (st_Lhs->column != st_Rhs->column))
		{
			enStatus = MAT_ERR_DIM;
		}
		else
		{
			st_Out->row		= st_Lhs->row;
			st_Out->column	= st_Lhs->column;

			for (i = 0; i < st_Lhs->row; i++)
			{
				for (j = 0; j < st_Lhs->column; j++)
				{
					st_Out->e[i][j] = st_Lhs->e[i][j] + st_Rhs->e[i][j];
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Sub(ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs)
{
	EN_MatStatus	enStatus;
	INT32			i;
	INT32			j;

	enStatus = f_Mat_ValidateOperands(st_Out, st_Lhs, st_Rhs);

	if (enStatus == MAT_OK)
	{
		if ((st_Lhs->row != st_Rhs->row) || (st_Lhs->column != st_Rhs->column))
		{
			enStatus = MAT_ERR_DIM;
		}
		else
		{
			st_Out->row		= st_Lhs->row;
			st_Out->column	= st_Lhs->column;

			for (i = 0; i < st_Lhs->row; i++)
			{
				for (j = 0; j < st_Lhs->column; j++)
				{
					st_Out->e[i][j] = st_Lhs->e[i][j] - st_Rhs->e[i][j];
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Scale(ST_Matrix *st_Out, const ST_Matrix *st_Src, FLOAT64 scalar)
{
	EN_MatStatus	enStatus;
	INT32			i;
	INT32			j;

	if (st_Out == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else
	{
		enStatus = f_Mat_Validate(st_Src);

		if (enStatus == MAT_OK)
		{
			st_Out->row		= st_Src->row;
			st_Out->column	= st_Src->column;

			for (i = 0; i < st_Src->row; i++)
			{
				for (j = 0; j < st_Src->column; j++)
				{
					st_Out->e[i][j] = st_Src->e[i][j] * scalar;
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Mul(ST_Matrix *st_Out, const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs)
{
	EN_MatStatus	enStatus;
	ST_Matrix		st_Temp;
	INT32			i;
	INT32			j;
	INT32			k;
	FLOAT64			sum;

	enStatus = f_Mat_ValidateOperands(st_Out, st_Lhs, st_Rhs);

	if (enStatus == MAT_OK)
	{
		if (st_Lhs->column != st_Rhs->row)
		{
			enStatus = MAT_ERR_DIM;
		}
		else
		{
			// 출력이 입력과 같은 객체여도 되도록 항상 임시 버퍼에 쌓은 뒤 옮긴다.
			enStatus = f_Mat_Init(&st_Temp, st_Lhs->row, st_Rhs->column);

			if (enStatus == MAT_OK)
			{
				for (i = 0; i < st_Lhs->row; i++)
				{
					for (j = 0; j < st_Rhs->column; j++)
					{
						sum = 0.0;

						for (k = 0; k < st_Lhs->column; k++)
						{
							sum += st_Lhs->e[i][k] * st_Rhs->e[k][j];
						}

						st_Temp.e[i][j] = sum;
					}
				}

				*st_Out = st_Temp;
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Mul3(ST_Matrix *st_Out, const ST_Matrix *st_A, const ST_Matrix *st_B, const ST_Matrix *st_C)
{
	EN_MatStatus	enStatus;
	ST_Matrix		st_Temp;

	enStatus = f_Mat_Mul(&st_Temp, st_A, st_B);

	if (enStatus == MAT_OK)
	{
		enStatus = f_Mat_Mul(st_Out, &st_Temp, st_C);
	}

	return enStatus;
}

EN_MatStatus f_Mat_Transpose(ST_Matrix *st_Out, const ST_Matrix *st_Src)
{
	EN_MatStatus	enStatus;
	ST_Matrix		st_Temp;
	INT32			i;
	INT32			j;

	if (st_Out == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else
	{
		enStatus = f_Mat_Validate(st_Src);

		if (enStatus == MAT_OK)
		{
			enStatus = f_Mat_Init(&st_Temp, st_Src->column, st_Src->row);

			if (enStatus == MAT_OK)
			{
				for (i = 0; i < st_Src->row; i++)
				{
					for (j = 0; j < st_Src->column; j++)
					{
						st_Temp.e[j][i] = st_Src->e[i][j];
					}
				}

				*st_Out = st_Temp;
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Inverse(ST_Matrix *st_Out, const ST_Matrix *st_Src)
{
	EN_MatStatus	enStatus;
	FLOAT64			aug[MAT_MAX_DIM][2 * MAT_MAX_DIM];
	INT32			n;
	INT32			i;
	INT32			j;

	if (st_Out == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else
	{
		enStatus = f_Mat_Validate(st_Src);

		if (enStatus == MAT_OK)
		{
			if (st_Src->row != st_Src->column)
			{
				enStatus = MAT_ERR_NOT_SQUARE;
			}
			else
			{
				n = st_Src->row;

				for (i = 0; i < n; i++)
				{
					for (j = 0; j < (2 * n); j++)
					{
						aug[i][j] = 0.0;
					}

					for (j = 0; j < n; j++)
					{
						aug[i][j] = st_Src->e[i][j];
					}

					aug[i][n + i] = 1.0;
				}

				enStatus = f_Mat_GaussJordan(aug, n, n, f_Mat_GetTolerance(st_Src));

				if (enStatus == MAT_OK)
				{
					enStatus = f_Mat_Init(st_Out, n, n);
				}

				if (enStatus == MAT_OK)
				{
					for (i = 0; i < n; i++)
					{
						for (j = 0; j < n; j++)
						{
							st_Out->e[i][j] = aug[i][n + j];
						}
					}
				}
			}
		}
	}

	return enStatus;
}

EN_MatStatus f_Mat_Solve(ST_Matrix *st_Out, const ST_Matrix *st_A, const ST_Matrix *st_B)
{
	EN_MatStatus	enStatus;
	FLOAT64			aug[MAT_MAX_DIM][2 * MAT_MAX_DIM];
	INT32			n;
	INT32			m;
	INT32			i;
	INT32			j;

	enStatus = f_Mat_ValidateOperands(st_Out, st_A, st_B);

	if (enStatus == MAT_OK)
	{
		if (st_A->row != st_A->column)
		{
			enStatus = MAT_ERR_NOT_SQUARE;
		}
		else if ((st_A->row != st_B->row) || (st_B->column > MAT_MAX_DIM))
		{
			enStatus = MAT_ERR_DIM;
		}
		else
		{
			n = st_A->row;
			m = st_B->column;

			for (i = 0; i < n; i++)
			{
				for (j = 0; j < (n + m); j++)
				{
					aug[i][j] = 0.0;
				}

				for (j = 0; j < n; j++)
				{
					aug[i][j] = st_A->e[i][j];
				}

				for (j = 0; j < m; j++)
				{
					aug[i][n + j] = st_B->e[i][j];
				}
			}

			enStatus = f_Mat_GaussJordan(aug, n, m, f_Mat_GetTolerance(st_A));

			if (enStatus == MAT_OK)
			{
				enStatus = f_Mat_Init(st_Out, n, m);
			}

			if (enStatus == MAT_OK)
			{
				for (i = 0; i < n; i++)
				{
					for (j = 0; j < m; j++)
					{
						st_Out->e[i][j] = aug[i][n + j];
					}
				}
			}
		}
	}

	return enStatus;
}

// LU 분해(부분 피벗팅)의 대각 곱으로 행렬식을 구한다. 행 교환 횟수만큼 부호가 뒤집힌다.
EN_MatStatus f_Mat_Det(const ST_Matrix *st_Src, FLOAT64 *pt_Det)
{
	EN_MatStatus	enStatus;
	FLOAT64			lu[MAT_MAX_DIM][MAT_MAX_DIM];
	FLOAT64			det = 1.0;
	FLOAT64			factor;
	FLOAT64			swapVal;
	FLOAT64			tolerance;
	INT32			n;
	INT32			col;
	INT32			row;
	INT32			k;
	INT32			pivotRow;
	INT32			singular = 0;

	if (pt_Det == NULL)
	{
		enStatus = MAT_ERR_NULL;
	}
	else
	{
		enStatus = f_Mat_Validate(st_Src);

		if (enStatus == MAT_OK)
		{
			if (st_Src->row != st_Src->column)
			{
				enStatus = MAT_ERR_NOT_SQUARE;
			}
			else
			{
				n			= st_Src->row;
				tolerance	= f_Mat_GetTolerance(st_Src);

				for (row = 0; row < n; row++)
				{
					for (col = 0; col < n; col++)
					{
						lu[row][col] = st_Src->e[row][col];
					}
				}

				for (col = 0; col < n; col++)
				{
					if (singular == 0)
					{
						pivotRow = col;

						for (row = col + 1; row < n; row++)
						{
							if (fabs(lu[row][col]) > fabs(lu[pivotRow][col]))
							{
								pivotRow = row;
							}
						}

						if (fabs(lu[pivotRow][col]) <= tolerance)
						{
							singular = 1;
						}
						else
						{
							if (pivotRow != col)
							{
								for (k = col; k < n; k++)
								{
									swapVal			= lu[col][k];
									lu[col][k]		= lu[pivotRow][k];
									lu[pivotRow][k]	= swapVal;
								}

								det = -det;
							}

							det *= lu[col][col];

							for (row = col + 1; row < n; row++)
							{
								factor = lu[row][col] / lu[col][col];

								for (k = col; k < n; k++)
								{
									lu[row][k] -= factor * lu[col][k];
								}
							}
						}
					}
				}

				if (singular == 1)
				{
					det = 0.0;
				}

				*pt_Det = det;
			}
		}
	}

	return enStatus;
}

INT32 f_Mat_IsEqual(const ST_Matrix *st_Lhs, const ST_Matrix *st_Rhs, FLOAT64 tolerance)
{
	INT32	isEqual = 1;
	INT32	i;
	INT32	j;

	if ((f_Mat_Validate(st_Lhs) != MAT_OK) || (f_Mat_Validate(st_Rhs) != MAT_OK))
	{
		isEqual = 0;
	}
	else if ((st_Lhs->row != st_Rhs->row) || (st_Lhs->column != st_Rhs->column))
	{
		isEqual = 0;
	}
	else
	{
		for (i = 0; i < st_Lhs->row; i++)
		{
			for (j = 0; j < st_Lhs->column; j++)
			{
				if (fabs(st_Lhs->e[i][j] - st_Rhs->e[i][j]) > tolerance)
				{
					isEqual = 0;
				}
			}
		}
	}

	return isEqual;
}

const CHAR *f_Mat_StatusStr(EN_MatStatus status)
{
	const CHAR *text;

	switch (status)
	{
	case MAT_OK:
		text = "MAT_OK";
		break;

	case MAT_ERR_NULL:
		text = "MAT_ERR_NULL";
		break;

	case MAT_ERR_DIM:
		text = "MAT_ERR_DIM";
		break;

	case MAT_ERR_NOT_SQUARE:
		text = "MAT_ERR_NOT_SQUARE";
		break;

	case MAT_ERR_SINGULAR:
		text = "MAT_ERR_SINGULAR";
		break;

	default:
		text = "MAT_ERR_UNKNOWN";
		break;
	}

	return text;
}
