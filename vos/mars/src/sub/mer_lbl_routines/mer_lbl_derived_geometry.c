/**  Copyright (c) 2002, California Institute of Technology             **/
/**  U. S. Government sponsorship under NASA contract is acknowledged   **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mer_lbl_derived_geometry.h"

/******************************************************************************
 *				MER_LBL_DERIVED_GEOMETRY
 *
 *	This module contains routines to help create, read/write and print
 *  a MER Derived Geometry label.  It is part of the MIPL label API package,
 *  using a lower-level label processor to do the real work.  This package
 *  basically defines a table that the lower-level routines use.  The table
 *  is the bridge between how the application access the label elements, and
 *  how the label processor specifies the label components to the VICAR label
 *  Run Time Library (RTL).
 *
 *	The label processor interface structures and routines are defined in
 *  the file "lbl_gen_api.h" (Check the label processor documentation for
 *  how to create APIs like this one).  The application program interface
 *  structures are defined in the file "lbl_derived_geometry.h".  The
 *  implementation supporting the interface is this module.
 *
 *	The primary routine used by a typical application program is
 *  LblDerivedGeometry.  This routine requires exactly 4 parameters.
 *  All label API routines must (should) have the same first three parameters:
 *		INT	VICAR RTL unit number of an opened image file.
 *			This is the file where the label will be read or
 *			written.  It must be open with the appropriate
 *			I/O mode
 *		INT	Read/Write flag.  If the value of this parameter is
 *			non-zero, the label will be read from the file.  If
 *			the value of the parameter is zero, a new label will
 *			be written to the file.
 *		VOID*	The structure that an application program will use
 *			to set or retreive the label element values.  Okay
 *			this really isn't a VOID*, but it is a pointer to
 *			the label specific structure.
 *		INT	The instance of this label type.  They typical value
 *			of this parameter should be '1'.
 *
 *	The other two routines contined in this module were included for
 *  development and testing purposes and like the label processing code, use
 *  generic lower-level routines.
 *
 *	All routines use the return_status.h macros to identify the
 *  success or failure of the routine.  Basically, a value of zero represents
 *  a successful completion of the label processing, a non-zero value
 *  indicates a failure.
 *****************************************************************************/


/******************************************************************************
 *				MER_LBL_ROVER_DERIVED_GEOMETRY
 *
 *****************************************************************************/
int     MerLblRoverDerivedGeometry(
  int   Unit,
  int   Obtain,
  LblDerivedGeometry_typ      *LabelItems,
  int	Instance )
{
  LblSetDerivedGeometry("ROVER_DERIVED_GEOMETRY_PARMS");
  return (LblDerivedGeometryApi(Unit,Obtain,LabelItems,Instance,(const char*)NULL));
}

/******************************************************************************
 *				MER_LBL_SITE_DERIVED_GEOMETRY
 *
 *****************************************************************************/
int     MerLblSiteDerivedGeometry(
  int   Unit,
  int   Obtain,
  LblDerivedGeometry_typ      *LabelItems,
  int	Instance )
{
  LblSetDerivedGeometry("SITE_DERIVED_GEOMETRY_PARMS");
  return (LblDerivedGeometryApi(Unit,Obtain,LabelItems,Instance,(const char*)NULL));
}
