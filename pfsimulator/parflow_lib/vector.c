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
 * Routines for manipulating vector structures.
 *
 *
 *****************************************************************************/

#include "parflow.h"
#include "vector.h"

#include "SAMRAI/hier/PatchDescriptor.h"

#include "SAMRAI/pdat/CellVariable.h"
#include "SAMRAI/pdat/SideVariable.h"

using namespace SAMRAI;

#include <stdlib.h>

static int samrai_vector_ids[4][2048];

/*--------------------------------------------------------------------------
 * NewVectorCommPkg:
 *--------------------------------------------------------------------------*/

CommPkg  *NewVectorCommPkg(
   Vector   *vector,
   ComputePkg  *compute_pkg)
{
   CommPkg     *new_commpkg;


   new_commpkg = NewCommPkg(ComputePkgSendRegion(compute_pkg),
		    ComputePkgRecvRegion(compute_pkg),
		    VectorDataSpace(vector), 1, VectorData(vector));

   return new_commpkg;
}

/*--------------------------------------------------------------------------
 * InitVectorUpdate
 *--------------------------------------------------------------------------*/

VectorUpdateCommHandle  *InitVectorUpdate(
   Vector      *vector,
   int          update_mode)
{

   Parflow::GridType grid_type = Parflow::invalid_grid_type;
   switch(vector -> type)
   {
      case cell_centered : 
      case side_centered_x :
      case side_centered_y :
      case side_centered_z :
      {
	 grid_type = Parflow::flow_3D_grid_type;
	 break;
      }
      case cell_centered_2D : 
      {
	 grid_type = Parflow::surface_2D_grid_type;
	 break;
      }
      case clm_topsoil : 
      {
	 grid_type = Parflow::clm_topsoil_grid_type;
	 break;
      }
      case non_samrai_centered : 
      {
	 grid_type = Parflow::invalid_grid_type;
	 break;
      }
   }

   CommHandle *amps_com_handle;
   if(grid_type == Parflow::invalid_grid_type)
   {
      
#ifdef SHMEM_OBJECTS
      amps_com_handle = NULL;
#else
      
#ifdef NO_VECTOR_UPDATE
      amps_com_handle = NULL;
#else
      amps_com_handle = InitCommunication(VectorCommPkg(vector, update_mode));
#endif
      
#endif
   } else {
      tbox::Dimension dim(GlobalsParflowSimulation -> getDim(grid_type));
      if(vector -> boundary_fill_refine_algorithm.isNull()) {
	 tbox::Pointer<hier::PatchHierarchy > hierarchy(GlobalsParflowSimulation -> getPatchHierarchy(grid_type));
	 const int level_number = 0;
	 
	 vector -> boundary_fill_refine_algorithm = new xfer::RefineAlgorithm(dim);
	 
	 vector -> boundary_fill_refine_algorithm -> registerRefine(
	    vector -> samrai_id,
	    vector -> samrai_id,
	    vector -> samrai_id,
	    tbox::Pointer<xfer::RefineOperator>(NULL));
	 
	 tbox::Pointer< hier::PatchLevel > level = 
	    hierarchy->getPatchLevel(level_number);
	 
	 vector -> boundary_fill_schedule = vector -> boundary_fill_refine_algorithm
	    -> createSchedule(level,
			      level_number-1,
			      hierarchy,
			      NULL);
	 
      }
      const double time = 1.0;
      vector -> boundary_fill_schedule -> fillData(time);
      
      amps_com_handle = NULL;

   }

   return new VectorUpdateCommHandle(vector, amps_com_handle);
}


/*--------------------------------------------------------------------------
 * FinalizeVectorUpdate
 *--------------------------------------------------------------------------*/

void         FinalizeVectorUpdate(
   VectorUpdateCommHandle  *handle)
{

   switch(handle -> vector -> type)
   {
      case cell_centered : 
      case side_centered_x :
      case side_centered_y :
      case side_centered_z :
      case cell_centered_2D : 
      case clm_topsoil : 
      {
	 break;
      }
      case non_samrai_centered : 
      {
#ifdef SHMEM_OBJECTS
	 amps_Sync(amps_CommWorld);
#else
	 
#ifdef NO_VECTOR_UPDATE
#else
	 FinalizeCommunication(handle -> comm_handle);
#endif
#endif
	 
	 break;
      }
   };

   delete handle;

}


