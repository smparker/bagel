//
// BAGEL - Parallel electron correlation program.
// Filename: meh_compute_diagonal.hpp
// Copyright (C) 2013 Toru Shiozaki
//
// Author: Shane Parker <shane.parker@u.northwestern.edu>
// Maintainer: Shiozaki Group
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

#ifdef MEH_HEADERS

#ifndef BAGEL_MEH_COMPUTE_DIAGONAL_H
#define BAGEL_MEH_COMPUTE_DIAGONAL_H

template <class VecType>
std::shared_ptr<Matrix> MultiExcitonHamiltonian<VecType>::compute_intra(const DSubSpace& AB, std::shared_ptr<const DimerJop> jop, const double diag) {
  std::shared_ptr<const VecType> ccvecA = AB.template ci<0>();
  std::shared_ptr<const VecType> ccvecB = AB.template ci<1>();
  std::shared_ptr<const VecType> sigmavecA = form_sigma(ccvecA, jop->monomer_jop<0>());
  std::shared_ptr<const VecType> sigmavecB = form_sigma(ccvecB, jop->monomer_jop<1>());

  const int nstatesA = AB.template nstates<0>();
  const int nstatesB = AB.template nstates<1>();
  const int dimerstates = AB.dimerstates();

  auto out = std::make_shared<Matrix>(dimerstates, dimerstates);

  // first H^{AA}_{AA}
  for(int stateA = 0; stateA < nstatesA; ++stateA) {
    std::shared_ptr<const CiType> isigma = sigmavecA->data(stateA);
    for(int stateAp = 0; stateAp < stateA; ++stateAp) {
      std::shared_ptr<const CiType> icc = ccvecA->data(stateAp);
      const double dotproduct = isigma->dot_product(*icc);
      for(int stateB = 0; stateB < nstatesB; ++stateB) {
        const int stateApB = AB.dimerindex(stateAp, stateB);
        const int stateAB = AB.dimerindex(stateA, stateB);
        out->element(stateAB, stateApB) += dotproduct;
        out->element(stateApB, stateAB) += dotproduct;
      }
    }
    const double dotproduct = ccvecA->data(stateA)->dot_product(*isigma);
    for(int stateB = 0; stateB < nstatesB; ++stateB) {
      const int stateAB = AB.dimerindex(stateA, stateB);
      out->element(stateAB,stateAB) += dotproduct;
    }
  }

  // H^{BB}_{BB}
  for(int stateB = 0; stateB < nstatesB; ++stateB) {
    std::shared_ptr<const CiType> isigma = sigmavecB->data(stateB);
    for(int stateBp = 0; stateBp < stateB; ++stateBp) {
      std::shared_ptr<const CiType> icc = ccvecB->data(stateBp);
      const double dotproduct = isigma->dot_product(*icc);
      for(int stateA = 0; stateA < nstatesA; ++stateA) {
        const int stateAB = AB.dimerindex(stateA, stateB);
        const int stateABp = AB.dimerindex(stateA, stateBp);
        out->element(stateAB, stateABp) += dotproduct;
        out->element(stateABp, stateAB) += dotproduct;
      }
    }
    const double dotproduct = ccvecB->data(stateB)->dot_product(*isigma);
    for(int stateA = 0; stateA < nstatesA; ++stateA) {
      const int stateAB = AB.dimerindex(stateA, stateB);
      out->element(stateAB,stateAB) += dotproduct;
    }
  }

  out->add_diag(diag);

  return out;
}

