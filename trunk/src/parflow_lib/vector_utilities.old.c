/*BHEADER**********************************************************************
 * (c) 1997   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision: 1.1.1.1 $
 *********************************************************************EHEADER*/
/* This file contains utility routines for ParFlow's Vector class.
 *
 * This file was modified from a coresponding PVODE package file to account
 * for the ParFlow Vector class and AMPS message-passing system. 
 *
 * Routines included:
 *
 * PFVLinearSum(a, x, b, y, z)       z = a * x + b * y
 * PFVConstInit(c, z)                z = c        
 * PFVProd(x, y, z)                  z_i = x_i * y_i
 * PFVDiv(x, y, z)                   z_i = x_i / y_i
 * PFVScale(c, x, z)                 z = c * x
 * PFVAbs(x, z)                      z_i = |x_i|
 * PFVInv(x, z)                      z_i = 1 / x_i
 * PFVAddConst(x, b, z)              z_i = x_i + b
 * PFVDotProd(x, y)                  Returns x dot y
 * PFVMaxNorm(x)                     Returns ||x||_{max}
 * PFVWrmsNorm(x, w)                 Returns sqrt((sum_i (x_i + w_i)^2)/length)
 * PFVWL2Norm(x, w)                  Returns sqrt(sum_i (x_i * w_i)^2)
 * PFVL1Norm(x)                      Returns sum_i |x_i|
 * PFVMin(x)                         Returns min_i x_i
 * PFVMax(x)                         Returns max_i x_i
 * PFVConstrProdPos(c, x)            Returns FALSE if some c_i = 0 & 
 *                                      c_i*x_i <= 0.0
 * PFVCompare(c, x, z)               z_i = (x_i > c)
 * PFVInvTest(x, z)                  Returns (x_i != 0 forall i), z_i = 1 / x_i
 * PFVCopy(x, y)                     y = x
 * PFVSum(x, y, z)                   z = x + y
 * PFVDiff(x, y, z)                  z = x - y
 * PFVNeg(x, z)                      z = - x
 * PFVScaleSum(c, x, y, z)           z = c * (x + y)
 * PFVScaleDiff(c, x, y, z)          z = c * (x - y)
 * PFVLin1(a, x, y, z)               z = a * x + y
 * PFVLin2(a, x, y, z)               z = a * x - y
 * PFVAxpy(a, x, y)                  y = y + a * x
 * PFVScaleBy(a, x)                  x = x * a
 *
 ****************************************************************************/

#include "parflow.h"

#define ZERO 0.0
#define ONE  1.0

void PFVLinearSum(a, x, b, y, z)
/* LinearSum : z = a * x + b * y              */
double  a;
Vector *x;
double  b;
Vector *y;
Vector *z;

{
  double c;
  Vector *v1, *v2;
  int test;

  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *y_sub;
  Subvector  *z_sub;

  double     *yp, *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_y, ny_y, nz_y;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_y, i_z;

  if ((b == ONE) && (z == y)) {    /* BLAS usage: axpy y <- ax+y */
    PFVAxpy(a,x,y);
    return;
  }

  if ((a == ONE) && (z == x)) {    /* BLAS usage: axpy x <- by+x */
    PFVAxpy(b,y,x);
    return;
  }

  /* Case: a == b == 1.0 */

  if ((a == ONE) && (b == ONE)) {
    PFVSum(x, y, z);
    return;
  }

  /* Cases: (1) a == 1.0, b = -1.0, (2) a == -1.0, b == 1.0 */

  if ((test = ((a == ONE) && (b == -ONE))) || ((a == -ONE) && (b == ONE))) {
    v1 = test ? y : x;
    v2 = test ? x : y;
    PFVDiff(v2, v1, z);
    return;
  }

  /* Cases: (1) a == 1.0, b == other or 0.0, (2) a == other or 0.0, b == 1.0 */
  /* if a or b is 0.0, then user should have called N_VScale */

  if ((test = (a == ONE)) || (b == ONE)) {
    c = test ? b : a;
    v1 = test ? y : x;
    v2 = test ? x : y;
    PFVLin1(c, v1, v2, z);
    return;
  }

  /* Cases: (1) a == -1.0, b != 1.0, (2) a != 1.0, b == -1.0 */

  if ((test = (a == -ONE)) || (b == -ONE)) {
    c = test ? b : a;
    v1 = test ? y : x;
    v2 = test ? x : y;
    PFVLin2(c, v1, v2, z);
    return;
  }

  /* Case: a == b */
  /* catches case both a and b are 0.0 - user should have called N_VConst */

  if (a == b) {
    PFVScaleSum(a, x, y, z);
    return;
  }

  /* Case: a == -b */

  if (a == -b) {
    PFVScaleDiff(a, x, y, z);
    return;
  }

  /* Do all cases not handled above:
     (1) a == other, b == 0.0 - user should have called N_VScale
     (2) a == 0.0, b == other - user should have called N_VScale
     (3) a,b == other, a !=b, a != -b */
  
  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);
     y_sub = VectorSubvector(y, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_y = SubvectorNX(y_sub);
     ny_y = SubvectorNY(y_sub);
     nz_y = SubvectorNZ(y_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);
     yp = SubvectorElt(y_sub, ix, iy, iz);

     i_x = 0;
     i_y = 0;
     i_z = 0;
     BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_y, nx_y, ny_y, nz_y, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = a * xp[i_x] + b * yp[i_y];
	       });
  }
  IncFLOPCount( 3 * VectorSize(z) );
}