/*--------------------------------------------------------------------------
 * NewTempVector
 *--------------------------------------------------------------------------*/

static Vector  *NewTempVector(
   Grid    *grid,
   int      nc,
   int      num_ghost)
{
   Vector    *new_vector;
   Subvector *new_sub;

   Subgrid   *subgrid;

   int        data_size;
   int        i, n;

   (void) nc;

   new_vector = ctalloc(Vector, 1); /*address of storage is assigned to the ptr "new_" of type Vector, which is also 
							    the return value of this function */

   (new_vector -> subvectors) = ctalloc(Subvector *, GridNumSubgrids(grid)); /* 1st arg.: variable type;
																	      2nd arg.: # of elements to be allocated*/

   data_size = 0;

   VectorDataSpace(new_vector) = NewSubgridArray();
   ForSubgridI(i, GridSubgrids(grid))
   {
      new_sub = ctalloc(Subvector, 1);

      subgrid = GridSubgrid(grid ,i);

      SubvectorDataSpace(new_sub) = 
	 NewSubgrid(SubgridIX(subgrid) - num_ghost,
		    SubgridIY(subgrid) - num_ghost,
		    SubgridIZ(subgrid) - num_ghost,
		    SubgridNX(subgrid) + 2*num_ghost,
		    SubgridNY(subgrid) + 2*num_ghost,
		    SubgridNZ(subgrid) + 2*num_ghost,
		    SubgridRX(subgrid),
		    SubgridRY(subgrid),
		    SubgridRZ(subgrid),
		    SubgridProcess(subgrid));
      AppendSubgrid(SubvectorDataSpace(new_sub),
		    VectorDataSpace(new_vector));

      n = SubvectorNX(new_sub) * SubvectorNY(new_sub) * SubvectorNZ(new_sub);

      data_size = n;

      VectorSubvector(new_vector, i) = new_sub;
   }

   (new_vector -> data_size) = data_size; /* data_size is sie of data inclduing ghost points */

   VectorGrid(new_vector) = grid; /* Grid that this vector is on */

   VectorSize(new_vector) = GridSize(grid); /* VectorSize(vector) is vector->size, which is the total number of coefficients */


   new_vector -> samrai_id = -1;
   new_vector -> table_index = -1;

   return new_vector;
}


/*--------------------------------------------------------------------------
 * SetTempVectorData
 *--------------------------------------------------------------------------*/

static void     SetTempVectorData(
   Vector  *vector,
   double  *data)
{ 
   Grid       *grid = VectorGrid(vector);

   int         i;


   /* if necessary, free old CommPkg's */
   if (VectorData(vector))
      for(i = 0; i < NumUpdateModes; i++)
	 FreeCommPkg(VectorCommPkg(vector, i));

   VectorData(vector) = data;

   ForSubgridI(i, GridSubgrids(grid))
      SubvectorData(VectorSubvector(vector, i)) = VectorData(vector);

   for(i = 0; i < NumUpdateModes; i++)
   {
      VectorCommPkg(vector, i) =
	 NewVectorCommPkg(vector, GridComputePkg(grid, i));
   }
}


/*--------------------------------------------------------------------------
 * NewVector
 *--------------------------------------------------------------------------*/

