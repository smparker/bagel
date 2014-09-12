//
// BAGEL - Parallel electron correlation program.
// Filename: asd_dmrg/block_ops_2.cc
// Copyright (C) 2014 Toru Shiozaki
//
// Author: Shane Parker <shane.parker@u.northwestern.edu>
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

#include <src/asd_dmrg/block_operators.h>
#include <src/asd_dmrg/dmrg_block.h>
#include <src/smith/prim_op.h>

using namespace std;
using namespace bagel;

BlockOperators2::BlockOperators2(shared_ptr<const DMRG_Block2> blocks, shared_ptr<DimerJop> jop) : blocks_(blocks), jop_(jop) {
  const int norb = jop->nocc();
  Matrix mo1e = *jop->mo1e()->matrix();
  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb, norb, norb, norb), jop->mo2e()->storage());

  const int rasnorb = norb - (blocks->left_block()->norb() + blocks->right_block()->norb());
  const int lnorb = blocks->left_block()->norb();
  const int rnorb = blocks->right_block()->norb();

  { // make left jop to be used for left BlockOperators1
    const int leftsize = rasnorb+lnorb;
    auto left1e = make_shared<CSymMatrix>(mo1e.get_submatrix(0, 0, leftsize, leftsize));
    auto left2e = make_shared<Matrix>(leftsize*leftsize, leftsize*leftsize);
    auto low = {0, 0, 0, 0};
    auto up = {leftsize, leftsize, leftsize, leftsize};
    const btas::TensorView4<double> mo2eslice = btas::make_view(mo2e.range().slice(low, up), mo2e.storage());
    copy(mo2eslice.begin(), mo2eslice.end(), left2e->begin());
    auto leftjop = make_shared<DimerJop>(rasnorb, lnorb, left1e, left2e);
    left_ops_ = make_shared<BlockOperators1>(blocks->left_block(), leftjop);
  }

  { // make double block one to be used for intra BlockOperators1
    const int blocksize = lnorb + rnorb;
    auto block1e = make_shared<CSymMatrix>(mo1e.get_submatrix(rasnorb, rasnorb, blocksize, blocksize));
    auto block2e = make_shared<Matrix>(blocksize*blocksize, blocksize*blocksize);
    auto low = {rasnorb, rasnorb, rasnorb, rasnorb};
    auto up = {norb, norb, norb, norb};
    const btas::TensorView4<double> mo2eslice = btas::make_view(mo2e.range().slice(low, up), mo2e.storage());
    copy(mo2eslice.begin(), mo2eslice.end(), block2e->begin());
    auto intrajop = make_shared<DimerJop>(lnorb, rnorb, block1e, block2e);
    intra_ops_ = make_shared<BlockOperators1>(blocks->left_block(), intrajop);
  }

  { // make right jop to be used for right BlockOperators1
    const int rightsize = rasnorb + rnorb;
    auto right1efull = mo1e.resize(rightsize, rightsize); // keeps the rasnorb x rasnorb terms in there
    right1efull->copy_block(0, rasnorb, rasnorb, rnorb, mo1e.get_submatrix(0, rasnorb+lnorb, rasnorb, rnorb));
    right1efull->copy_block(rasnorb, 0, rnorb, rasnorb, mo1e.get_submatrix(rasnorb+lnorb, 0, rnorb, rasnorb));
    right1efull->copy_block(rasnorb, rasnorb, rnorb, rnorb, mo1e.get_submatrix(rasnorb+lnorb, rasnorb+lnorb, rnorb, rnorb));
    auto right1e = make_shared<CSymMatrix>(right1efull);

    auto right2e = make_shared<Matrix>(rightsize*rightsize, rightsize*rightsize);
    btas::TensorView4<double> right2eView = btas::make_view(btas::CRange<4>(rightsize, rightsize, rightsize, rightsize), right2e->storage());
    for (int i = 0; i < rightsize; ++i) {
      const int ii = i < rasnorb ? i : i + lnorb;
      for (int j = 0; j < rightsize; ++j) {
        const int jj = j < rasnorb ? j : j + lnorb;
        for (int k = 0; k < rightsize; ++k) {
          const int kk = k < rasnorb ? k : k + lnorb;
          for (int l = 0; l < rightsize; ++l) {
            const int ll = l < rasnorb ? l : l + lnorb;
            right2eView(l, k, j, i) = mo2e(ll, kk, jj, ii);
          }
        }
      }
    }
    auto rightjop = make_shared<DimerJop>(rasnorb, rnorb, right1e, right2e);
    right_ops_ = make_shared<BlockOperators1>(blocks->right_block(), rightjop);
  }
}

shared_ptr<Matrix> BlockOperators2::gamma_a(const BlockKey bk, int i) const { return nullptr; }
shared_ptr<Matrix> BlockOperators2::gamma_b(const BlockKey bk, int i) const { return nullptr; }

