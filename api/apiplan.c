/*
 * Copyright (c) 2003 Matteo Frigo
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "api.h"

double X(timelimit) = 1e30;

static plan *mkplan(planner *plnr, unsigned flags, problem *prb, int bless)
WITH_ALIGNED_STACK({
     plan *pln;
     double timelimit = plnr->timelimit;
     
     /* never time out of estimator, since it is required as a fallback */
     if (flags & FFTW_ESTIMATE) plnr->timelimit = X(seconds)() + 1e30;
     
     /* map API flags into FFTW flags */
     X(mapflags)(plnr, flags);

     if (bless)
	  plnr->flags.hash_info = BLESSING;
     else
	  plnr->flags.hash_info = 0;

     /* create plan */
     pln = plnr->adt->mkplan(plnr, prb);

     plnr->timelimit = timelimit;

     return pln;
})

apiplan *X(mkapiplan)(int sign, unsigned flags, problem *prb)
{
     apiplan *p = 0;
     plan *pln;
     unsigned flags_used_for_planning;
     planner *plnr = X(the_planner)();
     unsigned int pats[] = {FFTW_ESTIMATE, FFTW_MEASURE,
			    FFTW_PATIENT, FFTW_EXHAUSTIVE};
     int pat, pat_max;

     pat_max = flags & FFTW_ESTIMATE ? 0 :
	  (flags & FFTW_EXHAUSTIVE ? 3 :
	   (flags & FFTW_PATIENT ? 2 : 1));
     pat = flags & FFTW_TIMELIMIT ? 0 : pat_max;

     flags &= ~(FFTW_ESTIMATE | FFTW_MEASURE | FFTW_PATIENT | FFTW_EXHAUSTIVE);

     plnr->timelimit = X(seconds)() + ((flags & FFTW_TIMELIMIT)
				       ? X(timelimit) : 1e30);
	  
     /* plan at incrementally increasing patience until we run out of time */
     for (pln = 0, flags_used_for_planning = 0; pat <= pat_max; ++pat) {
	  plan *pln1;
	  unsigned tmpflags = flags | pats[pat];
	  pln1 = mkplan(plnr, tmpflags, prb, 0);

	  if (!pln1) {
	       /* don't bother continuing if planner failed or timed out */
	       A(!pln || plnr->timed_out);
	       break;
	  }

	  X(plan_destroy_internal)(pln);
	  pln = pln1;
	  flags_used_for_planning = tmpflags;
     }

     if (pln) {
	  /* build apiplan */
	  p = (apiplan *) MALLOC(sizeof(apiplan), PLANS);
	  p->prb = prb;
	  p->sign = sign; /* cache for execute_dft */
	  
	  /* re-create plan from wisdom, adding blessing */
	  p->pln = mkplan(plnr, flags_used_for_planning, prb, 1);
	  AWAKE(p->pln, 1);
	  
	  /* we don't use pln for p->pln, above, since by re-creating the
	     plan we might use more patient wisdom from a timed-out mkplan */
	  X(plan_destroy_internal)(pln);
     } else
	  X(problem_destroy)(prb);
     
     /* discard all information not necessary to reconstruct the plan */
     plnr->adt->forget(plnr, FORGET_ACCURSED);
     
     return p;
}

void X(destroy_plan)(X(plan) p)
{
     if (p) {
          AWAKE(p->pln, 0);
          X(plan_destroy_internal)(p->pln);
          X(problem_destroy)(p->prb);
          X(ifree)(p);
     }
}
