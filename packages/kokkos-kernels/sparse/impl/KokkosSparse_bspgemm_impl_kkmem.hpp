//@HEADER
// ************************************************************************
//
//                        Kokkos v. 4.0
//       Copyright (2022) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Part of Kokkos, under the Apache License v2.0 with LLVM Exceptions.
// See https://kokkos.org/LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//@HEADER

#define HASHSCALAR 107

#include "KokkosKernels_Utils.hpp"
#include "KokkosKernels_BlockHashmapAccumulator.hpp"

namespace KokkosSparse {

namespace Impl {

template <typename HandleType, typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
          typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_>
template <typename a_row_view_t, typename a_nnz_view_t, typename a_scalar_view_t, typename b_row_view_t,
          typename b_nnz_view_t, typename b_scalar_view_t, typename c_row_view_t, typename c_nnz_view_t,
          typename c_scalar_view_t, typename pool_memory_type>
struct KokkosBSPGEMM<HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_, b_lno_row_view_t_,
                     b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::PortableNumericCHASH {
  using BlockAccumulator =
      KokkosKernels::Experimental::BlockHashmapAccumulator<nnz_lno_t, nnz_lno_t, scalar_t,
                                                           KokkosKernels::Experimental::HashOpType::bitwiseAnd>;

  static constexpr auto scalarAlignPad =
      KokkosBSPGEMM<HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_, b_lno_row_view_t_,
                    b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::scalarAlignPad;
  nnz_lno_t numrows;
  nnz_lno_t block_dim;
  const nnz_lno_t block_size;
  size_t block_bytes;

  a_row_view_t row_mapA;
  a_nnz_view_t entriesA;
  a_scalar_view_t valuesA;

  b_row_view_t row_mapB;
  b_nnz_view_t entriesB;
  b_scalar_view_t valuesB;

  c_row_view_t rowmapC;
  c_nnz_view_t entriesC;
  c_scalar_view_t valuesC;

  nnz_lno_t *pEntriesC;
  scalar_t *pvaluesC;
  const size_t shared_memory_size;
  const int vector_size;
  pool_memory_type memory_space;

  // nnz_lno_t max_nnz;
  const nnz_lno_t pow2_hash_size;
  const nnz_lno_t max_nnz;
  const nnz_lno_t pow2_hash_func;
  const KokkosKernels::Impl::ExecSpaceType my_exec_space;

  const int unit_memory;  // begins, nexts, and keys. No need for vals yet.
  int team_size;
  int thread_memory;
  nnz_lno_t thread_shmem_key_size;
  nnz_lno_t thread_shared_memory_hash_func;
  nnz_lno_t thread_shmem_hash_size;

  nnz_lno_t team_shmem_key_size;
  nnz_lno_t team_shared_memory_hash_func;
  nnz_lno_t team_shmem_hash_size;

  nnz_lno_t team_cuckoo_key_size, team_cuckoo_hash_func;

  nnz_lno_t max_first_level_hash_size;
  row_lno_persistent_work_view_t flops_per_row;

  PortableNumericCHASH(nnz_lno_t block_dim_, nnz_lno_t m_, a_row_view_t row_mapA_, a_nnz_view_t entriesA_,
                       a_scalar_view_t valuesA_,

                       b_row_view_t row_mapB_, b_nnz_view_t entriesB_, b_scalar_view_t valuesB_,

                       c_row_view_t rowmapC_, c_nnz_view_t entriesC_, c_scalar_view_t valuesC_,
                       size_t shared_memory_size_, int vector_size_, pool_memory_type mpool_, nnz_lno_t min_hash_size,
                       nnz_lno_t max_nnz_, int team_size_, const KokkosKernels::Impl::ExecSpaceType my_exec_space_,
                       double first_level_cut_off, row_lno_persistent_work_view_t flops_per_row_,
                       bool KOKKOSKERNELS_VERBOSE_)
      : numrows(m_),
        block_dim(block_dim_),
        block_size(block_dim_ * block_dim_),
        block_bytes(sizeof(scalar_t) * block_dim * block_dim),
        row_mapA(row_mapA_),
        entriesA(entriesA_),
        valuesA(valuesA_),

        row_mapB(row_mapB_),
        entriesB(entriesB_),
        valuesB(valuesB_),

        rowmapC(rowmapC_),
        entriesC(entriesC_),
        valuesC(valuesC_),
        pEntriesC(entriesC_.data()),
        pvaluesC(valuesC_.data()),
        shared_memory_size(shared_memory_size_ / 8 * 8),
        vector_size(vector_size_),
        memory_space(mpool_),
        // max_nnz(),
        pow2_hash_size(min_hash_size),
        max_nnz(max_nnz_),
        pow2_hash_func(min_hash_size - 1),
        my_exec_space(my_exec_space_),
        unit_memory(sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) + block_bytes),
        team_size(team_size_),
        thread_memory((shared_memory_size / 8 / team_size_) * 8),
        thread_shmem_key_size(),
        thread_shared_memory_hash_func(),
        thread_shmem_hash_size(1),
        team_shmem_key_size(),
        team_shared_memory_hash_func(),
        team_shmem_hash_size(1),
        team_cuckoo_key_size(1),
        team_cuckoo_hash_func(1),
        max_first_level_hash_size(1),
        flops_per_row(flops_per_row_)

  {
    nnz_lno_t tmp_team_cuckoo_key_size =
        ((shared_memory_size - sizeof(nnz_lno_t) * 2) / (sizeof(nnz_lno_t) + block_bytes));

    while (team_cuckoo_key_size * 2 < tmp_team_cuckoo_key_size) team_cuckoo_key_size = team_cuckoo_key_size * 2;
    team_cuckoo_hash_func = team_cuckoo_key_size - 1;
    team_shmem_key_size   = ((shared_memory_size - sizeof(nnz_lno_t) * 4 - scalarAlignPad) / unit_memory);
    thread_shmem_key_size = ((thread_memory - sizeof(nnz_lno_t) * 4 - scalarAlignPad) / unit_memory);
    if (KOKKOSKERNELS_VERBOSE_) {
      std::cout << "\t\tPortableNumericCHASH -- sizeof(scalar_t): " << sizeof(scalar_t)
                << "  sizeof(nnz_lno_t): " << sizeof(nnz_lno_t) << "  team_size: " << team_size << std::endl;
      std::cout << "\t\tPortableNumericCHASH -- thread_memory:" << thread_memory << " unit_memory:" << unit_memory
                << " initial key size:" << thread_shmem_key_size << std::endl;
      std::cout << "\t\tPortableNumericCHASH -- team shared_memory:" << shared_memory_size
                << " unit_memory:" << unit_memory << " initial team key size:" << team_shmem_key_size << std::endl;
    }
    while (thread_shmem_hash_size * 2 <= thread_shmem_key_size) {
      thread_shmem_hash_size = thread_shmem_hash_size * 2;
    }
    while (team_shmem_hash_size * 2 <= team_shmem_key_size) {
      team_shmem_hash_size = team_shmem_hash_size * 2;
    }
    team_shared_memory_hash_func   = team_shmem_hash_size - 1;
    thread_shared_memory_hash_func = thread_shmem_hash_size - 1;
    team_shmem_key_size = team_shmem_key_size + ((team_shmem_key_size - team_shmem_hash_size) * sizeof(nnz_lno_t)) /
                                                    (sizeof(nnz_lno_t) * 2 + block_bytes);
    team_shmem_key_size = (team_shmem_key_size >> 1) << 1;

    thread_shmem_key_size =
        thread_shmem_key_size +
        ((thread_shmem_key_size - thread_shmem_hash_size) * sizeof(nnz_lno_t)) / (sizeof(nnz_lno_t) * 2 + block_bytes);
    thread_shmem_key_size = (thread_shmem_key_size >> 1) << 1;

    if (KOKKOSKERNELS_VERBOSE_) {
      std::cout << "\t\tPortableNumericCHASH -- thread_memory:" << thread_memory << " unit_memory:" << unit_memory
                << " resized key size:" << thread_shmem_key_size << std::endl;
      std::cout << "\t\tPortableNumericCHASH -- team shared_memory:" << shared_memory_size
                << " unit_memory:" << unit_memory << " resized team key size:" << team_shmem_key_size << std::endl;
    }

    max_first_level_hash_size = first_level_cut_off * team_cuckoo_key_size;
    if (KOKKOSKERNELS_VERBOSE_) {
      std::cout << "\t\tPortableNumericCHASH -- thread_memory:" << thread_memory << " unit_memory:" << unit_memory
                << " initial key size:" << thread_shmem_key_size << std::endl;
      std::cout << "\t\tPortableNumericCHASH -- team_memory:" << shared_memory_size << " unit_memory:" << unit_memory
                << " initial team key size:" << team_shmem_key_size << std::endl;
      std::cout << "\t\tPortableNumericCHASH -- adjusted hashsize:" << thread_shmem_hash_size
                << " thread_shmem_key_size:" << thread_shmem_key_size << std::endl;
      std::cout << "\t\tPortableNumericCHASH -- adjusted team hashsize:" << team_shmem_hash_size
                << " team_shmem_key_size:" << team_shmem_key_size << std::endl;
      std::cout << "\t\t  team_cuckoo_key_size:" << team_cuckoo_key_size
                << " team_cuckoo_hash_func:" << team_cuckoo_hash_func
                << " max_first_level_hash_size:" << max_first_level_hash_size << std::endl;
      std::cout << "\t\t  pow2_hash_size:" << pow2_hash_size << " pow2_hash_func:" << pow2_hash_func << std::endl;
    }
  }

