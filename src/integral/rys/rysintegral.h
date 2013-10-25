//
// BAGEL - Parallel electron correlation program.
// Filename: rysint.h
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// The BAGEL package is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the BAGEL package; see COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//


// Base class for the Rys-type integral evaluator - now a class template

#ifndef __SRC_INTEGRAL_RYS_RYSINTEGRAL_H
#define __SRC_INTEGRAL_RYS_RYSINTEGRAL_H

#include <tuple>
#include <typeinfo>
#include <complex>
#include <src/molecule/shell.h>
#include <src/util/constants.h>
#include <src/parallel/resources.h>
#include <src/integral/integral.h>
#include <src/integral/rys/inline.h>

namespace bagel {

 template <typename DataType>
// class RysIntegral : public Integral {
 class RysIntegral : public Integral_Base<DataType> {
  protected:
    // some basic info for integral evaluations
    bool swap01_, swap23_;
    bool swap0123_;
    std::array<double,3> AB_, CD_;
    int amapping_[ANG_VRR_END * ANG_VRR_END * ANG_VRR_END];
    int cmapping_[ANG_VRR_END * ANG_VRR_END * ANG_VRR_END];
    DataType *p_, *q_;
    DataType *xp_, *xq_, *coeff_, *coeffy_;
    DataType *T_, *U_;
    unsigned int contsize_, primsize_;
    size_t size_block_, size_alloc_;
    int prim0size_, prim1size_, prim2size_, prim3size_;
    int cont0size_, cont1size_, cont2size_, cont3size_;
    int asize_, csize_, amax_, amin_, cmax_, cmin_, amax1_, cmax1_;
    DataType *buff_;
    double *bkup_;

    std::array<std::shared_ptr<const Shell>,4> basisinfo_;
    bool spherical1_;
    bool spherical2_;

    // information on how many derivatives you take
    // 0 for ERI, 1 for gradients, etc. Set to 0 in the constructor, and will be
    // over written in the constructor of a derived class
    int deriv_rank_;
    int tenno_;
    int breit_;

    DataType *data_;
    DataType *data2_;
    unsigned int size_final_;

    /// info for Rys quadruture
    DataType *roots_;
    DataType *weights_;
    int rank_;

    // for screening
    int* screening_;
    int screening_size_;

    // init functions
    void set_swap_info(const bool swap_bra_ket = false) {
      // swap 01 indices when needed: Larger angular momentum function comes first
      if (basisinfo_[0]->angular_number() < basisinfo_[1]->angular_number() || basisinfo_[0]->angular_number() == 0) {
        std::swap(basisinfo_[0], basisinfo_[1]);
        swap01_ = true;
      } else {
        swap01_ = false;
      }
      // swap 23 indices when needed
      if (basisinfo_[2]->angular_number() < basisinfo_[3]->angular_number() || basisinfo_[2]->angular_number() == 0) {
        std::swap(basisinfo_[2], basisinfo_[3]);
        swap23_ = true;
      } else {
        swap23_ = false;
      }

      swap0123_ = false;
      if (swap_bra_ket) {
        if (!basisinfo_[0]->angular_number() && !basisinfo_[2]->angular_number()) {
          swap0123_ = true;
          std::tie(basisinfo_[0], basisinfo_[1], basisinfo_[2], basisinfo_[3], swap01_, swap23_)
            = std::make_tuple(basisinfo_[2], basisinfo_[3], basisinfo_[0], basisinfo_[1], swap23_, swap01_);
          std::swap(spherical1_, spherical2_);
        }
      }
    }

