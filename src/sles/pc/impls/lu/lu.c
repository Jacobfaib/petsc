#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: lu.c,v 1.105 1998/12/17 22:09:43 bsmith Exp bsmith $";
#endif
/*
   Defines a direct factorization preconditioner for any Mat implementation
   Note: this need not be consided a preconditioner since it supplies
         a direct solver.
*/
#include "src/pc/pcimpl.h"                /*I "pc.h" I*/

typedef struct {
  Mat               fact;             /* factored matrix */
  double            fill, actualfill; /* expected and actual fill in factor */
  int               inplace;          /* flag indicating in-place factorization */
  IS                row, col;         /* index sets used for reordering */
  MatReorderingType ordering;         /* matrix ordering */
  int               reusereordering;  /* reuses previous reordering computed */
  int               reusefill;        /* reuse fill from previous LU */
} PC_LU;


EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PCLUSetReuseReordering_LU"
int PCLUSetReuseReordering_LU(PC pc,PetscTruth flag)
{
  PC_LU *lu;

  PetscFunctionBegin;
  lu                  = (PC_LU *) pc->data;
  lu->reusereordering = (int) flag;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PCLUSetReuseFill_LU"
int PCLUSetReuseFill_LU(PC pc,PetscTruth flag)
{
  PC_LU *lu;

  PetscFunctionBegin;
  lu = (PC_LU *) pc->data;
  lu->reusefill = (int) flag;
  PetscFunctionReturn(0);
}
EXTERN_C_END

#undef __FUNC__  
#define __FUNC__ "PCSetFromOptions_LU"
static int PCSetFromOptions_LU(PC pc)
{
  int    ierr,flg;
  double fill;

  PetscFunctionBegin;
  ierr = OptionsHasName(pc->prefix,"-pc_lu_in_place",&flg); CHKERRQ(ierr);
  if (flg) {
    ierr = PCLUSetUseInPlace(pc); CHKERRQ(ierr);
  }
  ierr = OptionsGetDouble(pc->prefix,"-pc_lu_fill",&fill,&flg); CHKERRQ(ierr);
  if (flg) {
    ierr = PCLUSetFill(pc,fill); CHKERRQ(ierr);
  }
  ierr = OptionsHasName(pc->prefix,"-pc_lu_reuse_fill",&flg); CHKERRQ(ierr);
  if (flg) {
    ierr = PCLUSetReuseFill(pc,PETSC_TRUE); CHKERRQ(ierr);
  }
  ierr = OptionsHasName(pc->prefix,"-pc_lu_reuse_reordering",&flg); CHKERRQ(ierr);
  if (flg) {
    ierr = PCLUSetReuseReordering(pc,PETSC_TRUE); CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCPrintHelp_LU"
static int PCPrintHelp_LU(PC pc,char *p)
{
  PetscFunctionBegin;
  (*PetscHelpPrintf)(pc->comm," Options for PCLU preconditioner:\n");
  (*PetscHelpPrintf)(pc->comm," %spc_lu_in_place: do factorization in place\n",p);
  (*PetscHelpPrintf)(pc->comm," %spc_lu_fill <fill>: expected fill in factor\n",p);
  (*PetscHelpPrintf)(pc->comm," -mat_order <name>: ordering to reduce fill",p);
  (*PetscHelpPrintf)(pc->comm," (nd,natural,1wd,rcm,qmd)\n");
  (*PetscHelpPrintf)(pc->comm," %spc_lu_nonzeros_along_diagonal <tol>: changes column ordering\n",p);
  (*PetscHelpPrintf)(pc->comm,"    to reduce the change of obtaining zero pivot during LU.\n");
  (*PetscHelpPrintf)(pc->comm,"    If <tol> not given defaults to 1.e-10.\n");
  (*PetscHelpPrintf)(pc->comm," %spc_lu_reuse_reordering:                          \n",p);
  (*PetscHelpPrintf)(pc->comm," %spc_lu_reuse_fill:                             \n",p);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCView_LU"
static int PCView_LU(PC pc,Viewer viewer)
{
  PC_LU      *lu = (PC_LU *) pc->data;
  int        ierr;
  char       *order;
  ViewerType vtype;

  PetscFunctionBegin;
  ierr = MatReorderingGetName(lu->ordering,&order); CHKERRQ(ierr);
  ierr = ViewerGetType(viewer,&vtype);CHKERRQ(ierr);
  if (PetscTypeCompare(vtype,ASCII_VIEWER)) {
    MatInfo info;

    if (lu->inplace) ViewerASCIIPrintf(viewer,"  LU: in-place factorization\n");
    else             ViewerASCIIPrintf(viewer,"  LU: out-of-place factorization\n");
    ViewerASCIIPrintf(viewer,"      matrix ordering: %s\n",order);
    if (lu->fact) {
      ierr = MatGetInfo(lu->fact,MAT_LOCAL,&info); CHKERRQ(ierr);
      ViewerASCIIPrintf(viewer,"      LU nonzeros %g\n",info.nz_used);
    }
    if (lu->reusefill)       ViewerASCIIPrintf(viewer,"         Reusing fill from past factorization\n");
    if (lu->reusereordering) ViewerASCIIPrintf(viewer,"         Reusing reordering from past factorization\n");
  } else if (PetscTypeCompare(vtype,STRING_VIEWER)) {
    ierr = ViewerStringSPrintf(viewer," order=%s",order);CHKERRQ(ierr);
  } else {
    SETERRQ(1,1,"Viewer type not supported for this object");
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCGetFactoredMatrix_LU"
static int PCGetFactoredMatrix_LU(PC pc,Mat *mat)
{
  PC_LU *dir = (PC_LU *) pc->data;

  PetscFunctionBegin;
  *mat = dir->fact;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCSetUp_LU"
static int PCSetUp_LU(PC pc)
{
  int         ierr,flg;
  PC_LU       *dir = (PC_LU *) pc->data;

  PetscFunctionBegin;
  if (dir->reusefill && pc->setupcalled) dir->fill = dir->actualfill;

  if (dir->inplace) {
    if (dir->row && dir->col && dir->row != dir->col) {ierr = ISDestroy(dir->row);CHKERRQ(ierr);}
    if (dir->col) {ierr = ISDestroy(dir->col); CHKERRQ(ierr);}
    ierr = MatGetReorderingTypeFromOptions(0,&dir->ordering); CHKERRQ(ierr);
    ierr = MatGetReordering(pc->pmat,dir->ordering,&dir->row,&dir->col); CHKERRQ(ierr);
    if (dir->row) {PLogObjectParent(pc,dir->row); PLogObjectParent(pc,dir->col);}
    ierr = MatLUFactor(pc->pmat,dir->row,dir->col,dir->fill); CHKERRQ(ierr);
    dir->fact = pc->pmat;
  } else {
    MatInfo info;
    if (!pc->setupcalled) {
      ierr = MatGetReorderingTypeFromOptions(0,&dir->ordering); CHKERRQ(ierr);
      ierr = MatGetReordering(pc->pmat,dir->ordering,&dir->row,&dir->col); CHKERRQ(ierr);
      ierr = OptionsHasName(pc->prefix,"-pc_lu_nonzeros_along_diagonal",&flg);CHKERRQ(ierr);
      if (flg) {
        double tol = 1.e-10;
        ierr = OptionsGetDouble(pc->prefix,"-pc_lu_nonzeros_along_diagonal",&tol,&flg);CHKERRQ(ierr);
        ierr = MatReorderForNonzeroDiagonal(pc->pmat,tol,dir->row,dir->col);CHKERRQ(ierr);
      }
      if (dir->row) {PLogObjectParent(pc,dir->row); PLogObjectParent(pc,dir->col);}
      ierr = MatLUFactorSymbolic(pc->pmat,dir->row,dir->col,dir->fill,&dir->fact); CHKERRQ(ierr);
      ierr = MatGetInfo(dir->fact,MAT_LOCAL,&info);CHKERRQ(ierr);
      dir->actualfill = info.fill_ratio_needed;
      PLogObjectParent(pc,dir->fact);
    } else if (pc->flag != SAME_NONZERO_PATTERN) {
      if (!dir->reusereordering) {
        if (dir->row && dir->col && dir->row != dir->col) {ierr = ISDestroy(dir->row);CHKERRQ(ierr);}
        if (dir->col) {ierr = ISDestroy(dir->col); CHKERRQ(ierr);}
        ierr = MatGetReordering(pc->pmat,dir->ordering,&dir->row,&dir->col); CHKERRQ(ierr);
        ierr = OptionsHasName(pc->prefix,"-pc_lu_nonzeros_along_diagonal",&flg);CHKERRQ(ierr);
        if (flg) {
          double tol = 1.e-10;
          ierr = OptionsGetDouble(pc->prefix,"-pc_lu_nonzeros_along_diagonal",&tol,&flg);CHKERRQ(ierr);
          ierr = MatReorderForNonzeroDiagonal(pc->pmat,tol,dir->row,dir->col);CHKERRQ(ierr);
        }
        if (dir->row) {PLogObjectParent(pc,dir->row); PLogObjectParent(pc,dir->col);}
      }
      ierr = MatDestroy(dir->fact); CHKERRQ(ierr);
      ierr = MatLUFactorSymbolic(pc->pmat,dir->row,dir->col,dir->fill,&dir->fact); CHKERRQ(ierr);
      ierr = MatGetInfo(dir->fact,MAT_LOCAL,&info);CHKERRQ(ierr);
      dir->actualfill = info.fill_ratio_needed;
      PLogObjectParent(pc,dir->fact);
    }
    ierr = MatLUFactorNumeric(pc->pmat,&dir->fact); CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCDestroy_LU"
static int PCDestroy_LU(PC pc)
{
  PC_LU *dir = (PC_LU*) pc->data;
  int   ierr;

  PetscFunctionBegin;
  if (!dir->inplace && dir->fact) {ierr = MatDestroy(dir->fact); CHKERRQ(ierr);}
  if (dir->row && dir->col && dir->row != dir->col) {ierr = ISDestroy(dir->row); CHKERRQ(ierr);}
  if (dir->col) {ierr = ISDestroy(dir->col); CHKERRQ(ierr);}
  PetscFree(dir); 
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApply_LU"
static int PCApply_LU(PC pc,Vec x,Vec y)
{
  PC_LU *dir = (PC_LU *) pc->data;
  int   ierr;

  PetscFunctionBegin;
  if (dir->inplace) {ierr = MatSolve(pc->pmat,x,y); CHKERRQ(ierr);}
  else              {ierr = MatSolve(dir->fact,x,y); CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCApply_LU"
static int PCApplyTrans_LU(PC pc,Vec x,Vec y)
{
  PC_LU *dir = (PC_LU *) pc->data;
  int   ierr;

  PetscFunctionBegin;
  if (dir->inplace) {ierr = MatSolveTrans(pc->pmat,x,y); CHKERRQ(ierr);}
  else              {ierr = MatSolveTrans(dir->fact,x,y); CHKERRQ(ierr);}
  PetscFunctionReturn(0);
}

/* -----------------------------------------------------------------------------------*/

EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PCLUSetFill_LU"
int PCLUSetFill_LU(PC pc,double fill)
{
  PC_LU *dir;

  PetscFunctionBegin;
  dir = (PC_LU *) pc->data;
  dir->fill = fill;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PCLUSetUseInPlace_LU"
int PCLUSetUseInPlace_LU(PC pc)
{
  PC_LU *dir;

  PetscFunctionBegin;
  dir = (PC_LU *) pc->data;
  dir->inplace = 1;
  PetscFunctionReturn(0);
}
EXTERN_C_END

EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PCLUSetMatReordering_LU"
int PCLUSetMatReordering_LU(PC pc, MatReorderingType ordering)
{
  PC_LU *dir = (PC_LU *) pc->data;

  PetscFunctionBegin;
  dir->ordering = ordering;
  PetscFunctionReturn(0);
}
EXTERN_C_END

/* -----------------------------------------------------------------------------------*/

#undef __FUNC__  
#define __FUNC__ "PCLUSetReuseReordering"
/*@
   PCLUSetReuseReordering - When similar matrices are factored, this
   causes the ordering computed in the first factor to be used for all
   following factors; applies to both fill and drop tolerance LUs.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  flag - PETSC_TRUE to reuse else PETSC_FALSE

   Options Database Key:
.  -pc_lu_reuse_reordering - Activate PCLUSetReuseReordering()

.keywords: PC, levels, reordering, factorization, incomplete, LU

.seealso: PCLUSetReuseFill(), PCILUSetReuseReordering(), PCILUSetReuseFill()
@*/
int PCLUSetReuseReordering(PC pc,PetscTruth flag)
{
  int ierr, (*f)(PC,PetscTruth);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCLUSetReuseReordering_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,flag);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCLUSetReuseFill"
/*@
   PCLUSetReuseFill - When matrices with same nonzero structure are LU factored,
     this causes later ones to use the fill computed in the initial factorization.

   Collective on PC

   Input Parameters:
+  pc - the preconditioner context
-  flag - PETSC_TRUE to reuse else PETSC_FALSE

   Options Database Key:
.  -pc_lu_reuse_fill - Activates PCLUSetReuseFill()

.keywords: PC, levels, reordering, factorization, incomplete, LU

.seealso: PCILUSetReuseReordering(), PCLUSetReuseReordering(), PCILUSetReuseFill()
@*/
int PCLUSetReuseFill(PC pc,PetscTruth flag)
{
  int ierr, (*f)(PC,PetscTruth);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCLUSetReuseFill_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,flag);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCLUSetFill"
/*@
   PCLUSetFill - Indicate the amount of fill you expect in the factored matrix,
       fill = number nonzeros in factor/number nonzeros in original matrix.

   Collective on PC
   
   Input Parameters:
+  pc - the preconditioner context
-  fill - amount of expected fill

   Options Database Key:
.  -pc_lu_fill <fill> - Sets fill amount

   Note:
   For sparse matrix factorizations it is difficult to predict how much 
   fill to expect. By running with the option -log_info PETSc will print the 
   actual amount of fill used; allowing you to set the value accurately for
   future runs. Bt default PETSc uses a value of 5.0

.keywords: PC, set, factorization, direct, fill

.seealso: PCILUSetFill()
@*/
int PCLUSetFill(PC pc,double fill)
{
  int ierr, (*f)(PC,double);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  if (fill < 1.0) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,1,"Fill factor cannot be less then 1.0");
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCLUSetFill_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,fill);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PCLUSetUseInPlace"
/*@
   PCLUSetUseInPlace - Tells the system to do an in-place factorization.
   For dense matrices, this enables the solution of much larger problems. 
   For sparse matrices the factorization cannot be done truly in-place 
   so this does not save memory during the factorization, but after the matrix
   is factored, the original unfactored matrix is freed, thus recovering that
   space.

   Collective on PC

   Input Parameters:
.  pc - the preconditioner context

   Options Database Key:
.  -pc_lu_in_place - Activates in-place factorization

   Note:
   PCLUSetUseInplace() can only be used with the KSP method KSPPREONLY or when 
   a different matrix is provided for the multiply and the preconditioner in 
   a call to SLESSetOperators().
   This is because the Krylov space methods require an application of the 
   matrix multiplication, which is not possible here because the matrix has 
   been factored in-place, replacing the original matrix.

.keywords: PC, set, factorization, direct, inplace, in-place, LU

.seealso: PCILUSetUseInPlace()
@*/
int PCLUSetUseInPlace(PC pc)
{
  int ierr, (*f)(PC);

  PetscFunctionBegin;
  PetscValidHeaderSpecific(pc,PC_COOKIE);
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCLUSetUseInPlace_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

/*@
    PCLUSetMatReordering - Sets the ordering routine (to reduce fill) to 
    be used it the LU factorization.

    Collective on PC

    Input Parameters:
+   pc - the preconditioner context
-   ordering - the matrix ordering name, for example, ORDER_ND or ORDER_RCM

   Options Database Key:
.  -mat_order <nd,rcm,...> - Sets ordering routine

.seealso: PCILUSetMatReordering()
@*/
int PCLUSetMatReordering(PC pc, MatReorderingType ordering)
{
  int ierr, (*f)(PC,MatReorderingType);

  PetscFunctionBegin;
  ierr = PetscObjectQueryFunction((PetscObject)pc,"PCLUSetMatReordering_C",(void **)&f);CHKERRQ(ierr);
  if (f) {
    ierr = (*f)(pc,ordering);CHKERRQ(ierr);
  } 
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------------ */

EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "PCCreate_LU"
int PCCreate_LU(PC pc)
{
  int   ierr;
  PC_LU *dir     = PetscNew(PC_LU); CHKPTRQ(dir);

  PetscFunctionBegin;
  PLogObjectMemory(pc,sizeof(PC_LU));

  dir->fact             = 0;
  dir->inplace          = 0;
  dir->fill             = 5.0;
  dir->col              = 0;
  dir->row              = 0;
  dir->ordering         = ORDER_ND;
  dir->reusefill        = 0;
  dir->reusereordering  = 0;
  pc->destroy           = PCDestroy_LU;
  pc->apply             = PCApply_LU;
  pc->applytrans        = PCApplyTrans_LU;
  pc->setup             = PCSetUp_LU;
  pc->data              = (void *) dir;
  pc->setfromoptions    = PCSetFromOptions_LU;
  pc->printhelp         = PCPrintHelp_LU;
  pc->view              = PCView_LU;
  pc->applyrich         = 0;
  pc->getfactoredmatrix = PCGetFactoredMatrix_LU;

  ierr = PetscObjectComposeFunction((PetscObject)pc,"PCLUSetFill_C","PCLUSetFill_LU",
                    (void*)PCLUSetFill_LU);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)pc,"PCLUSetUseInPlace_C","PCLUSetUseInPlace_LU",
                    (void*)PCLUSetUseInPlace_LU);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)pc,"PCLUSetMatReordering_C","PCLUSetMatReordering_LU",
                    (void*)PCLUSetMatReordering_LU);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)pc,"PCLUSetReuseReordering_C","PCLUSetReuseReordering_LU",
                    (void*)PCLUSetReuseReordering_LU);CHKERRQ(ierr);
  ierr = PetscObjectComposeFunction((PetscObject)pc,"PCLUSetReuseFill_C","PCLUSetReuseFill_LU",
                    (void*)PCLUSetReuseFill_LU);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
EXTERN_C_END
