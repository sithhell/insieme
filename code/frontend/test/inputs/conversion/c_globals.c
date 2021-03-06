/**
 * Copyright (c) 2002-2017 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
 */

#pragma test expect_ir("lit(\"x\": ref<int<4>,f,f>)")
int x;

#pragma test expect_ir("lit(\"y\": ref<real<4>,t,f>)")
const float y;

typedef enum { Bla, Alb } enum_t;
#pragma test expect_ir("lit(\"globalEnum\": ref<(type<enum_def<IMP_enum_t,int<4>,enum_entry<IMP__colon__colon_Bla,0>,enum_entry<IMP__colon__colon_Alb,1>>>, int<4>)>)")
enum_t globalEnum;

typedef struct { int x; } IAmTheTagType;
#pragma test expect_ir("lit(\"tt\": ref<struct IMP_IAmTheTagType { x: int<4>; },f,f>)")
IAmTheTagType tt;

typedef struct _omp_lock_t { int x; } omp_lock_t;
void omp_set_lock(omp_lock_t* lock);
#pragma test expect_ir("lit(\"lck\": ref<struct IMP__omp_lock_t { x : int<4>; },f,f>)")
omp_lock_t lck;

int main() {
	globalEnum;
	y;
	tt;
	#pragma test expect_ir("STRING","ptr_from_ref(lck)")
	&lck;
	#pragma test expect_ir("STRING","IMP_omp_set_lock(ptr_from_ref(lck))")
	omp_set_lock(&lck);
	return x;
}
