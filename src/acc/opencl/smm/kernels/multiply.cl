/*------------------------------------------------------------------------------------------------*
 * Copyright (C) by the DBCSR developers group - All rights reserved                              *
 * This file is part of the DBCSR library.                                                        *
 *                                                                                                *
 * For information on the license, see the LICENSE file.                                          *
 * For further information please visit https://dbcsr.cp2k.org                                    *
 * SPDX-License-Identifier: GPL-2.0+                                                              *
 *------------------------------------------------------------------------------------------------*/
#if (200/*CL_VERSION_2_0*/ <= __OPENCL_VERSION__) || defined(__NV_CL_C_VERSION)
# define UNROLL_FORCE(N) __attribute__((opencl_unroll_hint(N)))
#else
# define UNROLL_FORCE(N)
#endif

#if defined(LU) && (0 <= LU)
# if (0 == LU)
#   define UNROLL_OUTER(N) UNROLL_FORCE(N)
# elif (1 == LU)
#   define UNROLL_OUTER(N) UNROLL_FORCE(1)
# else
#   define UNROLL_OUTER(N) UNROLL_FORCE(N)
# endif
# define UNROLL(N) UNROLL_FORCE(N)
#else
# define UNROLL_OUTER(N)
# define UNROLL(N)
#endif

#if !defined(AL) || (SM != SN) || (SN != SK)
# define ADX(M, K) adata[SM*K+M+a0]
# define BDX(K, N) bdata[SN*K+N+b0]
# define CDX(M, N) cdata[SM*N+M+c0]
#else
# define ADX(M, K) adata[SK*M+K+a0]
# define BDX(K, N) bdata[SK*N+K+b0]
# define CDX(M, N) cdata[SN*M+N+c0]
#endif

#if defined(SLM_A)
# if (1 != BK || BM < SM || 1 != BN)
#   define AMK(M, K) amk[M][K]
# else
#   define AMK(M, K) amk[M]
# endif
#elif defined(REG_A)
# if (1 != BK)
#   define AMK(M, K) amk[K]
# else
#   define AMK(M, K) amk[M]
# endif
#else
# define AMK(M, K) ADX(M, K)
#endif
#if defined(SLM_B)
# define BNK(N, K) bnk[N][K]
#elif defined(REG_B)
# if (BM < SM || 1 != BN)
#   define BNK(N, K) bnk[N][K]
# else
#   define BNK(N, K) bnk[K]
# endif
#else
# define BNK(N, K) BDX(K, N)
#endif
#if (1 < BS) && (defined(SLM_C) || (BM < SM || 1 != BN))
# define CNM(N, M) cnm[N][M]
#else
# define CNM(N, M) cnm[M]
#endif

#if (1 == TN)
# define ZERO 0.f
#elif (3 == TN)
# define ZERO 0.0
#else
# define ZERO 0
#endif

#if defined(SLM_P) && (1 < BS)
# define IDXBASE 0
#else
# define IDXBASE 1
#endif
#if !defined(REPEAT)
# define REPEAT 1
#endif

#define NBM ((SM+BM-1)/BM)
#define NBN ((SN+BN-1)/BN)
#define WRK (NBM*NBN)

#define UM (SM/BK)
#define VM (SM%UM)


#if defined(ATOMIC_PROTOTYPES) || defined(__opencl_c_ext_fp64_global_atomic_add)
# if defined(__opencl_c_ext_fp64_global_atomic_add)
#   undef ATOMIC_ADD_GLOBAL
#   if defined(TF)
#     define ATOMIC_ADD_GLOBAL(A, B) atomic_fetch_add_explicit((global volatile TF*)A, B, \
        memory_order_relaxed, memory_scope_work_group)
#   else
#     define ATOMIC_ADD_GLOBAL(A, B) atomic_add(A, B)
#   endif
# else
#   if defined(TF)
__attribute__((overloadable)) T atomic_fetch_add_explicit(global volatile TF*, T, memory_order, memory_scope);
#   else
__attribute__((overloadable)) T atomic_add(global volatile T*, T);
#   endif
# endif
#endif

#if !defined(cl_intel_global_float_atomics) || (1 != TN)
#if defined(ATOMIC32_ADD64)
__attribute__((always_inline))
inline void atomic32_add64_global(global volatile double* dst, double inc)
{
  *dst += inc; /* TODO */
}
#endif

