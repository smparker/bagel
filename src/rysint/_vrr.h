//
// BAGEL - Parallel electron correlation program.
// Filename: _vrr.h
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


// replaces generated codes _vrr_xxxx.cc etc

#ifndef __SRC_RYSINT____VRR_H
#define __SRC_RYSINT____VRR_H

#include <algorithm>

namespace bagel {

template<int a_, int c_, int rank_>
void vrr(double* data_, const double* C00, const double* D00, const double* B00, const double* B01, const double* B10) {

  static_assert(a_>=0 && c_>=0 && rank_ >= 1, "parameter(s) wrong in vrr"); 

#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
  // TODO when GCC 4.7 is gone, we remove these lines. "alignas(32)" is based on the standard C++11
  double C00_[rank_]__attribute__((aligned(32)));
  double D00_[rank_]__attribute__((aligned(32)));
  double B00_[rank_]__attribute__((aligned(32)));
  double B01_[rank_]__attribute__((aligned(32)));
  double B10_[rank_]__attribute__((aligned(32)));
#else
  alignas(32) double C00_[rank_]; 
  alignas(32) double D00_[rank_]; 
  alignas(32) double B00_[rank_]; 
  alignas(32) double B01_[rank_]; 
  alignas(32) double B10_[rank_]; 
#endif
  std::copy_n(C00, rank_, C00_);
  std::copy_n(D00, rank_, D00_);
  std::copy_n(B00, rank_, B00_);
  std::copy_n(B01, rank_, B01_);
  std::copy_n(B10, rank_, B10_);
  
  if ((a_ > 1 && c_ > 1) || (a_ == 1 && c_ >  1)) { 
    // c == 0
    for (int t = 0; t != rank_; ++t)
      data_[rank_*0+t] = 1.0;

    for (int t = 0; t != rank_; ++t)
      data_[rank_*1+t] = C00_[t];

    if (a_ == 2) {
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = C00_[t] * data_[rank_*1+t] + B10_[t];
    } else if (a_ > 1) {
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
      double B10_current[rank_]__attribute__((aligned(32))); // TODO deprecated
#else
      alignas(32) double B10_current[rank_];
#endif
      // a == 2
      for (int t = 0; t != rank_; ++t)
        B10_current[t] = B10_[t];
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = C00_[t] * data_[rank_*1+t] + B10_current[t];
      // a > 2
      for (int a = 3; a != a_+1; ++a) {
        for (int t = 0; t != rank_; ++t)
          B10_current[t] += B10_[t];
        for (int t = 0; t != rank_; ++t)
          data_[rank_*a+t] = C00_[t] * data_[rank_*(a-1)+t] + B10_current[t] * data_[rank_*(a-2)+t];
      }
    }
    // c == 1
    for (int t = 0; t != rank_; ++t)
      data_[rank_*(a_+1)+t] = D00_[t];

#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
    double cB00_current[rank_]__attribute__((aligned(32))); // TODO deprecated
#else
    alignas(32) double cB00_current[rank_];
#endif
    for (int t = 0; t != rank_; ++t)
      cB00_current[t] = B00_[t];

    for (int t = 0; t != rank_; ++t)
      data_[rank_*(a_+2)+t] = C00_[t] * data_[rank_*(a_+1)+t] + cB00_current[t];

    if (a_ == 2) {
      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+3)+t] = C00_[t] * data_[rank_*(a_+2)+t] + B10_[t] * data_[rank_*(a_+1)+t] + cB00_current[t] * data_[rank_+t];
    } else if (a_ > 1) {

#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
      double B10_current[rank_]__attribute__((aligned(32)));  // TODO deprecated
#else
      alignas(32) double B10_current[rank_];
#endif
      for (int t = 0; t != rank_; ++t)
        B10_current[t] = B10_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+3)+t] = C00_[t] * data_[rank_*(a_+2)+t] + B10_current[t] * data_[rank_*(a_+1)+t] + cB00_current[t] * data_[rank_+t];

      for (int a = 3; a != a_+1; ++a) {
        for (int t = 0; t != rank_; ++t)
          B10_current[t] += B10_[t];

        for (int t = 0; t != rank_; ++t)
         data_[rank_*(a_+1+a)+t] = C00_[t] * data_[rank_*(a_+a)+t] + B10_current[t] * data_[rank_*(a_-1+a)+t] + cB00_current[t] * data_[rank_*(a-1)+t];
      }
    }
    // c > 1
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
    double B01_current[rank_]__attribute__((aligned(32))) = {0.0};  // TODO deprecated
