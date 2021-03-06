﻿#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define termi 4     // After encoding, 2 termination symbols are sent from each constituent encoder                    
#define N 2052     // interleaver length (in symbols) + termi

char *mapping = "8pskNatural",
     nextState0[8], nextState1[8], nextState2[8], nextState3[8], preState0[8], preState1[8], preState2[8], preState3[8];
double variance, **alpha, **beta, **gamma,
       nSymbol0[16], nSymbol1[16], nSymbol2[16], nSymbol3[16], bSymbol0[16], bSymbol1[16], bSymbol2[16], bSymbol3[16];

/* ***************************************************************************** */
/* Copyright:      Francois Panneton and Pierre L'Ecuyer, University of Montreal */
/*                 Makoto Matsumoto, Hiroshima University                        */
/* Notice:         This code can be used freely for personal, academic,          */
/*                 or non-commercial purposes. For commercial purposes,          */
/*                 please contact P. L'Ecuyer at: lecuyer@iro.UMontreal.ca       */
/* ***************************************************************************** */

#define W 32
#define R 32
#define M1 3
#define M2 24
#define M3 10

#define MAT0POS(t,v) (v^(v>>t))
#define MAT0NEG(t,v) (v^(v<<(-(t))))
#define Identity(v) (v)

#define V0            STATE[state_i                   ]
#define VM1           STATE[(state_i+M1) & 0x0000001fU]
#define VM2           STATE[(state_i+M2) & 0x0000001fU]
#define VM3           STATE[(state_i+M3) & 0x0000001fU]
#define VRm1          STATE[(state_i+31) & 0x0000001fU]
#define newV0         STATE[(state_i+31) & 0x0000001fU]
#define newV1         STATE[state_i                   ]

#define FACT 2.32830643653869628906e-10

static unsigned state_i = 0;
static unsigned STATE[R];
static unsigned z0, z1, z2;

void InitWELLRNG1024a(unsigned *init)
{
	int j;
	state_i = 0;
	for (j = 0; j<R; j++)
		STATE[j] = init[j];
}

int WELLRNG1024a(void)
{
	// the period of the random number generator is 2^1024-1
	z0 = VRm1;
	z1 = Identity(V0) ^ MAT0POS(8, VM1);
	z2 = MAT0NEG(-19, VM2) ^ MAT0NEG(-14, VM3);
	newV1 = z1^z2;
	newV0 = MAT0NEG(-11, z0) ^ MAT0NEG(-7, z1) ^ MAT0NEG(-13, z2);
	state_i = (state_i + 31) & 0x0000001fU;
	return STATE[state_i] - 2147483648;   // epistrefei enan omoiomorfa tyxaio int [-2^31,2^31-1]

}

void zigSet(int *k, double *w, double *f)
{
	double m = 2147483648.0, d = 3.6541528853610088, t = d, v = .00492867323399, q;
	int i;

	q = v / exp(-.5*d*d);
	k[0] = (int)(d / q*m);     k[1] = 0;
	w[0] = q / m;              w[255] = d / m;
	f[0] = 1.;               f[255] = exp(-.5*d*d);

	for (i = 254; i >= 1; i--)
	{
		d = sqrt(-2.*log(v / d + exp(-.5*d*d)));
		k[i + 1] = (int)(d / t*m);
		t = d;
		f[i] = exp(-.5*d*d);
		w[i] = d / m;
	}

}