    void set_ab_cd() {
      const double ax = basisinfo_[0]->position(0);
      const double ay = basisinfo_[0]->position(1);
      const double az = basisinfo_[0]->position(2);
      const double bx = basisinfo_[1]->position(0);
      const double by = basisinfo_[1]->position(1);
      const double bz = basisinfo_[1]->position(2);
      const double cx = basisinfo_[2]->position(0);
      const double cy = basisinfo_[2]->position(1);
      const double cz = basisinfo_[2]->position(2);
      const double dx = basisinfo_[3]->position(0);
      const double dy = basisinfo_[3]->position(1);
      const double dz = basisinfo_[3]->position(2);

      AB_[0] = ax - bx;
      AB_[1] = ay - by;
      AB_[2] = az - bz;
      CD_[0] = cx - dx;
      CD_[1] = cy - dy;
      CD_[2] = cz - dz;
    }

    void set_prim_contsizes() {
      prim0size_ = basisinfo_[0]->num_primitive();
      prim1size_ = basisinfo_[1]->num_primitive();
      prim2size_ = basisinfo_[2]->num_primitive();
      prim3size_ = basisinfo_[3]->num_primitive();
      primsize_ = prim0size_ * prim1size_ * prim2size_ * prim3size_;
      cont0size_ = basisinfo_[0]->num_contracted();
      cont1size_ = basisinfo_[1]->num_contracted();
      cont2size_ = basisinfo_[2]->num_contracted();
      cont3size_ = basisinfo_[3]->num_contracted();
      contsize_ = cont0size_ * cont1size_ * cont2size_ * cont3size_;
    }

    std::tuple<int, int, int, int> set_angular_info() {
      const int ang0 = basisinfo_[0]->angular_number();
      const int ang1 = basisinfo_[1]->angular_number();
      const int ang2 = basisinfo_[2]->angular_number();
      const int ang3 = basisinfo_[3]->angular_number();
      rank_ = ceil(0.5 * (ang0 + ang1 + ang2 + ang3 + 1 + deriv_rank_ + tenno_ + breit_));
      assert(2 * rank_ >= ang0 + ang1 + ang2 + ang3 + 1 + deriv_rank_ + tenno_ + breit_);

      amax_ = ang0 + ang1 + deriv_rank_;
      cmax_ = ang2 + ang3 + deriv_rank_;
      amin_ = std::max(ang0 - deriv_rank_, 0);
      cmin_ = std::max(ang2 - deriv_rank_, 0);
      amax1_ = amax_ + 1;
      cmax1_ = cmax_ + 1;

      asize_ = 0;
      csize_ = 0;
      for (int i = amin_; i <= amax_; ++i) asize_ += (i + 1) * (i + 2) / 2;
      for (int i = cmin_; i <= cmax_; ++i) csize_ += (i + 1) * (i + 2) / 2;

      const int asize_final = (ang0 + 1) * (ang0 + 2) * (ang1 + 1) * (ang1 + 2) / 4;
      const int csize_final = (ang2 + 1) * (ang2 + 2) * (ang3 + 1) * (ang3 + 2) / 4;

      const int asize_final_sph = spherical1_ ? (2 * ang0 + 1) * (2 * ang1 + 1) : asize_final;
      const int csize_final_sph = spherical2_ ? (2 * ang2 + 1) * (2 * ang3 + 1) : csize_final;

      int cnt = 0;
      for (int i = cmin_; i <= cmax_; ++i) {
        for (int iz = 0; iz <= i; ++iz) {
          for (int iy = 0; iy <= i - iz; ++iy) {
            const int ix = i - iy - iz;
            if (ix >= 0)
              cmapping_[ix + cmax1_ * (iy + cmax1_ * iz)] = cnt++;
          }
        }
      }
      cnt = 0;
      for (int j = amin_; j <= amax_; ++j) {
        for (int jz = 0; jz <= j; ++jz) {
          for (int jy = 0; jy <= j - jz; ++jy) {
            const int jx = j - jy - jz;
            if (jx >= 0)
              amapping_[jx + amax1_ * (jy + amax1_ * jz)] = cnt++;
          }
        }
      }
      return std::make_tuple(asize_final, csize_final, asize_final_sph, csize_final_sph);
    }