  void set_team_size(int team_size_) {
    this->team_size     = team_size_;
    this->thread_memory = (shared_memory_size / 8 / team_size_) * 8;
  }

  KOKKOS_INLINE_FUNCTION
  size_t get_thread_id(const size_t row_index) const {
    switch (my_exec_space) {
      default: return row_index;
#if defined(KOKKOS_ENABLE_SERIAL)
      case KokkosKernels::Impl::Exec_SERIAL: return 0;
#endif
#if defined(KOKKOS_ENABLE_OPENMP)
      case KokkosKernels::Impl::Exec_OMP: return Kokkos::OpenMP::impl_hardware_thread_id();
#endif
#if defined(KOKKOS_ENABLE_THREADS)
      case KokkosKernels::Impl::Exec_THREADS: return Kokkos::Threads::impl_hardware_thread_id();
#endif
#if defined(KOKKOS_ENABLE_CUDA)
      case KokkosKernels::Impl::Exec_CUDA: return row_index;
#endif
#if defined(KOKKOS_ENABLE_HIP)
      case KokkosKernels::Impl::Exec_HIP: return row_index;
#endif
    }
  }

  // linear probing with tracking.
  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag4 &, const team_member_t &teamMember) const {
    const nnz_lno_t team_row_begin = teamMember.league_rank() * teamMember.team_size();
    const nnz_lno_t team_row_end   = KOKKOSKERNELS_MACRO_MIN(team_row_begin + teamMember.team_size(), numrows);

    volatile nnz_lno_t *tmp = NULL;
    size_t tid              = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL) {
      tmp = (volatile nnz_lno_t *)(memory_space.allocate_chunk(tid));
    }

    nnz_lno_t *used_indices = (nnz_lno_t *)(tmp);
    tmp += max_nnz;
    nnz_lno_t *hash_ids = (nnz_lno_t *)(tmp);
    tmp += pow2_hash_size;

    scalar_t *hash_values = KokkosKernels::Impl::alignPtrTo<scalar_t>(tmp);