void PFVConstInit(c, z)
/* ConstInit : z = c   */
double  c;
Vector *z;
{
  Grid       *grid     = VectorGrid(z);
  Subgrid    *subgrid;
 
  Subvector  *z_sub;

  double     *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_z, ny_z, nz_z;

  int         sg, i, j, k, i_z;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);

     i_z = 0;
     BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = c;
	       });
  }
}

void PFVProd(x, y, z)
/* Prod : z_i = x_i * y_i   */
Vector *x;
Vector *y;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *y_sub;
  Subvector  *z_sub;

  double     *yp, *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_y, ny_y, nz_y;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_y, i_z;

  grid = VectorGrid(x);
  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);
     y_sub = VectorSubvector(y, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_y = SubvectorNX(y_sub);
     ny_y = SubvectorNY(y_sub);
     nz_y = SubvectorNZ(y_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);
     yp = SubvectorElt(y_sub, ix, iy, iz);

     i_x = 0;
     i_y = 0;
     i_z = 0;
     BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_y, nx_y, ny_y, nz_y, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = xp[i_x] * yp[i_y];
	       });
  }
  IncFLOPCount( VectorSize(x) );
}

void PFVDiv(x, y, z)
/* Div : z_i = x_i / y_i   */
Vector *x;
Vector *y;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *y_sub;
  Subvector  *z_sub;

  double     *yp, *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_y, ny_y, nz_y;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_y, i_z;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);
     y_sub = VectorSubvector(y, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_y = SubvectorNX(y_sub);
     ny_y = SubvectorNY(y_sub);
     nz_y = SubvectorNZ(y_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);
     yp = SubvectorElt(y_sub, ix, iy, iz);

     i_x = 0;
     i_y = 0;
     i_z = 0;
     BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_y, nx_y, ny_y, nz_y, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = xp[i_x] / yp[i_y];
	       });
  }
  IncFLOPCount( VectorSize(x) );
}

