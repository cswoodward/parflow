/*BHEADER**********************************************************************
 * (c) 1995   The Regents of the University of California
 *
 * See the file COPYRIGHT_and_DISCLAIMER for a complete copyright
 * notice, contact person, and disclaimer.
 *
 * $Revision: 1.12 $
 *********************************************************************EHEADER*/
/******************************************************************************
 * Header file for `readdatabox.c'
 *
 * (C) 1993 Regents of the University of California.
 *
 *-----------------------------------------------------------------------------
 * $Revision: 1.12 $
 *
 *-----------------------------------------------------------------------------
 *
 *****************************************************************************/

#ifndef READDATABOX_HEADER
#define READDATABOX_HEADER

#include "parflow_config.h"

#include <stdio.h>

#ifdef HAVE_HDF5
#include <hdf5.h>
// #include <netcdf.h>
#endif

#include "databox.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/*-----------------------------------------------------------------------
 * function prototypes
 *-----------------------------------------------------------------------*/

#ifdef __STDC__
# define        P(s) s
#else
# define P(s) ()
#endif


/* readdatabox.c */
Databox *ReadParflowB P((char *file_name ));
Databox *ReadParflowSB P((char *file_name ));
Databox *ReadSimpleA P((char *file_name ));
Databox *ReadRealSA P((char *file_name ));
Databox *ReadSimpleB P((char *file_name ));
Databox *ReadAVSField P((char *filename ));
Databox *ReadSDS P((char *filename , int ds_num ));

#undef P

#endif
