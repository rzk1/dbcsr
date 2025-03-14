/*------------------------------------------------------------------------------------------------*
 * Copyright (C) by the DBCSR developers group - All rights reserved                              *
 * This file is part of the DBCSR library.                                                        *
 *                                                                                                *
 * For information on the license, see the LICENSE file.                                          *
 * For further information please visit https://dbcsr.cp2k.org                                    *
 * SPDX-License-Identifier: GPL-2.0+                                                              *
 *------------------------------------------------------------------------------------------------*/

#ifndef DBCSR_TENSOR_H
#define DBCSR_TENSOR_H

#include <mpi.h>
#include <stdbool.h> /* we need bool from C99 */

#:include "dbcsr_tensor.fypp"
#:set ndims = range(2, maxrank + 1)
#:set ddims = range(1, maxrank + 1)

typedef void* dbcsr_matrix;
typedef void* dbcsr_t_tensor;
typedef void* dbcsr_t_distribution;
typedef void* dbcsr_t_pgrid;
typedef void* dbcsr_t_iterator;

#if defined(__cplusplus)
extern "C" {
#endif

void c_dbcsr_t_finalize(const dbcsr_t_tensor tensor);

void c_dbcsr_t_pgrid_create(const MPI_Fint *fcomm, int *c_dims,
                            const int dims_size, dbcsr_t_pgrid *c_pgrid,
                            const int *c_tensor_dims);

void c_dbcsr_t_pgrid_create_expert(const MPI_Fint *fcomm, int *c_dims,
                                   const int dims_size, dbcsr_t_pgrid *c_pgrid,
                                   const int *c_map1_2d, const int map1_2d_size,
                                   const int *c_map2_2d, const int map2_2d_size,
                                   const int *c_tensor_dims, const int *nsplit,
                                   const int *dimsplit);

void c_dbcsr_t_pgrid_destroy(dbcsr_t_pgrid *c_pgrid, const bool *c_keep_comm);

void c_dbcsr_t_distribution_new(dbcsr_t_distribution *c_dist, const dbcsr_t_pgrid c_pgrid,
                               ${extern_varlist_and_size("c_nd_dist", "const int")}$);

void c_dbcsr_t_distribution_destroy(dbcsr_t_distribution *c_dist);

void c_dbcsr_t_create_new(dbcsr_t_tensor *c_tensor, const char *c_name,
                          const dbcsr_t_distribution c_dist, const int *c_map1_2d,
                          const int c_map1_2d_size, const int *c_map2_2d,
                          const int c_map2_2d_size, const int *data_type,
                         ${extern_varlist_and_size("c_blk_size", "const int")}$);

void c_dbcsr_t_create_template(const dbcsr_t_tensor c_tensor_in, dbcsr_t_tensor *c_tensor,
                               const char *c_name, const void *c_dist,
                               const int *c_map1_2d, const int map1_2d_size,
                               const int *c_map2_2d, const int map2_2d_size,
                               const int *data_type);

void c_dbcsr_t_create_matrix(const dbcsr_matrix c_matrix_in, dbcsr_t_tensor *c_tensor,
                             const int *c_order, const char *c_name);

void c_dbcsr_t_destroy(dbcsr_t_tensor *c_tensor);

#:for dsuffix, ctype in c_dtype_float_list
#:for ndim in ndims

void c_dbcsr_t_get_${ndim}$d_block_${dsuffix}$(
                                               const dbcsr_t_tensor c_tensor,
                                               const int tensor_dim, const int *c_ind,
                                               const int *c_sizes, ${ctype}$ *c_block,
                                               bool *c_found);

void c_dbcsr_t_put_${ndim}$d_block_${dsuffix}$(const dbcsr_t_tensor c_tensor, const int tensor_dim,
                                               const int *c_ind, const int *c_sizes, const ${ctype}$ *c_block,
                                               const bool *c_summation, const ${ctype}$ *c_scale);

void c_dbcsr_t_get_${ndim}$d_block_p_${dsuffix}$(const dbcsr_t_tensor c_tensor,
                                                 const int *c_ind,${ctype}$ **c_block,
                                                 bool *c_found);

#:endfor

void c_dbcsr_t_get_data_${dsuffix}$(const dbcsr_t_tensor c_tensor, ${ctype}$ **c_data,
                                    long long int *c_data_size,
                                    ${ctype}$ c_select_data_type, int *c_lb,
                                    int *c_ub);

void c_dbcsr_t_contract_${dsuffix}$(
    const ${ctype}$ c_alpha, dbcsr_t_tensor c_tensor_1, dbcsr_t_tensor c_tensor_2,
    const ${ctype}$ c_beta, dbcsr_t_tensor c_tensor_3, const int *c_contract_1,
    const int c_contract_1_size, const int *c_notcontract_1,
    const int c_notcontract_1_size, const int *c_contract_2,
    const int c_contract_2_size, const int *c_notcontract_2,
    const int c_notcontract_2_size, const int *c_map_1, const int c_map_1_size,
    const int *c_map_2, const int c_map_2_size, const int *c_bounds_1,
    const int *c_bounds_2, const int *c_bounds_3, const bool *c_optimize_dist,
    dbcsr_t_pgrid *c_pgrid_opt_1, dbcsr_t_pgrid *c_pgrid_opt_2, dbcsr_t_pgrid *c_pgrid_opt_3,
    const double *filter_eps, long long int *flop, const bool *move_data,
    const bool *retain_sparsity, const int *unit_nr, const bool *log_verbose);

void c_dbcsr_t_contract_index_${dsuffix}$(
    const ${ctype}$ c_alpha, dbcsr_t_tensor c_tensor_1, dbcsr_t_tensor c_tensor_2,
    const ${ctype}$ c_beta, dbcsr_t_tensor c_tensor_3, const int *c_contract_1,
    const int contract_1_size, const int *c_notcontract_1,
    const int notcontract_1_size, const int *c_contract_2,
    const int contract_2_size, const int *c_notcontract_2,
    const int notcontract_2_size, const int *c_map_1, const int map_1_size,
    const int *c_map_2, const int map_2_size, const int *c_bounds_1,
    const int *c_bounds_2, const int *c_bounds_3, const double *c_filter_eps,
    int *c_nblks_local, int *c_result_index, long long int result_index_size,
    int tensor3_dim);

void c_dbcsr_t_filter_${dsuffix}$(const dbcsr_t_tensor c_tensor, const ${ctype}$ c_eps,
                                   const int *c_method,
                                   const bool *c_use_absolute);

void c_dbcsr_t_set_${dsuffix}$(const dbcsr_t_tensor c_tensor, const ${ctype}$ c_alpha);

void c_dbcsr_t_scale_${dsuffix}$(const dbcsr_t_tensor c_tensor,
                                 const ${ctype}$ c_alpha);

#:endfor

void c_dbcsr_t_get_stored_coordinates(const dbcsr_t_tensor c_tensor,
                                      const int tensor_dim, const int *c_ind_nd,
                                      int *c_processor);

void c_dbcsr_t_reserve_blocks_index(const dbcsr_t_tensor c_tensor, const int nblocks,
                                   ${varlist("const int* c_blk_ind")}$);

void c_dbcsr_t_reserve_blocks_template(const dbcsr_t_tensor c_tensor_in,
                                       const dbcsr_t_tensor tensor_out);

int c_ndims_iterator(const dbcsr_t_iterator c_iterator);

void c_dbcsr_t_iterator_start(dbcsr_t_iterator *c_iterator, const dbcsr_t_tensor c_tensor);

void c_dbcsr_t_iterator_stop(dbcsr_t_iterator *c_iterator);

void c_dbcsr_t_iterator_next_block(const dbcsr_t_iterator c_iterator,
                                   const int iterator_size, int *c_ind_nd,
                                   int *c_blk, int *c_blk_p, int *c_blk_size,
                                   int *c_blk_offset);

bool c_dbcsr_t_iterator_blocks_left(const dbcsr_t_iterator c_iterator);

void c_dbcsr_t_split_blocks(const dbcsr_t_tensor c_tensor_in, const int tensor_dim,
                            dbcsr_t_tensor *c_tensor_out, const int *c_block_sizes,
                            const bool *c_nodata);

void c_dbcsr_t_copy_matrix_to_tensor(const dbcsr_matrix c_matrix_in,
                                     dbcsr_t_tensor c_tensor_out,
                                     const bool *c_summation);

void c_dbcsr_t_copy_tensor_to_matrix(const dbcsr_t_tensor c_tensor_in,
                                     dbcsr_matrix c_matrix_out,
                                     const bool *c_summation);

void c_dbcsr_t_copy(const dbcsr_t_tensor c_tensor_in, const int tensor_dim,
                    dbcsr_t_tensor c_tensor_out, const int *c_order,
                    const bool *c_summation, const int *c_bounds,
                    const bool *c_move_data, const int *c_unit_nr);

void c_dbcsr_t_clear(dbcsr_t_tensor c_tensor);

void c_dbcsr_t_get_info(const dbcsr_t_tensor c_tensor, const int tensor_dim,
                        int *c_nblks_total, int *c_nfull_total,
                        int *c_nblks_local, int *c_nfull_local, int *c_pdims,
                        int *my_ploc,${varlist("int nblks_local")}$,
                        ${varlist("int nblks_total")}$,
                        ${varlist("int* c_blks_local")}$,
                        ${varlist("int* c_proc_dist")}$,
                        ${varlist("int* c_blk_size")}$,
                        ${varlist("int* c_blk_offset")}$,
                        dbcsr_t_distribution *c_distribution, char **name, int *data_type);

void c_dbcsr_t_get_nd_index_blk(const dbcsr_t_tensor c_tensor, void **c_nd_index_blk);

void c_dbcsr_t_get_nd_index(const dbcsr_t_tensor c_tensor, void **c_nd_index);

void c_dbcsr_t_get_mapping_info(const dbcsr_t_tensor c_tensor, const int nd_size,
                                const int nd_row_size, const int nd_col_size,
                                int *ndim_nd, int *ndim1_2d, int *ndim2_2d,
                                long long int *c_dims_2d_i8, int *c_dims_2d,
                                int *c_dims_nd, int *c_dims1_2d,
                                int *c_dims2_2d, int *c_map1_2d, int *c_map2_2d,
                                int *c_map_nd, int *base, bool *c_col_major);

int c_dbcsr_t_get_num_blocks(const dbcsr_t_tensor c_tensor);

long long int c_dbcsr_t_get_num_blocks_total(const dbcsr_t_tensor c_tensor);

void c_dbcsr_t_dims(const dbcsr_t_tensor c_tensor, const int tensor_dim,
                    const int *c_dims);

int c_dbcsr_t_ndims(const dbcsr_t_tensor c_tensor);

int c_dbcsr_t_nblks_local(const dbcsr_t_tensor c_tensor, const int idim);

int c_dbcsr_t_nblks_total(const dbcsr_t_tensor c_tensor, const int idim);

int c_dbcsr_t_ndims_matrix_row(const dbcsr_t_tensor c_tensor);

int c_dbcsr_t_ndims_matrix_column(const dbcsr_t_tensor c_tensor);

int c_dbcsr_t_get_nze(const dbcsr_t_tensor c_tensor);

long long int c_dbcsr_t_get_nze_total(const dbcsr_t_tensor c_tensor);

int c_dbcsr_t_ndims_matrix_column(const dbcsr_t_tensor c_tensor);

int c_dbcsr_t_get_nze(const dbcsr_t_tensor c_tensor);

long long int c_dbcsr_t_get_nze_total(const dbcsr_t_tensor c_tensor);

long long int c_dbcsr_t_max_nblks_local(const dbcsr_t_tensor c_tensor);

void c_dbcsr_t_batched_contract_init(dbcsr_t_tensor c_tensor);

void c_dbcsr_t_batched_contract_finalize(dbcsr_t_tensor c_tensor, int* c_unit_nr);

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
// --------------------------------------------------- //
//         overloaded functions (cpp only)             //
// --------------------------------------------------- //
#:for dsuffix, ctype in c_dtype_float_list
inline void c_dbcsr_t_get_block(const dbcsr_t_tensor c_tensor, const int *c_ind,
                                const int *c_sizes, ${ctype}$ *c_block,
                                bool *c_found) {

  int tensor_dim = c_dbcsr_t_ndims(c_tensor);

  switch (tensor_dim) {
#:for ndim in ndims
  case ${ndim}$:
    c_dbcsr_t_get_${ndim}$d_block_${dsuffix}$(c_tensor, tensor_dim, c_ind,
                                                c_sizes, c_block, c_found);
    break;
#:endfor
  }
}

inline void c_dbcsr_t_get_block_p(const dbcsr_t_tensor c_tensor, const int *c_ind,
                                 ${ctype}$ **c_block, bool *c_found) {

  int tensor_dim = c_dbcsr_t_ndims(c_tensor);

  switch (tensor_dim) {
#:for ndim in ndims
  case ${ndim}$:
    c_dbcsr_t_get_${ndim}$d_block_p_${dsuffix}$(c_tensor, c_ind, c_block,
                                                  c_found);
    break;
#:endfor
  }
}

#:endfor

#:for dsuffix, ctype in c_dtype_float_list
inline void c_dbcsr_t_put_block(const dbcsr_t_tensor c_tensor, const int *c_ind,
                                const int *c_sizes, const ${ctype}$ *c_block,
                                const bool *c_summation,
                                const ${ctype}$ *c_scale) {

  int tensor_dim = c_dbcsr_t_ndims(c_tensor);

  switch (tensor_dim) {
#:for ndim in ndims
  case ${ndim}$:
    c_dbcsr_t_put_${ndim}$d_block_${dsuffix}$(
        c_tensor, tensor_dim, c_ind, c_sizes, c_block, c_summation, c_scale);
    break;
#:endfor
  }
}
#:endfor

inline void c_dbcsr_t_get_stored_coordinates(const dbcsr_t_tensor c_tensor,
                                             const int *c_ind_nd,
                                             int *c_processor) {

  int tensor_dim = c_dbcsr_t_ndims(c_tensor);
  c_dbcsr_t_get_stored_coordinates(c_tensor, tensor_dim, c_ind_nd, c_processor);
}

inline void c_dbcsr_t_iterator_next_block(const dbcsr_t_iterator c_iterator, int *c_ind_nd,
                                          int *c_blk, int *c_blk_p,
                                          int *c_blk_size, int *c_blk_offset) {

  int iterator_size = c_ndims_iterator(c_iterator);

  c_dbcsr_t_iterator_next_block(c_iterator, iterator_size, c_ind_nd, c_blk,
                                c_blk_p, c_blk_size, c_blk_offset);
}

#:for dsuffix, ctype in c_dtype_float_list
inline void c_dbcsr_t_filter(const dbcsr_t_tensor c_tensor, const ${ctype}$ c_eps,
                             const int *c_method, const bool *c_use_absolute) {

  c_dbcsr_t_filter_${dsuffix}$(c_tensor, c_eps, c_method, c_use_absolute);
}

inline void c_dbcsr_t_set(const dbcsr_t_tensor c_tensor, const ${ctype}$ c_alpha) {

  c_dbcsr_t_set_${dsuffix}$(c_tensor, c_alpha);
}

inline void c_dbcsr_t_scale(const dbcsr_t_tensor c_tensor, const ${ctype}$ c_alpha) {

  c_dbcsr_t_scale_${dsuffix}$(c_tensor, c_alpha);
}

inline void c_dbcsr_t_get_data_p(const dbcsr_t_tensor c_tensor, ${ctype}$ **c_data,
                                 long long int *c_data_size,
                                 ${ctype}$ c_select_data_type, int *c_lb,
                                 int *c_ub) {
  c_dbcsr_t_get_data_${dsuffix}$(c_tensor, c_data, c_data_size,
                                  c_select_data_type, c_lb, c_ub);
}
#:endfor
#endif

#endif /*DBCSR_H*/