void PFVScale(c, x, z)
/* Scale : z = c * x   */
double  c;
Vector *x;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *z_sub;

  double     *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_z;

  if (z == x) 
  {       /* BLAS usage: scale x <- cx */
     PFVScaleBy(c, x);
     return;
  }

  if (c == ONE) 
  {
     PFVCopy(x, z);
  } 
  else if (c == -ONE) 
  {
     PFVNeg(x, z);
  } 
  else 
  {
     ForSubgridI(sg, GridSubgrids(grid))
     {
        subgrid = GridSubgrid(grid, sg);

	z_sub = VectorSubvector(z, sg);
	x_sub = VectorSubvector(x, sg);

	ix = SubgridIX(subgrid);
	iy = SubgridIY(subgrid);
	iz = SubgridIZ(subgrid);

	nx = SubgridNX(subgrid);
	ny = SubgridNY(subgrid);
	nz = SubgridNZ(subgrid);

	nx_x = SubvectorNX(x_sub);
	ny_x = SubvectorNY(x_sub);
	nz_x = SubvectorNZ(x_sub);

	nx_z = SubvectorNX(z_sub);
	ny_z = SubvectorNY(z_sub);
	nz_z = SubvectorNZ(z_sub);

	zp = SubvectorElt(z_sub, ix, iy, iz);
	xp = SubvectorElt(x_sub, ix, iy, iz);

	i_x = 0;
	i_z = 0;
	BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
		  i_x, nx_x, ny_x, nz_x, 1, 1, 1,
		  i_z, nx_z, ny_z, nz_z, 1, 1, 1,
		  {
		     zp[i_z] = c * xp[i_x];
		  });
     }
  }
  IncFLOPCount( VectorSize(x) );
}

void PFVAbs(x, z)
/* Abs : z_i = |x_i|   */
Vector *x;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *z_sub;

  double     *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_z;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     i_z = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = fabs(xp[i_x]);
	       });
  }
}

void PFVInv(x, z)
/* Inv : z_i = 1 / x_i    */
Vector *x;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *z_sub;

  double     *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_z;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     i_z = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = ONE / xp[i_x];
	       });
  }
  IncFLOPCount( VectorSize(x) );
}

void PFVAddConst(x, b, z)
/* AddConst : z_i = x_i + b  */
Vector *x;
double  b;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *z_sub;

  double     *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_z;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     i_z = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = xp[i_x] + b;
	       });
  }
  IncFLOPCount( VectorSize(x) );
}

double PFVDotProd(x, y)
/* DotProd = x dot y   */
Vector *x;
Vector *y;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *y_sub;

  double     *yp, *xp;
  double      sum = ZERO;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_y, ny_y, nz_y;

  int         sg, i, j, k, i_x, i_y;

  amps_Invoice   result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);
     y_sub = VectorSubvector(y, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_y = SubvectorNX(y_sub);
     ny_y = SubvectorNY(y_sub);
     nz_y = SubvectorNZ(y_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);
     yp = SubvectorElt(y_sub, ix, iy, iz);

     i_x = 0;
     i_y = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
               i_x, nx_x, ny_x, nz_x, 1, 1, 1,
               i_y, nx_y, ny_y, nz_y, 1, 1, 1,
	       {
		  sum += xp[i_x] * yp[i_y];
               });
  }

  result_invoice = amps_NewInvoice("%d", &sum);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Add);
  amps_FreeInvoice(result_invoice);

  IncFLOPCount( 2 * VectorSize(x) );

  return(sum);
}

double PFVMaxNorm(x)
/* MaxNorm = || x ||_{max}   */
Vector *x;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;

  double     *xp;
  double      max_val = ZERO;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;

  int         sg, i, j, k, i_x;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

    xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
               i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       {
                  if (fabs(xp[i_x]) > max_val) max_val = fabs(xp[i_x]);
	       });
  }

  result_invoice = amps_NewInvoice("%d", &max_val);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Max);
  amps_FreeInvoice(result_invoice);

  return(max_val);
}

double PFVWrmsNorm(x, w)
/* WrmsNorm = sqrt((sum_i (x_i * w_i)^2)/length)  */
Vector *x;
Vector *w;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *w_sub;

  double     *xp, *wp;
  double      prod, sum = ZERO;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_w, ny_w, nz_w;
  
  int         sg, i, j, k, i_x, i_w;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);
     w_sub = VectorSubvector(w, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_w = SubvectorNX(w_sub);
     ny_w = SubvectorNY(w_sub);
     nz_w = SubvectorNZ(w_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);
     wp = SubvectorElt(w_sub, ix, iy, iz);

     i_x = 0;
     i_w = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_w, nx_w, ny_w, nz_w, 1, 1, 1,
	       {
		  prod = xp[i_x] * wp[i_w];
		  sum += prod * prod;
	       });
  }

  result_invoice = amps_NewInvoice("%d", &sum);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Add);
  amps_FreeInvoice(result_invoice);

  IncFLOPCount( 3 * VectorSize(x) );

  return( sqrt( sum / (x -> size) ) );
}

