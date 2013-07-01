/**
 * ------------------------ Auto-generated Code ------------------------ 
 *           This code was generated by the Insieme Compiler 
 * --------------------------------------------------------------------- 
 */
#include <irt_all_impls.h>
#include <standalone.h>
#include <stdint.h>
#include <stdio.h>

struct __insieme_type_12 {
    irt_type_id c0;
    int32_t c1;
    char** c2;
};

/* ------- Function Definitions --------- */
void utils_func_2() {
    printf("Utils func 2\n");
}
/* ------- Function Definitions --------- */
void common_func_1() {
    printf("Common func 1\n");
}
/* ------- Function Definitions --------- */
int32_t __insieme_fun_8(int32_t argc, char** argv) {
    common_func_1();
    utils_func_2();
    return 0;
}
/* ------- Function Definitions --------- */
void insieme_wi_0_var_0_impl(irt_work_item* var_120) {
    __insieme_fun_8((*(struct __insieme_type_12*)(*var_120).parameters).c1, (*(struct __insieme_type_12*)(*var_120).parameters).c2);
}
// --- work item variants ---
irt_wi_implementation_variant g_insieme_wi_0_variants[] = {
    { IRT_WI_IMPL_SHARED_MEM, &insieme_wi_0_var_0_impl, NULL, 0, NULL, 0, NULL, {0ull, 0, -1ll, -1ll} },
};
// --- the implementation table --- 
irt_wi_implementation g_insieme_impl_table[] = {
    { 1, g_insieme_wi_0_variants },
};

// --- componenents for type table entries ---
irt_type_id g_type_3_components[] = {2};
irt_type_id g_type_4_components[] = {3};
irt_type_id g_type_5_components[] = {0,1,4};

// --- the type table ---
irt_type g_insieme_type_table[] = {
    {IRT_T_UINT32, sizeof(irt_type_id), 0, 0},
    {IRT_T_INT32, sizeof(int32_t), 0, 0},
    {IRT_T_UINT32, sizeof(char), 0, 0},
    {IRT_T_POINTER, sizeof(char*), 1, g_type_3_components},
    {IRT_T_POINTER, sizeof(char**), 1, g_type_4_components},
    {IRT_T_STRUCT, sizeof(struct __insieme_type_12), 3, g_type_5_components}
};

void insieme_init_context(irt_context* context) {
    context->type_table_size = 6;
    context->type_table = g_insieme_type_table;
    context->impl_table_size = 1;
    context->impl_table = g_insieme_impl_table;
}

void insieme_cleanup_context(irt_context* context) {
}

/* ------- Function Definitions --------- */
int32_t main(int32_t var_118, char** var_119) {
    irt_runtime_standalone(irt_get_default_worker_count(), &insieme_init_context, &insieme_cleanup_context, 0, (irt_lw_data_item*)(&(struct __insieme_type_12){5, var_118, var_119}));
    return 0;
}


