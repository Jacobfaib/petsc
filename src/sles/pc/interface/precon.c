#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: precon.c,v 1.159 1998/12/17 22:09:36 bsmith Exp bsmith $";
#endif
/*
    The PC (preconditioner) interface routines, callable by users.
*/
#include "src/pc/pcimpl.h"            /*I "sles.h" I*/

#undef __FUNC__  
#define __FUNC__ "PCDestroy"
/*@C
   PCDestroy - Destroys PC context that was created with PCCreate().

   Collective on PC

   Input Parameter:
.  pc - the preconditioner context

.keywords: PC, destroy

.seealso: PCCreate(), PCSetUp()
@*/
int PCDestroy(PC pc)
{
  int ierr = 0;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (--pc->refct > 0) PetscFunctionReturn(0);

  if (pc->destroy) {ierr =  (*pc->destroy)(pc);CHKERRQ(ierr);}
  else {if (pc->data) PetscFree(pc->data);}
  PLogObjectDestroy(pc);
  PetscHeaderDestroy(pc);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCPublish_Petsc"
static int PCPublish_Petsc(PetscObject object)
{
#if defined(HAVE_AMS)
  PC          v = (PC) object;
  int         ierr;
  
  PetscFunctionBegin;

  /* if it is already published then return */
  if (v->amem >=0 ) PetscFunctionReturn(0);

  ierr = PetscObjectPublishBaseBegin(object);CHKERRQ(ierr);
  ierr = PetscObjectPublishBaseEnd(object);CHKERRQ(ierr);
#else
  PetscFunctionBegin;
#endif

  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCCreate"
/*@C
   PCCreate - Creates a preconditioner context.

   Collective on MPI_Comm

   Input Parameter:
.  comm - MPI communicator 

   Output Parameter:
.  pc - location to put the preconditioner context

   Notes:
   The default preconditioner on one processor is PCILU with 0 fill on more 
   then one it is PCBJACOBI with ILU() on each processor.

.keywords: PC, create, context

.seealso: PCSetUp(), PCApply(), PCDestroy()
@*/
int PCCreate(MPI_Comm comm,PC *newpc)
{
  PC     pc;

  PetscFunctionBegin;
  *newpc          = 0;

  PetscHeaderCreate(pc,_p_PC,int,PC_COOKIE,-1,"PC",comm,PCDestroy,PCView);
  PLogObjectCreate(pc);
  pc->bops->publish      = PCPublish_Petsc;
  pc->vec                = 0;
  pc->mat                = 0;
  pc->setupcalled        = 0;
  pc->destroy            = 0;
  pc->data               = 0;
  pc->apply              = 0;
  pc->applytrans         = 0;
  pc->applyBA            = 0;
  pc->applyBAtrans       = 0;
  pc->applyrich          = 0;
  pc->view               = 0;
  pc->getfactoredmatrix  = 0;
  pc->nullsp             = 0;
  pc->applysymmetricright = 0;
  pc->applysymmetricleft  = 0;
  pc->setuponblocks       = 0;
  pc->modifysubmatrices   = 0;
  pc->modifysubmatricesP  = 0;
  *newpc                  = pc;
  PetscPublishAll(pc);
  PetscFunctionReturn(0);

}

/* -------------------------------------------------------------------------------*/
/*
       This variable is used to insure that profiling is never turned on for
    more then one PCApplyXXX() routine at a time.
*/
static int apply_double_count = 0;

#undef __FUNC__  
#define __FUNC__ "PCApply"
/*@
   PCApply - Applies the preconditioner to a vector.

   Input Parameters:
.  pc - the preconditioner context
.  x - input vector

   Output Parameter:
.  y - output vector

   Collective on PC and Vec

.keywords: PC, apply

.seealso: PCApplyTrans(), PCApplyBAorAB()
@*/
int PCApply(PC pc,Vec x,Vec y)
{
  int        ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  if (x == y) SETERRQ(PETSC_ERR_ARG_IDN,0,"x and y must be different vectors");

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  if (!apply_double_count) {PLogEventBegin(PC_Apply,pc,x,y,0);}apply_double_count++;
  ierr = (*pc->apply)(pc,x,y); CHKERRQ(ierr);
  if (apply_double_count == 1) {PLogEventEnd(PC_Apply,pc,x,y,0);}apply_double_count--;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApplySymmetricLeft"
/*@
   PCApplySymmetricLeft - Applies the left part of a symmetric preconditioner to a vector.

   Collective on PC and Vec

   Input Parameters:
+  pc - the preconditioner context
-  x - input vector

   Output Parameter:
.  y - output vector

   Notes:
   Currently, this routine is implemented only for PCICC and PCJACOBI preconditioners.

.keywords: PC, apply, symmetric, left

.seealso: PCApply(), PCApplySymmetricRight()
@*/
int PCApplySymmetricLeft(PC pc,Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  if (!apply_double_count) {PLogEventBegin(PC_ApplySymmetricLeft,pc,x,y,0);}apply_double_count++;
  ierr = (*pc->applysymmetricleft)(pc,x,y); CHKERRQ(ierr);
  if (apply_double_count == 1) {PLogEventEnd(PC_ApplySymmetricLeft,pc,x,y,0);}apply_double_count--;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApplySymmetricRight"
/*@
   PCApplySymmetricRight - Applies the right part of a symmetric preconditioner to a vector.

   Collective on PC and Vec

   Input Parameters:
+  pc - the preconditioner context
-  x - input vector

   Output Parameter:
.  y - output vector

   Notes:
   Currently, this routine is implemented only for PCICC and PCJACOBI preconditioners.

.keywords: PC, apply, symmetric, right

.seealso: PCApply(), PCApplySymmetricLeft()
@*/
int PCApplySymmetricRight(PC pc,Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  if (!apply_double_count) {PLogEventBegin(PC_ApplySymmetricRight,pc,x,y,0);}apply_double_count++;
  ierr = (*pc->applysymmetricright)(pc,x,y); CHKERRQ(ierr);
  if (apply_double_count == 1){PLogEventEnd(PC_ApplySymmetricRight,pc,x,y,0);}apply_double_count--;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApplyTrans"
/*@
   PCApplyTrans - Applies the transpose of preconditioner to a vector.

   Collective on PC and Vec

   Input Parameters:
+  pc - the preconditioner context
-  x - input vector

   Output Parameter:
.  y - output vector

.keywords: PC, apply, transpose

.seealso: PCApplyTrans(), PCApplyBAorAB(), PCApplyBAorABTrans()
@*/
int PCApplyTrans(PC pc,Vec x,Vec y)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  if (x == y) SETERRQ(PETSC_ERR_ARG_IDN,0,"x and y must be different vectors");
  if (!pc->applytrans) SETERRQ(PETSC_ERR_SUP,0,"");

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  if (!apply_double_count) {PLogEventBegin(PC_Apply,pc,x,y,0);}apply_double_count++;
  ierr = (*pc->applytrans)(pc,x,y); CHKERRQ(ierr);
  if (apply_double_count == 1) {PLogEventEnd(PC_Apply,pc,x,y,0);}apply_double_count--;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApplyBAorAB"
/*@
   PCApplyBAorAB - Applies the preconditioner and operator to a vector. 

   Collective on PC and Vec

   Input Parameters:
+  pc - the preconditioner context
.  side - indicates the preconditioner side, one of
$   PC_LEFT, PC_RIGHT, or PC_SYMMETRIC
.  x - input vector
-  work - work vector

   Output Parameter:
.  y - output vector

.keywords: PC, apply, operator

.seealso: PCApply(), PCApplyTrans(), PCApplyBAorABTrans()
@*/
int PCApplyBAorAB(PC pc, PCSide side,Vec x,Vec y,Vec work)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(work,VEC_COOKIE);
  if (x == y) SETERRQ(PETSC_ERR_ARG_IDN,0,"x and y must be different vectors");
  if (side != PC_LEFT && side != PC_SYMMETRIC && side != PC_RIGHT) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,0,"Side must be right, left, or symmetric");
  }

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  if (pc->applyBA) {
    ierr = (*pc->applyBA)(pc,side,x,y,work); CHKERRQ(ierr);
  } else if (side == PC_RIGHT) {
    ierr = PCApply(pc,x,work); CHKERRQ(ierr);
    ierr = MatMult(pc->mat,work,y); CHKERRQ(ierr);
    PetscFunctionReturn(0);
  } else if (side == PC_LEFT) {
    ierr = MatMult(pc->mat,x,work); CHKERRQ(ierr);
    ierr = PCApply(pc,work,y); CHKERRQ(ierr);
    PetscFunctionReturn(0);
  } else if (side == PC_SYMMETRIC) {
    /* There's an extra copy here; maybe should provide 2 work vectors instead? */
    ierr = PCApplySymmetricRight(pc,x,work); CHKERRQ(ierr);
    ierr = MatMult(pc->mat,work,y); CHKERRQ(ierr);
    ierr = VecCopy(y,work); CHKERRQ(ierr);
    ierr = PCApplySymmetricLeft(pc,work,y);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,0,"Invalid preconditioner side");
#if !defined(USE_PETSC_DEBUG)
  PetscFunctionReturn(0);   /* so we get no warning message about no return code */
#endif
}

#undef __FUNC__  
#define __FUNC__ "PCApplyBAorABTrans"
/*@ 
   PCApplyBAorABTrans - Applies the transpose of the preconditioner
   and operator to a vector.

   Collective on PC and Vec

   Input Parameters:
+  pc - the preconditioner context
.  side - indicates the preconditioner side, one of
$   PC_LEFT, PC_RIGHT, or PC_SYMMETRIC
.  x - input vector
-  work - work vector

   Output Parameter:
.  y - output vector

.keywords: PC, apply, operator, transpose

.seealso: PCApply(), PCApplyTrans(), PCApplyBAorAB()
@*/
int PCApplyBAorABTrans(PC pc,PCSide side,Vec x,Vec y,Vec work)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(work,VEC_COOKIE);
  if (x == y) SETERRQ(PETSC_ERR_ARG_IDN,0,"x and y must be different vectors");
  if (pc->applyBAtrans) {
    ierr = (*pc->applyBAtrans)(pc,side,x,y,work);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }
  if (side != PC_LEFT && side != PC_RIGHT) {
    SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,0,"Side must be right or left");
  }

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  if (side == PC_RIGHT) {
    ierr = MatMultTrans(pc->mat,x,work); CHKERRQ(ierr);
    ierr = PCApplyTrans(pc,work,y); CHKERRQ(ierr);
  } else if (side == PC_LEFT) {
    ierr = PCApplyTrans(pc,x,work); CHKERRQ(ierr);
    ierr = MatMultTrans(pc->mat,work,y); CHKERRQ(ierr);
  }
  /* add support for PC_SYMMETRIC */
  PetscFunctionReturn(0); /* actually will never get here */
}