double BlockOperators2::D_a(const BlockKey bk, int brastate, int ketstate, int i, int j, int k) const {
  const DMRG::BlockPair source_pair = *find_if(blocks_->blockpairs(bk).begin(), blocks_->blockpairs(bk).end(), [ketstate] (const DMRG::BlockPair bp)
    { return (ketstate >= bp.offset && ketstate < bp.offset+bp.nstates()); } );

  const BlockKey target_key(bk.nelea+1, bk.neleb);
  const DMRG::BlockPair target_pair = *find_if(blocks_->blockpairs(target_key).begin(), blocks_->blockpairs(target_key).end(), [brastate] (const DMRG::BlockPair bp)
    { return (brastate >= bp.offset && brastate < bp.offset+bp.nstates()); } );

  const int left_ketstate = (ketstate - source_pair.offset) % source_pair.left.nstates;
  const int right_ketstate = (ketstate - source_pair.offset) / source_pair.left.nstates;

  const int left_brastate = (brastate - target_pair.offset) % target_pair.left.nstates;
  const int right_brastate = (brastate - target_pair.offset) / target_pair.left.nstates;

  double out = 0.0;

  if (source_pair.left.key() == target_pair.left.key()) {
    if (left_ketstate==left_brastate)
      out += right_ops_->D_a(source_pair.right.key(), right_brastate, right_ketstate, i, j, k);
  }
  else if (source_pair.right.key() == target_pair.right.key()) {
    if (right_ketstate==right_brastate)
      out += left_ops_->D_a(source_pair.left.key(), left_brastate, left_ketstate, i, j, k);
  }
  return out;
}

double BlockOperators2::D_b(const BlockKey bk, int brastate, int ketstate, int i, int j, int k) const {
  const DMRG::BlockPair source_pair = *find_if(blocks_->blockpairs(bk).begin(), blocks_->blockpairs(bk).end(), [ketstate] (const DMRG::BlockPair bp)
    { return (ketstate >= bp.offset && ketstate < bp.offset+bp.nstates()); } );

  const BlockKey target_key(bk.nelea, bk.neleb+1);
  const DMRG::BlockPair target_pair = *find_if(blocks_->blockpairs(target_key).begin(), blocks_->blockpairs(target_key).end(), [brastate] (const DMRG::BlockPair bp)
    { return (brastate >= bp.offset && brastate < bp.offset+bp.nstates()); } );

  const int left_ketstate = (ketstate - source_pair.offset) % source_pair.left.nstates;
  const int right_ketstate = (ketstate - source_pair.offset) / source_pair.left.nstates;

  const int left_brastate = (brastate - target_pair.offset) % target_pair.left.nstates;
  const int right_brastate = (brastate - target_pair.offset) / target_pair.left.nstates;

  double out = 0.0;

  if (source_pair.left.key() == target_pair.left.key()) {
    if (left_ketstate==left_brastate)
      out += right_ops_->D_b(source_pair.right.key(), right_brastate, right_ketstate, i, j, k);
  }
  else if (source_pair.right.key() == target_pair.right.key()) {
    if (right_ketstate==right_brastate)
      out += left_ops_->D_b(source_pair.left.key(), left_brastate, left_ketstate, i, j, k);
  }
  return out;
}

#define UNORDERED

// all of the offdiagonal blocks get an extra factor of two so I can call H = 0.5 * (H + H^t) at the end
shared_ptr<Matrix> BlockOperators2::ham(const BlockKey bk) const {
  const vector<DMRG::BlockPair>& block_pairs = blocks_->blockpairs(bk);

  BlockInfo binfo = blocks_->blockinfo(bk);

  auto out = make_shared<Matrix>(binfo.nstates, binfo.nstates);

  const int rnorb = blocks_->right_block()->norb();

  for (auto& source_pair : block_pairs) {
    { // diag parts
      // H_A (x) I_B + I_A (x) H_B
      shared_ptr<const Matrix> leftham = left_ops_->ham(source_pair.left);
      Matrix leftunit = *leftham->clone(); leftunit.unit();
      shared_ptr<const Matrix> rightham = right_ops_->ham(source_pair.right);
      Matrix rightunit = *rightham->clone(); rightunit.unit();

      Matrix diag = kronecker_product(false, rightunit, false, *leftham) + kronecker_product(false, *rightham, false, leftunit);

      // Q_aa (x) A^t   A
      const MatView Q_aa_view = intra_ops_->Q_aa_as_matview(source_pair.left.key());
      const MatView gamma_aa_view(btas::make_view(btas::CRange<2>(source_pair.right.nstates*source_pair.right.nstates, rnorb*rnorb),
                               blocks_->right_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({source_pair.right.key(),source_pair.right.key()}).data->storage()), false);

      const MatView Q_bb_view = intra_ops_->Q_bb_as_matview(source_pair.left.key());
      const MatView gamma_bb_view(btas::make_view(btas::CRange<2>(source_pair.right.nstates*source_pair.right.nstates, rnorb*rnorb),
                               blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({source_pair.right.key(),source_pair.right.key()}).data->storage()), false);

      Matrix ham_block = (Q_aa_view % gamma_aa_view) + (Q_bb_view % gamma_bb_view);

      SMITH::sort_indices<0,2,1,3,1,1,1,1>(ham_block.data(), diag.data(), source_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, source_pair.right.nstates);

      out->add_block(1.0, source_pair.offset, source_pair.offset, source_pair.nstates(), source_pair.nstates(), diag);
    }

    { // Q_ab (x) A^t   B
      const BlockKey left_target(bk.nelea-1, bk.neleb+1);
      const BlockKey right_target(bk.nelea+1, bk.neleb-1);

      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView Q_ab_view = intra_ops_->Q_ab_as_matview(source_pair.left.key());
        const MatView gamma_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb*rnorb),
                                 blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}).at({source_pair.right.key(),right_target}).data->storage()), false);

        // TODO maybe it would make more sense to reorder the block_ops in the first place?