void mapper(char *help, double *symbol)
{
	char i;

	if (strcmp(mapping, "Gray") == 0) // Gray mapping
	{
		for (i = 0; i < 8; i++)
			switch (help[i])
			{    
				case 0:
					symbol[i << 1] = 1.; symbol[i << 1 | 1] = 0.;
					break;
				case 1:
					symbol[i << 1] = symbol[i << 1 | 1] = 0.707106781186548;
					break;
				case 2:
					symbol[i << 1] = -0.707106781186548; symbol[i << 1 | 1] = 0.707106781186548;
					break;
				case 3:
					symbol[i << 1] = 0.; symbol[i << 1 | 1] = 1.;
					break;
				case 4:
					symbol[i << 1] = 0.707106781186548; symbol[i << 1 | 1] = -0.707106781186548;
					break;
				case 5:
					symbol[i << 1] = 0.; symbol[i << 1 | 1] = -1.;
					break;
				case 6:
					symbol[i << 1] = -1.; symbol[i << 1 | 1] = 0.;
					break;
				default:
					symbol[i << 1] = symbol[i << 1 | 1] = -0.707106781186548;
			}

	}
	else if (strcmp(mapping, "8pskNatural") == 0) // natural mapping
	{
		for (i = 0; i < 8; i++)
			switch (help[i])
			{    
				case 0:
					symbol[i << 1] = 1.; symbol[i << 1 | 1] = 0.;
					break;
				case 1:
					symbol[i << 1] = symbol[i << 1 | 1] = 0.707106781186548;
					break;
				case 2:
					symbol[i << 1] = 0.; symbol[i << 1 | 1] = 1.;
					break;
				case 3:
					symbol[i << 1] = -0.707106781186548; symbol[i << 1 | 1] = 0.707106781186548;
					break;
				case 4:
					symbol[i << 1] = -1.; symbol[i << 1 | 1] = 0.;
					break;
				case 5:
					symbol[i << 1] = symbol[i << 1 | 1] = -0.707106781186548;
					break;
				case 6:
					symbol[i << 1] = 0.; symbol[i << 1 | 1] = -1.;
					break;
				default:
					symbol[i << 1] = 0.707106781186548; symbol[i << 1 | 1] = -0.707106781186548;
			}
	}

	for (i = 0; i < 16; i++)
		symbol[i] /= variance;
}

void trellis()
{
	char c, i, help[8];

	FILE *fp;
	fp = fopen("trellis.txt", "r");
	if (fp == NULL)
	{
		printf("Can't open trellis.txt.\n");
		system("pause");
		exit(0);
	}

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", nextState0 + i);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", nextState1 + i);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", nextState2 + i);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", nextState3 + i);


	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, nSymbol0);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, nSymbol1);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, nSymbol2);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, nSymbol3);


	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", preState0 + i);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", preState1 + i);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", preState2 + i);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", preState3 + i);


	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, bSymbol0);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, bSymbol1);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, bSymbol2);

	do { c = getc(fp); } while (c != ':');
	for (i = 0; i < 8; i++)
		fscanf(fp, "%hhd", help + i);
	mapper(help, bSymbol3);

	fclose(fp);
}

void createInt(int *Int, int *deInt)
{
	// random interleaver
	int i, index, *A;

	A = new int[N - termi];
	for (i = 0; i < N - termi; i++)
		A[i] = i;

	for (i = 0; i < N - termi; i++)
	{
	 here:
		index = abs(WELLRNG1024a()) & (N - termi - 1);
		if (A[index] == -1)
			goto here;
		//if (i&1 && !(index&1) || !(i&1) && index&1) // odd-even constraint
		//    goto here;

		Int[i] = index;
		deInt[index] = i;
		A[index] = -1;
	}
	delete[] A;
}

void source(char *bitStream)
{
	for (int i = 0; i < N - termi; i++)
	{
		bitStream[i << 1] = WELLRNG1024a() > 0 ? 1 : 0;
		bitStream[i << 1 | 1] = WELLRNG1024a() > 0 ? 1 : 0;
	}
}

void encode(char *bitStream, char *codeword, int *Int, int *deInt)
{
	char Upbit, Downbit, input, D1[3] = { 0 }, D2[3] = { 0 };
	static char parity2[N];
	int i;

	for (i = 0; i < N - termi; i++)
	{
		Upbit = bitStream[i << 1];
		Downbit = bitStream[i << 1 | 1];

		if (i & 1)
			codeword[i] = Upbit << 2 | Downbit << 1; // lower encoder
		else
			codeword[i] = Upbit << 2 | Downbit << 1 | D1[2]; // upper encoder

		input = D1[2];
		D1[2] = D1[1] ^ Downbit;
		D1[1] = D1[0] ^ Upbit;
		D1[0] = input;


		Upbit = bitStream[deInt[i] << 1];
		Downbit = bitStream[deInt[i] << 1 | 1];

		parity2[i] = D2[2];

		input = D2[2];
		D2[2] = D2[1] ^ Downbit;
		D2[1] = D2[0] ^ Upbit;
		D2[0] = input;

	}
	for (i = 1; i < N - termi; i+=2)
		codeword[i] |= parity2[Int[i]];

	for (i = N - termi; i < N; i+=2) // 1st encoder termination
	{
		Upbit = D1[0] ^ D1[2];
		Downbit = D1[1];

		codeword[i] = Upbit << 2 | Downbit << 1 | D1[2];

		input = D1[2];
		D1[2] = 0;
		D1[1] = input;
		D1[0] = input;
	}

	for (i = N - termi + 1; i < N; i+=2) // 2nd encoder termination
	{
		Upbit = D2[0] ^ D2[2];
		Downbit = D2[1];

		codeword[i] = Upbit << 2 | Downbit << 1 | D2[2];

		input = D2[2];
		D2[2] = 0;
		D2[1] = input;
		D2[0] = input;
	}
}