/* -------------------------------------------------------------------------------*/

#undef __FUNC__  
#define __FUNC__ "PCApplyRichardsonExists"
/*@
   PCApplyRichardsonExists - Determines whether a particular preconditioner has a 
   built-in fast application of Richardson's method.

   Not Collective

   Input Parameter:
.  pc - the preconditioner

   Output Parameter:
.  exists - PETSC_TRUE or PETSC_FALSE

.keywords: PC, apply, Richardson, exists

.seealso: PCApplyRichardson()
@*/
int PCApplyRichardsonExists(PC pc, PetscTruth *exists)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidIntPointer(exists);
  if (pc->applyrich) *exists = PETSC_TRUE; 
  else               *exists = PETSC_FALSE;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApplyRichardson"
/*@
   PCApplyRichardson - Applies several steps of Richardson iteration with 
   the particular preconditioner. This routine is usually used by the 
   Krylov solvers and not the application code directly.

   Collective on PC

   Input Parameters:
+  pc  - the preconditioner context
.  x   - the initial guess 
.  w   - one work vector
-  its - the number of iterations to apply.

   Output Parameter:
.  y - the solution

   Notes: 
   Most preconditioners do not support this function. Use the command
   PCApplyRichardsonExists() to determine if one does.

.keywords: PC, apply, Richardson

.seealso: PCApplyRichardsonExists()
@*/
int PCApplyRichardson(PC pc,Vec x,Vec y,Vec w,int its)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(x,VEC_COOKIE);
  PetscValidHeaderSpecific(y,VEC_COOKIE);
  PetscValidHeaderSpecific(w,VEC_COOKIE);
  if (!pc->applyrich) SETERRQ(PETSC_ERR_SUP,0,"");

  if (pc->setupcalled < 2) {
    ierr = PCSetUp(pc); CHKERRQ(ierr);
  }

  ierr = (*pc->applyrich)(pc,x,y,w,its);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* 
      a setupcall of 0 indicates never setup, 
                     1 needs to be resetup,
                     2 does not need any changes.