#ifdef UNORDERED
        Matrix Qab(Q_ab_view);
        SMITH::sort_indices<0,2,1,1,1,0,1>(Q_ab_view.data(), Qab.data(), Qab.ndim(), rnorb, rnorb);

        Matrix ham_block = Qab % gamma_view;
#else
        Matrix ham_block = Q_ab_view % gamma_view;
#endif

        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
      }
    }

    { // P_aa (x) A^t A^t
      const BlockKey left_target(bk.nelea-2, bk.neleb  );
      const BlockKey right_target(bk.nelea+2, bk.neleb  );
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView P_aa_view = intra_ops_->P_aa_as_matview(source_pair.left.key());
        const MatView gamma_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb*rnorb),
                                 blocks_->right_block()->coupling({GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}).at({source_pair.right.key(),right_target}).data->storage()), false);

        Matrix ham_block = P_aa_view % gamma_view;

#ifdef UNORDERED
        SMITH::sort_indices<0,2,3,1,-2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
#else
        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
#endif
      }
    }

    { // P_bb (x) B^t B^t
      const BlockKey left_target(bk.nelea  , bk.neleb-2);
      const BlockKey right_target(bk.nelea  , bk.neleb+2);
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView P_bb_view = intra_ops_->P_bb_as_matview(source_pair.left.key());
        const MatView gamma_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb*rnorb),
                                 blocks_->right_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}).at({source_pair.right.key(),right_target}).data->storage()), false);

        Matrix ham_block = P_bb_view % gamma_view;

#ifdef UNORDERED
        SMITH::sort_indices<0,2,3,1,-2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
#else
        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
#endif
      }
    }

    { // P_ab (x) A^t B^t
      const BlockKey left_target(bk.nelea-1, bk.neleb-1);
      const BlockKey right_target(bk.nelea+1, bk.neleb+1);
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView P_ab_view = intra_ops_->P_ab_as_matview(source_pair.left.key());
        const MatView gamma_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb*rnorb),
                                 blocks_->right_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}).at({source_pair.right.key(),right_target}).data->storage()), false);

#ifdef UNORDERED
        Matrix Pab(P_ab_view);
        SMITH::sort_indices<0,2,1,1,1,0,1>(P_ab_view.data(), Pab.data(), Pab.ndim(), rnorb, rnorb);

        Matrix ham_block = Pab % gamma_view;
#else
        Matrix ham_block = P_ab_view % gamma_view;
#endif

        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
      }
    }

    { // S_a (x)   A
      const BlockKey left_target(bk.nelea+1, bk.neleb  );
      const BlockKey right_target(bk.nelea-1, bk.neleb  );
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView S_a_view = intra_ops_->S_a_as_matview(source_pair.left.key());
        const MatView gamma_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb),
                                 blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({source_pair.right.key(),right_target}).data->storage()), false);

        Matrix ham_block = S_a_view % gamma_view;

        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
      }
    }

    { // D_a (x)   A^t A^t A + A^t B^t B
      const BlockKey left_target(bk.nelea+1, bk.neleb  );
      const BlockKey right_target(bk.nelea-1, bk.neleb  );
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView D_a_view = intra_ops_->D_a_as_matview(source_pair.left.key());
        shared_ptr<const btas::Tensor3<double>> gamma_aa = blocks_->right_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({source_pair.right.key(),right_target}).data;
        shared_ptr<const btas::Tensor3<double>> gamma_bb = blocks_->right_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({source_pair.right.key(),right_target}).data;
        const MatView gamma_aa_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb, rnorb, rnorb), gamma_aa->storage()), false);
        const MatView gamma_bb_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb, rnorb, rnorb), gamma_bb->storage()), false);

        Matrix gamma(gamma_aa_view + gamma_bb_view);

#ifdef UNORDERED
        Matrix Da(D_a_view);
        SMITH::sort_indices<0,3,2,1,1,1,0,1>(D_a_view.data(), Da.data(), Da.ndim(), rnorb, rnorb, rnorb);

        Matrix ham_block = Da % gamma;
#else
        Matrix ham_block = D_a_view % gamma;
#endif

        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
      }
    }

    { // S_b (x)   B
      const BlockKey left_target(bk.nelea  , bk.neleb+1);
      const BlockKey right_target(bk.nelea  , bk.neleb-1);
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView S_b_view = intra_ops_->S_b_as_matview(source_pair.left.key());
        const MatView gamma_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb),
                                 blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({source_pair.right.key(),right_target}).data->storage()), false);

        Matrix ham_block = S_b_view % gamma_view;

        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
      }
    }

    { // D_b (x)   B^t A^t A + B^t B^t B
      const BlockKey left_target(bk.nelea  , bk.neleb+1);
      const BlockKey right_target(bk.nelea  , bk.neleb-1);
      auto iter = find_if(block_pairs.begin(), block_pairs.end(), [&left_target, &right_target] ( DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );

      if (iter != block_pairs.end()) {
        DMRG::BlockPair target_pair = *iter;

        const MatView D_b_view = intra_ops_->D_b_as_matview(source_pair.left.key());
        shared_ptr<const btas::Tensor3<double>> gamma_aa = blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({source_pair.right.key(),right_target}).data;
        shared_ptr<const btas::Tensor3<double>> gamma_bb = blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({source_pair.right.key(),right_target}).data;
        const MatView gamma_aa_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb, rnorb, rnorb), gamma_aa->storage()), false);
        const MatView gamma_bb_view(btas::make_view(btas::CRange<2>(target_pair.right.nstates*source_pair.right.nstates, rnorb, rnorb, rnorb), gamma_bb->storage()), false);

        Matrix gamma(gamma_aa_view + gamma_bb_view);