void modulate(char *codeword, double *I, double *Q)
{
	int i;

	if (strcmp(mapping, "Gray") == 0) // Gray mapping
	{
		for (i = 0; i < N; i++)
			switch (codeword[i])
			{    
				case 0:
					I[i] = 1.; Q[i] = 0.;
					break;
				case 1:
					I[i] = Q[i] = 0.707106781186548;
					break;
				case 2:
					I[i] = -0.707106781186548; Q[i] = 0.707106781186548;
					break;
				case 3:
					I[i] = 0.; Q[i] = 1.;
					break;
				case 4:
					I[i] = 0.707106781186548; Q[i] = -0.707106781186548;
					break;
				case 5:
					I[i] = 0.; Q[i] = -1.;
					break;
				case 6:
					I[i] = -1.; Q[i] = 0.;
					break;
				default:
					I[i] = Q[i] = -0.707106781186548;
			}
	}
	else if (strcmp(mapping, "8pskNatural") == 0) // natural mapping
	{
		for (i = 0; i < N; i++)
			switch (codeword[i])
			{    
				case 0:
					I[i] = 1.; Q[i] = 0.;
					break;
				case 1:
					I[i] = Q[i] = 0.707106781186548;
					break;
				case 2:
					I[i] = 0.; Q[i] = 1.;
					break;
				case 3:
					I[i] = -0.707106781186548; Q[i] = 0.707106781186548;
					break;
				case 4:
					I[i] = -1.; Q[i] = 0.;
					break;
				case 5:
					I[i] = Q[i] = -0.707106781186548;
					break;
				case 6:
					I[i] = 0.; Q[i] = -1.;
					break;
				default:
					I[i] = 0.707106781186548; Q[i] = -0.707106781186548;
			}
	}
}

void wgn(double *GN, int length, int *k, double *w, double *f)
{
	// creates WGN samples by applying the Ziggurat algorithm of Marsaglia & Tsang (2000)
	int randomInt;
	double uni, uni2, x, y, r = 3.6541528853610088;
	short int i;
	int j = 0;

	while (j < length)
	{
		randomInt = WELLRNG1024a();    // gennaei enan tyxaio int, apo ton opoio 8a prokypsei to deigma 8oryvou
		i = WELLRNG1024a() & 255;      // epilegei tyxaia ena apo ta 128 strwmata tou ziggurat, gennwntas* enan kainourio int,
									   // *symfwna me to "An Improved Ziggurat Method to Generate Normal Random Samples" tou Doornik (2005)

		if (abs(randomInt)<k[i])    // to 99.33% twn deigmatwn proerxontai apo auto to if
		{
			GN[j++] = randomInt*w[i];
			continue;
		}

		if (i == 0)
		{
			do
			{
				uni = .5 + WELLRNG1024a()*FACT;
				if (uni == 0) uni = .5 + WELLRNG1024a()*FACT;
				x = -log(uni)*0.27366123732975827;   // o antistrofos tou r
				uni = .5 + WELLRNG1024a()*FACT;
				if (uni == 0) uni = .5 + WELLRNG1024a()*FACT;
				y = -log(uni);
			} while (y + y<x*x);

			GN[j++] = randomInt > 0 ? r + x : -r - x;
			continue;
		}

		uni = randomInt*w[i];
		uni2 = .5 + WELLRNG1024a()*FACT;
		if (f[i] + uni2*(f[i - 1] - f[i]) < exp(-.5*uni*uni))
			GN[j++] = uni;

	}
}