*/
#undef __FUNC__  
#define __FUNC__ "PCSetUp"
/*@
   PCSetUp - Prepares for the use of a preconditioner.

   Collective on PC

   Input Parameter:
.  pc - the preconditioner context

.keywords: PC, setup

.seealso: PCCreate(), PCApply(), PCDestroy()
@*/
int PCSetUp(PC pc)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);

  if (pc->setupcalled > 1) {
    PLogInfo(pc,"PCSetUp:Setting PC with identical preconditioner\n");
  } else if (pc->setupcalled == 0) {
    PLogInfo(pc,"PCSetUp:Setting up new PC\n");
  } else if (pc->flag == SAME_NONZERO_PATTERN) {
    PLogInfo(pc,"PCSetUp:Setting up PC with same nonzero pattern\n");
  } else {
    PLogInfo(pc,"PCSetUp:Setting up PC with different nonzero pattern\n");
  }
  if (pc->setupcalled > 1) PetscFunctionReturn(0);
  PLogEventBegin(PC_SetUp,pc,0,0,0);
  if (!pc->vec) {SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Vector must be set first");}
  if (!pc->mat) {SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Matrix must be set first");}
  if (!pc->type_name) {
    int size;

    MPI_Comm_size(pc->comm,&size);
    if (size == 1) {
      ierr = PCSetType(pc,PCILU);CHKERRQ(ierr);
    } else {
      ierr = PCSetType(pc,PCBJACOBI);CHKERRQ(ierr);
    }
  }
  if (pc->setup) {
    ierr = (*pc->setup)(pc); CHKERRQ(ierr);
  }
  pc->setupcalled = 2;
  PLogEventEnd(PC_SetUp,pc,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCSetUpOnBlocks"
/*@
   PCSetUpOnBlocks - Sets up the preconditioner for each block in
   the block Jacobi, block Gauss-Seidel, and overlapping Schwarz 
   methods.

   Collective on PC

   Input Parameters:
.  pc - the preconditioner context

.keywords: PC, setup, blocks

.seealso: PCCreate(), PCApply(), PCDestroy(), PCSetUp()
@*/
int PCSetUpOnBlocks(PC pc)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (!pc->setuponblocks) PetscFunctionReturn(0);
  PLogEventBegin(PC_SetUpOnBlocks,pc,0,0,0);
  ierr = (*pc->setuponblocks)(pc); CHKERRQ(ierr);
  PLogEventEnd(PC_SetUpOnBlocks,pc,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCSetModifySubMatrices"
/*@
   PCSetModifySubMatrices - Sets a user-defined routine for modifying the
   submatrices that arise within certain subdomain-based preconditioners.
   The basic submatrices are extracted from the preconditioner matrix as
   usual; the user can then alter these (for example, to set different boundary
   conditions for each submatrix) before they are used for the local solves.

   Input Parameters:
+  pc - the preconditioner context
.  func - routine for modifying the submatrices
-  ctx - optional user-defined context (may be null)

   Collective on PC

   Calling sequence of func:
$     func (PC pc,int nsub,IS *row,IS *col,Mat *submat,void *ctx);

.  row - an array of index sets that contain the global row numbers
         that comprise each local submatrix
.  col - an array of index sets that contain the global column numbers
         that comprise each local submatrix
.  submat - array of local submatrices
-  ctx - optional user-defined context for private data for the 
         user-defined func routine (may be null)

   Notes:
   PCSetModifySubMatrices() MUST be called before SLESSetUp() and
   SLESSolve().

   A routine set by PCSetModifySubMatrices() is currently called within
   the block Jacobi (PCBJACOBI), additive Schwarz (PCASM), and block
   Gauss-Seidel (PCBGS) preconditioners.  All other preconditioners 
   ignore this routine.

.keywords: PC, set, modify, submatrices

.seealso: PCModifySubMatrices()
@*/
int PCSetModifySubMatrices(PC pc,int(*func)(PC,int,IS*,IS*,Mat*,void*),void *ctx)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  pc->modifysubmatrices  = func;
  pc->modifysubmatricesP = ctx;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCModifySubMatrices"
/*@
   PCModifySubMatrices - Calls an optional user-defined routine within 
   certain preconditioners if one has been set with PCSetModifySubMarices().

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
.  nsub - the number of local submatrices
.  row - an array of index sets that contain the global row numbers
         that comprise each local submatrix
.  col - an array of index sets that contain the global column numbers
         that comprise each local submatrix
.  submat - array of local submatrices
-  ctx - optional user-defined context for private data for the 
         user-defined routine (may be null)

   Output Parameter:
.  submat - array of local submatrices (the entries of which may
            have been modified)

   Notes:
   The user should NOT generally call this routine, as it will
   automatically be called within certain preconditioners (currently
   block Jacobi, additive Schwarz, and block Gauss-Seidel) if set.

   The basic submatrices are extracted from the preconditioner matrix
   as usual; the user can then alter these (for example, to set different
   boundary conditions for each submatrix) before they are used for the
   local solves.

.keywords: PC, modify, submatrices

.seealso: PCSetModifySubMatrices()
@*/
int PCModifySubMatrices(PC pc,int nsub,IS *row,IS *col,Mat *submat,void *ctx)
{
  int ierr;

  PetscFunctionBegin;
  if (!pc->modifysubmatrices) PetscFunctionReturn(0);
  PLogEventBegin(PC_ModifySubMatrices,pc,0,0,0);
  ierr = (*pc->modifysubmatrices)(pc,nsub,row,col,submat,ctx); CHKERRQ(ierr);
  PLogEventEnd(PC_ModifySubMatrices,pc,0,0,0);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCSetOperators"
/*@
   PCSetOperators - Sets the matrix associated with the linear system and 
   a (possibly) different one associated with the preconditioner.

   Collective on PC and Mat

   Input Parameters:
+  pc - the preconditioner context
.  Amat - the matrix associated with the linear system
.  Pmat - the matrix to be used in constructing the preconditioner, usually
          the same as Amat. 
-  flag - flag indicating information about the preconditioner matrix structure
   during successive linear solves.  This flag is ignored the first time a
   linear system is solved, and thus is irrelevant when solving just one linear
   system.

   Notes: 
   The flag can be used to eliminate unnecessary work in the preconditioner 
   during the repeated solution of linear systems of the same size.  The 
   available options are
+    SAME_PRECONDITIONER -
       Pmat is identical during successive linear solves.
       This option is intended for folks who are using
       different Amat and Pmat matrices and wish to reuse the
       same preconditioner matrix.  For example, this option
       saves work by not recomputing incomplete factorization
       for ILU/ICC preconditioners.
.     SAME_NONZERO_PATTERN -
       Pmat has the same nonzero structure during
       successive linear solves. 
-     DIFFERENT_NONZERO_PATTERN -
       Pmat does not have the same nonzero structure.

   Caution:
   If you specify SAME_NONZERO_PATTERN, PETSc believes your assertion
   and does not check the structure of the matrix.  If you erroneously
   claim that the structure is the same when it actually is not, the new
   preconditioner will not function correctly.  Thus, use this optimization
   feature carefully!

   If in doubt about whether your preconditioner matrix has changed
   structure or not, use the flag DIFFERENT_NONZERO_PATTERN.

   More Notes about Repeated Solution of Linear Systems:
   PETSc does NOT reset the matrix entries of either Amat or Pmat
   to zero after a linear solve; the user is completely responsible for
   matrix assembly.  See the routine MatZeroEntries() if desiring to
   zero all elements of a matrix.
    
.keywords: PC, set, operators, matrix, linear system

.seealso: PCGetOperators(), MatZeroEntries()
 @*/
int PCSetOperators(PC pc,Mat Amat,Mat Pmat,MatStructure flag)
{
  MatType type;
  int     ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(Amat,MAT_COOKIE);
  PetscValidHeaderSpecific(Pmat,MAT_COOKIE);

  /*
      BlockSolve95 cannot use default BJacobi preconditioning
  */
  ierr = MatGetType(Amat,&type,PETSC_NULL); CHKERRQ(ierr);
  if (type == MATMPIROWBS) {
    if (PetscTypeCompare(pc->type_name,PCBJACOBI)) {
      ierr = PCSetType(pc,PCILU); CHKERRQ(ierr);
      PLogInfo(pc,"PCSetOperators:Switching default PC to PCILU since BS95 doesn't support PCBJACOBI\n");
    }
  }
  /*
      Shell matrix (probably) cannot support a preconditioner
  */
  ierr = MatGetType(Pmat,&type,PETSC_NULL); CHKERRQ(ierr);
  if (type == MATSHELL && PetscStrcmp(pc->type_name,PCSHELL) && PetscStrcmp(pc->type_name,PCMG)) {
    ierr = PCSetType(pc,PCNONE); CHKERRQ(ierr);
    PLogInfo(pc,"PCSetOperators:Setting default PC to PCNONE since MATSHELL doesn't support\n\
    preconditioners (unless defined by the user)\n");
  }

  pc->mat  = Amat;
  pc->pmat = Pmat;
  if (pc->setupcalled == 2 && flag != SAME_PRECONDITIONER) {
    pc->setupcalled = 1;
  }
  pc->flag = flag;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCGetOperators"
/*@C
   PCGetOperators - Gets the matrix associated with the linear system and
   possibly a different one associated with the preconditioner.

   Not Collective though parallel Mats are returned if the PC is parallel

   Input Parameter:
.  pc - the preconditioner context

   Output Parameters:
+  mat - the matrix associated with the linear system
.  pmat - matrix associated with the preconditioner, usually the same
          as mat. 
-  flag - flag indicating information about the preconditioner
          matrix structure.  See PCSetOperators() for details.

.keywords: PC, get, operators, matrix, linear system

.seealso: PCSetOperators()
@*/
int PCGetOperators(PC pc,Mat *mat,Mat *pmat,MatStructure *flag)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (mat)  *mat  = pc->mat;
  if (pmat) *pmat = pc->pmat;
  if (flag) *flag = pc->flag;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCSetVector"
/*@
   PCSetVector - Sets a vector associated with the preconditioner.

   Collective on PC and Vec

   Input Parameters:
+  pc - the preconditioner context
-  vec - the vector

   Notes:
   The vector must be set so that the preconditioner knows what type
   of vector to allocate if necessary.

.keywords: PC, set, vector

.seealso: PCGetVector()

@*/
int PCSetVector(PC pc,Vec vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  PetscValidHeaderSpecific(vec,VEC_COOKIE);
  pc->vec = vec;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCGetVector"
/*@
   PCGetVector - Gets a vector associated with the preconditioner; if the 
     vector was not get set it will return a 0 pointer.

   Not Collective, but vector is shared by all processors that share the PC

   Input Parameter:
.  pc - the preconditioner context

   Output Parameter:
.  vec - the vector

.keywords: PC, get, vector

.seealso: PCSetVector()

@*/
int PCGetVector(PC pc,Vec *vec)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  *vec = pc->vec;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCGetFactoredMatrix"
/*@C 
   PCGetFactoredMatrix - Gets the factored matrix from the
   preconditioner context.  This routine is valid only for the LU, 
   incomplete LU, Cholesky, and incomplete Cholesky methods.

   Not Collective on PC though Mat is parallel if PC is parallel

   Input Parameters:
.  pc - the preconditioner context

   Output parameters:
.  mat - the factored matrix

.keywords: PC, get, factored, matrix
@*/
int PCGetFactoredMatrix(PC pc,Mat *mat)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (pc->getfactoredmatrix) {
    ierr = (*pc->getfactoredmatrix)(pc,mat);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCSetOptionsPrefix"
/*@C
   PCSetOptionsPrefix - Sets the prefix used for searching for all 
   PC options in the database.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  prefix - the prefix string to prepend to all PC option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

.keywords: PC, set, options, prefix, database

.seealso: PCAppendOptionsPrefix(), PCGetOptionsPrefix()
@*/
int PCSetOptionsPrefix(PC pc,char *prefix)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectSetOptionsPrefix((PetscObject)pc, prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCAppendOptionsPrefix"
/*@C
   PCAppendOptionsPrefix - Appends to the prefix used for searching for all 
   PC options in the database.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  prefix - the prefix string to prepend to all PC option requests

   Notes:
   A hyphen (-) must NOT be given at the beginning of the prefix name.
   The first character of all runtime options is AUTOMATICALLY the
   hyphen.

.keywords: PC, append, options, prefix, database

.seealso: PCSetOptionsPrefix(), PCGetOptionsPrefix()
@*/
int PCAppendOptionsPrefix(PC pc,char *prefix)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectAppendOptionsPrefix((PetscObject)pc, prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCGetOptionsPrefix"
/*@
   PCGetOptionsPrefix - Gets the prefix used for searching for all 
   PC options in the database.

   Not Collective

   Input Parameters:
.  pc - the preconditioner context

   Output Parameters:
.  prefix - pointer to the prefix string used, is returned

.keywords: PC, get, options, prefix, database

.seealso: PCSetOptionsPrefix(), PCAppendOptionsPrefix()
@*/
int PCGetOptionsPrefix(PC pc,char **prefix)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectGetOptionsPrefix((PetscObject)pc, prefix);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCPreSolve"
/*@
   PCPreSolve - Optional pre-solve phase, intended for any
   preconditioner-specific actions that must be performed before 
   the iterative solve itself.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  ksp - the Krylov subspace context

   Sample of Usage:
.vb
    PCPreSolve(pc,ksp);
    KSPSolve(ksp,its);
    PCPostSolve(pc,ksp);
.ve

   Note:
   The pre-solve phase is distinct from the PCSetUp() phase.

.keywords: PC, pre-solve

.seealso: PCPostSolve()
@*/
int PCPreSolve(PC pc,KSP ksp)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (pc->presolve) {
    ierr = (*pc->presolve)(pc,ksp);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCPostSolve"
/*@
   PCPostSolve - Optional post-solve phase, intended for any
   preconditioner-specific actions that must be performed after
   the iterative solve itself.

   Input Parameters:
+  pc - the preconditioner context
-  ksp - the Krylov subspace context

   Collective on PC

   Sample of Usage:
.vb
    PCPreSolve(pc,ksp);
    KSPSolve(ksp,its);
    PCPostSolve(pc,ksp);
.ve

.keywords: PC, post-solve

.seealso: PCPreSolve()
@*/
int PCPostSolve(PC pc,KSP ksp)
{
  int ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (pc->postsolve) {
    ierr =  (*pc->postsolve)(pc,ksp);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCView"
/*@ 
   PCView - Prints the PC data structure.

   Input Parameters:
+  PC - the PC context
-  viewer - optional visualization context

   Collective on PC unless Viewer is VIEWER_STDOUT_SELF  

   Note:
   The available visualization contexts include
+     VIEWER_STDOUT_SELF - standard output (default)
-     VIEWER_STDOUT_WORLD - synchronized standard
         output where only the first processor opens
         the file.  All other processors send their 
         data to the first processor to print. 

   The user can open an alternative visualization contexts with
   ViewerASCIIOpen() (output to a specified file).

.keywords: PC, view

.seealso: KSPView(), ViewerASCIIOpen()
@*/
int PCView(PC pc,Viewer viewer)
{
  PCType      cstr;
  int         fmt, ierr, mat_exists;
  ViewerType  vtype;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (viewer) {PetscValidHeader(viewer);} 
  else { viewer = VIEWER_STDOUT_SELF;}

  ViewerGetType(viewer,&vtype);
  if (PetscTypeCompare(vtype,ASCII_VIEWER)) {
    ierr = ViewerGetFormat(viewer,&fmt); CHKERRQ(ierr);
    ViewerASCIIPrintf(viewer,"PC Object:\n");
    ierr = PCGetType(pc,&cstr);CHKERRQ(ierr);
    ViewerASCIIPrintf(viewer,"  method: %s\n",cstr);
    if (pc->view) {
      ierr = ViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = (*pc->view)(pc,viewer);CHKERRQ(ierr);
      ierr = ViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
    PetscObjectExists((PetscObject)pc->mat,&mat_exists);
    if (mat_exists) {
      ViewerPushFormat(viewer,VIEWER_FORMAT_ASCII_INFO,0);
      if (pc->pmat == pc->mat) {
        ViewerASCIIPrintf(viewer,"  linear system matrix = precond matrix:\n");
        ierr = ViewerASCIIPushTab(viewer);CHKERRQ(ierr);
        ierr = MatView(pc->mat,viewer); CHKERRQ(ierr);
        ierr = ViewerASCIIPopTab(viewer);CHKERRQ(ierr);
      } else {
        PetscObjectExists((PetscObject)pc->pmat,&mat_exists);
        if (mat_exists) {
          ViewerASCIIPrintf(viewer,"  linear system matrix followed by preconditioner matrix:\n");
        } else {
          ViewerASCIIPrintf(viewer,"  linear system matrix:\n");
        }
        ierr = ViewerASCIIPushTab(viewer);CHKERRQ(ierr);
        ierr = MatView(pc->mat,viewer); CHKERRQ(ierr);
        if (mat_exists) {ierr = MatView(pc->pmat,viewer); CHKERRQ(ierr);}
        ierr = ViewerASCIIPopTab(viewer);CHKERRQ(ierr);
      }
      ViewerPopFormat(viewer);
    }
  } else if (PetscTypeCompare(vtype,STRING_VIEWER)) {
    PCGetType(pc,&cstr);
    ViewerStringSPrintf(viewer," %-7.7s",cstr);
    if (pc->view) {ierr = (*pc->view)(pc,viewer);CHKERRQ(ierr);}
  }
  PetscFunctionReturn(0);
}

/*MC
   PCRegister - Adds a method to the preconditioner package.

   Synopsis:
   PCRegister(char *name_solver,char *path,char *name_create,int (*routine_create)(PC))

   Not collective

   Input Parameters:
+  name_solver - name of a new user-defined solver
.  path - path (either absolute or relative) the library containing this solver
.  name_create - name of routine to create method context
-  routine_create - routine to create method context

   Notes:
   PCRegister() may be called multiple times to add several user-defined preconditioners.

   If dynamic libraries are used, then the fourth input argument (routine_create)
   is ignored.

   Sample usage:
.vb
   PCRegister("my_solver","/home/username/my_lib/lib/libO/solaris/mylib.a",
              "MySolverCreate",MySolverCreate);
.ve

   Then, your solver can be chosen with the procedural interface via
$     PCSetType(pc,"my_solver")
   or at runtime via the option
$     -pc_type my_solver

.keywords: PC, register

.seealso: PCRegisterAll(), PCRegisterDestroy()
M*/

#undef __FUNC__  
#define __FUNC__ "PCRegister_Private"
int PCRegister_Private(char *sname,char *path,char *name,int (*function)(PC))
{
  int  ierr;
  char fullname[256];

  PetscFunctionBegin;
  PetscStrcpy(fullname,path); PetscStrcat(fullname,":");PetscStrcat(fullname,name);
  ierr = FListAdd_Private(&PCList,sname,fullname,(int (*)(void*))function);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