Vector  *NewVectorType(
   Grid    *grid,
   int      nc,
   int      num_ghost,
   vector_type type)
{
    Vector  *new_vector;

    new_vector = NewTempVector(grid, nc, num_ghost);

    Parflow::GridType grid_type = Parflow::invalid_grid_type;
    switch(type)
    {
       case cell_centered : 
       case side_centered_x :
       case side_centered_y :
       case side_centered_z :
       {
	  grid_type = Parflow::flow_3D_grid_type;
	  break;
       }
       case cell_centered_2D : 
       {
	  grid_type = Parflow::surface_2D_grid_type;
	  break;
       }
      case clm_topsoil : 
      {
	  grid_type = Parflow::clm_topsoil_grid_type;
	  break;
      }
       case non_samrai_centered :
       {
	  grid_type = Parflow::invalid_grid_type;
	  break;
       }
    }

    tbox::Dimension dim(GlobalsParflowSimulation -> getDim(grid_type));

    hier::IntVector ghosts(dim, num_ghost);

    int index = 0;
    if(grid_type > 0) {
       for(int i = 0; i < 2048; i++)
       {
	  if(samrai_vector_ids[grid_type][i] == 0) {
	     index = i;
	     break;
	  }
       }
    }

    std::string variable_name("Variable_" + tbox::Utilities::intToString(grid_type, 1) + "_" +
			      tbox::Utilities::intToString(index, 4) );

    tbox::Pointer< hier::Variable > variable;

    new_vector -> type = type;
    switch(type)
    {
       case cell_centered : 
       {
	  variable = new pdat::CellVariable<double>(dim, variable_name, 1);	 
	  break;
       }
       case cell_centered_2D : 
       {
	  variable = new pdat::CellVariable<double>(dim, variable_name, 1);	 
	  break;
       }
       case clm_topsoil : 
       {
	  variable = new pdat::CellVariable<double>(dim, variable_name, 1);	 
	  break;
       }
       case side_centered_x : 
       {
	  variable = new pdat::SideVariable<double>(dim, variable_name, 1, true, 0);
	  break;
       }
       case side_centered_y :
       {
	  variable = new pdat::SideVariable<double>(dim, variable_name, 1, true, 1);
	  break;
       }
       case side_centered_z :
       {
	  variable = new pdat::SideVariable<double>(dim, variable_name, 1, true, 2);
	  break;
       }
       case non_samrai_centered :
       {
	  double  *data;

#ifdef SHMEM_OBJECTS
	  /* Node 0 allocates */
	  if(!amps_Rank(amps_CommWorld))
	     data = amps_CTAlloc(double, SizeOfVector(new_vector));
#else
	  data = amps_CTAlloc(double, SizeOfVector(new_vector));
#endif
	  
	  SetTempVectorData(new_vector, data);

	  break;
       }
       default :
       {
	  TBOX_ERROR("Invalid variable type");
       }
    }


    /* For SAMRAI vectors set the data pointer */
    switch(type)
    {
       case cell_centered : 
       case cell_centered_2D : 
       case side_centered_x :
       case side_centered_y :
       case side_centered_z :
       case clm_topsoil : 
       {

	  tbox::Pointer<hier::PatchHierarchy > hierarchy(GlobalsParflowSimulation -> getPatchHierarchy(grid_type));
	  tbox::Pointer<hier::PatchLevel > level(hierarchy -> getPatchLevel(0));

	  tbox::Pointer<hier::PatchDescriptor> patch_descriptor(hierarchy -> getPatchDescriptor());
	  
	  new_vector -> samrai_id = patch_descriptor -> definePatchDataComponent(
	     variable_name, 
	     variable->getPatchDataFactory()->cloneFactory(ghosts));
	  
	  
	  samrai_vector_ids[grid_type][index] = new_vector -> samrai_id;
	  new_vector -> table_index = index;
	  
	  std::cout << "samrai_id " << new_vector -> samrai_id << std::endl;
	  
	  level -> allocatePatchData(new_vector -> samrai_id );
	  
	  for(hier::PatchLevel::Iterator patch_iterator(level); 
	      patch_iterator; 
	      patch_iterator++) {
	     
	     const tbox::Pointer<hier::Patch > patch = 
		level -> getPatch(patch_iterator());
	     
	     const hier::Box patch_box = patch -> getBox();
	     
	     std::cout << "In vector box " << patch_box << std::endl;
	     
	     switch(type)
	     {
		case cell_centered : 
		case cell_centered_2D : 
		case clm_topsoil : 
		{
		   tbox::Pointer< pdat::CellData<double> > patch_data(
		      patch -> getPatchData(new_vector -> samrai_id));
		   SetTempVectorData(new_vector, patch_data -> getPointer(0));
		   patch_data -> fillAll(0);
		   break;
		}
		case side_centered_x :
		{
		   const int side = 0;
		   tbox::Pointer< pdat::SideData<double> > patch_data(
		      patch -> getPatchData(new_vector -> samrai_id));
		   SetTempVectorData(new_vector, patch_data -> getPointer(side, 0));
		   patch_data -> fillAll(0);
		   break;
		}
		case side_centered_y :
		{
		   const int side = 1;
		   tbox::Pointer< pdat::SideData<double> > patch_data(
		      patch -> getPatchData(new_vector -> samrai_id));
		   SetTempVectorData(new_vector, patch_data -> getPointer(side, 0));
		   patch_data -> fillAll(0);
		   break;
		}
		case side_centered_z :
		{
		   const int side = 2;
		   tbox::Pointer< pdat::SideData<double> > patch_data(
		      patch -> getPatchData(new_vector -> samrai_id));
		   SetTempVectorData(new_vector, patch_data -> getPointer(side, 0));
		   patch_data -> fillAll(0);
		   break;
		}
		default :
		   TBOX_ERROR("Invalid variable type");
	     }
	  }
	  break;
       }
       case non_samrai_centered :
	  // This case was already handled above
	  break;
    }    
    return new_vector;
}

