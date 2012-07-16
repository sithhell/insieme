/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#pragma once
#include "irt_scheduling.h"
#include "utils/timing.h"
#include "impl/instrumentation.impl.h"

#if IRT_SCHED_POLICY == IRT_SCHED_POLICY_STATIC
#include "sched_policies/impl/irt_sched_static.impl.h"
#endif

#if IRT_SCHED_POLICY == IRT_SCHED_POLICY_LAZY_BINARY_SPLIT
#include "sched_policies/impl/irt_sched_lazy_binary_splitting.impl.h"
#endif

#include <time.h>

void irt_scheduling_loop(irt_worker* self) {
	while(self->state != IRT_WORKER_STATE_STOP) {
		self->have_wait_mutex = false;
		// while there is something to do, continue scheduling
		while(irt_scheduling_iteration(self)) 
			IRT_DEBUG("%sWorker %3d scheduled something.\n", self->id.value.components.thread==0?"":"\t\t\t\t\t\t", self->id.value.components.thread);
		// check if self is the last worker
		uint32 active = irt_g_active_worker_count;
		if(active<=1) continue; // continue scheduling if last active
		if(!irt_atomic_bool_compare_and_swap(&irt_g_active_worker_count, active, active-1)) continue;
		// nothing to schedule, wait for signal
		IRT_DEBUG("%sWorker %3d trying sleep A.\n", self->id.value.components.thread==0?"":"\t\t\t\t\t\t", self->id.value.components.thread);
		pthread_mutex_lock(&self->wait_mutex);
		IRT_DEBUG("%sWorker %3d trying sleep B.\n", self->id.value.components.thread==0?"":"\t\t\t\t\t\t", self->id.value.components.thread);
		// try one more scheduling iteration, in case something happened before lock
		self->have_wait_mutex = true;
		if(irt_scheduling_iteration(self) || self->state == IRT_WORKER_STATE_STOP) {
			IRT_DEBUG("%sWorker %3d rescheduling before sleep.\n", self->id.value.components.thread==0?"":"\t\t\t\t\t\t", self->id.value.components.thread);
			irt_atomic_inc(&irt_g_active_worker_count);
			continue;
		}
		IRT_DEBUG("%sWorker %3d actually sleeping.\n", self->id.value.components.thread==0?"":"\t\t\t\t\t\t", self->id.value.components.thread);
		int wait_err = pthread_cond_wait(&self->wait_cond, &self->wait_mutex);
		IRT_ASSERT(wait_err == 0, IRT_ERR_INTERNAL, "Worker failed to wait on scheduling condition");
		IRT_DEBUG("%sWorker %3d woken.\n", self->id.value.components.thread==0?"":"\t\t\t\t\t\t", self->id.value.components.thread);
		// we were woken up by the signal and now own the mutex
		pthread_mutex_unlock(&self->wait_mutex);
		irt_atomic_inc(&irt_g_active_worker_count);
	}
}

void irt_signal_worker(irt_worker* target) {
	pthread_mutex_lock(&target->wait_mutex);
	pthread_cond_signal(&target->wait_cond);
	//IRT_DEBUG("%sWorker %3d signalled.\n", target->id.value.components.thread==0?"":"\t\t\t\t\t\t", target->id.value.components.thread);
	pthread_mutex_unlock(&target->wait_mutex);
}
