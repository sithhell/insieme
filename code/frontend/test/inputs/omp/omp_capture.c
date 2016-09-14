/**
 * Copyright (c) 2002-2016 Distributed and Parallel Systems Group,
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

int main() {
	int magic;

	#pragma test expect_ir(R"(
		def __any_string__parfun = function (v0 : ref<ref<int<4>,f,f,plain>,f,f,plain>) -> unit {
			{
				var ref<int<4>,f,f,plain> v1 = **v0;
			}
			merge_all();
		};
		{
			var ref<int<4>,f,f,plain> v0 = 42;
			{
				merge(parallel(job[1ul...] => __any_string__parfun(v0)));
			}
		}
	)")
	{
		int a = 42;
		#pragma omp parallel
		{
			int i = a;
		}
	}

	#pragma test expect_ir(R"(
		def __any_string__parfun = function (v0 : ref<ref<int<4>,f,f,plain>,f,f,plain>) -> unit {
			var ref<int<4>,f,f,plain> v1 = ref_decl(type_lit(ref<int<4>,f,f,plain>));
			{
				var ref<int<4>,f,f,plain> v2 = **v0;
				v1 = 5;
			}
			merge_all();
		};
		{
			var ref<int<4>,f,f,plain> v0 = 42;
			var ref<int<4>,f,f,plain> v1 = 1337;
			{
				merge(parallel(job[1ul...] => __any_string__parfun(v0)));
			}
		}
	)")
	{
		int a = 42, b = 1337;
		#pragma omp parallel shared(a) private(b)
		{
			int i = a;
			b = 5;
		}
	}

	#pragma test expect_ir(R"(
		def __any_string__parfun = function (v0 : ref<ptr<int<4>>,f,f,plain>) -> unit {
			{
				ptr_to_ref(*v0) = 5;
			}
			merge_all();
		};
		{
			var ref<int<4>,f,f,plain> v0 = 42;
			var ref<ptr<int<4>>,f,f,plain> v1 = ptr_from_ref(v0);
			{
				merge(parallel(job[1ul...] => __any_string__parfun(*v1)));
			}
		}
	)")
	{
		int a = 42, *b = &a;
		#pragma omp parallel shared(b)
		{
			*b = 5;
		}
	}

	//{
	//	int num = 8;
	//	int arr[num];
	//	#pragma omp parallel
	//	{
	//		arr[3] = 0;
	//	}
	//}
}