Vector  *NewVector(
   Grid    *grid,
   int      nc,
   int      num_ghost)
{
   if(!(grid == globals -> grid3d || grid == globals -> grid2d) ) {
      std::cout << "Error: NewVector on grid not supported by SAMRAI" << std::endl;
   }
   return NewVectorType(grid, nc, num_ghost, cell_centered);
//   return NewVectorType(grid, nc, num_ghost, non_samrai_centered);
}


Vector  *NewNoCommunicationVector(
   Grid    *grid,
   int      nc,
   int      num_ghost)
{
   return NewVectorType(grid, nc, num_ghost, non_samrai_centered);
}

/*--------------------------------------------------------------------------
 * FreeTempVector
 *--------------------------------------------------------------------------*/

void FreeTempVector(Vector *vector)
{
   int i;


   for(i = 0; i < NumUpdateModes; i++)
      FreeCommPkg(VectorCommPkg(vector, i));

   ForSubgridI(i, GridSubgrids(VectorGrid(vector)))
   {
      tfree(VectorSubvector(vector, i));
   }

   FreeSubgridArray(VectorDataSpace(vector));

   tfree(vector -> subvectors);
   tfree(vector);
}


/*--------------------------------------------------------------------------
 * FreeVector
 *--------------------------------------------------------------------------*/

void     FreeVector(
   Vector  *vector)
{
    std::cout << "freeing samrai_id " << vector -> samrai_id << std::endl;


    switch(vector -> type)
    {
       case cell_centered : 
       case cell_centered_2D : 
       case side_centered_x :
       case side_centered_y :
       case side_centered_z :
       case clm_topsoil :
       {
	  Parflow::GridType grid_type = Parflow::flow_3D_grid_type;
	  if(vector -> type == cell_centered_2D) {
	     grid_type = Parflow::surface_2D_grid_type;
	  }

	  tbox::Pointer<hier::PatchHierarchy > hierarchy(GlobalsParflowSimulation -> getPatchHierarchy(grid_type));
	  tbox::Pointer<hier::PatchLevel > level(hierarchy -> getPatchLevel(0));
	  
	  level -> deallocatePatchData(vector -> samrai_id);
	  
	  tbox::Pointer<hier::PatchDescriptor> patch_descriptor(hierarchy -> getPatchDescriptor());
	  patch_descriptor -> removePatchDataComponent(vector -> samrai_id);

	  
	  samrai_vector_ids[grid_type][vector -> table_index] = 0;
	  break;
       }
       case non_samrai_centered :
       {
#ifndef SHMEM_OBJECTS
	  amps_TFree(VectorData(vector));
#endif
	  break;
       }
       default :
	  TBOX_ERROR("Invalid variable type");
    }

   FreeTempVector(vector);
}


/*--------------------------------------------------------------------------
 * InitVector
 *--------------------------------------------------------------------------*/