    BlockAccumulator hm(block_dim, pow2_hash_size, pow2_hash_func, nullptr, nullptr, hash_ids, hash_values);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end),
                         [&](const nnz_lno_t &row_index) {
                           nnz_lno_t used_count = 0;

                           const size_type col_begin = row_mapA[row_index];
                           const nnz_lno_t left_work = row_mapA[row_index + 1] - col_begin;
                           for (nnz_lno_t ii = 0; ii < left_work; ++ii) {
                             size_type a_col      = col_begin + ii;
                             nnz_lno_t rowB       = entriesA[a_col];
                             const scalar_t *valA = valuesA.data() + a_col * block_size;

                             size_type rowBegin   = row_mapB(rowB);
                             nnz_lno_t left_workB = row_mapB(rowB + 1) - rowBegin;

                             for (nnz_lno_t i = 0; i < left_workB; ++i) {
                               const size_type adjind = i + rowBegin;
                               nnz_lno_t b_col_ind    = entriesB[adjind];
                               const scalar_t *valB   = valuesB.data() + adjind * block_size;

                               hm.sequential_insert_into_hash_simple(b_col_ind, valA, valB, used_count, used_indices);
                             }
                           }
                           size_type c_row_begin = rowmapC[row_index];
                           hm.sequential_export_values_simple(used_count, used_indices, pEntriesC + c_row_begin,
                                                              pvaluesC + c_row_begin * block_size);
                         });
    memory_space.release_chunk(used_indices);
  }

  // assumes that the vector lane is 1, as in cpus
  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag &, const team_member_t &teamMember) const {
    const nnz_lno_t team_row_begin = teamMember.league_rank() * teamMember.team_size();
    const nnz_lno_t team_row_end   = KOKKOSKERNELS_MACRO_MIN(team_row_begin + teamMember.team_size(), numrows);

    BlockAccumulator hm2(block_dim, pow2_hash_size, pow2_hash_func, nullptr, nullptr, nullptr, nullptr);

    volatile nnz_lno_t *tmp = NULL;
    size_t tid              = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL) {
      tmp = (volatile nnz_lno_t *)(memory_space.allocate_chunk(tid));
    }
    nnz_lno_t *globally_used_hash_indices = (nnz_lno_t *)tmp;
    tmp += pow2_hash_size;

    hm2.hash_begins = (nnz_lno_t *)(tmp);
    tmp += pow2_hash_size;
    hm2.hash_nexts = (nnz_lno_t *)(tmp);

    Kokkos::parallel_for(
        Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&](const nnz_lno_t &row_index) {
          nnz_lno_t globally_used_hash_count = 0;
          nnz_lno_t used_hash_sizes          = 0;

          const size_type c_row_begin = rowmapC[row_index];

          hm2.keys   = pEntriesC + c_row_begin;
          hm2.values = pvaluesC + c_row_begin * block_size;

          const size_type col_begin = row_mapA[row_index];
          const nnz_lno_t left_work = row_mapA[row_index + 1] - col_begin;

          for (nnz_lno_t ii = 0; ii < left_work; ++ii) {
            size_type a_col       = col_begin + ii;
            nnz_lno_t rowB        = entriesA[a_col];
            const scalar_t *a_val = valuesA.data() + a_col * block_size;

            size_type rowBegin   = row_mapB(rowB);
            nnz_lno_t left_workB = row_mapB(rowB + 1) - rowBegin;

            for (nnz_lno_t i = 0; i < left_workB; ++i) {
              const size_type adjind = i + rowBegin;
              nnz_lno_t b_col_ind    = entriesB[adjind];
              const scalar_t *b_val  = valuesB.data() + adjind * block_size;
              // nnz_lno_t hash = (b_col_ind * 107) & pow2_hash_func;

              // this has to be a success, we do not need to check for the
              // success. int insertion =
              hm2.sequential_insert_into_hash_mergeAdd_TrackHashes(
                  b_col_ind, a_val, b_val, &used_hash_sizes, &globally_used_hash_count, globally_used_hash_indices);
            }
          }
          for (nnz_lno_t i = 0; i < globally_used_hash_count; ++i) {
            nnz_lno_t dirty_hash        = globally_used_hash_indices[i];
            hm2.hash_begins[dirty_hash] = -1;
          }
        });
    memory_space.release_chunk(globally_used_hash_indices);
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const GPUTag &, const team_member_t &teamMember) const {
    nnz_lno_t team_row_begin     = teamMember.league_rank() * teamMember.team_size();
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + teamMember.team_size(), numrows);

    // int thread_memory = (shared_memory_size / 8 / teamMember.team_size()) *
    // 8;
    char *all_shared_memory = (char *)(teamMember.team_shmem().get_shmem(shared_memory_size));

    // shift it to the thread private part
    all_shared_memory += thread_memory * teamMember.team_rank();

    // used_hash_sizes hold the size of 1st and 2nd level hashes
    volatile nnz_lno_t *used_hash_sizes = (volatile nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;

    nnz_lno_t *globally_used_hash_count = (nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;

    // int unit_memory = sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) + sizeof
    // (scalar_t) ; //begins, nexts, keys and vals . nnz_lno_t shmem_key_size =
    // (thread_memory - sizeof(nnz_lno_t) * 4) / unit_memory; if (shmem_key_size
    // & 1) shmem_key_size -= 1;

    nnz_lno_t *begins = (nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * thread_shmem_hash_size;

    // points to the next elements
    nnz_lno_t *nexts = (nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * thread_shmem_key_size;

    // holds the keys
    nnz_lno_t *keys = (nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * thread_shmem_key_size;
    // remainder of shmem allocation for vals
    scalar_t *vals = KokkosKernels::Impl::alignPtrTo<scalar_t>(all_shared_memory);

    BlockAccumulator hm(block_dim, thread_shmem_key_size, thread_shared_memory_hash_func, begins, nexts, keys, vals);

    BlockAccumulator hm2(block_dim, pow2_hash_size, pow2_hash_func, nullptr, nullptr, nullptr, nullptr);
    Kokkos::parallel_for(
        Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&](const nnz_lno_t &row_index) {
          const size_type c_row_begin             = rowmapC[row_index];
          const size_type c_row_end               = rowmapC[row_index + 1];
          const nnz_lno_t global_memory_hash_size = nnz_lno_t(c_row_end - c_row_begin);

          bool is_global_alloced                = false;
          nnz_lno_t *globally_used_hash_indices = NULL;

          if (global_memory_hash_size > thread_shmem_key_size) {
            nnz_lno_t *tmp = NULL;
            // size_t tid = get_thread_id(row_index);
            // the code gets internal compiler error on gcc 4.7.2
            // assuming that this part only runs on GPUs for now, below fix
            // has the exact same behaviour and runs okay.
            size_t tid = row_index;

            while (tmp == NULL) {
              Kokkos::single(
                  Kokkos::PerThread(teamMember),
                  [&](nnz_lno_t *&memptr) { memptr = (nnz_lno_t *)(memory_space.allocate_chunk(tid)); }, tmp);
            }

            is_global_alloced          = true;
            globally_used_hash_indices = (nnz_lno_t *)tmp;
            tmp += pow2_hash_size;
            hm2.hash_begins = (nnz_lno_t *)(tmp);
            tmp += pow2_hash_size;
            hm2.hash_nexts = (nnz_lno_t *)(tmp);
          }
          hm2.keys   = pEntriesC + c_row_begin;
          hm2.values = pvaluesC + c_row_begin * block_size;

          // initialize begins.
          Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, thread_shmem_hash_size),
                               [&](nnz_lno_t i) { begins[i] = -1; });

          // initialize hash usage sizes
          Kokkos::single(Kokkos::PerThread(teamMember), [&]() {
            used_hash_sizes[0]          = 0;
            used_hash_sizes[1]          = 0;
            globally_used_hash_count[0] = 0;
          });

          const size_type col_begin = row_mapA[row_index];
          const nnz_lno_t left_work = row_mapA[row_index + 1] - col_begin;
          nnz_lno_t ii              = left_work;
          // for ( nnz_lno_t ii = 0; ii < left_work; ++ii){
          while (ii-- > 0) {
            size_type a_col      = col_begin + ii;
            nnz_lno_t rowB       = entriesA[a_col];
            const scalar_t *valA = valuesA.data() + a_col * block_size;

            size_type rowBegin   = row_mapB(rowB);
            nnz_lno_t left_work_ = row_mapB(rowB + 1) - rowBegin;
            Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, left_work_), [&](nnz_lno_t i) {
              const size_type adjind = i + rowBegin;
              nnz_lno_t b_col_ind    = entriesB[adjind];
              const scalar_t *valB   = valuesB.data() + adjind * block_size;
              volatile int num_unsuccess =
                  hm.vector_atomic_insert_into_hash_mergeAdd(b_col_ind, valA, valB, used_hash_sizes);
              if (num_unsuccess) {
                hm2.vector_atomic_insert_into_hash_mergeAdd_TrackHashes(
                    b_col_ind, valA, valB, used_hash_sizes + 1, globally_used_hash_count, globally_used_hash_indices);
              }
            });
          }

          if (is_global_alloced) {
            nnz_lno_t dirty_hashes = globally_used_hash_count[0];
            Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, dirty_hashes), [&](nnz_lno_t i) {
              nnz_lno_t dirty_hash        = globally_used_hash_indices[i];
              hm2.hash_begins[dirty_hash] = -1;
            });

            Kokkos::single(Kokkos::PerThread(teamMember),
                           [&]() { memory_space.release_chunk(globally_used_hash_indices); });
          }

          Kokkos::single(Kokkos::PerThread(teamMember), [&]() {
            if (used_hash_sizes[0] > thread_shmem_key_size) used_hash_sizes[0] = thread_shmem_key_size;
          });

          nnz_lno_t num_elements = used_hash_sizes[0];

          nnz_lno_t written_index = used_hash_sizes[1];
          Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, num_elements), [&](nnz_lno_t i) {
            const auto idx = c_row_begin + written_index + i;
            pEntriesC[idx] = keys[i];
            kk_block_set(block_dim, pvaluesC + idx * block_size, vals + i * block_size);
          });
        });
  }

  // one row does not fit into shmem, with thread-flat-parallel
  KOKKOS_INLINE_FUNCTION
  void operator()(const GPUTag6 &, const team_member_t &teamMember) const {
    nnz_lno_t team_row_begin     = teamMember.league_rank() * teamMember.team_size();
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + teamMember.team_size(), numrows);
    char *all_shared_memory      = (char *)(teamMember.team_shmem().get_shmem(shared_memory_size));

    // shmem == sizeof(nnz_lno_t)*2 + sizeof(nnz_lno_t)*team_cuckoo_key_size +
    // sizeof(scalar_t)*nvals
    const nnz_lno_t init_value          = -1;
    volatile nnz_lno_t *used_hash_sizes = (volatile nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;
    // holds the keys
    nnz_lno_t *keys = (nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * team_cuckoo_key_size;
    scalar_t *vals = KokkosKernels::Impl::alignPtrTo<scalar_t>(all_shared_memory);

    int thread_rank = teamMember.team_rank();

    int vector_rank = 0;
    typedef typename std::remove_reference<decltype(*used_hash_sizes)>::type atomic_incr_type;
    Kokkos::parallel_scan(Kokkos::ThreadVectorRange(teamMember, vector_size),
                          [&](const int /* threadid */, int &update, const bool final) {
                            if (final) {
                              vector_rank = update;
                            }
                            update += 1;
                          });
    int bs           = vector_size * team_size;
    int vector_shift = thread_rank * vector_size + vector_rank;

    for (nnz_lno_t row_index = team_row_begin; row_index < team_row_end; ++row_index) {
      if (row_mapA[row_index] == row_mapA[row_index + 1])  // skip empty A rows
        continue;
#if 1
      teamMember.team_barrier();
#endif
      const size_type c_row_begin    = rowmapC[row_index];
      const size_type c_row_end      = rowmapC[row_index + 1];
      const nnz_lno_t c_row_size     = c_row_end - c_row_begin;
      nnz_lno_t *c_row               = entriesC.data() + c_row_begin;
      scalar_t *c_row_vals           = valuesC.data() + c_row_begin * block_size;
      nnz_lno_t *global_acc_row_keys = c_row;
      scalar_t *global_acc_row_vals  = c_row_vals;
      nnz_lno_t *tmp                 = NULL;

      if (c_row_size > max_first_level_hash_size) {
        {
          while (tmp == NULL) {
            Kokkos::single(
                Kokkos::PerTeam(teamMember),
                [&](nnz_lno_t *&memptr) { memptr = (nnz_lno_t *)(memory_space.allocate_chunk(row_index)); }, tmp);
          }
          global_acc_row_keys = (nnz_lno_t *)(tmp);
          global_acc_row_vals = KokkosKernels::Impl::alignPtrTo<scalar_t>(tmp + pow2_hash_size);
        }
        // initialize begins.
        {
          nnz_lno_t num_threads = pow2_hash_size / vector_size;
          // not needed as team_cuckoo_key_size is always pow2. +
          // (team_cuckoo_key_size & (vector_size - 1)) * 1;
          Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, num_threads), [&](nnz_lno_t teamind) {
            Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, vector_size), [&](nnz_lno_t i) {
              const auto idx = teamind * vector_size + i;
              kk_block_init(block_dim, global_acc_row_vals + idx * block_size);
            });
          });
        }
      }

      // initialize begins.
      {
        nnz_lno_t num_threads = team_cuckoo_key_size / vector_size;
        // not needed as team_cuckoo_key_size is always pow2. +
        // (team_cuckoo_key_size & (vector_size - 1)) * 1;
        Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, num_threads), [&](nnz_lno_t teamind) {
          Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, vector_size), [&](nnz_lno_t i) {
            const auto idx = teamind * vector_size + i;
            keys[idx]      = init_value;
            kk_block_init(block_dim, vals + idx * block_size);
          });
        });
      }

      // initialize hash usage sizes
      Kokkos::single(Kokkos::PerTeam(teamMember), [&]() {
        used_hash_sizes[0] = 0;
        used_hash_sizes[1] = 0;
      });

      bool insert_is_on                  = true;
      const size_type a_col_begin_offset = row_mapA[row_index];

      nnz_lno_t a_col_ind   = entriesA[a_col_begin_offset];
      const scalar_t *a_val = valuesA.data() + a_col_begin_offset * block_size;

      nnz_lno_t current_a_column_offset_inrow = 0;
      nnz_lno_t flops_on_the_left_of_offsett  = 0;
      size_type current_b_read_offsett        = row_mapB[a_col_ind];
      nnz_lno_t current_a_column_flops        = row_mapB[a_col_ind + 1] - current_b_read_offsett;

      nnz_lno_t row_flops = flops_per_row(row_index);