void sisoDec1(double *noisyI, double *susieQ, double *LLR1, double *LLR2, double *LLR3, double *La1, double *La2, double *La3)
{
	// log-APP algorithm
	char numOfStates = 1, s;
	int i;
	double x, y, LL0;

	//-------------------------- gamma branch metrics calculation ----------------------------//

	for (i = 0; i < N - termi; i+=2)
	{
		for (s = 0; s < numOfStates; s++)
		{
			gamma[i][s] = noisyI[i] * nSymbol0[s << 1] + susieQ[i] * nSymbol0[s << 1 | 1];
			gamma[i][s + 8] = La1[i] + noisyI[i] * nSymbol1[s << 1] + susieQ[i] * nSymbol1[s << 1 | 1];
			gamma[i][s + 16] = La2[i] + noisyI[i] * nSymbol2[s << 1] + susieQ[i] * nSymbol2[s << 1 | 1];
			gamma[i][s + 24] = La3[i] + noisyI[i] * nSymbol3[s << 1] + susieQ[i] * nSymbol3[s << 1 | 1];
		}
		if (i == 0) numOfStates = 8;
	}

	numOfStates = 4;
	for (i = 1; i < N - termi; i+=2)
	{
		for (s = 0; s < numOfStates; s++)
		{
			gamma[i][s] = 0.;
			gamma[i][s + 8] = La1[i];
			gamma[i][s + 16] = La2[i];
			gamma[i][s + 24] = La3[i];
		}
		if (i == 1) numOfStates = 8;
	}

	i = N - termi;
	// sto telos tou 1ou vimatos tou termatismou 8a yparxoun oi katastaseis 0 & 6
	for (s = 0; s<numOfStates; s += 6)
	{
		gamma[i][preState0[s]] = noisyI[i] * bSymbol0[s << 1] + susieQ[i] * bSymbol0[s << 1 | 1];
		gamma[i][preState1[s] + 8] = noisyI[i] * bSymbol1[s << 1] + susieQ[i] * bSymbol1[s << 1 | 1];
		gamma[i][preState2[s] + 16] = noisyI[i] * bSymbol2[s << 1] + susieQ[i] * bSymbol2[s << 1 | 1];
		gamma[i][preState3[s] + 24] = noisyI[i] * bSymbol3[s << 1] + susieQ[i] * bSymbol3[s << 1 | 1];
	}

	i++;
	gamma[i][0] = noisyI[i + 1] * bSymbol0[0] + susieQ[i + 1] * bSymbol0[1];
	// apo tin 6 pame sti 0 me to symvolo 3
	gamma[i][6 + 24] = noisyI[i + 1] * bSymbol3[0] + susieQ[i + 1] * bSymbol3[1];

	//-------------------------- alpha node metrics calculation ----------------------------//

	i = 0;
	alpha[i + 1][0] = gamma[i][0];
	alpha[i + 1][1] = gamma[i][8];
	alpha[i + 1][2] = gamma[i][16];
	alpha[i + 1][3] = gamma[i][24];
	i++;

	for (s = 0; s < numOfStates; s++)
	{
		if (s == 0 || s == 1 || s == 4 || s == 5)
		{
			x = gamma[i][preState0[s]] + alpha[i][preState0[s]];
			y = gamma[i][preState1[s] + 8] + alpha[i][preState1[s]];
		}
		else
		{
			x = gamma[i][preState2[s] + 16] + alpha[i][preState2[s]];
			y = gamma[i][preState3[s] + 24] + alpha[i][preState3[s]];
		}
		alpha[i + 1][s] = x > y ? x + log(1 + exp(y - x)) : y + log(1 + exp(x - y));
	}

	for (i = 2; i < N - termi - 1; i++)
	{
		for (s = 0; s < numOfStates; s++)
		{
			y = gamma[i][preState0[s]] + alpha[i][preState0[s]];
			x = 1.;
			x += exp(gamma[i][preState1[s] + 8] + alpha[i][preState1[s]] - y);
			x += exp(gamma[i][preState2[s] + 16] + alpha[i][preState2[s]] - y);
			x += exp(gamma[i][preState3[s] + 24] + alpha[i][preState3[s]] - y);

			alpha[i + 1][s] = y + log(x);
		}
	}

	//-------------------------- beta node metrics calculation -----------------------------//

	i = N - 3;
	beta[i - 1][preState0[0]] = gamma[i][preState0[0]];
	beta[i - 1][preState3[0]] = gamma[i][preState3[0] + 24];
	i--;

	beta[i - 1][preState0[0]] = gamma[i][preState0[0]] + beta[i][0];
	beta[i - 1][preState1[0]] = gamma[i][preState1[0] + 8] + beta[i][0];
	beta[i - 1][preState2[0]] = gamma[i][preState2[0] + 16] + beta[i][0];
	beta[i - 1][preState3[0]] = gamma[i][preState3[0] + 24] + beta[i][0];

	beta[i - 1][preState0[6]] = gamma[i][preState0[6]] + beta[i][6];
	beta[i - 1][preState1[6]] = gamma[i][preState1[6] + 8] + beta[i][6];
	beta[i - 1][preState2[6]] = gamma[i][preState2[6] + 16] + beta[i][6];
	beta[i - 1][preState3[6]] = gamma[i][preState3[6] + 24] + beta[i][6];
	i--;

	for (; i > 0; i--)
	{
		for (s = 0; s < numOfStates; s++)
		{
			y = gamma[i][s] + beta[i][nextState0[s]];
			x = 1.;
			x += exp(gamma[i][s + 8] + beta[i][nextState1[s]] - y);
			x += exp(gamma[i][s + 16] + beta[i][nextState2[s]] - y);
			x += exp(gamma[i][s + 24] + beta[i][nextState3[s]] - y);

			beta[i - 1][s] = y + log(x);
		}
	}

	//-------------------------- LLR calculation ----------------------------//

	LL0 = gamma[0][0] + beta[0][0];
	LLR1[0] = gamma[0][8] + beta[0][1] - LL0;
	LLR2[0] = gamma[0][16] + beta[0][2] - LL0;
	LLR3[0] = gamma[0][24] + beta[0][3] - LL0;

	numOfStates = 4;
	for (i = 1; i < N - termi; i++)
	{
		y = alpha[i][0] + gamma[i][0] + beta[i][0];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s] + beta[i][nextState0[s]] - y);
		LL0 = y + log(x);

		y = alpha[i][0] + gamma[i][8] + beta[i][1];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s + 8] + beta[i][nextState1[s]] - y);
		LLR1[i] = y + log(x) - LL0;

		y = alpha[i][0] + gamma[i][16] + beta[i][2];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s + 16] + beta[i][nextState2[s]] - y);
		LLR2[i] = y + log(x) - LL0;

		y = alpha[i][0] + gamma[i][24] + beta[i][3];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s + 24] + beta[i][nextState3[s]] - y);
		LLR3[i] = y + log(x) - LL0;

		if (i == 1) numOfStates = 8;
	}
}

