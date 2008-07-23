/*BHEADER**********************************************************************
 * (c) 1995   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision: 1.1.1.1 $
 *********************************************************************EHEADER*/
/******************************************************************************
 *
 * Routine to help generate evaluation points (w.r.t. f' and h')
 * to be maximumized over to find dt values.
 *
 *****************************************************************************/

#include "parflow.h"

/*--------------------------------------------------------------------------
 * NewEvalStruct
 *--------------------------------------------------------------------------*/

EvalStruct *NewEvalStruct(problem)
Problem   *problem;
{
   EvalStruct    *eval_struct;

   double         a, b;
   double         phi, arg, scale, x, xnew;
   double         tmp_array[6];
   int            nterms;
   double         pcoeff[7], qcoeff[7], deriv[7];
   int            i, k, iters, number;
   int            searching, not_found;

   eval_struct = ctalloc(EvalStruct, 1);

   a = 1.0 / ProblemPhaseViscosity(problem, 0);
   b = 1.0 / ProblemPhaseViscosity(problem, 1);

   /***************************************************/
   /*                                                 */ 
   /*   Find the f'' roots analytically (see notes)   */
   /*   Save the f' value there as well as the root   */
   /*                                                 */ 
   /***************************************************/

   number = 0;

   if ( fabs((a-b)/(a+b)) <= 1.0 )
   {
      phi = acos((a-b)/(a+b));

      arg = phi / ((double) 3.0);
      tmp_array[number] = ((double) 0.5) + cos( arg );
      number++;

      arg = (phi + (((double) 2.0) * ((double) M_PI))) / ((double) 3.0);
      tmp_array[number] = ((double) 0.5) + cos( arg );
      number++;

      arg = (phi + (((double) 4.0) * ((double) M_PI))) / ((double) 3.0);
      tmp_array[number] = ((double) 0.5) + cos( arg );
      number++;
   }

   EvalNumFPoints(eval_struct) = number;
   if ( EvalNumFPoints(eval_struct) > 0 )
   {
      EvalFPoints(eval_struct) = ctalloc( double, number);
      EvalFValues(eval_struct) = ctalloc( double, number);
      for(i = 0; i < number; i++)
      {
         EvalFPoint(eval_struct, i) = tmp_array[i];
         EvalFValue(eval_struct, i) = Fprime_OF_S(tmp_array[i],a,b);
      }
   }

   /***************************************************/
   /*                                                 */ 
   /*   Find the h'' roots analytically (see notes)   */
   /*   Save the h' value there as well as the root   */
   /*                                                 */ 
   /***************************************************/

   number = 0;

   scale = (a + b) * (a + b);

   pcoeff[0] =  1.0*b*b         / scale;
   pcoeff[1] = -6.0*b*b         / scale;
   pcoeff[2] = -3.0*b*(a - 5*b) / scale;
   pcoeff[3] =  4.0*b*(a - 5*b) / scale;
   pcoeff[4] =  3.0*b*(a + 5*b) / scale;
   pcoeff[5] = -6.0*b*(a +   b) / scale;
   pcoeff[6] =  1.0;

   searching = 1;
   nterms = 6;
   while ( searching && (nterms > 1) )
   {

      not_found = 1; iters = 0;

      x = XSTART;
      deriv[nterms] = qcoeff[nterms] = pcoeff[nterms];
      while ( not_found && (iters <= MAXITERATIONS) )
      {

         for(k = nterms-1; k > 0; k--)
         {
            qcoeff[k] = pcoeff[k] + x * qcoeff[k+1];
            deriv[k]  = qcoeff[k] + x * deriv[k+1];
         }
         qcoeff[0] = pcoeff[0] + x * qcoeff[1];
         xnew = x - qcoeff[0] / deriv[1];

         if ( (fabs(qcoeff[0]) <= EPSILON1) || (fabs(x - xnew) <= EPSILON2 ) )
         {
               not_found = 0;
               tmp_array[number] = xnew;
               number++;
         }
         else
         {
            x = xnew;
            iters++;
         }
      }

      if ( (iters > MAXITERATIONS) && not_found )
      {
         searching = 0;
      }
      else
      {
         for(k = 0; k < nterms; k++)
         {
            pcoeff[k] = qcoeff[k+1];
         }
         nterms--;
      }
   }

   if ( nterms == 1 )
   {
      tmp_array[number] = -pcoeff[0];
      number++;
   }

   EvalNumHPoints(eval_struct) = number;
   if ( EvalNumHPoints(eval_struct) > 0 )
   {
      EvalHPoints(eval_struct) = ctalloc( double, number);
      EvalHValues(eval_struct) = ctalloc( double, number);
      for(i = 0; i < number; i++)
      {
         EvalHPoint(eval_struct, i) = tmp_array[i];
         EvalHValue(eval_struct, i) = Hprime_OF_S(tmp_array[i],a,b);
      }
   }

   /*****************************************************/
   /*                                                   */ 
   /* Print out some diagnostics on the computed values */
   /*                                                   */ 
   /*****************************************************/

#if 0
   if(!amps_Rank(amps_CommWorld))
   {
      amps_Printf("F_prime:\n");
      for(i = 0; i < EvalNumFPoints(eval_struct); i++)
      {
         amps_Printf("   Point = %e, Value = %e\n",
                         EvalFPoint(eval_struct, i),
                         EvalFValue(eval_struct, i));
      }

      amps_Printf("H_prime:\n");
      for(i = 0; i < EvalNumHPoints(eval_struct); i++)
      {
         amps_Printf("   Point = %e, Value = %e\n",
                         EvalHPoint(eval_struct, i),
                         EvalHValue(eval_struct, i));
      }
   }
#endif

   return eval_struct;
}

/*--------------------------------------------------------------------------
 * FreeEvalStruct
 *--------------------------------------------------------------------------*/

void     FreeEvalStruct(eval_struct)
EvalStruct *eval_struct;
{

   if ( EvalNumHPoints(eval_struct) > 0 )
   {
      tfree(EvalHValues(eval_struct));
      tfree(EvalHPoints(eval_struct));
   }
   if ( EvalNumHPoints(eval_struct) > 0 )
   {
      tfree(EvalFValues(eval_struct));
      tfree(EvalFPoints(eval_struct));
   }

   tfree(eval_struct);

}