#if 1
      teamMember.team_barrier();
#endif
      for (nnz_lno_t vector_read_shift = vector_shift; vector_read_shift < row_flops; vector_read_shift += bs) {
        {
          nnz_lno_t my_b_col_shift = vector_read_shift - flops_on_the_left_of_offsett;
          nnz_lno_t my_b_col       = init_value;
          nnz_lno_t hash           = init_value;
          int fail                 = 0;

          if (my_b_col_shift >= current_a_column_flops) {
            do {
              ++current_a_column_offset_inrow;
              my_b_col_shift -= current_a_column_flops;
              flops_on_the_left_of_offsett += current_a_column_flops;
              a_col_ind = entriesA[a_col_begin_offset + current_a_column_offset_inrow];

              current_b_read_offsett = row_mapB[a_col_ind];
              current_a_column_flops = row_mapB[a_col_ind + 1] - current_b_read_offsett;
            } while (my_b_col_shift >= current_a_column_flops);
            const auto idx = a_col_begin_offset + current_a_column_offset_inrow;
            a_val          = valuesA.data() + idx * block_size;
          }

          const auto idx        = my_b_col_shift + current_b_read_offsett;
          my_b_col              = entriesB[idx];
          const scalar_t *b_val = valuesB.data() + idx * block_size;
          // now insert it to first level hashmap accumulator.
          hash               = (my_b_col * HASHSCALAR) & team_cuckoo_hash_func;
          fail               = 1;
          bool try_to_insert = true;

          // nnz_lno_t max_tries = team_cuckoo_key_size;
          nnz_lno_t search_end = team_cuckoo_key_size;  // KOKKOSKERNELS_MACRO_MIN(team_cuckoo_key_size,
                                                        // hash + max_tries);
          for (nnz_lno_t trial = hash; trial < search_end;) {
            if (keys[trial] == my_b_col) {
              kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
              fail = 0;
              break;
            } else if (keys[trial] == init_value) {
              if (!insert_is_on) {
                try_to_insert = false;
                break;
              } else if (init_value == Kokkos::atomic_compare_exchange(keys + trial, init_value, my_b_col)) {
                kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
                Kokkos::atomic_inc(used_hash_sizes);
                if (used_hash_sizes[0] > max_first_level_hash_size) insert_is_on = false;
                fail = 0;
                break;
              }
            } else {
              ++trial;
            }
          }
          if (fail) {
            search_end = hash;  // max_tries - (team_cuckoo_key_size -  hash);

            for (nnz_lno_t trial = 0; try_to_insert && trial < search_end;) {
              if (keys[trial] == my_b_col) {
                kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
                fail = 0;
                break;
              } else if (keys[trial] == init_value) {
                if (!insert_is_on) {
                  break;
                } else if (init_value == Kokkos::atomic_compare_exchange(keys + trial, init_value, my_b_col)) {
                  kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
                  Kokkos::atomic_inc(used_hash_sizes);
                  if (used_hash_sizes[0] > max_first_level_hash_size) insert_is_on = false;
                  fail = 0;
                  break;
                }
              } else {
                ++trial;
              }
            }

            if (fail) {
              nnz_lno_t new_hash = (my_b_col * HASHSCALAR) & pow2_hash_func;

              for (nnz_lno_t trial = new_hash; trial < pow2_hash_size;) {
                if (global_acc_row_keys[trial] == my_b_col) {
                  kk_vector_block_add_mul(block_dim, global_acc_row_vals + trial * block_size, a_val, b_val);
                  // c_row_vals[trial] += my_b_val;
                  fail = 0;
                  break;
                } else if (global_acc_row_keys[trial] == init_value) {
                  if (init_value ==
                      Kokkos::atomic_compare_exchange(global_acc_row_keys + trial, init_value, my_b_col)) {
                    kk_vector_block_add_mul(block_dim, global_acc_row_vals + trial * block_size, a_val, b_val);
                    // Kokkos::atomic_inc(used_hash_sizes + 1);
                    // c_row_vals[trial] = my_b_val;
                    fail = 0;
                    break;
                  }
                } else {
                  ++trial;
                }
              }
              if (fail) {
                for (nnz_lno_t trial = 0; trial < new_hash;) {
                  if (global_acc_row_keys[trial] == my_b_col) {
                    // c_row_vals[trial] += my_b_val;
                    kk_vector_block_add_mul(block_dim, global_acc_row_vals + trial * block_size, a_val, b_val);
                    break;
                  } else if (global_acc_row_keys[trial] == init_value) {
                    if (init_value ==
                        Kokkos::atomic_compare_exchange(global_acc_row_keys + trial, init_value, my_b_col)) {
                      // Kokkos::atomic_inc(used_hash_sizes + 1);
                      kk_vector_block_add_mul(block_dim, global_acc_row_vals + trial * block_size, a_val, b_val);
                      // c_row_vals[trial] = my_b_val;
                      break;
                    }
                  } else {
                    ++trial;
                  }
                }
              }
            }
          }
        }
      }

      teamMember.team_barrier();

      if (tmp != NULL) {
        for (nnz_lno_t my_index = vector_shift; my_index < pow2_hash_size; my_index += bs) {
          nnz_lno_t my_b_col = global_acc_row_keys[my_index];
          if (my_b_col != init_value) {
            const scalar_t *b_val = global_acc_row_vals + my_index * block_size;
            int fail              = 1;
            {
              nnz_lno_t trial = (my_b_col * HASHSCALAR) & team_cuckoo_hash_func;
              for (nnz_lno_t max_tries = team_cuckoo_key_size; max_tries-- > 0;
                   trial               = (trial + 1) & team_cuckoo_hash_func) {
                if (keys[trial] == my_b_col) {
                  kk_block_add(block_dim, vals + trial * block_size, b_val);
                  fail = 0;
                  break;
                } else if (keys[trial] == init_value) {
                  break;
                }
              }
            }
            if (fail) {
              nnz_lno_t write_index = 0;
              write_index           = Kokkos::atomic_fetch_add(used_hash_sizes + 1, atomic_incr_type(1));
              c_row[write_index]    = my_b_col;
              kk_block_set(block_dim, c_row_vals + write_index * block_size, b_val);
            }
            global_acc_row_keys[my_index] = init_value;
          }
        }

        teamMember.team_barrier();
        Kokkos::single(Kokkos::PerTeam(teamMember), [&]() { memory_space.release_chunk(global_acc_row_keys); });
      }

      for (nnz_lno_t my_index = vector_shift; my_index < team_cuckoo_key_size; my_index += bs) {
        nnz_lno_t my_key = keys[my_index];
        if (my_key != init_value) {
          const scalar_t *my_val = vals + my_index * block_size;
          nnz_lno_t write_index  = 0;
          write_index            = Kokkos::atomic_fetch_add(used_hash_sizes + 1, atomic_incr_type(1));
          c_row[write_index]     = my_key;
          kk_block_set(block_dim, c_row_vals + write_index * block_size, my_val);
        }
      }
    }
  }

  // In this one row fits into shmem with team-flat-parallel
  KOKKOS_INLINE_FUNCTION
  void operator()(const GPUTag4 &, const team_member_t &teamMember) const {
    const nnz_lno_t init_value   = -1;
    nnz_lno_t team_row_begin     = teamMember.league_rank() * teamMember.team_size();
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + teamMember.team_size(), numrows);

    // shmem == sizeof(nnz_lno_t)*2 + sizeof(nnz_lno_t)*team_cuckoo_key_size +
    // sizeof(scalar_t)*nvals
    char *all_shared_memory = (char *)(teamMember.team_shmem().get_shmem(shared_memory_size));

    volatile nnz_lno_t *used_hash_sizes = (volatile nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;

    // holds the keys
    nnz_lno_t *keys = (nnz_lno_t *)(all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * team_cuckoo_key_size;
    scalar_t *vals = KokkosKernels::Impl::alignPtrTo<scalar_t>(all_shared_memory);

    int thread_rank = teamMember.team_rank();

    int vector_rank = 0;
    typedef typename std::remove_reference<decltype(*used_hash_sizes)>::type atomic_incr_type;
    Kokkos::parallel_scan(Kokkos::ThreadVectorRange(teamMember, vector_size),
                          [&](const int /* threadid */, int &update, const bool final) {
                            if (final) {
                              vector_rank = update;
                            }
                            update += 1;
                          });
    int bs           = vector_size * team_size;
    int vector_shift = thread_rank * vector_size + vector_rank;
    for (nnz_lno_t row_index = team_row_begin; row_index < team_row_end; ++row_index) {
      if (row_mapA[row_index] == row_mapA[row_index + 1])  // skip empty A rows
        continue;
#if 1
      teamMember.team_barrier();
#endif
      const size_type c_row_begin = rowmapC[row_index];
      // const size_type c_row_end = rowmapC[row_index + 1];
      // const nnz_lno_t c_row_size = c_row_end -  c_row_begin;
      nnz_lno_t *c_row     = entriesC.data() + c_row_begin;
      scalar_t *c_row_vals = valuesC.data() + c_row_begin * block_size;

      // initialize begins.
      {
        nnz_lno_t num_threads =
            team_cuckoo_key_size / vector_size;  // not needed as team_cuckoo_key_size is always pow2.
                                                 // + (team_cuckoo_key_size & (vector_size - 1)) * 1;
        Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, num_threads), [&](nnz_lno_t teamind) {
          // nnz_lno_t team_shift = teamind * vector_size;
          // nnz_lno_t work_to_handle = KOKKOSKERNELS_MACRO_MIN(vector_size,
          // team_shmem_hash_size - team_shift);
          Kokkos::parallel_for(Kokkos::ThreadVectorRange(teamMember, vector_size), [&](nnz_lno_t i) {
            const auto idx = teamind * vector_size + i;
            keys[idx]      = init_value;
            kk_block_init(block_dim, vals + idx * block_size);
          });
        });
      }

#if 0
      teamMember.team_barrier();

      Kokkos::single(Kokkos::PerTeam(teamMember),[&] () {

      for (int i = 0; i < team_shmem_hash_size; ++i){
    	  if (begins[i] != init_value){
    		  std::cout << "row_index:" << row_index << " i:" << i << " team_shmem_hash_size:" << team_shmem_hash_size << " is not init_value begins[i]:" << begins[i] << std::endl;
    	  }
      }
      });

      teamMember.team_barrier();
#endif
      // initialize hash usage sizes
      Kokkos::single(Kokkos::PerTeam(teamMember), [&]() {
        used_hash_sizes[0] = 0;
        used_hash_sizes[1] = 0;
#if 0
        globally_used_hash_count[0] = 0;
#endif
      });
#if 0

      teamMember.team_barrier();
#endif
#if 0
      bool is_global_alloced = false;
      nnz_lno_t *globally_used_hash_indices = NULL;
#endif
      const size_type a_col_begin_offset = row_mapA[row_index];

      nnz_lno_t a_col_ind   = entriesA[a_col_begin_offset];
      const scalar_t *a_val = valuesA.data() + a_col_begin_offset * block_size;

      nnz_lno_t current_a_column_offset_inrow = 0;
      nnz_lno_t flops_on_the_left_of_offsett  = 0;
      size_type current_b_read_offsett        = row_mapB[a_col_ind];
      nnz_lno_t current_a_column_flops        = row_mapB[a_col_ind + 1] - current_b_read_offsett;

      // nnz_lno_t ii = left_work;
      nnz_lno_t row_flops = flops_per_row(row_index);

#if 1
      teamMember.team_barrier();
#endif

      for (nnz_lno_t vector_read_shift = vector_shift; vector_read_shift < row_flops; vector_read_shift += bs) {
        {
          nnz_lno_t my_b_col_shift = vector_read_shift - flops_on_the_left_of_offsett;
          nnz_lno_t my_b_col       = init_value;
          nnz_lno_t hash           = init_value;
          int fail                 = 0;

          if (my_b_col_shift >= current_a_column_flops) {
            do {
              ++current_a_column_offset_inrow;
              my_b_col_shift -= current_a_column_flops;
              flops_on_the_left_of_offsett += current_a_column_flops;
              a_col_ind = entriesA[a_col_begin_offset + current_a_column_offset_inrow];

              current_b_read_offsett = row_mapB[a_col_ind];
              current_a_column_flops = row_mapB[a_col_ind + 1] - current_b_read_offsett;
            } while (my_b_col_shift >= current_a_column_flops);
            const auto idx = a_col_begin_offset + current_a_column_offset_inrow;
            a_val          = valuesA.data() + idx * block_size;
          }

          my_b_col = entriesB[my_b_col_shift + current_b_read_offsett];

          const auto idx        = my_b_col_shift + current_b_read_offsett;
          const scalar_t *b_val = valuesB.data() + idx * block_size;

          // now insert it to first level hashmap accumulator.
          hash = (my_b_col * HASHSCALAR) & team_cuckoo_hash_func;
          fail = 1;

          for (nnz_lno_t trial = hash; trial < team_cuckoo_key_size;) {
            if (keys[trial] == my_b_col) {
              kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
              fail = 0;
              break;
            } else if (keys[trial] == init_value) {
              if (init_value == Kokkos::atomic_compare_exchange(keys + trial, init_value, my_b_col)) {
                kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
                fail = 0;
                break;
              }
            } else {
              ++trial;
            }
          }
          if (fail) {
            for (nnz_lno_t trial = 0; trial < hash;) {
              if (keys[trial] == my_b_col) {
                kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
                fail = 0;
                break;
              } else if (keys[trial] == init_value) {
                if (init_value == Kokkos::atomic_compare_exchange(keys + trial, init_value, my_b_col)) {
                  kk_vector_block_add_mul(block_dim, vals + trial * block_size, a_val, b_val);
                  fail = 0;
                  break;
                }
              } else {
                ++trial;
              }
            }
          }
        }
      }

      teamMember.team_barrier();
      for (nnz_lno_t my_index = vector_shift; my_index < team_cuckoo_key_size; my_index += bs) {
        nnz_lno_t my_key = keys[my_index];
        if (my_key != init_value) {
          const scalar_t *my_val = vals + my_index * block_size;
          nnz_lno_t write_index  = Kokkos::atomic_fetch_add(used_hash_sizes, atomic_incr_type(1));
          c_row[write_index]     = my_key;
          kk_block_set(block_dim, c_row_vals + write_index * block_size, my_val);
        }
      }
    }
  }

  size_t team_shmem_size(int /* team_size */) const { return shared_memory_size; }
};