void sisoDec2(int *deInt, double *noisyI, double *susieQ, double *LLR1, double *LLR2, double *LLR3, double *La1, double *La2, double *La3)
{
	// log-APP algorithm
	char numOfStates = 1, s, step = 2;
	int i, j = 0;
	double x, y, LL0;

	//-------------------------- gamma branch metrics calculation ----------------------------//

	for (i = 0; i < N - termi; i++)
	{
		if (deInt[i] & 1) // if in index i there exists a symbol from ENC2
		{
			j++;
			for (s = 0; s < numOfStates; s++)
			{
				gamma[i][s] = noisyI[i] * nSymbol0[s << 1] + susieQ[i] * nSymbol0[s << 1 | 1];
				gamma[i][s + 8] = La1[i] + noisyI[i] * nSymbol1[s << 1] + susieQ[i] * nSymbol1[s << 1 | 1];
				gamma[i][s + 16] = La2[i] + noisyI[i] * nSymbol2[s << 1] + susieQ[i] * nSymbol2[s << 1 | 1];
				gamma[i][s + 24] = La3[i] + noisyI[i] * nSymbol3[s << 1] + susieQ[i] * nSymbol3[s << 1 | 1];
			}
		}
		else
		{
			for (s = 0; s < numOfStates; s++)
			{
				gamma[i][s] = 0.;
				gamma[i][s + 8] = La1[i];
				gamma[i][s + 16] = La2[i];
				gamma[i][s + 24] = La3[i];
			}
		}

		if (numOfStates < 8) numOfStates <<= step--;
	}

	i = N - termi;
	// sto telos tou 1ou vimatos tou termatismou 8a yparxoun oi katastaseis 0 & 6
	for (s = 0; s<numOfStates; s += 6)
	{
		gamma[i][preState0[s]] = noisyI[i + 1] * bSymbol0[s << 1] + susieQ[i + 1] * bSymbol0[s << 1 | 1];
		gamma[i][preState1[s] + 8] = noisyI[i + 1] * bSymbol1[s << 1] + susieQ[i + 1] * bSymbol1[s << 1 | 1];
		gamma[i][preState2[s] + 16] = noisyI[i + 1] * bSymbol2[s << 1] + susieQ[i + 1] * bSymbol2[s << 1 | 1];
		gamma[i][preState3[s] + 24] = noisyI[i + 1] * bSymbol3[s << 1] + susieQ[i + 1] * bSymbol3[s << 1 | 1];
	}

	i++;
	gamma[i][0] = noisyI[i + 2] * bSymbol0[0] + susieQ[i + 2] * bSymbol0[1];
	// apo tin 6 pame sti 0 me to symvolo 3
	gamma[i][6 + 24] = noisyI[i + 2] * bSymbol3[0] + susieQ[i + 2] * bSymbol3[1];

	//-------------------------- alpha node metrics calculation ----------------------------//

	i = 0;
	alpha[i + 1][0] = gamma[i][0];
	alpha[i + 1][1] = gamma[i][8];
	alpha[i + 1][2] = gamma[i][16];
	alpha[i + 1][3] = gamma[i][24];
	i++;

	for (s = 0; s < numOfStates; s++)
	{
		if (s == 0 || s == 1 || s == 4 || s == 5)
		{
			x = gamma[i][preState0[s]] + alpha[i][preState0[s]];
			y = gamma[i][preState1[s] + 8] + alpha[i][preState1[s]];
		}
		else
		{
			x = gamma[i][preState2[s] + 16] + alpha[i][preState2[s]];
			y = gamma[i][preState3[s] + 24] + alpha[i][preState3[s]];
		}
		alpha[i + 1][s] = x > y ? x + log(1 + exp(y - x)) : y + log(1 + exp(x - y));
	}

	for (i = 2; i < N - termi - 1; i++)
	{
		for (s = 0; s < numOfStates; s++)
		{
			y = gamma[i][preState0[s]] + alpha[i][preState0[s]];
			x = 1.;
			x += exp(gamma[i][preState1[s] + 8] + alpha[i][preState1[s]] - y);
			x += exp(gamma[i][preState2[s] + 16] + alpha[i][preState2[s]] - y);
			x += exp(gamma[i][preState3[s] + 24] + alpha[i][preState3[s]] - y);

			alpha[i + 1][s] = y + log(x);
		}
	}

	//-------------------------- beta node metrics calculation -----------------------------//

	i = N - 3;
	beta[i - 1][preState0[0]] = gamma[i][preState0[0]];
	beta[i - 1][preState3[0]] = gamma[i][preState3[0] + 24];
	i--;

	beta[i - 1][preState0[0]] = gamma[i][preState0[0]] + beta[i][0];
	beta[i - 1][preState1[0]] = gamma[i][preState1[0] + 8] + beta[i][0];
	beta[i - 1][preState2[0]] = gamma[i][preState2[0] + 16] + beta[i][0];
	beta[i - 1][preState3[0]] = gamma[i][preState3[0] + 24] + beta[i][0];

	beta[i - 1][preState0[6]] = gamma[i][preState0[6]] + beta[i][6];
	beta[i - 1][preState1[6]] = gamma[i][preState1[6] + 8] + beta[i][6];
	beta[i - 1][preState2[6]] = gamma[i][preState2[6] + 16] + beta[i][6];
	beta[i - 1][preState3[6]] = gamma[i][preState3[6] + 24] + beta[i][6];
	i--;

	for (; i > 0; i--)
	{
		for (s = 0; s < numOfStates; s++)
		{
			y = gamma[i][s] + beta[i][nextState0[s]];
			x = 1.;
			x += exp(gamma[i][s + 8] + beta[i][nextState1[s]] - y);
			x += exp(gamma[i][s + 16] + beta[i][nextState2[s]] - y);
			x += exp(gamma[i][s + 24] + beta[i][nextState3[s]] - y);

			beta[i - 1][s] = y + log(x);
		}
	}

	//-------------------------- LLR calculation ----------------------------//

	LL0 = gamma[0][0] + beta[0][0];
	LLR1[0] = gamma[0][8] + beta[0][1] - LL0;
	LLR2[0] = gamma[0][16] + beta[0][2] - LL0;
	LLR3[0] = gamma[0][24] + beta[0][3] - LL0;


	numOfStates = 4;
	for (i = 1; i < N - termi; i++)
	{
		y = alpha[i][0] + gamma[i][0] + beta[i][0];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s] + beta[i][nextState0[s]] - y);
		LL0 = y + log(x);

		y = alpha[i][0] + gamma[i][8] + beta[i][1];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s + 8] + beta[i][nextState1[s]] - y);
		LLR1[i] = y + log(x) - LL0;

		y = alpha[i][0] + gamma[i][16] + beta[i][2];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s + 16] + beta[i][nextState2[s]] - y);
		LLR2[i] = y + log(x) - LL0;

		y = alpha[i][0] + gamma[i][24] + beta[i][3];
		x = 1.;
		for (s = 1; s < numOfStates; s++)
			x += exp(alpha[i][s] + gamma[i][s + 24] + beta[i][nextState3[s]] - y);
		LLR3[i] = y + log(x) - LL0;

		if (i == 1) numOfStates = 8;
	}
}

