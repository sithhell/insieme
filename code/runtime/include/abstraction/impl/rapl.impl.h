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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include "irt_inttypes.h"

#ifdef _MSC_VER
	#include <io.h>
#else
	#include <unistd.h>
#endif

int32 _irt_open_msr(uint32 core) {
	char path_to_msr[512];
	int32 file;

	sprintf(path_to_msr, "/dev/cpu/%u/msr", core);

	if ((file = open(path_to_msr, O_RDONLY)) < 0) {
		IRT_DEBUG("Instrumentation: Unable to open MSR file for reading, file %s, reason: %s\n", path_to_msr, strerror(errno));
		return -1;
	}

	return file;
}

int64 _irt_read_msr(int32 file, int32 subject) {
	int64 data;

	if (pread(file, &data, sizeof data, subject) != sizeof data) {
		IRT_DEBUG("Instrumentation: Unable to read MSR file %d, reason: %s\n", file, strerror(errno));
		return -1.0;
	}

	return data;
}

int32 _irt_close_msr(int32 file) {
	return close(file);
}

void _irt_get_rapl_energy_consumption(rapl_energy_data* data) {
        int32 file = 0;
        //int32 numcores = irt_get_num_cpus();
        int64 result = 0;
        double energy_units = -1.0;
        const uint32 core_start = (irt_affinity_mask_get_first_cpu(irt_worker_get_current()->affinity))/8;
        const uint32 core_end = (core_start + ceil((double)irt_g_worker_count/8));

        // for core_end until (all cores excl. HT), stepsize (all cores excl. HT / number of sockets)
        for(uint32 core = core_start; core < core_end; ++core) {
                data->package[core] = 0.0;
                data->mc[core] = 0.0;
                data->cores[core] = 0.0;
                if((file = _irt_open_msr(core*8)) > 0) {
                        if((result = _irt_read_msr(file, MSR_RAPL_POWER_UNIT)) >= 0) {
                                energy_units = pow(0.5, (double)((result>>8) & 0x1F));
                                if((result = _irt_read_msr(file, MSR_PKG_ENERGY_STATUS)) >= 0)
                                        data->package[core] = (double) (result&0xFFFFFFFF) * energy_units;
                                if((result = _irt_read_msr(file, MSR_DRAM_ENERGY_STATUS)) >= 0)
                                        data->mc[core] = (double) (result&0xFFFFFFFF) * energy_units;
                                if((result = _irt_read_msr(file, MSR_PP0_ENERGY_STATUS)) >= 0)
                                        data->cores[core] = (double) (result&0xFFFFFFFF) * energy_units;
                        }
                        _irt_close_msr(file);
                }
        }
}

bool irt_rapl_is_supported() {
	volatile unsigned a, b, c, d;

	const unsigned vendor_string_ebx = 0x756E6547; // Genu
	const unsigned vendor_string_ecx = 0x6C65746E; // ineI
	const unsigned vendor_string_edx = 0x49656E69; // ntel

	__asm__ __volatile__("cpuid" : "=b" (b), "=c" (c), "=d" (d) : "a" (0x0));

	// if not an intel cpu
	if(b != vendor_string_ebx || c != vendor_string_ecx || d != vendor_string_edx)
		return false;

	__asm__ __volatile__("cpuid" : "=a" (a) : "a" (0x00000001) : "ebx", "ecx", "edx");

	const unsigned model_number = (a>>4)&0xF; // bits 4-7
	const unsigned family_code = (a>>8)&0xF; // bits 8-11
	const unsigned extended_model = (a>>16)&0xF; // bits 16-19

	if(family_code == 0x6) {
		if(model_number == 0xA && extended_model == 0x2) // SandyBridge 32nm
			return true;
		if(model_number == 0xE && extended_model == 0x2) // SandyBridge EN 32nm
			return true;
		if(model_number == 0xD && extended_model == 0x2) // dx1 = E5-2660
			return true;
		if(model_number == 0xA && extended_model == 0x3) // IvyBridge 22nm
			return true;
	}

	return false;
}