#ifdef UNORDERED
        Matrix Db(D_b_view);
        SMITH::sort_indices<0,3,2,1,1,1,0,1>(D_b_view.data(), Db.data(), Db.ndim(), rnorb, rnorb, rnorb);

        Matrix ham_block = Db % gamma;
#else
        Matrix ham_block = D_b_view % gamma;
#endif

        SMITH::sort_indices<0,2,3,1,2,1,0,1>(ham_block.data(), out->element_ptr(target_pair.offset, source_pair.offset), target_pair.left.nstates, source_pair.left.nstates, source_pair.right.nstates, target_pair.right.nstates);
      }
    }
  }

  out->symmetrize();
  return out;
}

/*-----------------------------------------------------------------------------------------------------
  All code below this point was generated by gen/blockops.py
-------------------------------------------------------------------------------------------------------*/

shared_ptr<Matrix> BlockOperators2::S_a(BlockKey bk, const int i) const {
  BlockKey target_bk(bk.nelea+1,bk.neleb  );
  assert(blocks_->contains(target_bk));

  const vector<DMRG::BlockPair>& source_pvec = blocks_->blockpairs(bk);
  const vector<DMRG::BlockPair>& target_pvec = blocks_->blockpairs(target_bk);

  const BlockInfo source_info = blocks_->blockinfo(bk);
  const BlockInfo target_info = blocks_->blockinfo(target_bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(target_info.nstates, source_info.nstates);

  for (auto& spair : source_pvec) {
    { //  - 1.0 <L'|A^t   B |L> (x) <R'|B^t |R>
      BlockKey left_target(bk.nelea+1,bk.neleb-1);
      BlockKey right_target(bk.nelea  ,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            for (int b = 0; b < lnorb; ++b) {
              blas::ax_plus_y_n(-1.0 * mo2e(i, p+roffset, b+loffset, a+loffset), &(*Lgamma)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, true, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|  B |L> (x) <R'|A^t B^t |R>
      BlockKey left_target(bk.nelea  ,bk.neleb-1);
      BlockKey right_target(bk.nelea+1,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int a = 0; a < lnorb; ++a) {
          Lmat.zero();
          blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

          Rmat.zero();
          for (int p = 0; p < rnorb; ++p) {
            for (int q = 0; q < rnorb; ++q) {
              blas::ax_plus_y_n(1.0 * mo2e(i, p+roffset, q+roffset, a+loffset), &(*Rgamma)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|  A |L> (x) <R'|A^t A^t |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea+2,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int a = 0; a < lnorb; ++a) {
          Lmat.zero();
          blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

          Rmat.zero();
          for (int p = 0; p < rnorb; ++p) {
            for (int q = 0; q < rnorb; ++q) {
              blas::ax_plus_y_n(1.0 * mo2e(i, p+roffset, q+roffset, a+loffset), &(*Rgamma)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  S_a (x) I  + 1.0 <L'|A^t |L> (x) <R'|A^t   A |R> - 1.0 <L'|A^t |L> (x) <R'|A^t   A |R> + 1.0 <L'|A^t |L> (x) <R'|B^t   B |R>
      BlockKey left_target(bk.nelea+1,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // S_a (x) I
        Matrix Lterms = *left_ops_->S_a(tpair.right.key(), i);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
        { // ["1.0 <L'|A^t |L> (x) <R'|A^t   A |R>", "1.0 <L'|A^t |L> (x) <R'|B^t   B |R>"]
          shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Rgamma1 = blocks_->right_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({tpair.right.key(),spair.right.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Rgamma2 = blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({tpair.right.key(),spair.right.key()}).data;

          Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
          Matrix Rmat(Rgamma1->extent(0), Rgamma1->extent(1));

          for (int a = 0; a < lnorb; ++a) {
            Lmat.zero();
            blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

            Rmat.zero();
            for (int p = 0; p < rnorb; ++p) {
              for (int q = 0; q < rnorb; ++q) {
                blas::ax_plus_y_n(1.0 * (mo2e(i, p+roffset, a+loffset, q+roffset) - mo2e(i, a+loffset, p+roffset, q+roffset)), &(*Rgamma1)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
                blas::ax_plus_y_n(1.0 * mo2e(i, p+roffset, a+loffset, q+roffset), &(*Rgamma2)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
              }
            }
            out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, false, Lmat));
          }
        }

      }
    }

    { //  + 1.0 <L'|A^t A^t |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea+2,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::AnnihilateAlpha, GammaSQ::AnnihilateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            for (int b = 0; b < lnorb; ++b) {
              blas::ax_plus_y_n(1.0 * mo2e(i, a+loffset, b+loffset, p+roffset), &(*Lgamma)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  I (x) S_a - 1.0 <L'|A^t   A |L> (x) <R'|A^t |R> + 1.0 <L'|A^t   A |L> (x) <R'|A^t |R> + 1.0 <L'|B^t   B |L> (x) <R'|A^t |R>
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea+1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) S_a
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->S_a(spair.right.key(), i);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));
        { // ["-1.0 <L'|A^t   A |L> (x) <R'|A^t |R>", "1.0 <L'|B^t   B |L> (x) <R'|A^t |R>"]
          shared_ptr<const btas::Tensor3<double>> Lgamma1 = blocks_->left_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Lgamma2 = blocks_->left_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({tpair.left.key(),spair.left.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.right.key(),spair.right.key()}).data;

          Matrix Lmat(Lgamma1->extent(0), Lgamma1->extent(1));
          Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

          for (int p = 0; p < rnorb; ++p) {
            Rmat.zero();
            blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

            Lmat.zero();
            for (int a = 0; a < lnorb; ++a) {
              for (int b = 0; b < lnorb; ++b) {
                blas::ax_plus_y_n(-1.0 * (mo2e(i, p+roffset, a+loffset, b+loffset) - mo2e(i, a+loffset, p+roffset, b+loffset)), &(*Lgamma1)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
                blas::ax_plus_y_n(1.0 * mo2e(i, a+loffset, p+roffset, b+loffset), &(*Lgamma2)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
              }
            }
            out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, false, Lmat));
          }
        }

      }
    }

    { //  - 1.0 <L'|B^t |L> (x) <R'|A^t   B |R>
      BlockKey left_target(bk.nelea  ,bk.neleb+1);
      BlockKey right_target(bk.nelea+1,bk.neleb-1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int a = 0; a < lnorb; ++a) {
          Lmat.zero();
          blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

          Rmat.zero();
          for (int p = 0; p < rnorb; ++p) {
            for (int q = 0; q < rnorb; ++q) {
              blas::ax_plus_y_n(-1.0 * mo2e(i, a+loffset, q+roffset, p+roffset), &(*Rgamma)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, false, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|A^t B^t |L> (x) <R'|  B |R>
      BlockKey left_target(bk.nelea+1,bk.neleb+1);
      BlockKey right_target(bk.nelea  ,bk.neleb-1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            for (int b = 0; b < lnorb; ++b) {
              blas::ax_plus_y_n(1.0 * mo2e(i, a+loffset, b+loffset, p+roffset), &(*Lgamma)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::S_b(BlockKey bk, const int i) const {
  BlockKey target_bk(bk.nelea  ,bk.neleb+1);
  assert(blocks_->contains(target_bk));

  const vector<DMRG::BlockPair>& source_pvec = blocks_->blockpairs(bk);
  const vector<DMRG::BlockPair>& target_pvec = blocks_->blockpairs(target_bk);

  const BlockInfo source_info = blocks_->blockinfo(bk);
  const BlockInfo target_info = blocks_->blockinfo(target_bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(target_info.nstates, source_info.nstates);

  for (auto& spair : source_pvec) {
    { //  - 1.0 <L'|B^t   A |L> (x) <R'|A^t |R>
      BlockKey left_target(bk.nelea-1,bk.neleb+1);
      BlockKey right_target(bk.nelea+1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            for (int b = 0; b < lnorb; ++b) {
              blas::ax_plus_y_n(-1.0 * mo2e(i, p+roffset, a+loffset, b+loffset), &(*Lgamma)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, false, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|  B |L> (x) <R'|B^t B^t |R>
      BlockKey left_target(bk.nelea  ,bk.neleb-1);
      BlockKey right_target(bk.nelea  ,bk.neleb+2);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int a = 0; a < lnorb; ++a) {
          Lmat.zero();
          blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

          Rmat.zero();
          for (int p = 0; p < rnorb; ++p) {
            for (int q = 0; q < rnorb; ++q) {
              blas::ax_plus_y_n(1.0 * mo2e(i, p+roffset, q+roffset, a+loffset), &(*Rgamma)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  - 1.0 <L'|B^t A^t |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea+1,bk.neleb+1);
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            for (int b = 0; b < lnorb; ++b) {
              blas::ax_plus_y_n(-1.0 * mo2e(i, b+loffset, a+loffset, p+roffset), &(*Lgamma)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  S_b (x) I  + 1.0 <L'|B^t |L> (x) <R'|B^t   B |R> - 1.0 <L'|B^t |L> (x) <R'|B^t   B |R> + 1.0 <L'|B^t |L> (x) <R'|A^t   A |R>
      BlockKey left_target(bk.nelea  ,bk.neleb+1);
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // S_b (x) I
        Matrix Lterms = *left_ops_->S_b(tpair.right.key(), i);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
        { // ["1.0 <L'|B^t |L> (x) <R'|B^t   B |R>", "1.0 <L'|B^t |L> (x) <R'|A^t   A |R>"]
          shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({tpair.left.key(),spair.left.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Rgamma1 = blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({tpair.right.key(),spair.right.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Rgamma2 = blocks_->right_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({tpair.right.key(),spair.right.key()}).data;

          Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
          Matrix Rmat(Rgamma1->extent(0), Rgamma1->extent(1));

          for (int a = 0; a < lnorb; ++a) {
            Lmat.zero();
            blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

            Rmat.zero();
            for (int p = 0; p < rnorb; ++p) {
              for (int q = 0; q < rnorb; ++q) {
                blas::ax_plus_y_n(1.0 * (mo2e(i, p+roffset, a+loffset, q+roffset) - mo2e(i, a+loffset, p+roffset, q+roffset)), &(*Rgamma1)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
                blas::ax_plus_y_n(1.0 * mo2e(i, p+roffset, a+loffset, q+roffset), &(*Rgamma2)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
              }
            }
            out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, false, Lmat));
          }
        }

      }
    }

    { //  - 1.0 <L'|A^t |L> (x) <R'|B^t   A |R>
      BlockKey left_target(bk.nelea+1,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateAlpha}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int a = 0; a < lnorb; ++a) {
          Lmat.zero();
          blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

          Rmat.zero();
          for (int p = 0; p < rnorb; ++p) {
            for (int q = 0; q < rnorb; ++q) {
              blas::ax_plus_y_n(-1.0 * mo2e(i, a+loffset, p+roffset, q+roffset), &(*Rgamma)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, false, Lmat));
        }
      }
    }

    { //  - 1.0 <L'|  A |L> (x) <R'|B^t A^t |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea+1,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int a = 0; a < lnorb; ++a) {
          Lmat.zero();
          blas::ax_plus_y_n(1.0, &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());

          Rmat.zero();
          for (int p = 0; p < rnorb; ++p) {
            for (int q = 0; q < rnorb; ++q) {
              blas::ax_plus_y_n(-1.0 * mo2e(i, q+roffset, p+roffset, a+loffset), &(*Rgamma)(0, 0, p + q*rnorb), Rmat.size(), Rmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  I (x) S_b - 1.0 <L'|B^t   B |L> (x) <R'|B^t |R> + 1.0 <L'|B^t   B |L> (x) <R'|B^t |R> + 1.0 <L'|A^t   A |L> (x) <R'|B^t |R>
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) S_b
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->S_b(spair.right.key(), i);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));
        { // ["-1.0 <L'|B^t   B |L> (x) <R'|B^t |R>", "1.0 <L'|A^t   A |L> (x) <R'|B^t |R>"]
          shared_ptr<const btas::Tensor3<double>> Lgamma1 = blocks_->left_block()->coupling({GammaSQ::CreateBeta, GammaSQ::AnnihilateBeta}).at({tpair.left.key(),spair.left.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Lgamma2 = blocks_->left_block()->coupling({GammaSQ::CreateAlpha, GammaSQ::AnnihilateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
          shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({tpair.right.key(),spair.right.key()}).data;

          Matrix Lmat(Lgamma1->extent(0), Lgamma1->extent(1));
          Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

          for (int p = 0; p < rnorb; ++p) {
            Rmat.zero();
            blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

            Lmat.zero();
            for (int a = 0; a < lnorb; ++a) {
              for (int b = 0; b < lnorb; ++b) {
                blas::ax_plus_y_n(-1.0 * (mo2e(i, p+roffset, a+loffset, b+loffset) - mo2e(i, a+loffset, p+roffset, b+loffset)), &(*Lgamma1)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
                blas::ax_plus_y_n(1.0 * mo2e(i, a+loffset, p+roffset, b+loffset), &(*Lgamma2)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
              }
            }
            out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, false, Lmat));
          }
        }

      }
    }

    { //  + 1.0 <L'|B^t B^t |L> (x) <R'|  B |R>
      BlockKey left_target(bk.nelea  ,bk.neleb+2);
      BlockKey right_target(bk.nelea  ,bk.neleb-1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::AnnihilateBeta, GammaSQ::AnnihilateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            for (int b = 0; b < lnorb; ++b) {
              blas::ax_plus_y_n(1.0 * mo2e(i, a+loffset, b+loffset, p+roffset), &(*Lgamma)(0, 0, a + b*lnorb), Lmat.size(), Lmat.data());
            }
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::Q_aa(BlockKey bk, const int i, const int j) const {

  const vector<DMRG::BlockPair>& pvec = blocks_->blockpairs(bk);

  const BlockInfo binfo = blocks_->blockinfo(bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(binfo.nstates, binfo.nstates);

  for (auto& spair : pvec) {
    { //  I (x) Q_aa +  Q_aa (x) I
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) Q_aa
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->Q_aa(spair.right.key(), i, j);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));

        // Q_aa (x) I
        Matrix Lterms = *left_ops_->Q_aa(tpair.right.key(), i, j);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
      }
    }

    { //  - 1.0 <L'|  B |L> (x) <R'|B^t |R>
      BlockKey left_target(bk.nelea  ,bk.neleb-1);
      BlockKey right_target(bk.nelea  ,bk.neleb+1);

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(-1.0 * mo2e(p+roffset, i, a+loffset, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, true, Lmat));
        }
      }
    }

    { //  - 1.0 <L'|  A |L> (x) <R'|A^t |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea+1,bk.neleb  );

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(-1.0 * (mo2e(p+roffset, i, a+loffset, j) - mo2e(p+roffset, a+loffset, i, j)), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, true, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|B^t |L> (x) <R'|  B |R>
      BlockKey left_target(bk.nelea  ,bk.neleb+1);
      BlockKey right_target(bk.nelea  ,bk.neleb-1);

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * mo2e(a+loffset, i, p+roffset, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, false, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|A^t |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea+1,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * (mo2e(a+loffset, i, p+roffset, j) - mo2e(a+loffset, p+roffset, i, j)), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, false, Lmat));
        }
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::Q_bb(BlockKey bk, const int i, const int j) const {

  const vector<DMRG::BlockPair>& pvec = blocks_->blockpairs(bk);

  const BlockInfo binfo = blocks_->blockinfo(bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(binfo.nstates, binfo.nstates);

  for (auto& spair : pvec) {
    { //  I (x) Q_bb +  Q_bb (x) I
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) Q_bb
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->Q_bb(spair.right.key(), i, j);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));

        // Q_bb (x) I
        Matrix Lterms = *left_ops_->Q_bb(tpair.right.key(), i, j);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
      }
    }

    { //  - 1.0 <L'|  B |L> (x) <R'|B^t |R>
      BlockKey left_target(bk.nelea  ,bk.neleb-1);
      BlockKey right_target(bk.nelea  ,bk.neleb+1);

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(-1.0 * (mo2e(p+roffset, i, a+loffset, j) - mo2e(p+roffset, a+loffset, i, j)), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, true, Lmat));
        }
      }
    }

    { //  - 1.0 <L'|  A |L> (x) <R'|A^t |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea+1,bk.neleb  );

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(-1.0 * mo2e(p+roffset, i, a+loffset, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, true, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|A^t |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea+1,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * mo2e(a+loffset, i, p+roffset, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, false, Lmat));
        }
      }
    }

    { //  + 1.0 <L'|B^t |L> (x) <R'|  B |R>
      BlockKey left_target(bk.nelea  ,bk.neleb+1);
      BlockKey right_target(bk.nelea  ,bk.neleb-1);

      auto iter = find_if(pvec.begin(), pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * (mo2e(a+loffset, i, p+roffset, j) - mo2e(a+loffset, p+roffset, i, j)), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, false, Lmat));
        }
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::Q_ab(BlockKey bk, const int i, const int j) const {
  BlockKey target_bk(bk.nelea-1,bk.neleb+1);
  assert(blocks_->contains(target_bk));

  const vector<DMRG::BlockPair>& source_pvec = blocks_->blockpairs(bk);
  const vector<DMRG::BlockPair>& target_pvec = blocks_->blockpairs(target_bk);

  const BlockInfo source_info = blocks_->blockinfo(bk);
  const BlockInfo target_info = blocks_->blockinfo(target_bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(target_info.nstates, source_info.nstates);

  for (auto& spair : source_pvec) {
    { //  I (x) Q_ab
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) Q_ab
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->Q_ab(spair.right.key(), i, j);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));
      }
    }

    { //  + 1.0 <L'|  A |L> (x) <R'|B^t |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb+1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({tpair.right.key(),spair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * mo2e(p+roffset, a+loffset, i, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rmat, true, Lmat));
        }
      }
    }

    { //  Q_ab (x) I
      BlockKey left_target(bk.nelea-1,bk.neleb+1);
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // Q_ab (x) I
        Matrix Lterms = *left_ops_->Q_ab(tpair.right.key(), i, j);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
      }
    }

    { //  - 1.0 <L'|B^t |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea  ,bk.neleb+1);
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({tpair.left.key(),spair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(-1.0 * mo2e(a+loffset, p+roffset, i, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, false, Lmat));
        }
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::P_aa(BlockKey bk, const int i, const int j) const {
  BlockKey target_bk(bk.nelea-2,bk.neleb  );
  assert(blocks_->contains(target_bk));

  const vector<DMRG::BlockPair>& source_pvec = blocks_->blockpairs(bk);
  const vector<DMRG::BlockPair>& target_pvec = blocks_->blockpairs(target_bk);

  const BlockInfo source_info = blocks_->blockinfo(bk);
  const BlockInfo target_info = blocks_->blockinfo(target_bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(target_info.nstates, source_info.nstates);

  for (auto& spair : source_pvec) {
    { //  + 1.0 <L'|  A |L> (x) <R'|  A |R> - 1.0 <L'|  A |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * (mo2e(a+loffset, p+roffset, i, j) - mo2e(p+roffset, a+loffset, i, j)), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  I (x) P_aa
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea-2,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) P_aa
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->P_aa(spair.right.key(), i, j);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));
      }
    }

    { //  P_aa (x) I
      BlockKey left_target(bk.nelea-2,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // P_aa (x) I
        Matrix Lterms = *left_ops_->P_aa(tpair.right.key(), i, j);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::P_bb(BlockKey bk, const int i, const int j) const {
  BlockKey target_bk(bk.nelea  ,bk.neleb-2);
  assert(blocks_->contains(target_bk));

  const vector<DMRG::BlockPair>& source_pvec = blocks_->blockpairs(bk);
  const vector<DMRG::BlockPair>& target_pvec = blocks_->blockpairs(target_bk);

  const BlockInfo source_info = blocks_->blockinfo(bk);
  const BlockInfo target_info = blocks_->blockinfo(target_bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(target_info.nstates, source_info.nstates);

  for (auto& spair : source_pvec) {
    { //  + 1.0 <L'|  B |L> (x) <R'|  B |R> - 1.0 <L'|  B |L> (x) <R'|  B |R>
      BlockKey left_target(bk.nelea  ,bk.neleb-1);
      BlockKey right_target(bk.nelea  ,bk.neleb-1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * (mo2e(a+loffset, p+roffset, i, j) - mo2e(p+roffset, a+loffset, i, j)), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  I (x) P_bb
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb-2);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) P_bb
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->P_bb(spair.right.key(), i, j);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));
      }
    }

    { //  P_bb (x) I
      BlockKey left_target(bk.nelea  ,bk.neleb-2);
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // P_bb (x) I
        Matrix Lterms = *left_ops_->P_bb(tpair.right.key(), i, j);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
      }
    }

  }
  return out;
}


shared_ptr<Matrix> BlockOperators2::P_ab(BlockKey bk, const int i, const int j) const {
  BlockKey target_bk(bk.nelea-1,bk.neleb-1);
  assert(blocks_->contains(target_bk));

  const vector<DMRG::BlockPair>& source_pvec = blocks_->blockpairs(bk);
  const vector<DMRG::BlockPair>& target_pvec = blocks_->blockpairs(target_bk);

  const BlockInfo source_info = blocks_->blockinfo(bk);
  const BlockInfo target_info = blocks_->blockinfo(target_bk);

  const int norb = jop_->nocc();
  const int lnorb = blocks_->left_block()->norb();
  const int rnorb = blocks_->right_block()->norb();
  const int loffset = norb - (lnorb + rnorb);
  const int roffset = norb - rnorb; // convenience variable for offset of right orbitals from zero

  const btas::TensorView4<double> mo2e = btas::make_view(btas::CRange<4>(norb,norb,norb,norb), jop_->mo2e()->storage());

  auto out = make_shared<Matrix>(target_info.nstates, source_info.nstates);

  for (auto& spair : source_pvec) {
    { //  P_ab (x) I
      BlockKey left_target(bk.nelea-1,bk.neleb-1);
      BlockKey right_target(bk.nelea  ,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // P_ab (x) I
        Matrix Lterms = *left_ops_->P_ab(tpair.right.key(), i, j);
        Matrix Rident(spair.right.nstates, spair.right.nstates); Rident.unit();

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rident, false, Lterms));
      }
    }

    { //  - 1.0 <L'|  A |L> (x) <R'|  B |R>
      BlockKey left_target(bk.nelea-1,bk.neleb  );
      BlockKey right_target(bk.nelea  ,bk.neleb-1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateAlpha}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateBeta}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(-1.0 * mo2e(p+roffset, a+loffset, i, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

    { //  I (x) P_ab
      BlockKey left_target(bk.nelea  ,bk.neleb  );
      BlockKey right_target(bk.nelea-1,bk.neleb-1);

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;

        // I (x) P_ab
        Matrix Lident(spair.left.nstates, spair.left.nstates); Lident.unit();
        Matrix Rterms = *right_ops_->P_ab(spair.right.key(), i, j);

        out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(false, Rterms, false, Lident));
      }
    }

    { //  + 1.0 <L'|  B |L> (x) <R'|  A |R>
      BlockKey left_target(bk.nelea  ,bk.neleb-1);
      BlockKey right_target(bk.nelea-1,bk.neleb  );

      auto iter = find_if(target_pvec.begin(), target_pvec.end(), [&left_target, &right_target] (DMRG::BlockPair bp)
        { return make_pair(bp.left.key(), bp.right.key())==make_pair(left_target, right_target); }
      );
      if(iter!=target_pvec.end()) {
        DMRG::BlockPair tpair = *iter;
        shared_ptr<const btas::Tensor3<double>> Lgamma = blocks_->left_block()->coupling({GammaSQ::CreateBeta}).at({spair.left.key(),tpair.left.key()}).data;
        shared_ptr<const btas::Tensor3<double>> Rgamma = blocks_->right_block()->coupling({GammaSQ::CreateAlpha}).at({spair.right.key(),tpair.right.key()}).data;

        Matrix Lmat(Lgamma->extent(0), Lgamma->extent(1));
        Matrix Rmat(Rgamma->extent(0), Rgamma->extent(1));

        for (int p = 0; p < rnorb; ++p) {
          Rmat.zero();
          blas::ax_plus_y_n(1.0, &(*Rgamma)(0, 0, p), Rmat.size(), Rmat.data());

          Lmat.zero();
          for (int a = 0; a < lnorb; ++a) {
            blas::ax_plus_y_n(1.0 * mo2e(a+loffset, p+roffset, i, j), &(*Lgamma)(0, 0, a), Lmat.size(), Lmat.data());
          }
          out->add_block(1.0, tpair.offset, spair.offset, tpair.nstates(), spair.nstates(), kronecker_product(true, Rmat, true, Lmat));
        }
      }
    }

  }
  return out;
}