template <class VecType>
std::shared_ptr<Matrix> MultiExcitonHamiltonian<VecType>::compute_diagonal_1e(const DSubSpace& AB, const double* hAA, const double* hBB, const double diag) const {
  std::shared_ptr<const VecType> ccvecA = AB.template ci<0>();
  std::shared_ptr<const VecType> ccvecB = AB.template ci<1>();
  std::shared_ptr<const VecType> sigmavecA = form_sigma_1e(ccvecA, hAA);
  std::shared_ptr<const VecType> sigmavecB = form_sigma_1e(ccvecB, hBB);

  const int nstatesA = AB.template nstates<0>();
  const int nstatesB = AB.template nstates<1>();
  const int dimerstates = AB.dimerstates();

  auto out = std::make_shared<Matrix>(dimerstates, dimerstates);

  // first H^{AA}_{AA}
  for(int stateA = 0; stateA < nstatesA; ++stateA) {
    std::shared_ptr<const CiType> isigma = sigmavecA->data(stateA);
    for(int stateAp = 0; stateAp < stateA; ++stateAp) {
      std::shared_ptr<const CiType> icc = ccvecA->data(stateAp);
      const double dotproduct = isigma->dot_product(*icc);
      for(int stateB = 0; stateB < nstatesB; ++stateB) {
        const int stateApB = AB.dimerindex(stateAp, stateB);
        const int stateAB = AB.dimerindex(stateA, stateB);
        out->element(stateAB, stateApB) += dotproduct;
        out->element(stateApB, stateAB) += dotproduct;
      }
    }
    const double dotproduct = isigma->dot_product(*ccvecA->data(stateA));
    for(int stateB = 0; stateB < nstatesB; ++stateB) {
      const int stateAB = AB.dimerindex(stateA, stateB);
      out->element(stateAB,stateAB) += dotproduct;
    }
  }

  // H^{BB}_{BB}
  for(int stateB = 0; stateB < nstatesB; ++stateB) {
    std::shared_ptr<const CiType> isigma = sigmavecB->data(stateB);
    for(int stateBp = 0; stateBp < stateB; ++stateBp) {
      std::shared_ptr<const CiType> icc = ccvecB->data(stateBp);
      const double dotproduct = isigma->dot_product(*icc);
      for(int stateA = 0; stateA < nstatesA; ++stateA) {
        const int stateAB = AB.dimerindex(stateA, stateB);
        const int stateABp = AB.dimerindex(stateA, stateBp);
        out->element(stateAB, stateABp) += dotproduct;
        out->element(stateABp, stateAB) += dotproduct;
      }
    }
    const double dotproduct = ccvecB->data(stateB)->dot_product(*isigma);
    for(int stateA = 0; stateA < nstatesA; ++stateA) {
      const int stateAB = AB.dimerindex(stateA, stateB);
      out->element(stateAB,stateAB) += dotproduct;
    }
  }

  out->add_diag(diag);

  return out;
}


template <class VecType>
std::shared_ptr<Matrix> MultiExcitonHamiltonian<VecType>::compute_diagonal_block(DSubSpace& subspace) {
  const double core = ref_->geom()->nuclear_repulsion() + jop_->core_energy();

  // Would be better to allocate here and then send to subprocesses
  std::shared_ptr<Matrix> out = compute_intra(subspace, jop_, core);
  *out += *compute_inter_2e(subspace, subspace);

  return out;
}

// This term will couple off-diagonal blocks since it has no delta functions involved
template <class VecType>
std::shared_ptr<Matrix> MultiExcitonHamiltonian<VecType>::compute_inter_2e(DSubSpace& AB, DSubSpace& ApBp) {
  const int nstatesA = AB.template nstates<0>();
  const int nstatesB = AB.template nstates<1>();
  const int nstates = nstatesA * nstatesB;

  const int nstatesAp = ApBp.template nstates<0>();
  const int nstatesBp = ApBp.template nstates<1>();
  const int nstatesp = nstatesAp * nstatesBp;

  // alpha-alpha
  Matrix gamma_AA_alpha = *gammaforest_->template get<0>(AB.offset(), ApBp.offset(), GammaSQ::AnnihilateAlpha, GammaSQ::CreateAlpha);
  Matrix gamma_BB_alpha = *gammaforest_->template get<1>(AB.offset(), ApBp.offset(), GammaSQ::AnnihilateAlpha, GammaSQ::CreateAlpha);

  // beta-beta
  Matrix gamma_AA_beta = *gammaforest_->template get<0>(AB.offset(), ApBp.offset(), GammaSQ::AnnihilateBeta, GammaSQ::CreateBeta);
  Matrix gamma_BB_beta = *gammaforest_->template get<1>(AB.offset(), ApBp.offset(), GammaSQ::AnnihilateBeta, GammaSQ::CreateBeta);

  // build J and K matrices
  std::shared_ptr<const Matrix> Jmatrix = jop_->coulomb_matrix<0,1,0,1>();
  std::shared_ptr<const Matrix> Kmatrix = jop_->coulomb_matrix<0,1,1,0>();

  Matrix tmp(nstatesA*nstatesAp, nstatesB*nstatesBp);

  tmp += (gamma_AA_alpha + gamma_AA_beta) * (*Jmatrix) ^ (gamma_BB_alpha + gamma_BB_beta);

  tmp -= gamma_AA_alpha * (*Kmatrix) ^ gamma_BB_alpha;
  tmp -= gamma_AA_beta * (*Kmatrix) ^ gamma_BB_beta;

  auto out = std::make_shared<Matrix>(nstates, nstatesp);
  reorder_matrix(tmp.data(), out->data(), nstatesA, nstatesAp, nstatesB, nstatesBp);

  return out;
}

#endif

#endif