int main()
{
	bool test = true, genie = true;
	double *GN, *I, *noisyI, *Q, *susieQ, *IntNoisyI, *IntSusieQ,
		   LLR0, *LLR1, *LLR2, *LLR3,
	       *La1DEC1, *La2DEC1, *La3DEC1, *La1DEC2, *La2DEC2, *La3DEC2,
	       sigma, EbNo, EsNo, 
	       wNor[256], fNor[256];
	unsigned seed[R];
	char *bitStream, *codeword, hd, iters, maxIters = 8;
	int kNor[256], i, *Int, *deInt,
	    frameErrors = 0, bitErrors = 0, trials = 0;

	//----------------------------------------------------------------------------//

	alpha = new double*[N - termi];
	for (i = 0; i < N - termi; i++)
		alpha[i] = new double[8];
	alpha[0][0] = 0.;

	beta = new double*[N - 2];
	for (i = 0; i < N - 2; i++)
		beta[i] = new double[8];
	beta[N - 3][0] = 0.;

	gamma = new double*[N - 2];
	for (i = 0; i < N - 2; i++)
		gamma[i] = new double[32];


	bitStream = new char[(N - termi) << 1];
	codeword = new char[N];

	Int = new int[N - termi];
	deInt = new int[N - termi];

	LLR1 = new double[N - termi];
	LLR2 = new double[N - termi];
	LLR3 = new double[N - termi];

	La1DEC1 = new double[N - termi];
	La2DEC1 = new double[N - termi];
	La3DEC1 = new double[N - termi];
	La1DEC2 = new double[N - termi];
	La2DEC2 = new double[N - termi];
	La3DEC2 = new double[N - termi];

	GN = new double[N << 1];
	I = new double[N];
	noisyI = new double[N];
	Q = new double[N];
	susieQ = new double[N];
	IntNoisyI = new double[N];
	IntSusieQ = new double[N];

	//----------------------------------------------------------------------------//

	// random number generator initialization
	for (i = 0; i < R; i++)
		seed[i] = 12345 + i;
	InitWELLRNG1024a(seed);
	zigSet(kNor, wNor, fNor);

	EbNo = 3.8; // Eb/No in dB
	EsNo = EbNo + 10 * log10(2.*double(N - termi)/double(N)); // each 8-PSK symbol bears 2 information bits (Es=2Eb)
	variance = pow(10, -EsNo / 10) / 2.;
	sigma = sqrt(variance);
	trellis();	

	//----------------------------------------------------------------------------//

	while (bitErrors < 1000)
	{
		if ((trials & 31) == 0)
			createInt(Int, deInt); // creates a different random interleaver every 32 trialss

		source(bitStream);
		encode(bitStream, codeword, Int, deInt);
		modulate(codeword, I, Q);
		wgn(GN, N << 1, kNor, wNor, fNor);

		for (i = 0; i < N; i++)
		{
			noisyI[i] = I[i] + sigma*GN[i];
			susieQ[i] = Q[i] + sigma*GN[N + i];
		}

		for (i = 0; i < N - termi; i++)
		{
			IntNoisyI[Int[i]] = noisyI[i];
			IntSusieQ[Int[i]] = susieQ[i];
		}
		for (; i < N; i++)
		{
			IntNoisyI[i] = noisyI[i];
			IntSusieQ[i] = susieQ[i];
		}

		// turbo decoder
		for (i = 0; i < N - termi; i++) { La1DEC1[i] = La2DEC1[i] = La3DEC1[i] = 0.; }
		for (iters = 0; iters < maxIters; iters++)
		{
			sisoDec1(noisyI, susieQ, LLR1, LLR2, LLR3, La1DEC1, La2DEC1, La3DEC1);

			for (i = 0; i < N - termi; i++)
			{
				La1DEC2[Int[i]] = LLR1[i] - La1DEC1[i];
				La2DEC2[Int[i]] = LLR2[i] - La2DEC1[i];
				La3DEC2[Int[i]] = LLR3[i] - La3DEC1[i];
			}

			sisoDec2(deInt, IntNoisyI, IntSusieQ, LLR1, LLR2, LLR3, La1DEC2, La2DEC2, La3DEC2);

			if (genie)
			{
				if (iters > 5 && iters < maxIters - 1)
				{
					for (i = 0; i < N - termi; i++)
					{
						LLR0 = 0.;
						hd = 0;
						if (LLR1[i] > LLR0) { LLR0 = LLR1[i]; hd = 1; }
						if (LLR2[i] > LLR0) { LLR0 = LLR2[i]; hd = 2; }
						if (LLR3[i] > LLR0) hd = 3;
						if (hd >> 1 ^ bitStream[deInt[i] << 1]) break;
						if (hd & 1 ^ bitStream[deInt[i] << 1 | 1]) break;
					}
					if (i == N - termi) goto there;
				}
			}

			if (iters < maxIters - 1)
				for (i = 0; i < N - termi; i++)
				{
					La1DEC1[deInt[i]] = LLR1[i] - La1DEC2[i];
					La2DEC1[deInt[i]] = LLR2[i] - La2DEC2[i];
					La3DEC1[deInt[i]] = LLR3[i] - La3DEC2[i];
				}
		}

		// error counting
		for (i = 0; i < N - termi; i++)
		{
			LLR0 = 0.;
			hd = 0;
			if (LLR1[i] > LLR0)	{ LLR0 = LLR1[i]; hd = 1; }
			if (LLR2[i] > LLR0)	{ LLR0 = LLR2[i]; hd = 2; }
			if (LLR3[i] > LLR0)	hd = 3;

			if (hd >> 1 ^ bitStream[deInt[i] << 1])
			{
				bitErrors++;
				if (test) {	frameErrors++; test = false; }
			}
			if (hd & 1 ^ bitStream[deInt[i] << 1 | 1])
			{
				bitErrors++;
				if (test) {	frameErrors++; test = false; }
			}
		}
		if (!test)
			test = true;

	there:
		trials++;
		printf("%2d %d %d %d %.2e %.2e\n", iters, trials, bitErrors, frameErrors, double(bitErrors) / double(N - termi) / double(trials) / 2., double(frameErrors) / double(trials));

	}

	printf("\nEbNo=%.2f bitErrors=%d BER=%.3e FER=%.3e\n", EbNo, bitErrors, double(bitErrors) / double(N - termi) / double(trials) / 2., double(frameErrors) / double(trials));

	system("pause");
	return 0;
}
