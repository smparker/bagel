//
// BAGEL - Parallel electron correlation program.
// Filename: cdmatrix.h
// Copyright (C) 2013 Matthew Kelley
//
// Author: Matthew Kelley <matthewkelley2017@northwestern.edu>
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


#ifndef __SRC_REL_CDMATRIX_H
#define __SRC_REL_CDMATRIX_H

#include <cassert>
#include <list>
#include <memory>
#include <src/util/zmatrix.h>
#include <src/rel/breit2index.h>

namespace bagel {

class CDMatrix : public ZMatrix {
  protected:
    const int comp_;

  public:
    CDMatrix(const ZMatrix& o, const int comp) : ZMatrix(o), comp_(comp) { }

    // multiply cd and breit2index for use in Jop in dfock.cc
    std::list<std::shared_ptr<const CDMatrix>> compute_breit_cd(std::list<std::shared_ptr<Breit2Index>>& b) const {
      std::list<std::shared_ptr<const CDMatrix>> out;
      for (auto i : b) {
        if (i->index().second == comp_)
          out.push_back(std::shared_ptr<CDMatrix>(new CDMatrix(*i->j_term() * *this, i->index().first)));
      }
      return out;
    }

    const int comp() const { return comp_; }

};

}

#endif