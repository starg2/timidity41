#include <stdio.h>
#include <math.h>

double newt_coeffs[58][58] = {0};


int main(int argc, const char *argv[])
{
    int i, j, n = 57;
    double sign = 0;
	FILE *fp;

	if (argc != 2)
	{
		fprintf(stderr, "usage: calcnewt <filename>\n");
		return 1;
	}
	fp = fopen(argv[1], "w");
	if (fp == NULL)
	{
		fprintf(stderr, "cannot open file:%s\n", argv[1]);
		return 2;
	}

    newt_coeffs[0][0] = 1;
    for (i = 0; i <= n; i++)
    {
    	newt_coeffs[i][0] = 1;
    	newt_coeffs[i][i] = 1;

	if (i > 1)
	{
	    newt_coeffs[i][0] = newt_coeffs[i-1][0] / i;
	    newt_coeffs[i][i] = newt_coeffs[i-1][0] / i;
	}

    	for (j = 1; j < i; j++)
    	{
    	    newt_coeffs[i][j] = newt_coeffs[i-1][j-1] + newt_coeffs[i-1][j];

			if (i > 1)
	    		newt_coeffs[i][j] /= i;
		}
    }
    for (i = 0; i <= n; i++)
    	for (j = 0, sign = pow((double)-1, i); j <= i; j++, sign *= -1)
    	    newt_coeffs[i][j] *= sign;
///r
    for (i = 0; i <= n; i++)
	for (j = 0; j <= n; j++)
	    fprintf(fp, "(double)%32.32g,\n", newt_coeffs[i][j]);
	fclose(fp);

    return 0;
}