#else
    alignas(32) double B01_current[rank_] = {0.0};
#endif
    for (int c = 2; c != c_+1; ++c) {
      for (int t = 0; t != rank_; ++t)
        B01_current[t] += B01_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+1)*c+t] = D00_[t] * data_[rank_*(a_+1)*(c-1)+t] + B01_current[t] * data_[rank_*(a_+1)*(c-2)+t];

      for (int t = 0; t != rank_; ++t)
        cB00_current[t] += B00_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*((a_+1)*c+1)+t] = C00_[t] * data_[rank_*((a_+1)*c)+t] + cB00_current[t] * data_[rank_*((a_+1)*(c-1))+t];

      if (a_ > 1) {
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
        double B10_current[rank_]__attribute__((aligned(32))); // TODO deprecated
#else
        alignas(32) double B10_current[rank_];
#endif
        for (int t = 0; t != rank_; ++t)
          B10_current[t] = B10_[t];

        for (int t = 0; t != rank_; ++t)
          data_[rank_*((a_+1)*c+2)+t] = C00_[t] * data_[rank_*((a_+1)*c+1)+t] + B10_current[t] * data_[rank_*(a_+1)*c+t] + cB00_current[t] * data_[rank_*((a_+1)*(c-1)+1)+t];

        for (int a = 3; a != a_+1; ++a) {
          for (int t = 0; t != rank_; ++t)
            B10_current[t] += B10_[t];

          for (int t = 0; t != rank_; ++t)
            data_[rank_*((a_+1)*c+a)+t] = C00_[t] * data_[rank_*((a_+1)*c+a-1)+t] + B10_current[t] * data_[rank_*((a_+1)*c+a-2)+t] + cB00_current[t] * data_[rank_*((a_+1)*(c-1)+a-1)+t];
        }
      }
    }

  } else if (a_ == 0 && c_ >  0) {
    for (int t = 0; t != rank_; ++t)
      data_[rank_*0+t] = 1.0;

    for (int t = 0; t != rank_; ++t)
      data_[rank_*1+t] = D00_[t];

    if (c_ == 2) {
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = D00_[t] * data_[rank_*1+t] + B01_[t];
    } else {
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
      double B01_current[rank_]__attribute__((aligned(32))); // TODO deprecated
#else
      alignas(32) double B01_current[rank_];
#endif
      // c == 2
      for (int t = 0; t != rank_; ++t)
        B01_current[t] = B01_[t];
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = D00_[t] * data_[rank_+t] + B01_current[t];
      // c > 2
      for (int c = 3; c != c_+1; ++c) {
        for (int t = 0; t != rank_; ++t)
          B01_current[t] += B01_[t];

        for (int t = 0; t != rank_; ++t)
          data_[rank_*c+t] = D00_[t] * data_[rank_*(c-1)+t] + B01_current[t] * data_[rank_*(c-2)+t];
      }
    }
  } else if (a_ >  1 && c_ == 1) {
    for (int t = 0; t != rank_; ++t)
      data_[rank_*0+t] = 1.0;

    for (int t = 0; t != rank_; ++t)
      data_[rank_*1+t] = C00_[t];

    if (a_ == 2) {
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = C00_[t] * data_[rank_*1+t] + B10_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+1)+t] = D00_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+2)+t] = C00_[t] * data_[rank_*(a_+1)+t] + B00_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+3)+t] = C00_[t] * data_[rank_*(a_+2)+t] + B10_[t] * data_[rank_*(a_+1)+t] + B00_[t] * data_[rank_+t];
    } else {
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
      double B10_current[rank_]__attribute__((aligned(32)));  // TODO deprecated
#else
      alignas(32) double B10_current[rank_];
#endif
      // a == 2
      for (int t = 0; t != rank_; ++t)
        B10_current[t] = B10_[t];
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = C00_[t] * data_[rank_*1+t] + B10_current[t];
      // a > 2
      for (int a = 3; a != a_+1; ++a) {
        for (int t = 0; t != rank_; ++t)
          B10_current[t] += B10_[t];
        for (int t = 0; t != rank_; ++t)
          data_[rank_*a+t] = C00_[t] * data_[rank_*(a-1)+t] + B10_current[t] * data_[rank_*(a-2)+t];
      }

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+1)+t] = D00_[t];

      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+2)+t] = C00_[t] * data_[rank_*(a_+1)+t] + B00_[t];

      // a == 2
      for (int t = 0; t != rank_; ++t)
        B10_current[t] = B10_[t];
      for (int t = 0; t != rank_; ++t)
        data_[rank_*(a_+3)+t] = C00_[t] * data_[rank_*(a_+2)+t] + B10_current[t] * data_[rank_*(a_+1)+t] + B00_[t] * data_[rank_+t];

      for (int a = 3; a != a_+1; ++a) {
        for (int t = 0; t != rank_; ++t)
          B10_current[t] += B10_[t];
        for (int t = 0; t != rank_; ++t)
          data_[rank_*(a_+1+a)+t] = C00_[t] * data_[rank_*(a_+a)+t] + B10_current[t] * data_[rank_*(a_+a-1)+t] + B00_[t] * data_[rank_*(a-1)+t];
      }
    }

  } else if (a_ == 1 && c_ == 1) {
    for (int t = 0; t != rank_; ++t)
      data_[rank_*0+t] = 1.0; 

    for (int t = 0; t != rank_; ++t)
      data_[rank_*1+t] = C00_[t];

    for (int t = 0; t != rank_; ++t)
      data_[rank_*2+t] = D00_[t];

    for (int t = 0; t != rank_; ++t)
      data_[rank_*3+t] = C00_[t] * data_[rank_*2+t] + B00_[t];

  } else if (a_ >  0 && c_ == 0) { 
    for (int t = 0; t != rank_; ++t)
      data_[rank_*0+t] = 1.0;

    for (int t = 0; t != rank_; ++t)
      data_[rank_*1+t] = C00_[t];

    if (a_ == 2) {
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = C00_[t] * data_[rank_*1+t] + B10_[t];
    } else {
#if __GNUC__ == 4 && __GNUC_MINOR__ <= 7
      double B10_current[rank_]__attribute__((aligned(32))); // TODO deprecated
#else
      alignas(32) double B10_current[rank_];
#endif
      // a == 2
      for (int t = 0; t != rank_; ++t)
        B10_current[t] = B10_[t];
      for (int t = 0; t != rank_; ++t)
        data_[rank_*2+t] = C00_[t] * data_[rank_*1+t] + B10_current[t];
      // a > 2
      for (int a = 3; a != a_+1; ++a) {
        for (int t = 0; t != rank_; ++t)
          B10_current[t] += B10_[t];
        for (int t = 0; t != rank_; ++t)
          data_[rank_*a+t] = C00_[t] * data_[rank_*(a-1)+t] + B10_current[t] * data_[rank_*(a-2)+t];
      }
    }
  } else if (a_ == 0 && c_ == 0) {
    for (int t = 0; t != rank_; ++t)
      data_[t] = 1.0;
  } else {

  }
}


}
#endif