double PFVWL2Norm(x, w)
/* WL2Norm = sqrt(sum_i (x_i * w_i)^2)  */
Vector *x;
Vector *w;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *w_sub;

  double     *xp, *wp;
  double      prod, sum = ZERO;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_w, ny_w, nz_w;
  
  int         sg, i, j, k, i_x, i_w;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);
     w_sub = VectorSubvector(w, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_w = SubvectorNX(w_sub);
     ny_w = SubvectorNY(w_sub);
     nz_w = SubvectorNZ(w_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);
     wp = SubvectorElt(w_sub, ix, iy, iz);

     i_x = 0;
     i_w = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_w, nx_w, ny_w, nz_w, 1, 1, 1,
	       {
                  prod = xp[i_x] * wp[i_w];
		  sum += prod * prod;
	       });
  }

  result_invoice = amps_NewInvoice("%d", &sum);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Add);
  amps_FreeInvoice(result_invoice);

  IncFLOPCount( 3 * VectorSize(x) );

  return(sqrt(sum));
}

double PFVL1Norm(x)
/* L1Norm = sum_i |x_i|  */
Vector *x;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;

  double     *xp;
  double      sum = ZERO;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  
  int         sg, i, j, k, i_x;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       {
		  sum += fabs(xp[i_x]);
	       });
  }

  result_invoice = amps_NewInvoice("%d", &sum);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Add);
  amps_FreeInvoice(result_invoice);

  return(sum);
}

double PFVMin(x)
/* Min = min_i(x_i)   */
Vector *x;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;

  double     *xp;
  double      min_val;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;

  int         sg, i, j, k, i_x;

  amps_Invoice    result_invoice;

  result_invoice = amps_NewInvoice("%d", &min_val);

  grid = VectorGrid(x);

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);

     /* Get initial guess for min_val */
     if (sg == 0)
     {
        i_x = 0;
        BoxLoopI1(i, j, k, ix, iy, iz, 1, 1, 1,
		  i_x, nx_x, ny_x, nz_x, 1, 1, 1,
		  {
		     min_val = xp[i_x];
		  });
     }

     i_x = 0;
     BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       {
		  if (xp[i_x] < min_val) min_val = xp[i_x];
	       });
  }

  amps_AllReduce(amps_CommWorld, result_invoice, amps_Min);
  amps_FreeInvoice(result_invoice);

  return(min_val);
}

double PFVMax(x)
/* Max = max_i(x_i)   */
Vector *x;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;

  double     *xp;
  double      max_val;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;

  int         sg, i, j, k, i_x;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);

     /* Get initial guess for max_val */
     if (sg == 0)
     {
        i_x = 0;
        BoxLoopI1(i, j, k, ix, iy, iz, 1, 1, 1,
		  i_x, nx_x, ny_x, nz_x, 1, 1, 1,
		  {
		     max_val = xp[i_x];
		  });
     }

     i_x = 0;
     BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       {
		  if (xp[i_x] > max_val) max_val = xp[i_x];
	       });
  }

  result_invoice = amps_NewInvoice("%d", &max_val);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Max);
  amps_FreeInvoice(result_invoice);

  return(max_val);
}

int PFVConstrProdPos(c, x)
/* ConstrProdPos: Returns a boolean FALSE if some c[i]!=0.0  */
/*                and x[i]*c[i]<=0.0 */
Vector *c;
Vector *x;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *c_sub;
  Subvector  *x_sub;

  double     *xp, *cp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_c, ny_c, nz_c;

  int         sg, i, j, k, i_x, i_c;

  int  	      val;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     x_sub = VectorSubvector(x, sg);
     c_sub = VectorSubvector(c, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_c = SubvectorNX(c_sub);
     ny_c = SubvectorNY(c_sub);
     nz_c = SubvectorNZ(c_sub);

     xp = SubvectorElt(x_sub, ix, iy, iz);
     cp = SubvectorElt(c_sub, ix, iy, iz);

     val = 1;
     i_c = 0;
     i_x = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_c, nx_c, ny_c, nz_c, 1, 1, 1,
	       {
		  if (cp[i_c] != ZERO)
		     {
		        if ( (xp[i_x] * cp[i_c]) <= ZERO )
			   val = 0;
		     }
	       });
   }

   result_invoice = amps_NewInvoice("%i", &val);
   amps_AllReduce(amps_CommWorld, result_invoice, amps_Min);
   amps_FreeInvoice(result_invoice);

  if (val == 0)
     return(FALSE);
  else
     return(TRUE);
} 

