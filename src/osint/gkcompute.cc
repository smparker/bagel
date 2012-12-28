//
// BAGEL - Parallel electron correlation program.
// Filename: gkcompute.cc
// Copyright (C) 2012 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and\/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
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


#include <stddef.h>
#include <src/osint/gkineticbatch.h>
#include <src/rysint/carsphlist.h>
#include <src/util/comb.h>

using namespace std;
using namespace bagel;

const static Comb comb;

void GKineticBatch::compute() {

  fill(data_, data_+size_alloc_, 0.0);

  const int a = basisinfo_[0]->angular_number();
  const int b = basisinfo_[1]->angular_number();
  const int a2 = a+1+deriv_rank_;
  const int b2 = b+1+deriv_rank_;
  assert(amax_ == a+b+deriv_rank_);
  const int acsize = (a+1)*(a+2)*(b+1)*(b+2)/4;
  const size_t acpsize = acsize*prim0_*prim1_;
  assert(size_alloc_ == acpsize*6);

  double* const transx = stack_->get((amax_+1)*a2*b2);
  double* const transy = stack_->get((amax_+1)*a2*b2);
  double* const transz = stack_->get((amax_+1)*a2*b2);
  fill(transx, transx+(amax_+1)*a2*b2, 0.0);
  fill(transy, transy+(amax_+1)*a2*b2, 0.0);
  fill(transz, transz+(amax_+1)*a2*b2, 0.0);
  for (int ib = 0, k = 0; ib <= b+deriv_rank_; ++ib) {
    for (int ia = 0; ia <= a+deriv_rank_; ++ia, ++k) {
      if (ia > a && ib > b) continue;
      for (int i = ia; i <= ia+ib; ++i) {
        transx[i + (amax_+1)*k] = comb.c(ib, ia+ib-i) * pow(AB_[0], ia+ib-i);
        transy[i + (amax_+1)*k] = comb.c(ib, ia+ib-i) * pow(AB_[1], ia+ib-i);
        transz[i + (amax_+1)*k] = comb.c(ib, ia+ib-i) * pow(AB_[2], ia+ib-i);
      }
    }
  }
  const int worksize = amax_+1;
  double* const workx = stack_->get(worksize);
  double* const worky = stack_->get(worksize);
  double* const workz = stack_->get(worksize);

  double* const bufx = stack_->get(a2*b2);
  double* const bufy = stack_->get(a2*b2);
  double* const bufz = stack_->get(a2*b2);
  double* const bufx_a = stack_->get(a2*b2*deriv_rank_);
  double* const bufx_b = stack_->get(a2*b2*deriv_rank_);
  double* const bufy_a = stack_->get(a2*b2*deriv_rank_);
  double* const bufy_b = stack_->get(a2*b2*deriv_rank_);
  double* const bufz_a = stack_->get(a2*b2*deriv_rank_);
  double* const bufz_b = stack_->get(a2*b2*deriv_rank_);

  // Perform VRR
  for (int ii = 0; ii != prim0_ * prim1_; ++ii) {

    /// Sx(0 : i+j+1, 0) will be made here
    workx[0] = coeffsx_[ii];
    worky[0] = coeffsy_[ii];
    workz[0] = coeffsz_[ii];
    workx[1] = (p_[ii * 3    ] - basisinfo_[0]->position(0)) * workx[0];
    worky[1] = (p_[ii * 3 + 1] - basisinfo_[0]->position(1)) * worky[0];
    workz[1] = (p_[ii * 3 + 2] - basisinfo_[0]->position(2)) * workz[0];
    for (int i = 2; i <= amax_; ++i) {
      workx[i] = (p_[ii * 3    ] - basisinfo_[0]->position(0)) * workx[i - 1] + 0.5 * (i - 1) / xp_[ii] * workx[i - 2];
      worky[i] = (p_[ii * 3 + 1] - basisinfo_[0]->position(1)) * worky[i - 1] + 0.5 * (i - 1) / xp_[ii] * worky[i - 2];
      workz[i] = (p_[ii * 3 + 2] - basisinfo_[0]->position(2)) * workz[i - 1] + 0.5 * (i - 1) / xp_[ii] * workz[i - 2];
    }
    // HRR is done in one shot
    dgemv_("T", amax_+1, a2*b2, 1.0, transx, amax_+1, workx, 1, 0.0, bufx, 1);
    dgemv_("T", amax_+1, a2*b2, 1.0, transy, amax_+1, worky, 1, 0.0, bufy, 1);
    dgemv_("T", amax_+1, a2*b2, 1.0, transz, amax_+1, workz, 1, 0.0, bufz, 1);


    // TODO USE translational invariance!!
    const double alpha = xa_[ii];
    const double beta_ = xb_[ii];
    const double* tmpx = bufx;
    const double* tmpy = bufy;
    const double* tmpz = bufz;
    for (int i = 0; i != deriv_rank_; ++i) {
      const int rem = deriv_rank_-i-1;
      for (int ib = 0; ib <= b+rem; ++ib) {
        for (int ia = 0; ia <= a+rem; ++ia) {
          bufx_a[ia+a2*ib+a2*b2*i] = 2.0*alpha*tmpx[ia+1+a2*(ib)] - ia*tmpx[ia-1+a2*(ib)];
          bufy_a[ia+a2*ib+a2*b2*i] = 2.0*alpha*tmpy[ia+1+a2*(ib)] - ia*tmpy[ia-1+a2*(ib)];
          bufz_a[ia+a2*ib+a2*b2*i] = 2.0*alpha*tmpz[ia+1+a2*(ib)] - ia*tmpz[ia-1+a2*(ib)];
        }
      }
      tmpx = bufx_a+a2*b2*i;
      tmpy = bufy_a+a2*b2*i;
      tmpz = bufz_a+a2*b2*i;
    }
    tmpx = bufx;
    tmpy = bufy;
    tmpz = bufz;
    for (int i = 0; i != deriv_rank_; ++i) {
      const int rem = deriv_rank_-i-1;
      for (int ib = 0; ib <= b+rem; ++ib) {
        for (int ia = 0; ia <= a+rem; ++ia) {
          bufx_b[ia+a2*ib+a2*b2*i] = 2.0*beta_*tmpx[ia+a2*(ib+1)] - ib*tmpx[ia+a2*(ib-1)];
          bufy_b[ia+a2*ib+a2*b2*i] = 2.0*beta_*tmpy[ia+a2*(ib+1)] - ib*tmpy[ia+a2*(ib-1)];
          bufz_b[ia+a2*ib+a2*b2*i] = 2.0*beta_*tmpz[ia+a2*(ib+1)] - ib*tmpz[ia+a2*(ib-1)];
        }
      }
      tmpx = bufx_b+a2*b2*i;
      tmpy = bufy_b+a2*b2*i;
      tmpz = bufz_b+a2*b2*i;
    }

    /// assembly process
    const int offset_ii = ii * acsize;
    double* current_data0 = data_ + offset_ii;
    double* current_data1 = data_ + offset_ii + acpsize;
    double* current_data2 = data_ + offset_ii + acpsize*2;
    double* current_data3 = data_ + offset_ii + acpsize*3;
    double* current_data4 = data_ + offset_ii + acpsize*4;
    double* current_data5 = data_ + offset_ii + acpsize*5;

    for (int iaz = 0; iaz <= a; ++iaz) {
      for (int iay = 0; iay <= a - iaz; ++iay) {
        const int iax = a - iaz - iay;
        for (int ibz = 0; ibz <= b; ++ibz) {
          for (int iby = 0; iby <= b - ibz; ++iby) {
            const int ibx = b - ibz - iby;

            *current_data0 += bufx_a[iax+a2*ibx+a2*b2*2] * bufy  [iay+a2*iby        ] * bufz  [iaz+a2*ibz        ]
                            + bufx_a[iax+a2*ibx        ] * bufy_a[iay+a2*iby+a2*b2*1] * bufz  [iaz+a2*ibz        ]
                            + bufx_a[iax+a2*ibx        ] * bufy  [iay+a2*iby        ] * bufz_a[iaz+a2*ibz+a2*b2*1];
            *current_data1 += bufx_a[iax+a2*ibx+a2*b2*1] * bufy_a[iay+a2*iby        ] * bufz  [iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy_a[iay+a2*iby+a2*b2*2] * bufz  [iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy_a[iay+a2*iby        ] * bufz_a[iaz+a2*ibz+a2*b2*1];
            *current_data2 += bufx_a[iax+a2*ibx+a2*b2*1] * bufy  [iay+a2*iby        ] * bufz_a[iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy_a[iay+a2*iby+a2*b2*1] * bufz_a[iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy  [iay+a2*iby        ] * bufz_a[iaz+a2*ibz+a2*b2*2];
            *current_data3 += bufx_b[iax+a2*ibx+a2*b2*2] * bufy  [iay+a2*iby        ] * bufz  [iaz+a2*ibz        ]
                            + bufx_b[iax+a2*ibx        ] * bufy_b[iay+a2*iby+a2*b2*1] * bufz  [iaz+a2*ibz        ]
                            + bufx_b[iax+a2*ibx        ] * bufy  [iay+a2*iby        ] * bufz_b[iaz+a2*ibz+a2*b2*1];
            *current_data4 += bufx_b[iax+a2*ibx+a2*b2*1] * bufy_b[iay+a2*iby        ] * bufz  [iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy_b[iay+a2*iby+a2*b2*2] * bufz  [iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy_b[iay+a2*iby        ] * bufz_b[iaz+a2*ibz+a2*b2*1];
            *current_data5 += bufx_b[iax+a2*ibx+a2*b2*1] * bufy  [iay+a2*iby        ] * bufz_b[iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy_b[iay+a2*iby+a2*b2*1] * bufz_b[iaz+a2*ibz        ]
                            + bufx  [iax+a2*ibx        ] * bufy  [iay+a2*iby        ] * bufz_b[iaz+a2*ibz+a2*b2*2];
            ++current_data0;
            ++current_data1;
            ++current_data2;
            ++current_data3;
            ++current_data4;
            ++current_data5;
          }
        }
      }
    }

  } // end of primsize loop

  stack_->release(a2*b2*deriv_rank_, bufz_b);
  stack_->release(a2*b2*deriv_rank_, bufz_a);
  stack_->release(a2*b2*deriv_rank_, bufy_b);
  stack_->release(a2*b2*deriv_rank_, bufy_a);
  stack_->release(a2*b2*deriv_rank_, bufx_b);
  stack_->release(a2*b2*deriv_rank_, bufx_a);

  stack_->release(a2*b2, bufz);
  stack_->release(a2*b2, bufy);
  stack_->release(a2*b2, bufx);

  stack_->release(worksize, workz);
  stack_->release(worksize, worky);
  stack_->release(worksize, workx);

  stack_->release((amax_+1)*a2*b2, transz);
  stack_->release((amax_+1)*a2*b2, transy);
  stack_->release((amax_+1)*a2*b2, transx);

  double* const bkup = stack_->get(acpsize);
  double* cdata = data_;
  for (int i = 0; i != 6; ++i, cdata += acpsize) {
    // first, contraction.
    const double* source = cdata;
    double* target = bkup;
    perform_contraction(acsize, source, prim0_, prim1_, target,
                        basisinfo_[0]->contractions(), basisinfo_[0]->contraction_ranges(), cont0_,
                        basisinfo_[1]->contractions(), basisinfo_[1]->contraction_ranges(), cont1_);

    if (spherical_) {
      struct CarSphList carsphlist;
      const unsigned int carsph_index = basisinfo_[0]->angular_number() * ANG_HRR_END + basisinfo_[1]->angular_number();
      const int nloops = cont0_ * cont1_;
      source = bkup;
      target = cdata;
      carsphlist.carsphfunc_call(carsph_index, nloops, source, target);

      const unsigned int sort_index = basisinfo_[1]->angular_number() * ANG_HRR_END + basisinfo_[0]->angular_number();
      source = cdata;
      target = bkup;
      sort_.sortfunc_call(sort_index, target, source, cont1_, cont0_, 1, swap01_);
      copy(bkup, bkup+acpsize, cdata);
    } else {
      const unsigned int sort_index = basisinfo_[1]->angular_number() * ANG_HRR_END + basisinfo_[0]->angular_number();
      source = bkup;
      target = cdata;
      sort_.sortfunc_call(sort_index, target, source, cont1_, cont0_, 1, swap01_);
    }
    // since this is a kinetic operator
    dscal_(cont0_*cont1_*acsize, -0.5, cdata, 1);
  }

  stack_->release(acpsize, bkup);

}