void    InitVector(
   Vector *v,
   double  value)
{
   Grid       *grid = VectorGrid(v);

   Subvector  *v_sub;
   double     *vp;

   Subgrid    *subgrid;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_v, ny_v, nz_v;

   int         i_s;
   int         i, j, k, iv;


   ForSubgridI(i_s, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, i_s);

      v_sub = VectorSubvector(v, i_s);

      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);

      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);

      nx_v = SubvectorNX(v_sub);
      ny_v = SubvectorNY(v_sub);
      nz_v = SubvectorNZ(v_sub);

      vp = SubvectorElt(v_sub, ix, iy, iz);

      iv = 0;
      BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
		iv, nx_v, ny_v, nz_v, 1, 1, 1,
		{
		   vp[iv] = value;
		});
   }
}

/*--------------------------------------------------------------------------
 * InitVectorAll
 *--------------------------------------------------------------------------*/

void    InitVectorAll(
   Vector *v,
   double  value)
{
   Grid       *grid = VectorGrid(v);

   Subvector  *v_sub;
   double     *vp;

   Subgrid    *subgrid;

   int         ix_v, iy_v, iz_v;
   int         nx_v, ny_v, nz_v;

   int         i_s;
   int         i, j, k, iv;


   ForSubgridI(i_s, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, i_s);

      v_sub = VectorSubvector(v, i_s);

      ix_v = SubvectorIX(v_sub);
      iy_v = SubvectorIY(v_sub);
      iz_v = SubvectorIZ(v_sub);

      nx_v = SubvectorNX(v_sub);
      ny_v = SubvectorNY(v_sub);
      nz_v = SubvectorNZ(v_sub);

      vp = SubvectorData(v_sub);

      iv = 0;
      BoxLoopI1(i, j, k, ix_v, iy_v, iz_v, nx_v, ny_v, nz_v,
		iv, nx_v, ny_v, nz_v, 1, 1, 1,
		{
		   vp[iv] = value;
		});
   }

#ifdef SHMEM_OBJECTS
   amps_Sync(amps_CommWorld);
#endif
}


/*--------------------------------------------------------------------------
 * InitVectorInc
 *--------------------------------------------------------------------------*/


void    InitVectorInc(
   Vector *v,
   double  value,
   double  inc)
{
   Grid       *grid = VectorGrid(v);

   Subvector  *v_sub;
   double     *vp;

   Subgrid    *subgrid;


   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_v, ny_v, nz_v;

   int         i_s;
   int         i, j, k, iv;


   ForSubgridI(i_s, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, i_s);

      v_sub = VectorSubvector(v, i_s);

      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);

      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);

      nx_v = SubvectorNX(v_sub);
      ny_v = SubvectorNY(v_sub);
      nz_v = SubvectorNZ(v_sub);

      vp = SubvectorElt(v_sub, ix, iy, iz);

      iv = 0;
      BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
		iv, nx_v, ny_v, nz_v, 1, 1, 1,
		{
		   vp[iv] = value + (i + j + k)*inc;
		});
   }
}


/*--------------------------------------------------------------------------
 * InitVectorRandom
 *--------------------------------------------------------------------------*/

void    InitVectorRandom(
   Vector *v,
   long    seed)
{
   Grid       *grid = VectorGrid(v);

   Subvector  *v_sub;
   double     *vp;

   Subgrid    *subgrid;

   int         ix,   iy,   iz;
   int         nx,   ny,   nz;
   int         nx_v, ny_v, nz_v;

   int         i_s;
   int         i, j, k, iv;

   srand48(seed);

   ForSubgridI(i_s, GridSubgrids(grid))
   {
      subgrid = GridSubgrid(grid, i_s);

      v_sub = VectorSubvector(v, i_s);

      ix = SubgridIX(subgrid);
      iy = SubgridIY(subgrid);
      iz = SubgridIZ(subgrid);

      nx = SubgridNX(subgrid);
      ny = SubgridNY(subgrid);
      nz = SubgridNZ(subgrid);

      nx_v = SubvectorNX(v_sub);
      ny_v = SubvectorNY(v_sub);
      nz_v = SubvectorNZ(v_sub);

      vp = SubvectorElt(v_sub, ix, iy, iz);

      iv = 0;
      BoxLoopI1(i, j, k, ix, iy, iz, nx, ny, nz,
		iv, nx_v, ny_v, nz_v, 1, 1, 1,
		{
		   vp[iv] = drand48();
		});
   }
}

