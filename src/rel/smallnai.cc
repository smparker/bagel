//
// BAGEL - Parallel electron correlation program.
// Filename: smallnai.cc
// Copyright (C) 2012 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the BAGEL package.
//
// The BAGEL package is free software; you can redistribute it and/or modify
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
#include <src/rel/relshell.h>
#include <src/rel/smallnai.h>
#include <src/rel/smallnaibatch.h>

using namespace std;
using namespace bagel;

SmallNAI::SmallNAI(const shared_ptr<const Geometry> geom) : geom_(geom) {

  for (int i = 0; i != 4; ++i) {
    shared_ptr<Matrix> tmp(new Matrix(geom->nbasis(), geom->nbasis())); 
    dataarray_[i] = tmp;
  }

  init();

}


void SmallNAI::print() const {
  int j = 0;
  for (auto i = dataarray_.begin(); i != dataarray_.end(); ++i, ++j) {
    stringstream ss;
    ss << "SmallNAI " << j;
    (*i)->print(ss.str());
  }
}


void SmallNAI::computebatch(const array<shared_ptr<const RelShell>,2>& input, const int offsetb0, const int offsetb1) {

  // input = [b1, b0]
  assert(input.size() == 2);
  const int dimb1 = input[0]->nbasis();
  const int dimb0 = input[1]->nbasis();
  SmallNAIBatch batch(input, geom_);
  batch.compute();

  dataarray_[0]->copy_block(offsetb1, offsetb0, dimb1, dimb0, batch[0]);
  dataarray_[1]->copy_block(offsetb1, offsetb0, dimb1, dimb0, batch[1]);
  dataarray_[2]->copy_block(offsetb1, offsetb0, dimb1, dimb0, batch[2]);
  dataarray_[3]->copy_block(offsetb1, offsetb0, dimb1, dimb0, batch[3]);
}


// same function as in Matrix; I simply could not use it (without allocating memory space).
void SmallNAI::init() {

  // only lower half will be stored
  auto o0 = geom_->offsets().begin();
  for (auto a0 = geom_->atoms().begin(); a0 != geom_->atoms().end(); ++a0, ++o0) {
    // iatom0 = iatom1;
    auto offset0 = o0->begin();
    for (auto b0 = (*a0)->shells().begin(); b0 != (*a0)->shells().end(); ++b0, ++offset0) {
      auto offset1 = o0->begin();
      for (auto b1 = (*a0)->shells().begin(); b1 != (*a0)->shells().end(); ++b1, ++offset1) {
        shared_ptr<const RelShell> c1(new RelShell(*b1));
        shared_ptr<const RelShell> c0(new RelShell(*b0));
        array<shared_ptr<const RelShell>,2> input = {{c1, c0}};
        computebatch(input, *offset0, *offset1);
      }
    }

    auto o1 = o0+1;
    for (auto a1 = a0+1; a1 != geom_->atoms().end(); ++a1, ++o1) {
      auto offset0 = o0->begin();
      for (auto b0 = (*a0)->shells().begin(); b0 != (*a0)->shells().end(); ++b0, ++offset0) {
        auto offset1 = o1->begin();
        for (auto b1 = (*a1)->shells().begin(); b1 != (*a1)->shells().end(); ++b1, ++offset1) {
          shared_ptr<const RelShell> c1(new RelShell(*b1));
          shared_ptr<const RelShell> c0(new RelShell(*b0));
          array<shared_ptr<const RelShell>,2> input = {{c1, c0}};
          computebatch(input, *offset0, *offset1);
        }
      }
    }
  }

}

