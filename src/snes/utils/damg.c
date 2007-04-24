#define PETSCSNES_DLL
 
#include "petscda.h"            /*I "petscda.h"   I*/
#include "petscksp.h"           /*I "petscksp.h"  I*/
#include "petscmg.h"            /*I "petscmg.h"   I*/
#include "petscdmmg.h"          /*I "petscdmmg.h" I*/
#include "private/pcimpl.h"     /*I "petscpc.h"   I*/

/*
   Code for almost fully managing multigrid/multi-level linear solvers for DA grids
*/

#undef __FUNCT__  
#define __FUNCT__ "DMMGCreate"
/*@C
    DMMGCreate - Creates a DA based multigrid solver object. This allows one to 
      easily implement MG methods on regular grids.

    Collective on MPI_Comm

    Input Parameter:
+   comm - the processors that will share the grids and solution process
.   nlevels - number of multigrid levels 
-   user - an optional user context

    Output Parameters:
.    - the context

    Options Database:
+     -dmmg_nlevels <levels> - number of levels to use
.     -dmmg_galerkin - use Galerkin approach to compute coarser matrices
-     -dmmg_mat_type <type> - matrix type that DMMG should create, defaults to MATAIJ

    Notes:
      To provide a different user context for each level call DMMGSetUser() after calling
      this routine

    Level: advanced

.seealso DMMGDestroy(), DMMGSetUser(), DMMGGetUser(), DMMGSetMatType(), DMMGSetUseGalerkin(), DMMGSetNullSpace(), DMMGSetInitialGuess(),
         DMMGSetISColoringType()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGCreate(MPI_Comm comm,PetscInt nlevels,void *user,DMMG **dmmg)
{
  PetscErrorCode ierr;
  PetscInt       i;
  DMMG           *p;
  PetscTruth     galerkin,ftype;
  char           mtype[256];

  PetscFunctionBegin;
  ierr = PetscOptionsGetInt(0,"-dmmg_nlevels",&nlevels,PETSC_IGNORE);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(0,"-dmmg_galerkin",&galerkin);CHKERRQ(ierr);

  ierr = PetscMalloc(nlevels*sizeof(DMMG),&p);CHKERRQ(ierr);
  for (i=0; i<nlevels; i++) {
    ierr           = PetscNew(struct _n_DMMG,&p[i]);CHKERRQ(ierr);
    p[i]->nlevels  = nlevels - i;
    p[i]->comm     = comm;
    p[i]->user     = user;
    p[i]->galerkin = galerkin;
    p[i]->mtype    = MATAIJ;
    p[i]->isctype  = IS_COLORING_GHOSTED;   /* default to faster version, requires DMMGSetSNESLocal() */
  }
  p[nlevels-1]->galerkin = PETSC_FALSE;
  *dmmg = p;

  ierr = PetscOptionsGetString(PETSC_NULL,"-dmmg_mat_type",mtype,256,&ftype);CHKERRQ(ierr);
  if (ftype) {
    ierr = DMMGSetMatType(*dmmg,mtype);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetMatType"
/*@C
    DMMGSetMatType - Sets the type of matrices that DMMG will create for its solvers.

    Collective on MPI_Comm 

    Input Parameters:
+    dmmg - the DMMG object created with DMMGCreate()
-    mtype - the matrix type, defaults to MATAIJ

    Level: intermediate

.seealso DMMGDestroy(), DMMGSetUser(), DMMGGetUser(), DMMGCreate(), DMMGSetNullSpace()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetMatType(DMMG *dmmg,MatType mtype)
{
  PetscInt i;
  
  PetscFunctionBegin;
  for (i=0; i<dmmg[0]->nlevels; i++) {
    dmmg[i]->mtype  = mtype;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetPrefix"
/*@C
    DMMGSetPrefix - Sets the prefix used for the solvers inside a DMMG

    Collective on MPI_Comm 

    Input Parameters:
+    dmmg - the DMMG object created with DMMGCreate()
-    prefix - the prefix string

    Level: intermediate

.seealso DMMGDestroy(), DMMGSetUser(), DMMGGetUser(), DMMGCreate(), DMMGSetNullSpace()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetPrefix(DMMG *dmmg,const char* prefix)
{
  PetscInt       i;
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  for (i=0; i<dmmg[0]->nlevels; i++) {
    ierr = PetscStrallocpy(prefix,&dmmg[i]->prefix);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetUseGalerkinCoarse"
/*@C
    DMMGSetUseGalerkinCoarse - Courses the DMMG to use R*A_f*R^T to form
       the coarser matrices from finest 

    Collective on DMMG

    Input Parameter:
.    dmmg - the context

    Options Database Keys:
.    -dmmg_galerkin 

    Level: advanced

    Notes: After you have called this you can manually set dmmg[0]->galerkin = PETSC_FALSE
       to have the coarsest grid not compute via Galerkin but still have the intermediate
       grids computed via Galerkin.

       The default behavior of this should be idential to using -pc_mg_galerkin; this offers
       more potential flexibility since you can select exactly which levels are done via
       Galerkin and which are done via user provided function.

.seealso DMMGCreate(), PCMGSetUseGalerkin(), DMMGSetMatType(), DMMGSetNullSpace()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetUseGalerkinCoarse(DMMG* dmmg)
{
  PetscInt  i,nlevels = dmmg[0]->nlevels;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(PETSC_ERR_ARG_NULL,"Passing null as DMMG");

  for (i=0; i<nlevels-1; i++) {
    dmmg[i]->galerkin = PETSC_TRUE;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGDestroy"
/*@C
    DMMGDestroy - Destroys a DA based multigrid solver object. 

    Collective on DMMG

    Input Parameter:
.    - the context

    Level: advanced

.seealso DMMGCreate()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGDestroy(DMMG *dmmg)
{
  PetscErrorCode ierr;
  PetscInt       i,nlevels = dmmg[0]->nlevels;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(PETSC_ERR_ARG_NULL,"Passing null as DMMG");

  for (i=1; i<nlevels; i++) {
    if (dmmg[i]->R) {ierr = MatDestroy(dmmg[i]->R);CHKERRQ(ierr);}
  }
  for (i=0; i<nlevels; i++) {
    ierr = PetscStrfree(dmmg[i]->prefix);CHKERRQ(ierr);
    if (dmmg[i]->dm)      {ierr = DMDestroy(dmmg[i]->dm);CHKERRQ(ierr);}
    if (dmmg[i]->x)       {ierr = VecDestroy(dmmg[i]->x);CHKERRQ(ierr);}
    if (dmmg[i]->b)       {ierr = VecDestroy(dmmg[i]->b);CHKERRQ(ierr);}
    if (dmmg[i]->r)       {ierr = VecDestroy(dmmg[i]->r);CHKERRQ(ierr);}
    if (dmmg[i]->work1)   {ierr = VecDestroy(dmmg[i]->work1);CHKERRQ(ierr);}
    if (dmmg[i]->w)       {ierr = VecDestroy(dmmg[i]->w);CHKERRQ(ierr);}
    if (dmmg[i]->work2)   {ierr = VecDestroy(dmmg[i]->work2);CHKERRQ(ierr);}
    if (dmmg[i]->lwork1)  {ierr = VecDestroy(dmmg[i]->lwork1);CHKERRQ(ierr);}
    if (dmmg[i]->B && dmmg[i]->B != dmmg[i]->J) {ierr = MatDestroy(dmmg[i]->B);CHKERRQ(ierr);}
    if (dmmg[i]->J)         {ierr = MatDestroy(dmmg[i]->J);CHKERRQ(ierr);}
    if (dmmg[i]->Rscale)    {ierr = VecDestroy(dmmg[i]->Rscale);CHKERRQ(ierr);}
    if (dmmg[i]->fdcoloring){ierr = MatFDColoringDestroy(dmmg[i]->fdcoloring);CHKERRQ(ierr);}
    if (dmmg[i]->ksp && !dmmg[i]->snes) {ierr = KSPDestroy(dmmg[i]->ksp);CHKERRQ(ierr);}
    if (dmmg[i]->snes)      {ierr = PetscObjectDestroy((PetscObject)dmmg[i]->snes);CHKERRQ(ierr);} 
    if (dmmg[i]->inject)    {ierr = VecScatterDestroy(dmmg[i]->inject);CHKERRQ(ierr);} 
    ierr = PetscFree(dmmg[i]);CHKERRQ(ierr);
  }
  ierr = PetscFree(dmmg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetDM"
/*@C
    DMMGSetDM - Sets the coarse grid information for the grids

    Collective on DMMG

    Input Parameter:
+   dmmg - the context
-   dm - the DA or DMComposite object

    Options Database Keys:
+   -dmmg_refine: Use the input problem as the coarse level and refine. Otherwise, use it as the fine level and coarsen.
-   -dmmg_hierarchy: Construct all grids at once

    Level: advanced

.seealso DMMGCreate(), DMMGDestroy(), DMMGSetUseGalerkin(), DMMGSetMatType()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetDM(DMMG *dmmg, DM dm)
{
  PetscInt       nlevels     = dmmg[0]->nlevels;
  PetscTruth     doRefine    = PETSC_TRUE;
  PetscTruth     doHierarchy = PETSC_FALSE;
  PetscInt       i;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(PETSC_ERR_ARG_NULL,"Passing null as DMMG");

  /* Create DM data structure for all the levels */
  ierr = PetscOptionsGetTruth(PETSC_NULL, "-dmmg_refine", &doRefine, PETSC_IGNORE);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL, "-dmmg_hierarchy", &doHierarchy);CHKERRQ(ierr);
  ierr = PetscObjectReference((PetscObject) dm);CHKERRQ(ierr);
  if (doRefine) {
    dmmg[0]->dm = dm;
    if (doHierarchy) {
/*       DM *hierarchy; */

/*       ierr = DMRefineHierarchy(dm, nlevels-1, &hierarchy);CHKERRQ(ierr); */
/*       for(i = 1; i < nlevels; ++i) { */
/*         dmmg[i]->dm = hierarchy[i-1]; */
/*       } */
      SETERRQ(PETSC_ERR_SUP, "Refinement hierarchy not yet implemented");
    } else {
      for(i = 1; i < nlevels; ++i) {
        ierr = DMRefine(dmmg[i-1]->dm, dmmg[i]->comm, &dmmg[i]->dm);CHKERRQ(ierr);
      }
    }
  } else {
    dmmg[nlevels-1]->dm = dm;
    if (doHierarchy) {
      DM *hierarchy;

      ierr = DMCoarsenHierarchy(dm, nlevels-1, &hierarchy);CHKERRQ(ierr);
      for(i = 0; i < nlevels-1; ++i) {
        dmmg[nlevels-2-i]->dm = hierarchy[i];
      }
    } else {
/*       for(i = nlevels-2; i >= 0; --i) { */
/*         ierr = DMCoarsen(dmmg[i+1]->dm, dmmg[i]->comm, &dmmg[i]->dm);CHKERRQ(ierr); */
/*       } */
      SETERRQ(PETSC_ERR_SUP, "Sequential coarsening not yet implemented");
    }
  }
  ierr = DMMGSetUp(dmmg);CHKERRQ(ierr); 
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetUp"
/*@C
    DMMGSetUp - Prepares the DMMG to solve a system

    Collective on DMMG

    Input Parameter:
.   dmmg - the context

    Level: advanced

.seealso DMMGCreate(), DMMGDestroy(), DMMG, DMMGSetSNES(), DMMGSetKSP(), DMMGSolve(), DMMGSetMatType()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetUp(DMMG *dmmg)
{
  PetscErrorCode ierr;
  PetscInt       i,nlevels = dmmg[0]->nlevels;

  PetscFunctionBegin;

  /* Create work vectors and matrix for each level */
  for (i=0; i<nlevels; i++) {
    ierr = DMCreateGlobalVector(dmmg[i]->dm,&dmmg[i]->x);CHKERRQ(ierr);
    ierr = VecDuplicate(dmmg[i]->x,&dmmg[i]->b);CHKERRQ(ierr);
    ierr = VecDuplicate(dmmg[i]->x,&dmmg[i]->r);CHKERRQ(ierr);
  }

  /* Create interpolation/restriction between levels */
  for (i=1; i<nlevels; i++) {
    ierr = DMGetInterpolation(dmmg[i-1]->dm,dmmg[i]->dm,&dmmg[i]->R,PETSC_NULL);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSolve"
/*@C
    DMMGSolve - Actually solves the (non)linear system defined with the DMMG

    Collective on DMMG

    Input Parameter:
.   dmmg - the context

    Level: advanced

    Options Database:
+   -dmmg_grid_sequence - use grid sequencing to get the initial solution for each level from the previous
-   -dmmg_monitor_solution - display the solution at each iteration

     Notes: For linear (KSP) problems may be called more than once, uses the same 
    matrices but recomputes the right hand side for each new solve. Call DMMGSetKSP()
    to generate new matrices.
 
.seealso DMMGCreate(), DMMGDestroy(), DMMG, DMMGSetSNES(), DMMGSetKSP(), DMMGSetUp(), DMMGSetMatType()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSolve(DMMG *dmmg)
{
  PetscErrorCode ierr;
  PetscInt       i,nlevels = dmmg[0]->nlevels;
  PetscTruth     gridseq,vecmonitor,flg;

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(0,"-dmmg_grid_sequence",&gridseq);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(0,"-dmmg_monitor_solution",&vecmonitor);CHKERRQ(ierr);
  if (gridseq) {
    if (dmmg[0]->initialguess) {
      ierr = (*dmmg[0]->initialguess)(dmmg[0],dmmg[0]->x);CHKERRQ(ierr);
      if (dmmg[0]->ksp && !dmmg[0]->snes) {
        ierr = KSPSetInitialGuessNonzero(dmmg[0]->ksp,PETSC_TRUE);CHKERRQ(ierr);
      }
    }
    for (i=0; i<nlevels-1; i++) {
      ierr = (*dmmg[i]->solve)(dmmg,i);CHKERRQ(ierr);
      if (vecmonitor) {
        ierr = VecView(dmmg[i]->x,PETSC_VIEWER_DRAW_(dmmg[i]->comm));CHKERRQ(ierr);
      }
      ierr = MatInterpolate(dmmg[i+1]->R,dmmg[i]->x,dmmg[i+1]->x);CHKERRQ(ierr);
      if (dmmg[i+1]->ksp && !dmmg[i+1]->snes) {
        ierr = KSPSetInitialGuessNonzero(dmmg[i+1]->ksp,PETSC_TRUE);CHKERRQ(ierr);
      }
    }
  } else {
    if (dmmg[nlevels-1]->initialguess) {
      ierr = (*dmmg[nlevels-1]->initialguess)(dmmg[nlevels-1],dmmg[nlevels-1]->x);CHKERRQ(ierr);
    }
  }

  /*ierr = VecView(dmmg[nlevels-1]->x,PETSC_VIEWER_DRAW_WORLD);CHKERRQ(ierr);*/

  ierr = (*DMMGGetFine(dmmg)->solve)(dmmg,nlevels-1);CHKERRQ(ierr);
  if (vecmonitor) {
     ierr = VecView(dmmg[nlevels-1]->x,PETSC_VIEWER_DRAW_(dmmg[nlevels-1]->comm));CHKERRQ(ierr);
  }

  ierr = PetscOptionsHasName(PETSC_NULL,"-dmmg_view",&flg);CHKERRQ(ierr);
  if (flg && !PetscPreLoadingOn) {
    PetscViewer viewer;
    ierr = PetscViewerASCIIGetStdout(dmmg[0]->comm,&viewer);CHKERRQ(ierr);
    ierr = DMMGView(dmmg,viewer);CHKERRQ(ierr);
  }
  ierr = PetscOptionsHasName(PETSC_NULL,"-dmmg_view_binary",&flg);CHKERRQ(ierr);
  if (flg && !PetscPreLoadingOn) {
    ierr = DMMGView(dmmg,PETSC_VIEWER_BINARY_(dmmg[0]->comm));CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSolveKSP"
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSolveKSP(DMMG *dmmg,PetscInt level)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  if (dmmg[level]->rhs) {
    CHKMEMQ;
    ierr = (*dmmg[level]->rhs)(dmmg[level],dmmg[level]->b);CHKERRQ(ierr); 
    CHKMEMQ;
  }
  ierr = KSPSolve(dmmg[level]->ksp,dmmg[level]->b,dmmg[level]->x);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
    For each level (of grid sequencing) this sets the interpolation/restriction and 
    work vectors needed by the multigrid preconditioner within the KSP 
    (for nonlinear problems the KSP inside the SNES) of that level.

    Also sets the KSP monitoring on all the levels if requested by user.

*/
#undef __FUNCT__  
#define __FUNCT__ "DMMGSetUpLevel"
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetUpLevel(DMMG *dmmg,KSP ksp,PetscInt nlevels)
{
  PetscErrorCode          ierr;
  PetscInt                i;
  PC                      pc;
  PetscTruth              ismg,monitor,monitor_short,ismf,isshell,ismffd;
  KSP                     lksp; /* solver internal to the multigrid preconditioner */
  MPI_Comm                *comms,comm;
  PetscViewerASCIIMonitor ascii;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(PETSC_ERR_ARG_NULL,"Passing null as DMMG");

  ierr = PetscOptionsHasName(PETSC_NULL,"-dmmg_ksp_monitor",&monitor);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-dmmg_ksp_monitor_short",&monitor_short);CHKERRQ(ierr);
  if (monitor || monitor_short) {
    ierr = PetscObjectGetComm((PetscObject)ksp,&comm);CHKERRQ(ierr);
    ierr = PetscViewerASCIIMonitorCreate(comm,"stdout",1+dmmg[0]->nlevels-nlevels,&ascii);CHKERRQ(ierr);
    if (monitor) {
      ierr = KSPMonitorSet(ksp,KSPMonitorDefault,ascii,(PetscErrorCode(*)(void*))PetscViewerASCIIMonitorDestroy);CHKERRQ(ierr);
    } else {
      ierr = KSPMonitorSet(ksp,KSPMonitorDefaultShort,ascii,(PetscErrorCode(*)(void*))PetscViewerASCIIMonitorDestroy);CHKERRQ(ierr);
    }
  }

  /* use fgmres on outer iteration by default */
  ierr  = KSPSetType(ksp,KSPFGMRES);CHKERRQ(ierr);
  ierr  = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
  ierr  = PCSetType(pc,PCMG);CHKERRQ(ierr);
  ierr  = PetscMalloc(nlevels*sizeof(MPI_Comm),&comms);CHKERRQ(ierr);
  for (i=0; i<nlevels; i++) {
    comms[i] = dmmg[i]->comm;
  }
  ierr  = PCMGSetLevels(pc,nlevels,comms);CHKERRQ(ierr);
  ierr  = PetscFree(comms);CHKERRQ(ierr); 
  ierr =  PCMGSetType(pc,PC_MG_FULL);CHKERRQ(ierr);

  ierr = PetscTypeCompare((PetscObject)pc,PCMG,&ismg);CHKERRQ(ierr);
  if (ismg) {
    /* set solvers for each level */
    for (i=0; i<nlevels; i++) {
      if (i < nlevels-1) { /* don't set for finest level, they are set in PCApply_MG()*/
	ierr = PCMGSetX(pc,i,dmmg[i]->x);CHKERRQ(ierr); 
	ierr = PCMGSetRhs(pc,i,dmmg[i]->b);CHKERRQ(ierr); 
      }
      if (i > 0) {
        ierr = PCMGSetR(pc,i,dmmg[i]->r);CHKERRQ(ierr); 
      }
      if (monitor || monitor_short) {
        ierr = PCMGGetSmoother(pc,i,&lksp);CHKERRQ(ierr); 
        ierr = PetscObjectGetComm((PetscObject)lksp,&comm);CHKERRQ(ierr);
        ierr = PetscViewerASCIIMonitorCreate(comm,"stdout",1+dmmg[0]->nlevels-i,&ascii);CHKERRQ(ierr);
	if (monitor) {
	  ierr = KSPMonitorSet(lksp,KSPMonitorDefault,ascii,(PetscErrorCode(*)(void*))PetscViewerASCIIMonitorDestroy);CHKERRQ(ierr);
	} else {
	  ierr = KSPMonitorSet(lksp,KSPMonitorDefaultShort,ascii,(PetscErrorCode(*)(void*))PetscViewerASCIIMonitorDestroy);CHKERRQ(ierr);
	}
      }
      /* If using a matrix free multiply and did not provide an explicit matrix to build
         the preconditioner then must use no preconditioner 
      */
      ierr = PetscTypeCompare((PetscObject)dmmg[i]->B,MATSHELL,&isshell);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)dmmg[i]->B,MATDAAD,&ismf);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)dmmg[i]->B,MATMFFD,&ismffd);CHKERRQ(ierr);
      if (isshell || ismf || ismffd) {
        PC  lpc;
        ierr = PCMGGetSmoother(pc,i,&lksp);CHKERRQ(ierr); 
        ierr = KSPGetPC(lksp,&lpc);CHKERRQ(ierr);
        ierr = PCSetType(lpc,PCNONE);CHKERRQ(ierr);
      }
    }

    /* Set interpolation/restriction between levels */
    for (i=1; i<nlevels; i++) {
      ierr = PCMGSetInterpolation(pc,i,dmmg[i]->R);CHKERRQ(ierr); 
      ierr = PCMGSetRestriction(pc,i,dmmg[i]->R);CHKERRQ(ierr); 
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetKSP"
/*@C
    DMMGSetKSP - Sets the linear solver object that will use the grid hierarchy

    Collective on DMMG

    Input Parameter:
+   dmmg - the context
.   func - function to compute linear system matrix on each grid level
-   rhs - function to compute right hand side on each level (need only work on the finest grid
          if you do not use grid sequencing)

    Level: advanced

    Notes: For linear problems my be called more than once, reevaluates the matrices if it is called more
       than once. Call DMMGSolve() directly several times to solve with the same matrix but different 
       right hand sides.
   
.seealso DMMGCreate(), DMMGDestroy, DMMGSetDM(), DMMGSolve(), DMMGSetMatType()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetKSP(DMMG *dmmg,PetscErrorCode (*rhs)(DMMG,Vec),PetscErrorCode (*func)(DMMG,Mat,Mat))
{
  PetscErrorCode ierr;
  PetscInt       i,nlevels = dmmg[0]->nlevels,level;
  PetscTruth     galerkin,ismg;
  PC             pc;
  KSP            lksp;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(PETSC_ERR_ARG_NULL,"Passing null as DMMG");
  galerkin = dmmg[nlevels - 2 > 0 ? nlevels - 2 : 0]->galerkin;  

  if (galerkin) {
    ierr = DMGetMatrix(dmmg[nlevels-1]->dm,dmmg[nlevels-1]->mtype,&dmmg[nlevels-1]->B);CHKERRQ(ierr);
    if (!dmmg[nlevels-1]->J) {
      dmmg[nlevels-1]->J = dmmg[nlevels-1]->B;
    }
    if (func) {
      ierr = (*func)(dmmg[nlevels-1],dmmg[nlevels-1]->J,dmmg[nlevels-1]->B);CHKERRQ(ierr);
    }
    for (i=nlevels-2; i>-1; i--) {
      if (dmmg[i]->galerkin) {
        ierr = MatPtAP(dmmg[i+1]->B,dmmg[i+1]->R,MAT_INITIAL_MATRIX,1.0,&dmmg[i]->B);CHKERRQ(ierr);
        if (!dmmg[i]->J) {
          dmmg[i]->J = dmmg[i]->B;
        }
      }
    }
  }

  if (!dmmg[0]->ksp) {
    /* create solvers for each level if they don't already exist*/
    for (i=0; i<nlevels; i++) {

      if (!dmmg[i]->B && !dmmg[i]->galerkin) {
        ierr = DMGetMatrix(dmmg[i]->dm,dmmg[nlevels-1]->mtype,&dmmg[i]->B);CHKERRQ(ierr);
      } 
      if (!dmmg[i]->J) {
        dmmg[i]->J = dmmg[i]->B;
      }

      ierr = KSPCreate(dmmg[i]->comm,&dmmg[i]->ksp);CHKERRQ(ierr);
      ierr = KSPSetOptionsPrefix(dmmg[i]->ksp,dmmg[i]->prefix);CHKERRQ(ierr);
      ierr = DMMGSetUpLevel(dmmg,dmmg[i]->ksp,i+1);CHKERRQ(ierr);
      ierr = KSPSetFromOptions(dmmg[i]->ksp);CHKERRQ(ierr);
      dmmg[i]->solve = DMMGSolveKSP;
      dmmg[i]->rhs   = rhs;
    }
  }

  /* evalute matrix on each level */
  for (i=0; i<nlevels; i++) {
    if (!dmmg[i]->galerkin) {
      if (func) {
	ierr = (*func)(dmmg[i],dmmg[i]->J,dmmg[i]->B);CHKERRQ(ierr);
      }
    }
  }

  for (i=0; i<nlevels-1; i++) {
    ierr = KSPSetOptionsPrefix(dmmg[i]->ksp,"dmmg_");CHKERRQ(ierr);
  }

  for (level=0; level<nlevels; level++) {
    ierr = KSPSetOperators(dmmg[level]->ksp,dmmg[level]->J,dmmg[level]->B,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
    ierr = KSPGetPC(dmmg[level]->ksp,&pc);CHKERRQ(ierr);
    ierr = PetscTypeCompare((PetscObject)pc,PCMG,&ismg);CHKERRQ(ierr);
    if (ismg) {
      for (i=0; i<=level; i++) {
	ierr = PCMGGetSmoother(pc,i,&lksp);CHKERRQ(ierr); 
	ierr = KSPSetOperators(lksp,dmmg[i]->J,dmmg[i]->B,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
      }
    }
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGView"
/*@C
    DMMGView - prints information on a DA based multi-level preconditioner

    Collective on DMMG and PetscViewer

    Input Parameter:
+   dmmg - the context
-   viewer - the viewer

    Level: advanced

.seealso DMMGCreate(), DMMGDestroy(), DMMGSetMatType(), DMMGSetUseGalerkin()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGView(DMMG *dmmg,PetscViewer viewer)
{
  PetscErrorCode ierr;
  PetscInt       i,nlevels = dmmg[0]->nlevels;
  PetscMPIInt    flag;
  MPI_Comm       comm;
  PetscTruth     iascii,isbinary;

  PetscFunctionBegin;
  PetscValidPointer(dmmg,1);
  PetscValidHeaderSpecific(viewer,PETSC_VIEWER_COOKIE,2);
  ierr = PetscObjectGetComm((PetscObject)viewer,&comm);CHKERRQ(ierr);
  ierr = MPI_Comm_compare(comm,dmmg[0]->comm,&flag);CHKERRQ(ierr);
  if (flag != MPI_CONGRUENT && flag != MPI_IDENT) {
    SETERRQ(PETSC_ERR_ARG_NOTSAMECOMM,"Different communicators in the DMMG and the PetscViewer");
  }

  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_ASCII,&iascii);CHKERRQ(ierr);
  ierr = PetscTypeCompare((PetscObject)viewer,PETSC_VIEWER_BINARY,&isbinary);CHKERRQ(ierr);
  if (isbinary) {
    for (i=0; i<nlevels; i++) {
      ierr = MatView(dmmg[i]->J,viewer);CHKERRQ(ierr);
    }
    for (i=1; i<nlevels; i++) {
      ierr = MatView(dmmg[i]->R,viewer);CHKERRQ(ierr);
    }
  } else {
    if (iascii) {
      ierr = PetscViewerASCIIPrintf(viewer,"DMMG Object with %D levels\n",nlevels);CHKERRQ(ierr);
    }
    for (i=0; i<nlevels; i++) {
      ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
      ierr = DMView(dmmg[i]->dm,viewer);CHKERRQ(ierr);
      ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
    }
    if (iascii) {
      ierr = PetscViewerASCIIPrintf(viewer,"%s Object on finest level\n",dmmg[nlevels-1]->ksp ? "KSP" : "SNES");CHKERRQ(ierr);
      if (dmmg[nlevels-2 > 0 ? nlevels-2 : 0]->galerkin) {
	ierr = PetscViewerASCIIPrintf(viewer,"Using Galerkin R^T*A*R process to compute coarser matrices\n");CHKERRQ(ierr);
      }
      ierr = PetscViewerASCIIPrintf(viewer,"Using matrix type %s\n",dmmg[nlevels-1]->mtype);CHKERRQ(ierr);
    }
    if (dmmg[nlevels-1]->ksp) {
      ierr = KSPView(dmmg[nlevels-1]->ksp,viewer);CHKERRQ(ierr);
    } else {
      /* use of PetscObjectView() means we do not have to link with libpetscsnes if SNES is not being used */
      ierr = PetscObjectView((PetscObject)dmmg[nlevels-1]->snes,viewer);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetNullSpace"
/*@C
    DMMGSetNullSpace - Indicates the null space in the linear operator (this is needed by the linear solver)

    Collective on DMMG

    Input Parameter:
+   dmmg - the context
.   has_cnst - is the constant vector in the null space
.   n - number of null vectors (excluding the possible constant vector)
-   func - a function that fills an array of vectors with the null vectors (must be orthonormal), may be PETSC_NULL

    Level: advanced

.seealso DMMGCreate(), DMMGDestroy, DMMGSetDM(), DMMGSolve(), MatNullSpaceCreate(), KSPSetNullSpace(), DMMGSetUseGalerkin(), DMMGSetMatType()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetNullSpace(DMMG *dmmg,PetscTruth has_cnst,PetscInt n,PetscErrorCode (*func)(DMMG,Vec[]))
{
  PetscErrorCode ierr;
  PetscInt       i,j,nlevels = dmmg[0]->nlevels;
  Vec            *nulls = 0;
  MatNullSpace   nullsp;
  KSP            iksp;
  PC             pc,ipc;
  PetscTruth     ismg,isred;

  PetscFunctionBegin;
  if (!dmmg) SETERRQ(PETSC_ERR_ARG_NULL,"Passing null as DMMG");
  if (!dmmg[0]->ksp) SETERRQ(PETSC_ERR_ORDER,"Must call AFTER DMMGSetKSP() or DMMGSetSNES()");
  if ((n && !func) || (!n && func)) SETERRQ(PETSC_ERR_ARG_INCOMP,"Both n and func() must be set together");
  if (n < 0) SETERRQ1(PETSC_ERR_ARG_OUTOFRANGE,"Cannot have negative number of vectors in null space n = %D",n)

  for (i=0; i<nlevels; i++) {
    if (n) {
      ierr = VecDuplicateVecs(dmmg[i]->b,n,&nulls);CHKERRQ(ierr);
      ierr = (*func)(dmmg[i],nulls);CHKERRQ(ierr);
    }
    ierr = MatNullSpaceCreate(dmmg[i]->comm,has_cnst,n,nulls,&nullsp);CHKERRQ(ierr);
    ierr = KSPSetNullSpace(dmmg[i]->ksp,nullsp);CHKERRQ(ierr);
    for (j=i; j<nlevels; j++) {
      ierr = KSPGetPC(dmmg[j]->ksp,&pc);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)pc,PCMG,&ismg);CHKERRQ(ierr);
      if (ismg) {
        ierr = PCMGGetSmoother(pc,i,&iksp);CHKERRQ(ierr);
        ierr = KSPSetNullSpace(iksp, nullsp);CHKERRQ(ierr);
      }
    }
    ierr = MatNullSpaceDestroy(nullsp);CHKERRQ(ierr);
    if (n) {
      ierr = PetscFree(nulls);CHKERRQ(ierr);
    }
  }
  /* make all the coarse grid solvers have LU shift since they are singular */
  for (i=0; i<nlevels; i++) {
    ierr = KSPGetPC(dmmg[i]->ksp,&pc);CHKERRQ(ierr);
    ierr = PetscTypeCompare((PetscObject)pc,PCMG,&ismg);CHKERRQ(ierr);
    if (ismg) {
      ierr = PCMGGetSmoother(pc,0,&iksp);CHKERRQ(ierr);
      ierr = KSPGetPC(iksp,&ipc);CHKERRQ(ierr);
      ierr = PetscTypeCompare((PetscObject)ipc,PCREDUNDANT,&isred);CHKERRQ(ierr);
      if (isred) {
        ierr = PCRedundantGetPC(ipc,&ipc);CHKERRQ(ierr);
      }
      ierr = PCFactorSetShiftPd(ipc,PETSC_TRUE);CHKERRQ(ierr); 
    }
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGInitialGuessCurrent"
/*@C
    DMMGInitialGuessCurrent - Use with DMMGSetInitialGuess() to use the current value in the 
       solution vector (obtainable with DMMGGetx()) as the initial guess. Otherwise for linear
       problems zero is used for the initial guess (unless grid sequencing is used). For nonlinear 
       problems this is not needed; it always uses the previous solution as the initial guess.

    Collective on DMMG

    Input Parameter:
+   dmmg - the context
-   vec - dummy argument

    Level: intermediate

.seealso DMMGCreate(), DMMGDestroy, DMMGSetKSP(), DMMGSetSNES(), DMMGSetInitialGuess()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGInitialGuessCurrent(DMMG dmmg,Vec vec)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "DMMGSetInitialGuess"
/*@C
    DMMGSetInitialGuess - Sets the function that computes an initial guess.

    Collective on DMMG

    Input Parameter:
+   dmmg - the context
-   guess - the function

    Notes: For nonlinear problems, if this is not set, then the current value in the 
             solution vector (obtained with DMMGGetX()) is used. Thus is if you doing 'time
             stepping' it will use your current solution as the guess for the next timestep.
           If grid sequencing is used (via -dmmg_grid_sequence) then the "guess" function
             is used only on the coarsest grid.
           For linear problems, if this is not set, then 0 is used as an initial guess.
             If you would like the linear solver to also (like the nonlinear solver) use
             the current solution vector as the initial guess then use DMMGInitialGuessCurrent()
             as the function you pass in

    Level: intermediate


.seealso DMMGCreate(), DMMGDestroy, DMMGSetKSP(), DMMGSetSNES(), DMMGInitialGuessCurrent(), DMMGSetGalekin(), DMMGSetMatType(), DMMGSetNullSpace()

@*/
PetscErrorCode PETSCSNES_DLLEXPORT DMMGSetInitialGuess(DMMG *dmmg,PetscErrorCode (*guess)(DMMG,Vec))
{
  PetscInt i,nlevels = dmmg[0]->nlevels;

  PetscFunctionBegin;
  for (i=0; i<nlevels; i++) {
    dmmg[i]->initialguess = guess;
  }
  PetscFunctionReturn(0);
}