void PFVCompare(c, x, z)
/* Compare : z_i = (x_i > c)  */
double  c;
Vector *x;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *z_sub;

  double     *xp, *zp;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_z;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     i_z = 0;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  zp[i_z] = (fabs(xp[i_x]) >= c) ? ONE : ZERO;
	       });
  }
}


int PFVInvTest(x, z)
/* InvTest = (x_i != 0 forall i), z_i = 1 / x_i  */
Vector *x;
Vector *z;
{
  Grid       *grid     = VectorGrid(x);
  Subgrid    *subgrid;
 
  Subvector  *x_sub;
  Subvector  *z_sub;

  double     *xp, *zp;
  int         val;

  int         ix,   iy,   iz;
  int         nx,   ny,   nz;
  int         nx_x, ny_x, nz_x;
  int         nx_z, ny_z, nz_z;
  
  int         sg, i, j, k, i_x, i_z;

  amps_Invoice    result_invoice;

  ForSubgridI(sg, GridSubgrids(grid))
  {
     subgrid = GridSubgrid(grid, sg);

     z_sub = VectorSubvector(z, sg);
     x_sub = VectorSubvector(x, sg);

     ix = SubgridIX(subgrid);
     iy = SubgridIY(subgrid);
     iz = SubgridIZ(subgrid);

     nx = SubgridNX(subgrid);
     ny = SubgridNY(subgrid);
     nz = SubgridNZ(subgrid);

     nx_x = SubvectorNX(x_sub);
     ny_x = SubvectorNY(x_sub);
     nz_x = SubvectorNZ(x_sub);

     nx_z = SubvectorNX(z_sub);
     ny_z = SubvectorNY(z_sub);
     nz_z = SubvectorNZ(z_sub);

     zp = SubvectorElt(z_sub, ix, iy, iz);
     xp = SubvectorElt(x_sub, ix, iy, iz);

     i_x = 0;
     i_z = 0;
     val = 1;
     BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
	       i_x, nx_x, ny_x, nz_x, 1, 1, 1,
	       i_z, nx_z, ny_z, nz_z, 1, 1, 1,
	       {
		  if (xp[i_x] == ZERO) 
                     val = 0;
		  else
                     zp[i_z] = ONE / (xp[i_x]);
	       });
  }

  result_invoice = amps_NewInvoice("%i", &val);
  amps_AllReduce(amps_CommWorld, result_invoice, amps_Min);
  amps_FreeInvoice(result_invoice);

  if (val == 0)
    return(FALSE);
  else
    return(TRUE);

}

 
/***************** Private Helper Functions **********************/

void PFVCopy(x, y)
/* Copy : y = x   */
Vector *x;
Vector *y;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *y_sub;
   Subvector  *x_sub;

   double     *yp, *xp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;

   int         sg, i, j, k, i_x, i_y;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      yp = SubvectorElt(y_sub, ix, iy, iz);
      xp = SubvectorElt(x_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                {
                   yp[i_y] = xp[i_x];
                });
   }
}

void PFVSum(x, y, z)
/* Sum : z = x + y   */
Vector *x;
Vector *y;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;
   Subvector  *z_sub;

   double     *xp, *yp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_y, i_z;

   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      i_z = 0;
      BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = xp[i_x] + yp[i_y];
                });
   }
  IncFLOPCount( VectorSize(x) );
}

void PFVDiff(x, y, z)
/* Diff : z = x - y  */
Vector *x;
Vector *y;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;
   Subvector  *z_sub;

   double     *xp, *yp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_y, i_z;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      i_z = 0;
      BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = xp[i_x] - yp[i_y];
                });
   }
  IncFLOPCount( VectorSize(x) );
}