    // virtual init functions. The default is for ERI, NAI and their derivatives.
    // should be overloaded in Slater-type integrals
    virtual void root_weight(const int ps) = 0;
    virtual void compute_ssss(const DataType thr) = 0;
    virtual void allocate_data(const int asize_final, const int csize_final, const int asize_final_sph, const int csize_final_sph) = 0;

    void allocate_arrays(const size_t ps) {
      size_allocated_ = tenno_ > 0 ? ((rank_ * 2 + 13) * ps) : ((rank_ * 2 + 11) * ps);

      buff_ = stack_->get(size_allocated_);  // stack_->get(size_alloc_) stack_->get((rank_ * 2 + 10) * ps)
      DataType* pointer = buff_;
      screening_ = (int*)pointer;
      pointer += ps;
      p_ = pointer;     pointer += ps * 3;
      q_ = pointer;     pointer += ps * 3;
      xp_ = pointer;    pointer += ps;
      xq_ = pointer;    pointer += ps;
      coeff_ = pointer; pointer += ps;
      T_ = pointer;     pointer += ps;
      roots_ = pointer; pointer += rank_ * ps;
      weights_ = pointer; pointer += rank_ * ps;
      if (tenno_) {
        coeffy_ = pointer;pointer += ps;
        U_ = pointer;     pointer += ps;
      }
    }

    size_t size_allocated_;

    // TODO Is there a way to avoid having adding the pointer to a MemResources object to this class?
//    MemResources<DataType>* resources_;

    // for deallocation
    DataType* stack_save_;
    DataType* stack_save2_;


    // contraction
    void perform_contraction_new_outer(const int nsize, const double* prim, const int pdim0, const int pdim1, double* cont,
                     const std::vector<std::vector<double>>& coeff0, const std::vector<int>& upper0, const std::vector<int>& lower0, const int cdim0,
                     const std::vector<std::vector<double>>& coeff1, const std::vector<int>& upper1, const std::vector<int>& lower1, const int cdim1) {
      const int worksize = nsize * pdim1;
      double* const work = stack_->get(worksize);
      double* current_cont = cont;

      for (int i = 0; i != cdim0; ++i) {
        const int begin0 = lower0[i];
        const int end0   = upper0[i];
        std::fill_n(work, worksize, 0.0);
        for (int j = begin0; j != end0; ++j)
          daxpy_(worksize, coeff0[i][j], &prim[j * worksize], 1, work, 1);

        for (int k = 0; k != cdim1; ++k, current_cont += nsize) {
          const int begin1 = lower1[k];
          const int end1   = upper1[k];
          std::fill_n(current_cont, nsize, 0.0);
          for (int j = begin1; j != end1; ++j)
            daxpy_(nsize, coeff1[k][j], &work[j * nsize], 1, current_cont, 1);
        }
      }

      stack_->release(worksize, work);
    }

    void perform_contraction_new_inner(const int nsize, const int ac, const double* prim, const int pdim0, const int pdim1, double* cont,
                     const std::vector<std::vector<double>>& coeff0, const std::vector<int>& upper0, const std::vector<int>& lower0, const int cdim0,
                     const std::vector<std::vector<double>>& coeff1, const std::vector<int>& upper1, const std::vector<int>& lower1, const int cdim1) {
      const int worksize = pdim1 * ac;
      double* const work = stack_->get(worksize);
      double* current_cont = cont;

      for (int n = 0; n != nsize; ++n) { // loop of cdim * cdim
        const double* current_prim = &prim[ac * pdim1 * pdim0 * n];

        for (int i = 0; i != cdim0; ++i) {

          const int begin0 = lower0[i];
          const int end0   = upper0[i];
          std::fill_n(work, worksize,  0.0);
          for (int j = begin0; j != end0; ++j)
            daxpy_(worksize, coeff0[i][j], &current_prim[j * worksize], 1, work, 1);

          for (int k = 0; k != cdim1; ++k, current_cont += ac) {
            const int begin1 = lower1[k];
            const int end1   = upper1[k];
            std::fill_n(current_cont, ac, 0.0);
            for (int j = begin1; j != end1; ++j) {
              daxpy_(ac, coeff1[k][j], &work[j * ac], 1, current_cont, 1);
            }
          }
        }
      }
      stack_->release(worksize, work);
    }

