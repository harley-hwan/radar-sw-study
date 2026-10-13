#include <stdio.h>
#include <stdlib.h>

#include "NoiseGeneration.h"


long long int	SEED = 12345;

void changeSEED(long int Value)
{
	SEED = Value;
}

long int valueSEED(void)
{
	return SEED;
}

double UNIRAN(void)
{
	long long int	M	= 2147483647;
	long long int	a	= 16807;
	long long int	xn	= 0;

	double ans = 0.;

	//printf("xn:%lld  SEED:%lld\n", xn, SEED);

	xn = SEED;
	xn = abs((a * xn) % M);
	SEED = xn;

	ans = (double)xn / (double)M;

	return ans;
}

double GAUSS(double m, double dev)
{
	int i;
	double a = 0.;

	for (i = 0; i < 12; i++)
	{
		a += UNIRAN();
	}

	return ((a - 6.0) * dev + m);
}