//
// * Notes on KokkosBSPGEMM_numeric_hash *
//
// Prior to this routine, KokkosBSPGEMM_numeric(...) was called
//
//   KokkosBSPGEMM_numeric(...) :
//     if (this->spgemm_algorithm == SPGEMM_KK || SPGEMM_KK_LP ==
//     this->spgemm_algorithm) :
//       call KokkosBSPGEMM_numeric_speed(...)
//     else:
//       call  KokkosBSPGEMM_numeric_hash(...)  (this code!)
//
//     * NOTE: KokkosBSPGEMM_numeric_hash2(...) is not called
//
//
// KokkosBSPGEMM_numeric_hash:
//
// Algorithm selection may be modified as follows
//
//   algorithm_to_run: initialized to spgemm_algorithm input to
//   KokkosBSPGEMM_numeric_hash
//     * spgemm_algorithm CANNOT be SPGEMM_KK_SPEED or SPGEMM_KK_DENSE
//
//  if (this->spgemm_algorithm == SPGEMM_KK || SPGEMM_KK_LP ==
//  this->spgemm_algorithm) :
//     if Cuda enabled :
//       1. perform shmem-size + partition computations (used by
//       HashMapAccumulator) and flop estimate
//       2. from results of 1. select from SPGEMM_KK_MEMORY_SPREADTEAM,
//       SPGEMM_KK_MEMORY_BIGSPREADTEAM, SPGEMM_KK_MEMORY
//          * Note: These shmem calculations are not passed along to the
//          PortableNumericCHASH functor used by kernels
//            TODO check the pre-shmem calculations and functor shmem
//            calculations consistent - pass shmem values to functor
//     else :
//       1. determine if problem is "dense"
//       2. if dense: call "this->KokkosBSPGEMM_numeric_speed"
//          else : no change from algorithm_to_run; that is algorithm_to_run ==
//          SPGEMM_KK || SPGEMM_KK_LP
//
//  else :
//     skip modification of input algorithm
//
//
//
// Algorithm type matching to kernel Tag:
//
//   Policy typedefs with tags found in: KokkosSparse_spgemm_impl.hpp
//
//  Cuda algorithm options:
//   (algorithm_to_run == SPGEMM_KK_MEMORY_SPREADTEAM) : gpu_team_policy4_t,
//   i.e. GPUTag4 (algorithm_to_run == SPGEMM_KK_MEMORY_BIGSPREADTEAM) :
//   gpu_team_policy6_t,  i.e. GPUTag6 (default == SPGEMM_KK_MEMORY) :
//   gpu_team_policy_t,  i.e. GPUTag
//
//  Non-Cuda host algorithm options:
//   SPGEMM_KK_LP:
//     (algorithm_to_run == SPGEMM_KK_LP + Dynamic) :
//     dynamic_multicore_team_policy4_t,  i.e. MultiCoreTag4 (algorithm_to_run
//     == SPGEMM_KK_LP + Static) :  dynamic_multicore_team_policy4_t //
//     typo/bug, should be multicore_team_policy4_t?
//   else SPGEMM::KKMEM
//     kernel label: "KOKKOSPARSE::SPGEMM::KKMEM::DYNAMIC" :
//     dynamic_multicore_team_policy_t,  i.e. MultiCoreTag kernel label:
//     "KOKKOSPARSE::SPGEMM::KKMEM::STATIC"  : multicore_team_policy_t,  i.e.
//     MultiCoreTag