    // contraction for 1-e integrals
    void perform_contraction(const int asize, const double* prim, const int pdim0, const int pdim1, double* cont,
                               const std::vector<std::vector<double>>& coeff0, const std::vector<std::pair<int, int>>& ranges0, const int cdim0,
                               const std::vector<std::vector<double>>& coeff1, const std::vector<std::pair<int, int>>& ranges1, const int cdim1) {
      // transformation of index1
      const int worksize = pdim1 * asize;
      double* const work = stack_->get(worksize);

      for (int i = 0; i != cdim0; ++i) {
        const int begin0 = ranges0[i].first;
        const int end0   = ranges0[i].second;
        std::fill_n(work, worksize, 0.0);
        for (int j = begin0; j != end0; ++j)
          daxpy_(worksize, coeff0[i][j], &prim[j * worksize], 1, work, 1);

        for (int k = 0; k != cdim1; ++k, cont += asize) {
          const int begin1 = ranges1[k].first;
          const int end1   = ranges1[k].second;
          std::fill_n(cont, asize, 0.0);
          for (int j = begin1; j != end1; ++j) {
            daxpy_(asize, coeff1[k][j], &work[j * asize], 1, cont, 1);
          }
        }
      }
      stack_->release(worksize, work);
    }

    bool allocated_here_;
    std::shared_ptr<StackMemory<DataType>> stack_;

  public:

    RysIntegral(const std::array<std::shared_ptr<const Shell>,4>& info, std::shared_ptr<StackMemory<DataType>> stack)
     : basisinfo_(info), spherical1_(info[0]->spherical()), spherical2_(info[2]->spherical()), deriv_rank_(0), tenno_(0), breit_(0) {
      assert(spherical1_ == info[1]->spherical());
      assert(spherical2_ == info[3]->spherical());

      if (stack == nullptr) {
        stack_ = resources__->get();
        allocated_here_ = true;
      } else {
        stack_ = stack;
        allocated_here_ = false;
      }
    }

    RysIntegral(const std::array<std::shared_ptr<const Shell>,2>& info, std::shared_ptr<StackMemory<DataType>> stack)
     : spherical1_(info[0]->spherical()), spherical2_(spherical1_), deriv_rank_(0), tenno_(0), breit_(0) {
      auto dum = std::make_shared<const Shell>(spherical2_);
      basisinfo_ = {{ info[0], info[1], dum, dum }};

      if (stack == nullptr) {
        stack_ = resources__->get();
        allocated_here_ = true;
      } else {
        stack_ = stack;
        allocated_here_ = false;
      }
    }

    ~RysIntegral() {
      // TODO this is a little inconsistent
      // stack should be allocated in the constructor of this class

      stack_->release(size_allocated_, buff_);
      if (tenno_) stack_->release(size_alloc_, stack_save2_);
      stack_->release(size_alloc_, stack_save_);

      if (allocated_here_)
        resources__->release(stack_);
    }

    virtual void compute() = 0;

    /// retrieve a batch of integrals
    virtual DataType* data(const int i) override { assert(i == 0); return data_; }
    const DataType* data() const { return data_; }
    const DataType* data2() const { return data2_; }
    bool data2_exists() const { return data2_ != nullptr; }
    size_t data_size() const { return size_final_; }

    size_t size_block() const { return size_block_; }

    bool swap01() const { return swap01_; }
    bool swap23() const { return swap23_; }
    bool swap0123() const { return swap0123_; }
};

using RysInt = RysIntegral<double>;

}

#endif
