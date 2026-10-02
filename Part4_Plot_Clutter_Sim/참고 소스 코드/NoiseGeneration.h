#ifndef	NOISE_GENERATION_H
#define NOISE_GENERATION_H

#include "CommonDefine.h"

void changeSEED(long int Value);
long int valueSEED(void);
double UNIRAN(void);
double GAUSS(double m, double dev);

#endif