#if defined(CMPXCHG)
__attribute__((always_inline))
inline void atomic_add_global_cmpxchg(global volatile T* dst, T inc)
{
#if !defined(ATOMIC32_ADD64)
  union { T f; TA a; } exp_val, try_val, cur_val = { .f = *dst };
  do {
    exp_val.a = cur_val.a; try_val.f = exp_val.f + inc;
# if defined(TA2)
    if (0 == atomic_compare_exchange_weak_explicit(
        (global volatile TA2*)dst, &cur_val.a, try_val.a,
        memory_order_relaxed, memory_order_relaxed,
      memory_scope_work_group)) continue;
# else
    cur_val.a = CMPXCHG((global volatile TA*)dst, exp_val.a, try_val.a);
# endif
  } while (cur_val.a != exp_val.a);
#else
  atomic32_add64_global(dst, inc);
#endif
}
#endif

#if defined(ATOMIC_ADD2_GLOBAL) && (1 == TN)
__attribute__((always_inline))
inline void atomic_add_global_cmpxchg2(global volatile float* dst, float2 inc)
{
  union { float2 f; long a; } exp_val, try_val, cur_val = { .f = (float2)(dst[0], dst[1]) };
  do {
    exp_val.a = cur_val.a; try_val.f = exp_val.f + inc;
# if defined(TA2)
    if (0 == atomic_compare_exchange_weak_explicit(
        (global volatile atomic_long*)dst, &cur_val.a, try_val.a,
        memory_order_relaxed, memory_order_relaxed,
      memory_scope_work_group)) continue;
# else
    cur_val.a = atom_cmpxchg((global volatile long*)dst, exp_val.a, try_val.a);
# endif
  } while (cur_val.a != exp_val.a);
}
#endif

#if defined(XCHG) || (defined(__NV_CL_C_VERSION) && !defined(CMPXCHG))
__attribute__((always_inline))
inline void atomic_add_global_xchg(global volatile T* dst, T inc)
{
#if !defined(ATOMIC32_ADD64)
# if (defined(__NV_CL_C_VERSION) && !defined(XCHG)) && (1 == TN)
  asm("{ .reg .f32 t; atom.global.add.f32 t, [%0], %1; }" :: "l"(dst), "f"(inc));
# elif (defined(__NV_CL_C_VERSION) && !defined(XCHG)) && (3 == TN)
  asm("{ .reg .f64 t; atom.global.add.f64 t, [%0], %1; }" :: "l"(dst), "d"(inc));
# else
  union { T f; TA a; } exp_val = { .f = inc }, try_val, cur_val = { /*.f = ZERO*/.a = 0 };
  do {
#   if defined(TA2)
    try_val.a = atomic_exchange_explicit((global volatile TA2*)dst, cur_val.a,
      memory_order_relaxed, memory_scope_work_group);
#   else
    try_val.a = XCHG((global volatile TA*)dst, cur_val.a);
#   endif
    try_val.f += exp_val.f;
#   if defined(TA2)
    exp_val.a = atomic_exchange_explicit((global volatile TA2*)dst, try_val.a,
      memory_order_relaxed, memory_scope_work_group);
#   else
    exp_val.a = XCHG((global volatile TA*)dst, try_val.a);
#   endif
  } while (cur_val.a != exp_val.a);
# endif
#else
  atomic32_add64_global(dst, inc);
#endif
}
#endif
#endif


__attribute__((reqd_work_group_size(SWG, 1, 1)))
#if (0 < SGS)
__attribute__((intel_reqd_sub_group_size(SGS)))
#endif
kernel void FN(global T *restrict cdata,
  GLOBAL const T *restrict adata, GLOBAL const T *restrict bdata,
  /* indexes given by param_stack are one-based (Fortran) */
#if (1 < BS)
  GLOBAL const int *restrict param_stack, int stack_size)
#else
  GLOBAL const int *restrict param_stack)
