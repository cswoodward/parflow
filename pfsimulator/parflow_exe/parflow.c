/*BHEADER**********************************************************************

  Copyright (c) 1995-2009, Lawrence Livermore National Security,
  LLC. Produced at the Lawrence Livermore National Laboratory. Written
  by the Parflow Team (see the CONTRIBUTORS file)
  <parflow@lists.llnl.gov> CODE-OCEC-08-103. All rights reserved.

  This file is part of Parflow. For details, see
  http://www.llnl.gov/casc/parflow

  Please read the COPYRIGHT file or Our Notice and the LICENSE file
  for the GNU Lesser General Public License.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License (as published
  by the Free Software Foundation) version 2.1 dated February 1999.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
  and conditions of the GNU General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
**********************************************************************EHEADER*/
/******************************************************************************
 *
 * The main routine
 *
 *****************************************************************************/

#include "SAMRAI/SAMRAI_config.h"

#include "SAMRAI/tbox/SAMRAIManager.h"
#include "SAMRAI/tbox/SAMRAI_MPI.h"

#include "parflow.h"

#ifdef HAVE_CEGDB
#include <cegdb.h>
#endif

#include <string>

using namespace SAMRAI;

int main (int argc , char *argv [])
{
   tbox::Dimension dim(3);

   FILE *file = NULL;

   FILE *log_file = NULL;
      
   amps_Clock_t wall_clock_time;

   /*-----------------------------------------------------------------------
    * Initialize tbox::MPI and SAMRAI, enable logging, and process
    * command line.
    *-----------------------------------------------------------------------*/

   tbox::SAMRAI_MPI::init(&argc, &argv);
   tbox::SAMRAIManager::startup();

   /*-----------------------------------------------------------------------
    * Initialize AMPS from existing MPI state initialized by SAMRAI
    *-----------------------------------------------------------------------*/
   if (amps_EmbeddedInit())
   {
      amps_Printf("Error: amps_EmbeddedInit initalization failed\n");
      exit(1);
   }

#ifdef HAVE_CEGDB
   cegdb(&argc, &argv, amps_Rank(MPI_CommWorld));
#endif
   
   wall_clock_time = amps_Clock();

   if(argc != 2) {
      amps_Printf("Error: invalid number of arguments.\n");
      amps_Printf("       Invoke as parflow <pfidb file>.\n");
      exit(1);
   }

   /*-----------------------------------------------------------------------
    * Set up globals structure
    *-----------------------------------------------------------------------*/

   NewGlobals(argv[1]);

   /*-----------------------------------------------------------------------
    * Read the Users Input Deck
    *-----------------------------------------------------------------------*/

   amps_ThreadLocal(input_database) = IDB_NewDB(GlobalsInFileName);

   /*-----------------------------------------------------------------------
    * Setup log printing
    *-----------------------------------------------------------------------*/

   NewLogging();

   /*-----------------------------------------------------------------------
    * Setup timing table
    *-----------------------------------------------------------------------*/

   NewTiming();

   /*-----------------------------------------------------------------------
    * Solve the problem
    *-----------------------------------------------------------------------*/
   Solve();
   printf("Problem solved \n");
   fflush(NULL);

   /*-----------------------------------------------------------------------
    * Log global information
    *-----------------------------------------------------------------------*/

   LogGlobals();

   /*-----------------------------------------------------------------------
    * Print timing results
    *-----------------------------------------------------------------------*/

   PrintTiming();

   /*-----------------------------------------------------------------------
    * Clean up
    *-----------------------------------------------------------------------*/

   FreeLogging();

   FreeTiming();

   /*-----------------------------------------------------------------------
    * Finalize AMPS and exit
    *-----------------------------------------------------------------------*/

   wall_clock_time = amps_Clock() - wall_clock_time;


   IfLogging(0) 
   {
      if(!amps_Rank(amps_CommWorld)) {
	 log_file = OpenLogFile("ParFlow Total Time");

	 fprintf(log_file, "Total Run Time: %f seconds\n\n", 
		      (double)wall_clock_time/(double)AMPS_TICKS_PER_SEC);
      }
   }

   printMaxMemory(log_file);

   IfLogging(0) {
      fprintf(log_file, "\n");

      if(!amps_Rank(amps_CommWorld))
      {
	 printMemoryInfo(log_file);
	 fprintf(log_file, "\n");

	 CloseLogFile(log_file);
      }
   }

   if(!amps_Rank(amps_CommWorld))
   {
      std::string filename = GlobalsOutFileName + ".pftcl";
      
      file = fopen(filename.c_str(), "w" );
      
      IDB_PrintUsage(file, amps_ThreadLocal(input_database));
      
      fclose(file);
   }
      
   IDB_FreeDB(amps_ThreadLocal(input_database));

   FreeGlobals();

   /*-----------------------------------------------------------------------
    * Shutdown AMPS
    *-----------------------------------------------------------------------*/
   amps_Finalize();

   /*-----------------------------------------------------------------------
    * Shutdown SAMRAI and MPI.
    *-----------------------------------------------------------------------*/
   tbox::SAMRAIManager::shutdown();
   tbox::SAMRAI_MPI::finalize();

   return 0;
}