void PFVNeg(x, z)
/* Neg : z = - x   */
Vector *x;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *z_sub;

   double     *xp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_z;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_z = 0;
      BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = -xp[i_x];
                });
   }
}

void PFVScaleSum(c, x, y, z)
/* ScaleSum : z = c * x + y   */
double  c;
Vector *x;
Vector *y;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;
   Subvector  *z_sub;

   double     *xp, *yp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_y, i_z;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      i_z = 0;
      BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = c * (xp[i_x] + yp[i_y]);
                });
   }
  IncFLOPCount( 2 * VectorSize(x) );
}

void PFVScaleDiff(c, x, y, z)
/* ScaleDiff : z = c * x - y   */
double  c;
Vector *x;
Vector *y;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;
   Subvector  *z_sub;

   double     *xp, *yp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_y, i_z;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      i_z = 0;
      BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = c * (xp[i_x] - yp[i_y]);
                });
   }
  IncFLOPCount( 2 * VectorSize(x) );
}

void PFVLin1(a, x, y, z)
/* Lin1 : z = a * x + y   */
double  a;
Vector *x;
Vector *y;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;
   Subvector  *z_sub;

   double     *xp, *yp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_y, i_z;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      i_z = 0;
      BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = a * (xp[i_x]) + yp[i_y];
                });
   }
  IncFLOPCount( 2 * VectorSize(x) );
}

void PFVLin2(a, x, y, z)
/* Lin2 : z = a * x - y   */
double  a;
Vector *x;
Vector *y;
Vector *z;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;
   Subvector  *z_sub;

   double     *xp, *yp, *zp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;
   int         nx_z, ny_z, nz_z;

   int         sg, i, j, k, i_x, i_y, i_z;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      z_sub = VectorSubvector(z, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      nx_z = SubvectorNX(z_sub);
      ny_z = SubvectorNY(z_sub);
      nz_z = SubvectorNZ(z_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
      zp = SubvectorElt(z_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      i_z = 0;
      BoxLoopI3(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                i_z, nx_z, ny_z, nz_z, 1, 1, 1,
                {
                   zp[i_z] = a * (xp[i_x]) - yp[i_y];
                });
   }
  IncFLOPCount( 2 * VectorSize(x) );
}

void PFVAxpy (a, x, y)
/* axpy : y = y + a * x   */
double  a;
Vector *x;
Vector *y;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;
   Subvector  *y_sub;

   double     *xp, *yp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;
   int         nx_y, ny_y, nz_y;

   int         sg, i, j, k, i_x, i_y;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      y_sub = VectorSubvector(y, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      nx_y = SubvectorNX(y_sub);
      ny_y = SubvectorNY(y_sub);
      nz_y = SubvectorNZ(y_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
      yp = SubvectorElt(y_sub, ix, iy, iz);
         
      i_x = 0;
      i_y = 0;
      BoxLoopI2(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                i_y, nx_y, ny_y, nz_y, 1, 1, 1,
                {
		   yp[i_y] += a * (xp[i_x]);
                });
   }
  IncFLOPCount( 2 * VectorSize(x) );
}

void PFVScaleBy(a, x)
/* ScaleBy : x = x * a   */
double  a;
Vector *x;
{
   Grid       *grid     = VectorGrid(x);
   Subgrid    *subgrid;
 
   Subvector  *x_sub;

   double     *xp;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_x, ny_x, nz_x;

   int         sg, i, j, k, i_x;


   ForSubgridI(sg, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, sg);
      
      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);
      
      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);
      
      x_sub = VectorSubvector(x, sg);
      
      nx_x = SubvectorNX(x_sub);
      ny_x = SubvectorNY(x_sub);
      nz_x = SubvectorNZ(x_sub);
      
      xp = SubvectorElt(x_sub, ix, iy, iz);
         
      i_x = 0;
      BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
                i_x, nx_x, ny_x, nz_x, 1, 1, 1,
                {
                   xp[i_x] = xp[i_x] * a;
                });
   }
  IncFLOPCount( VectorSize(x) );
}