#endif
{
  const int gid = get_group_id(0), idx = get_local_id(0);
  GLOBAL const int *restrict pbase = param_stack + gid * (3 * BS);
#if defined(SLM_P) && (1 < BS)
  local int params[3*BS];
#else
  GLOBAL const int *restrict params = pbase;
#endif
#if defined(SLM_A)
# if (1 != BK || BM < SM || 1 != BN)
  local T amk[SM][SK+SLM_A-1];
# else
  local T amk[SM];
# endif
#elif defined(REG_A)
# if (1 != BK)
  T amk[SK];
# else
  T amk[SM];
# endif
#endif
#if defined(SLM_B)
  local T bnk[SN][SK+SLM_B-1];
#endif
#if (BM < SM || 1 != BN)
# if defined(REG_B) && !defined(SLM_B)
  T bnk[BN][SK];
# endif
# if !defined(SLM_C) && (1 < BS)
  T cnm[BN][BM];
# endif
  const int m0 = (idx / NBN) * BM, n0 = (idx % NBN) * BN;
#else
# if defined(REG_B) && !defined(SLM_B)
  T bnk[SK];
# endif
# if !defined(SLM_C) && (1 < BS)
  T cnm[SM];
# endif
#endif
#if defined(TRACK_B) && (1 < BS) && defined(REG_B) && !defined(SLM_B)
  int b1 = -1;
#endif

#if (1 < BS)
  /* intra-kernel mini-batch of SMMs */
  const int batchsize = min(BS, stack_size - BS * gid);
  int c0;
# if defined(SLM_C)
  local T cnm[SN][SM+SLM_C-1];
  for (int n = idx; n < SN; n += SWG)
  { UNROLL_FORCE(SM)
    for (int m = 0; m < SM; ++m) cnm[n][m] = ZERO;
  }
# elif (BM < SM || 1 != BN)
  UNROLL(BN)
  for (int bn = 0; bn < BN; ++bn)
  { UNROLL_FORCE(BM)
    for (int bm = 0; bm < BM; ++bm) cnm[bn][bm] = ZERO;
  }
# else
  UNROLL_FORCE(SM)
  for (int m = 0; m < SM; ++m) cnm[m] = ZERO;
# endif
# if defined(SLM_P)
  for (int i = idx; i < (3 * batchsize); i += SWG) params[i] = pbase[i] - 1;
# endif
# if (defined(SLM_C) || defined(SLM_P)) && defined(BARRIER)
  BARRIER(CLK_LOCAL_MEM_FENCE);
# endif
# if (WRK < SWG)
  if (WRK <= idx) return;
# endif
  c0 = params[2] - IDXBASE;
  UNROLL_FORCE(1)
# if (1 < REPEAT)
  for (int item = 0; item < (REPEAT * batchsize); ++item) {
    const int i = item % batchsize;
# else
  for (int item = 0; item < (REPEAT * batchsize); ++item) {
    const int i = item;
# endif
    const int a0 = params[3*i] - IDXBASE, b0 = params[3*i+1] - IDXBASE;
    const int c1 = ((i + 1) < batchsize ? (params[3*i+5] - IDXBASE) : -1);
#else
# if (WRK < SWG)
  if (WRK > idx)
# endif
  {
    const int a0 = params[0] - IDXBASE, b0 = params[1] - IDXBASE, c0 = params[2] - IDXBASE;
#endif

#if defined(SLM_A) && (1 != BK || BM < SM || 1 != BN)
    /* copy or transpose A-matrix into SLM */
    int m = idx;
# if (WRK != SM)
    for (; m < SM; m += WRK)
# endif
    { UNROLL_FORCE(SK)
      for (int k = 0; k < SK; ++k) amk[m][k] = ADX(m, k);
    }
#endif

#if defined(SLM_B)
    { /* copy or transpose B-matrix into SLM */
      int n = idx;
# if (WRK != SN)
      for (; n < SN; n += WRK)
# endif
      { UNROLL(SK)
        for (int k = 0; k < SK; ++k) bnk[n][k] = BDX(k, n);
      }
    }
#elif defined(REG_B)
# if defined(TRACK_B) && (1 < BS)
    if (b0 != b1) {
      b1 = b0;
# else
    { /* copy or transpose B-matrix into registers */
# endif
      UNROLL(SK)
      for (int k = 0; k < SK; ++k) {
# if (BM < SM || 1 != BN)
        UNROLL_FORCE(BN)
        for (int bn = 0; bn < BN; ++bn) {
#   if (SN % BN)
          const int n = min(bn + n0, SN - 1);
#   else
          const int n = bn + n0;
#   endif
          bnk[bn][k] = BDX(k, n);
        }
# else
        bnk[k] = BDX(k, idx);
# endif
      }
    }
#endif

#if defined(BARRIER) && (1 < WRK) && (defined(SLM_B) \
  || ((1 != BK || BM < SM || 1 != BN) && defined(SLM_A)))
    /* finish transpose/copy */
    BARRIER(CLK_LOCAL_MEM_FENCE);
#endif

#if (BM < SM || 1 != BN)
    { /* calculate result-tile using general tiles */
# if defined(REG_A) && !defined(SLM_A)
#   if (1 == BS)
      T cnm[BN] = { ZERO };
#   endif
      UNROLL(BM)
      for (int bm = 0; bm < BM; ++bm) {
        const int m = bm + m0;
#   if (SM % BM)
        if (m < SM)
#   endif
        { UNROLL_FORCE(SK)
          for (int k = 0; k < SK; ++k) amk[k] = ADX(m, k);
          UNROLL(BN)
          for (int bn = 0; bn < BN; ++bn) {
            const int n = bn + n0;
#   if (SN % BN)
            if (n < SN)
#   endif
            {
#   if (1 < BS)
#     if defined(SLM_C)
              const int im = m, in = n;
#     else
              const int im = bm, in = bn;
#     endif
#   else
              const int im = bn, in = idx;
#   endif
              UNROLL_FORCE(SK)
              for (int k = 0; k < SK; ++k) CNM(in, im) = MAD(
                AMK(m, k),
#   if defined(REG_B)
                BNK(bn, k),
#   else
                BNK(n, k),
#   endif
                CNM(in, im));
            }
          }
#   if (1 == BS)
          UNROLL(BN)
#     if defined(ATOMIC_INC_NZ)
          for (int bn = 0; bn < BN; ++bn) if (ZERO != CNM(idx, bn)) {
#     else
          for (int bn = 0; bn < BN; ++bn) {
#     endif
            ATOMIC_ADD_GLOBAL(&CDX(m, bn+n0), CNM(idx, bn));
            CNM(idx, bn) = ZERO; /* reset */
          }
#   endif
        }
      }
# else
      UNROLL(BN)
      for (int bn = 0; bn < BN; ++bn) {
        const int n = bn + n0;
#   if (SN % BN)
        if (n < SN)
#   endif
        {
#   if (1 == BS)
          T cnm[BM] = { ZERO };
#   endif
          UNROLL(BM)
          for (int bm = 0; bm < BM; ++bm) {
            const int m = bm + m0;
#   if (SM % BM)
            if (m < SM)
#   endif
            {
#   if defined(SLM_C) && (1 < BS)
              const int im = m, in = n;
#   else
              const int im = bm, in = bn;
#   endif
              UNROLL_FORCE(SK)
              for (int k = 0; k < SK; ++k) CNM(in, im) = MAD(
                AMK(m, k),
#   if defined(REG_B)
                BNK(bn, k),
#   else
                BNK(n, k),
#   endif
                CNM(in, im));
            }
          }
#   if (1 == BS)
          UNROLL(BM)
#     if defined(ATOMIC_INC_NZ)
          for (int bm = 0; bm < BM; ++bm) if (ZERO != CNM(idx, bm)) {
#     else
          for (int bm = 0; bm < BM; ++bm) {
#     endif
            ATOMIC_ADD_GLOBAL(&CDX(bm+m0, n), CNM(idx, bm));
            CNM(idx, bm) = ZERO; /* reset */
          }
#   endif
        }
      }
# endif
    }
#else
    { /* calculate result-tile using columns */
# if (1 == BS)
      T cnm[UM] = { ZERO };
# endif
# if (1 == BK)
      UNROLL_OUTER(SK)
      for (int k = 0; k < SK; ++k) {
        const T b = BNK(idx, k);
#   if defined(SLM_A)
#     if (WRK != SM)
        for (int m = idx; m < SM; m += WRK) amk[m] = ADX(m, k);
#     else
        amk[idx] = ADX(idx, k);
#     endif
#   elif defined(REG_A)
        UNROLL_FORCE(SM)
        for (int m = 0; m < SM; ++m) amk[m] = ADX(m, k);
#   endif
#   if defined(SLM_A) && defined(BARRIER)
        BARRIER(CLK_LOCAL_MEM_FENCE);
#   endif
#   if defined(SLM_A) || defined(REG_A) || (WRK != SM) || (0 == SGS)
        UNROLL_FORCE(SM)
        for (int m = 0; m < SM; ++m) CNM(idx, m) = MAD(AMK(m, k), b, CNM(idx, m));
#   else
        const T a = AMK(idx, k);
        UNROLL_FORCE(SM)
        for (int m = 0; m < SM; ++m) CNM(idx, m) = MAD(
          sub_group_broadcast(a, m),
          b, CNM(idx, m));
#   endif
      }
#   if (1 == BS)
      UNROLL(SM)
#     if defined(ATOMIC_INC_NZ)
      for (int m = 0; m < SM; ++m) if (ZERO != CNM(idx, m)) {
#     else
      for (int m = 0; m < SM; ++m) {
#     endif
        ATOMIC_ADD_GLOBAL(&CDX(m, idx), CNM(idx, m));
        CNM(idx, m) = ZERO; /* reset */
      }
#   endif
# else
      int m = 0, u;
#   if (1 == UM)
      UNROLL_OUTER(SM)
#   endif
      for (; m < (SM - UM + 1); m += UM) {
        u = 0;
#   if (1 < UM)
        UNROLL(UM)
        for (; u < UM; ++u)
#   endif
        { const int um = u + m;
#   if (1 < BS)
          const int vm = um;
#   else
          const int vm = u;
#   endif
#   if defined(REG_A) && !defined(SLM_A)
          UNROLL_FORCE(SK)
          for (int k = 0; k < SK; ++k) amk[k] = ADX(um, k);
#   endif
          UNROLL_FORCE(SK)
          for (int k = 0; k < SK; ++k) {
            CNM(idx, vm) = MAD(AMK(um, k), BNK(idx, k), CNM(idx, vm));
          }
        }
#   if (1 == BS)
        u = 0;
#     if (1 < UM)
        UNROLL(UM)
        for (; u < UM; ++u)
#     endif
#     if defined(ATOMIC_INC_NZ)
        if (ZERO != CNM(idx, u))
#     endif
        { ATOMIC_ADD_GLOBAL(&CDX(u+m, idx), CNM(idx, u));
          CNM(idx, u) = ZERO; /* reset */
        }
#   endif
      }
#   if (0 < VM)
      /* calculate remainder */
      u = 0;
#     if (1 < VM)
      UNROLL(VM)
      for (; u < VM; ++u)
#     endif
      { const int um = u + m;
#     if (1 < BS)
        const int vm = um;
#     else
        const int vm = u;
#     endif
#     if defined(REG_A) && !defined(SLM_A)
        UNROLL_FORCE(SK)
        for (int k = 0; k < SK; ++k) amk[k] = ADX(um, k);
#     endif
        UNROLL_FORCE(SK)
        for (int k = 0; k < SK; ++k) {
          CNM(idx, vm) = MAD(AMK(um, k), BNK(idx, k), CNM(idx, vm));
        }
      }
#     if (1 == BS)
      u = 0;
#     if (1 < VM)
      UNROLL(VM)
      for (; u < VM; ++u)
#     endif
#     if defined(ATOMIC_INC_NZ)
      if (ZERO != CNM(idx, u))
#     endif
      { ATOMIC_ADD_GLOBAL(&CDX(u+m, idx), CNM(idx, u));
        CNM(idx, u) = ZERO; /* reset */
      }
#     endif
#   endif
# endif
    }
#endif

#if (1 < BS)
# if defined(TRACK_C)
    if (c0 != c1)
# endif
# if (BM < SM || 1 != BN)
    { /* atomically commit C-tile to global memory */
      UNROLL(BN)
      for (int bn = 0; bn < BN; ++bn) {
        const int n = bn + n0;
#   if (SN % BN)
        if (n < SN)
#   endif
        { UNROLL_FORCE(BM)
          for (int bm = 0; bm < BM; ++bm) {
            const int m = bm + m0;
#   if (SM % BM)
            if (m < SM)
#   endif
            {
#   if defined(SLM_C)
              const int im = m, in = n;
#   else
              const int im = bm, in = bn;
#   endif
#   if defined(ATOMIC_INC_NZ)
              if (ZERO != CNM(in, im)) {
#   endif
                ATOMIC_ADD_GLOBAL(&CDX(m, n), CNM(in, im));
                CNM(in, im) = ZERO; /* reset */
#   if defined(ATOMIC_INC_NZ)
              }
#   endif
            }
          }
        }
      }
# else
    { /* atomically commit C-column to global memory */
      int m = 0;
#   if defined(ATOMIC_ADD2_GLOBAL)
#     if defined(ATOMIC_INC_NZ)
      for (; m < (SM - 1); m += 2) if (ZERO != CNM(idx, m) && ZERO != CNM(idx, m+1)) {
#     else
      for (; m < (SM - 1); m += 2) {
#     endif
        const float2 r2 = (float2)(CNM(idx, m), CNM(idx, m+1));
        ATOMIC_ADD2_GLOBAL(&CDX(m, idx), r2);
        CNM(idx, m) = CNM(idx, m+1) = ZERO; /* reset */
      }
#   endif
#   if !defined(ATOMIC_ADD2_GLOBAL) || (SM & 1)
#     if defined(ATOMIC_INC_NZ)
      for (; m < SM; ++m) if (ZERO != CNM(idx, m)) {
#     else
      for (; m < SM; ++m) {
#     endif
        ATOMIC_ADD_GLOBAL(&CDX(m, idx), CNM(idx, m));
        CNM(idx, m) = ZERO; /* reset */
      }
#   endif
# endif
      /* next iteration */
      c0 = c1;
    }
#endif
  }
}