template <typename HandleType, typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
          typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_>
template <typename c_row_view_t, typename c_lno_nnz_view_t, typename c_scalar_nnz_view_t>
void KokkosBSPGEMM<
    HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_, b_lno_row_view_t_, b_lno_nnz_view_t_,
    b_scalar_nnz_view_t_>::KokkosBSPGEMM_numeric_hash(c_row_view_t rowmapC_, c_lno_nnz_view_t entriesC_,
                                                      c_scalar_nnz_view_t valuesC_,
                                                      KokkosKernels::Impl::ExecSpaceType lcl_my_exec_space) {
  if (Base::KOKKOSKERNELS_VERBOSE) {
    std::cout << "\tHASH MODE" << std::endl;
  }
  KokkosSparse::SPGEMMAlgorithm algorithm_to_run = this->spgemm_algorithm;
  nnz_lno_t brows                                = Base::row_mapB.extent(0) - 1;
  size_type bnnz                                 = Base::valsB.extent(0);

  int suggested_vector_size = this->handle->get_suggested_vector_size(brows, bnnz);
  int suggested_team_size   = this->handle->get_suggested_team_size(suggested_vector_size);
  size_t shmem_size_to_use  = Base::shmem_size;

  row_lno_persistent_work_view_t flops_per_row = this->handle->get_spgemm_handle()->row_flops;
  size_t original_overall_flops                = this->handle->get_spgemm_handle()->original_overall_flops;
  nnz_lno_t max_nnz     = this->handle->get_spgemm_handle()->template get_max_result_nnz<c_row_view_t>(rowmapC_);
  size_type overall_nnz = this->handle->get_spgemm_handle()->get_c_nnz();

  typedef KokkosKernels::Impl::UniformMemoryPool<MyTempMemorySpace, nnz_lno_t> pool_memory_space;
  nnz_lno_t min_hash_size    = 1;
  size_t chunksize           = 1;
  double first_level_cut_off = this->handle->get_spgemm_handle()->get_first_level_hash_cut_off();
  int hash_scaler            = this->handle->get_spgemm_handle()->get_min_hash_size_scale();
  nnz_lno_t tmp_max_nnz      = max_nnz;

  if (hash_scaler == 0) {
    tmp_max_nnz = KOKKOSKERNELS_MACRO_MAX(max_nnz, nnz_lno_t(this->b_col_cnt / this->concurrency + 1));
  } else {
    tmp_max_nnz *= hash_scaler;
  }

  // START OF SHARED MEMORY SIZE CALCULATIONS
  // NOTE: the values computed here are not actually passed to functors
  // requiring shmem, the calculations here are used for algorithm selection
  const size_t block_bytes      = sizeof(scalar_t) * block_dim * block_dim;
  nnz_lno_t unit_memory         = sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) + block_bytes;
  nnz_lno_t team_shmem_key_size = ((shmem_size_to_use - sizeof(nnz_lno_t) * 4 - scalarAlignPad) / unit_memory);
  // alignment padding is per-thread for algorithms with per-thread hashmap
  nnz_lno_t thread_memory = ((shmem_size_to_use / suggested_team_size - scalarAlignPad) / 8) * 8;

  nnz_lno_t thread_shmem_key_size = ((thread_memory - sizeof(nnz_lno_t) * 4) / unit_memory);
  if (Base::KOKKOSKERNELS_VERBOSE) {
    std::cout << "\t\tinitial PortableNumericCHASH -- thread_memory:" << thread_memory << " unit_memory:" << unit_memory
              << " initial key size:" << thread_shmem_key_size << std::endl;
    std::cout << "\t\tinitial PortableNumericCHASH -- team_memory:" << shmem_size_to_use
              << " unit_memory:" << unit_memory << " initial team key size:" << team_shmem_key_size << std::endl;
  }
  nnz_lno_t thread_shmem_hash_size = 1;
  while (thread_shmem_hash_size * 2 <= thread_shmem_key_size) {
    thread_shmem_hash_size = thread_shmem_hash_size * 2;
  }
  nnz_lno_t team_shmem_hash_size = 1;
  while (team_shmem_hash_size * 2 <= team_shmem_key_size) {
    team_shmem_hash_size = team_shmem_hash_size * 2;
  }
  // nnz_lno_t team_shared_memory_hash_func = team_shmem_hash_size - 1;

  team_shmem_key_size = team_shmem_key_size + ((team_shmem_key_size - team_shmem_hash_size) * sizeof(nnz_lno_t)) /
                                                  (sizeof(nnz_lno_t) * 2 + block_bytes);
  team_shmem_key_size = (team_shmem_key_size >> 1) << 1;

  thread_shmem_key_size =
      thread_shmem_key_size +
      ((thread_shmem_key_size - thread_shmem_hash_size) * sizeof(nnz_lno_t)) / (sizeof(nnz_lno_t) * 2 + block_bytes);
  thread_shmem_key_size = (thread_shmem_key_size >> 1) << 1;

  // choose parameters
  if (this->spgemm_algorithm == SPGEMM_KK || SPGEMM_KK_LP == this->spgemm_algorithm) {
    if (KokkosKernels::Impl::is_gpu_exec_space_v<MyExecSpace>) {
      // then chose the best method and parameters.
      size_type average_row_nnz = 0;
      size_t average_row_flops  = 0;
      if (this->a_row_cnt > 0) {
        average_row_nnz   = overall_nnz / this->a_row_cnt;
        average_row_flops = original_overall_flops / this->a_row_cnt;
      }
      int vector_length_max = KokkosKernels::Impl::kk_get_max_vector_size<MyExecSpace>();
      // if we have very low flops per row, or our maximum number of nnz is
      // prett small, then we do row-base algorithm.
      if (SPGEMM_KK_LP != this->spgemm_algorithm &&
          (average_row_nnz < (size_type)vector_length_max || average_row_flops < 256)) {
        algorithm_to_run = SPGEMM_KK_MEMORY;
        // if (average_row_nnz / double (thread_shmem_key_size) > 1.5)
        while (average_row_nnz > size_type(thread_shmem_key_size) && suggested_vector_size < vector_length_max) {
          suggested_vector_size  = suggested_vector_size * 2;
          suggested_vector_size  = KOKKOSKERNELS_MACRO_MIN(vector_length_max, suggested_vector_size);
          suggested_team_size    = this->handle->get_suggested_team_size(suggested_vector_size);
          thread_memory          = (shmem_size_to_use / 8 / suggested_team_size) * 8;
          thread_shmem_key_size  = ((thread_memory - sizeof(nnz_lno_t) * 4) / unit_memory);
          thread_shmem_hash_size = 1;
          while (thread_shmem_hash_size * 2 <= thread_shmem_key_size) {
            thread_shmem_hash_size = thread_shmem_hash_size * 2;
          }
          thread_shmem_key_size =
              thread_shmem_key_size +
              ((thread_shmem_key_size - thread_shmem_hash_size) * sizeof(nnz_lno_t) - scalarAlignPad) /
                  (sizeof(nnz_lno_t) * 2 + block_bytes);
          thread_shmem_key_size = (thread_shmem_key_size >> 1) << 1;
        }

        if (Base::KOKKOSKERNELS_VERBOSE) {
          std::cout << "\t\t\tRunning KKMEM with suggested_vector_size:" << suggested_vector_size
                    << " suggested_team_size:" << suggested_team_size << std::endl;
        }
      } else {
        nnz_lno_t tmp_team_cuckoo_key_size =
            ((shmem_size_to_use - sizeof(nnz_lno_t) * 2 - scalarAlignPad) / (sizeof(nnz_lno_t) + block_bytes));
        int team_cuckoo_key_size = 1;
        while (team_cuckoo_key_size * 2 < tmp_team_cuckoo_key_size) team_cuckoo_key_size = team_cuckoo_key_size * 2;
        suggested_vector_size = vector_length_max;
        suggested_team_size   = this->handle->get_suggested_team_size(suggested_vector_size);
        algorithm_to_run      = SPGEMM_KK_MEMORY_BIGSPREADTEAM;
        while (average_row_nnz < team_cuckoo_key_size / 2 * (KOKKOSKERNELS_MACRO_MIN(first_level_cut_off + 0.05, 1))) {
          shmem_size_to_use = shmem_size_to_use / 2;
          tmp_team_cuckoo_key_size =
              ((shmem_size_to_use - sizeof(nnz_lno_t) * 2 - scalarAlignPad) / (sizeof(nnz_lno_t) + block_bytes));
          team_cuckoo_key_size = 1;
          while (team_cuckoo_key_size * 2 < tmp_team_cuckoo_key_size) team_cuckoo_key_size = team_cuckoo_key_size * 2;

          suggested_team_size = suggested_team_size / 2;
        }
        if (average_row_flops > size_t(2) * suggested_team_size * suggested_vector_size &&
            average_row_nnz >
                size_type(team_cuckoo_key_size) * (KOKKOSKERNELS_MACRO_MIN(first_level_cut_off + 0.05, 1))) {
          shmem_size_to_use = shmem_size_to_use * 2;
          tmp_team_cuckoo_key_size =
              ((shmem_size_to_use - sizeof(nnz_lno_t) * 2 - scalarAlignPad) / (sizeof(nnz_lno_t) + block_bytes));
          team_cuckoo_key_size = 1;
          while (team_cuckoo_key_size * 2 < tmp_team_cuckoo_key_size) team_cuckoo_key_size = team_cuckoo_key_size * 2;
          suggested_team_size = suggested_team_size * 2;
        }
#ifdef FIRSTPARAMS
        suggested_team_size = KOKKOSKERNELS_MACRO_MAX(4, suggested_team_size);
#else
        suggested_team_size = KOKKOSKERNELS_MACRO_MAX(2, suggested_team_size);
#endif
        if (max_nnz < team_cuckoo_key_size * KOKKOSKERNELS_MACRO_MIN(first_level_cut_off + 0.20, 1)) {
          algorithm_to_run = SPGEMM_KK_MEMORY_SPREADTEAM;
          if (Base::KOKKOSKERNELS_VERBOSE) {
            std::cout << "\t\t\tRunning SPGEMM_KK_MEMORY_SPREADTEAM with "
                         "suggested_vector_size:"
                      << suggested_vector_size << " suggested_team_size:" << suggested_team_size
                      << " shmem_size_to_use:" << shmem_size_to_use << std::endl;
          }
        } else {
          if (Base::KOKKOSKERNELS_VERBOSE) {
            std::cout << "\t\t\tRunning SPGEMM_KK_MEMORY_BIGSPREADTEAM with "
                         "suggested_vector_size:"
                      << suggested_vector_size << " suggested_team_size:" << suggested_team_size
                      << " shmem_size_to_use:" << shmem_size_to_use << std::endl;
          }
        }
      }
    } else {
      bool run_dense               = false;
      nnz_lno_t max_column_cut_off = this->handle->get_spgemm_handle()->MaxColDenseAcc;
      nnz_lno_t col_size           = this->b_col_cnt;
      if (col_size < max_column_cut_off) {
        run_dense = true;
        if (Base::KOKKOSKERNELS_VERBOSE) {
          std::cout << "\t\t\tRunning SPGEMM_KK_DENSE col_size:" << col_size
                    << " max_column_cut_off:" << max_column_cut_off << std::endl;
        }
      } else {
        // round up maxNumRoughNonzeros to closest power of 2.
        nnz_lno_t tmp_min_hash_size = 1;
        while (tmp_max_nnz > tmp_min_hash_size) {
          tmp_min_hash_size *= 4;
        }

        size_t kkmem_chunksize = tmp_min_hash_size;  // this is for used hash indices
        kkmem_chunksize += tmp_min_hash_size;        // this is for the hash begins
        kkmem_chunksize += max_nnz;                  // this is for hash nexts
        kkmem_chunksize        = kkmem_chunksize * sizeof(nnz_lno_t) + scalarAlignPad;
        size_t dense_chunksize = (col_size + col_size / block_bytes + 1) * block_bytes;

        if (kkmem_chunksize >= dense_chunksize * 0.5) {
          run_dense = true;
          if (Base::KOKKOSKERNELS_VERBOSE) {
            std::cout << "\t\t\tRunning SPGEMM_KK_SPEED kkmem_chunksize:" << kkmem_chunksize
                      << " dense_chunksize:" << dense_chunksize << std::endl;
          }
        } else {
          run_dense = false;
          if (Base::KOKKOSKERNELS_VERBOSE) {
            std::cout << "\t\t\tRunning SPGEMM_KK_MEMORY col_size:" << col_size
                      << " max_column_cut_off:" << max_column_cut_off << std::endl;
          }
        }
      }

      if (run_dense) {
        this->KokkosBSPGEMM_numeric_speed(rowmapC_, entriesC_, valuesC_, lcl_my_exec_space);
        return;
      }
    }
  }
  if (Base::KOKKOSKERNELS_VERBOSE) {
    std::cout << "\t\tPortableNumericCHASH -- adjusted hashsize:" << thread_shmem_hash_size
              << " thread_shmem_key_size:" << thread_shmem_key_size << std::endl;
    std::cout << "\t\tPortableNumericCHASH -- adjusted team hashsize:" << team_shmem_hash_size
              << " team_shmem_key_size:" << team_shmem_key_size << std::endl;
  }
  // END OF SHARED MEMORY SIZE CALCULATIONS

  // required memory for L2
  if (KokkosKernels::Impl::is_gpu_exec_space_v<typename HandleType::HandleExecSpace>) {
    if (algorithm_to_run == SPGEMM_KK_MEMORY_SPREADTEAM) {
      tmp_max_nnz = 1;
    } else if (algorithm_to_run == SPGEMM_KK_MEMORY_BIGSPREADTEAM) {
    } else if (algorithm_to_run == SPGEMM_KK_MEMORY_BIGTEAM || algorithm_to_run == SPGEMM_KK_MEMORY_TEAM) {
      // tmp_max_nnz -= team_shmem_key_size;
    } else {
      // tmp_max_nnz -= thread_shmem_key_size;
    }
  }

  // START SIZE CALCULATIONS FOR MEMORYPOOL
  if (algorithm_to_run == SPGEMM_KK_LP) {
    while (tmp_max_nnz > min_hash_size) {
      min_hash_size *= 4;
    }
    chunksize = min_hash_size;                                     // this is for used hash keys
    chunksize += max_nnz;                                          // this is for used hash keys
    chunksize += scalarAlignPad;                                   // for padding betwen keys and values
    chunksize += min_hash_size * block_bytes / sizeof(nnz_lno_t);  // this is for the hash values
  } else if (algorithm_to_run == SPGEMM_KK_MEMORY_BIGSPREADTEAM) {
    while (tmp_max_nnz > min_hash_size) {
      min_hash_size *= 2;  // try to keep it as low as possible because hashes
                           // are not tracked.
    }
    chunksize = min_hash_size;                                     // this is for used hash keys
    chunksize += scalarAlignPad;                                   // for padding between keys and values
    chunksize += min_hash_size * block_bytes / sizeof(nnz_lno_t);  // this is for the hash values
  } else {
    while (tmp_max_nnz > min_hash_size) {
      min_hash_size *= 4;
    }
    chunksize = min_hash_size;   // this is for used hash indices
    chunksize += min_hash_size;  // this is for the hash begins
    chunksize += max_nnz;        // this is for hash nexts
  }

  nnz_lno_t num_chunks = this->template compute_num_pool_chunks<pool_memory_space>(
      chunksize * sizeof(nnz_lno_t), this->concurrency / suggested_vector_size);

  // END SIZE CALCULATIONS FOR MEMORYPOOL

  if (this->KOKKOSKERNELS_VERBOSE) {
    std::cout << "\t\t max_nnz: " << max_nnz << " min_hash_size:" << min_hash_size
              << " concurrency:" << this->concurrency << " MyExecSpace().concurrency():" << MyExecSpace().concurrency()
              << " numchunks:" << num_chunks << std::endl;
  }

  KokkosKernels::Impl::PoolType my_pool_type = KokkosKernels::Impl::OneThread2OneChunk;

  if (KokkosKernels::Impl::is_gpu_exec_space_v<MyExecSpace>) {
    my_pool_type = KokkosKernels::Impl::ManyThread2OneChunk;
  }

  Kokkos::Timer timer1;
  pool_memory_space m_space(num_chunks, chunksize, -1, my_pool_type);
  MyExecSpace().fence();

  if (this->KOKKOSKERNELS_VERBOSE) {
    m_space.print_memory_pool();
    std::cout << "\t\tPool Alloc Time:" << timer1.seconds() << std::endl;
    std::cout << "\t\tPool Size(MB):" << sizeof(nnz_lno_t) * (num_chunks * chunksize) / 1024. / 1024. << std::endl;
  }

  PortableNumericCHASH<const_a_lno_row_view_t, const_a_lno_nnz_view_t, const_a_scalar_nnz_view_t,
                       const_b_lno_row_view_t, const_b_lno_nnz_view_t, const_b_scalar_nnz_view_t, c_row_view_t,
                       c_lno_nnz_view_t, c_scalar_nnz_view_t, pool_memory_space>
      sc(block_dim, this->a_row_cnt, Base::row_mapA, Base::entriesA, Base::valsA, Base::row_mapB, Base::entriesB,
         Base::valsB,

         rowmapC_, entriesC_, valuesC_, shmem_size_to_use, suggested_vector_size, m_space, min_hash_size, max_nnz,
         suggested_team_size,

         lcl_my_exec_space, first_level_cut_off, flops_per_row, this->KOKKOSKERNELS_VERBOSE);

  if (this->KOKKOSKERNELS_VERBOSE) {
    std::cout << "\t\tvector_size:" << suggested_vector_size << " suggested_team_size:" << suggested_team_size
              << std::endl;
  }
  timer1.reset();

  if (KokkosKernels::Impl::is_gpu_exec_space_v<MyExecSpace>) {
    if (algorithm_to_run == SPGEMM_KK_MEMORY_SPREADTEAM) {
      if (thread_shmem_key_size <= 0) {
        std::cout << "KokkosBSPGEMM_numeric_hash SPGEMM_KK_MEMORY_SPREADTEAM: "
                     "Insufficient shmem available for key for hash map "
                     "accumulator - Terminating"
                  << std::endl;
        std::cout << "    thread_shmem_key_size = " << thread_shmem_key_size << std::endl;
        throw std::runtime_error(
            " KokkosBSPGEMM_numeric_hash SPGEMM_KK_MEMORY_SPREADTEAM: "
            "Insufficient shmem available for key for hash map accumulator ");
      }
      int max_team_size = gpu_team_policy4_t(1, 1, suggested_vector_size).team_size_max(sc, Kokkos::ParallelForTag());
      int team_size     = std::min(suggested_team_size, max_team_size);
      sc.set_team_size(team_size);
      Kokkos::parallel_for(
          "KOKKOSPARSE::SPGEMM::SPGEMM_KK_MEMORY_SPREADTEAM",
          gpu_team_policy4_t((this->a_row_cnt + team_size - 1) / team_size, team_size, suggested_vector_size), sc);
      MyExecSpace().fence();

    } else if (algorithm_to_run == SPGEMM_KK_MEMORY_BIGSPREADTEAM) {
      if (thread_shmem_key_size <= 0) {
        std::cout << "KokkosBSPGEMM_numeric_hash "
                     "SPGEMM_KK_MEMORY_BIGSPREADTEAM: Insufficient shmem "
                     "available for key for hash map accumulator - Terminating"
                  << std::endl;
        std::cout << "    thread_shmem_key_size = " << thread_shmem_key_size << std::endl;
        throw std::runtime_error(
            " KokkosBSPGEMM_numeric_hash SPGEMM_KK_MEMORY_BIGSPREADTEAM: "
            "Insufficient shmem available for key for hash map accumulator ");
      }
      int max_team_size = gpu_team_policy6_t(1, 1, suggested_vector_size).team_size_max(sc, Kokkos::ParallelForTag());
      int team_size     = std::min(suggested_team_size, max_team_size);
      sc.set_team_size(team_size);
      Kokkos::parallel_for(
          "KOKKOSPARSE::SPGEMM::SPGEMM_KK_MEMORY_BIGSPREADTEAM",
          gpu_team_policy6_t((this->a_row_cnt + team_size - 1) / team_size, team_size, suggested_vector_size), sc);
    } else {
      if (team_shmem_key_size <= 0) {
        std::cout << "KokkosBSPGEMM_numeric_hash SPGEMM_KK_MEMORY: "
                     "Insufficient shmem "
                     "available for key for hash map accumulator - Terminating"
                  << std::endl;
        std::cout << "    team_shmem_key_size = " << team_shmem_key_size << std::endl;
        throw std::runtime_error(
            " KokkosBSPGEMM_numeric_hash SPGEMM_KK_MEMORY: Insufficient shmem "
            "available for key for hash map accumulator ");
      }
      int max_team_size = gpu_team_policy_t(1, 1, suggested_vector_size).team_size_max(sc, Kokkos::ParallelForTag());
      int team_size     = std::min(suggested_team_size, max_team_size);
      sc.set_team_size(team_size);
      Kokkos::parallel_for(
          "KOKKOSPARSE::SPGEMM::SPGEMM_KK_MEMORY",
          gpu_team_policy_t((this->a_row_cnt + team_size - 1) / team_size, team_size, suggested_vector_size), sc);
    }
    MyExecSpace().fence();
  } else {
    if (algorithm_to_run == SPGEMM_KK_LP) {
      if (Base::use_dynamic_schedule) {
        Kokkos::parallel_for(
            "KOKKOSPARSE::SPGEMM::SPGEMM_KK_LP::DYNAMIC",
            dynamic_multicore_team_policy4_t((this->a_row_cnt + suggested_team_size - 1) / suggested_team_size,
                                             suggested_team_size, suggested_vector_size),
            sc);
      } else {
        Kokkos::parallel_for("KOKKOSPARSE::SPGEMM::SPGEMM_KK_LP::STATIC",
                             multicore_team_policy4_t((this->a_row_cnt + suggested_team_size - 1) / suggested_team_size,
                                                      suggested_team_size, suggested_vector_size),
                             sc);
      }
    } else {
      if (Base::use_dynamic_schedule) {
        Kokkos::parallel_for(
            "KOKKOSPARSE::SPGEMM::KKMEM::DYNAMIC",
            dynamic_multicore_team_policy_t((this->a_row_cnt + suggested_team_size - 1) / suggested_team_size,
                                            suggested_team_size, suggested_vector_size),
            sc);
      } else {
        Kokkos::parallel_for("KOKKOSPARSE::SPGEMM::KKMEM::STATIC",
                             multicore_team_policy_t((this->a_row_cnt + suggested_team_size - 1) / suggested_team_size,
                                                     suggested_team_size, suggested_vector_size),
                             sc);
      }
    }
    MyExecSpace().fence();
  }

  if (this->KOKKOSKERNELS_VERBOSE) {
    std::cout << "\t\tNumeric TIME:" << timer1.seconds() << std::endl;
  }
}

}  // namespace Impl
}  // namespace KokkosSparse
