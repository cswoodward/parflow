/*BHEADER**********************************************************************
 * (c) 1995   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision: 1.1.1.1 $
 *********************************************************************EHEADER*/
/* The following needs to be in the input file test9.input */
/*
   11
   ATestString
   4 10 234 5 6
   65555 200 234 678 890 6789 2789
   100000 2789 78 8 1 98 987 98765
   12.5 12.0005 17.4 679.8
   12.5 0.078 679.8 0.5
   */

#include <stdio.h>
#include "amps.h"

char *filename = "test9.input";

int main (argc, argv)
int argc;
char *argv[];
{
 
   amps_File file;
   amps_Invoice recv_invoice;
    
   int   num;
   int   me;
   int   i;

   char *string = "ATestString";
   char recvd_string[20];
   int string_length;
   
   short shorts[] = {4, 10, 234, 5, 6};
   short recvd_shorts[5];
   int shorts_length = 5;
   
   int ints[] = { 65555, 200, 234, 678, 890, 6789, 2789};
   int recvd_ints[7];
   int ints_length = 7;
   
   long longs[] = { 100000, 2789, 78, 8, 1, 98, 987, 98765};
   long recvd_longs[8];
   int longs_length = 8;
   
   double doubles[] = { 12.5, 10, 12.0005, 0.078, 17.4, 13.5, 679.8, 189.7 };
   double recvd_doubles[8];
   int doubles_length = 4;
   
   float floats[]= { 12.5, 10, 12.0005, 0.078, 17.4, 13.5,679.8,189.7,0.01,
			0.5};
   float recvd_floats[12];
   int floats_length = 4;
   int floats_stride = 3;

   int loop;

   int result = 0;

   if (amps_Init(&argc, &argv))
   {
      amps_Printf("Error amps_Init\n");
      exit(1);
   }

   loop=atoi(argv[1]);
   
   num = amps_Size(amps_CommWorld);

   me = amps_Rank(amps_CommWorld);

   for(;loop;loop--)
   {

      recv_invoice = amps_NewInvoice("%i%&c%&s%*i%*l%*.2d%*.&f", 
				     &string_length,
				     &string_length, recvd_string,
				     &shorts_length, recvd_shorts,
				     ints_length, recvd_ints,
				     longs_length, recvd_longs,
				     doubles_length, recvd_doubles,
				     floats_length, &floats_stride, recvd_floats);
      
      if(!(file = amps_SFopen(filename, "r")))
      {
	 amps_Printf("Error on file open\n");
	 amps_Exit(1);
      }
      
      
      amps_SFBCast(amps_CommWorld, file, recv_invoice);
      
      
      amps_SFclose(file);
      
      if(strncmp(recvd_string, string, 11))
      {
	 amps_Printf("ERROR: chars do not match expected (%s) recvd (%s)\n",
		     string, recvd_string);
	 result |= 1;
      }
      else
      {
	 
	 result |= 0;
      }
      
      for(i=0; i < shorts_length; i++)
	 if(shorts[i] != recvd_shorts[i])
	 {
	    amps_Printf("ERROR: shorts do not match expected (%hd) recvd (%hd)\n",
			shorts[i], recvd_shorts[i]);
	    result |= 1;
	 }
	 else
	 {
	    result |= 0;
	 }
      
      for(i=0; i < ints_length; i++)
	 if(ints[i] != recvd_ints[i])
	 {
	    amps_Printf("ERROR: ints do not match expected (%i) recvd (%i)\n",
			ints[i], recvd_ints[i]);
	    result |= 1;
	 }
	 else
	 {
	    result |= 0;
	 }
      
      for(i=0; i < longs_length; i++)
	 if(longs[i] != recvd_longs[i])
	 {
	    amps_Printf("ERROR: longs do not match expected (%ld) recvd (%ld)\n",
			longs[i], recvd_longs[i]);
	    result |= 1;
	 }
	 else
	 {
	    result |= 0;
	 }
      
      for(i=0; i < doubles_length*2; i += 2)
	 if(doubles[i] != recvd_doubles[i])
	 {
	    amps_Printf("ERROR: doubles do not match (%lf) recvd (%lf)\n", 
			doubles[i], recvd_doubles[i]);
	    result |= 1;
	 }
	 else
	 {
	    result |= 0;
	 }
      
      for(i=0; i < floats_length*3; i += 3)
	 if(floats[i] != recvd_floats[i])
	 {
	    amps_Printf("ERROR: floats do not match expected (%f) recvd (%f)\n",
			floats[i], recvd_floats[i]);
	    result |= 1;
	 }
	 else
	 {
	    result |= 0;
	 }
      
      if(result == 0 )
	 amps_Printf("Success\n");
      else
	 amps_Printf("ERROR\n");
      
      amps_FreeInvoice(recv_invoice);
   }

   amps_Finalize();

   return result;
}
