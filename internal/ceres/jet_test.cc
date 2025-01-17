// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: keir@google.com (Keir Mierle)

#include "ceres/jet.h"

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

#include "ceres/stringprintf.h"
#include "ceres/test_util.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

#define VL VLOG(1)

namespace ceres {
namespace internal {

namespace {

const double kE = 2.71828182845904523536;

typedef Jet<double, 2> J;

// Convenient shorthand for making a jet.
J MakeJet(double a, double v0, double v1) {
  J z;
  z.a = a;
  z.v[0] = v0;
  z.v[1] = v1;
  return z;
}

// On a 32-bit optimized build, the mismatch is about 1.4e-14.
double const kTolerance = 1e-13;

void ExpectJetsClose(const J& x, const J& y) {
  ExpectClose(x.a, y.a, kTolerance);
  ExpectClose(x.v[0], y.v[0], kTolerance);
  ExpectClose(x.v[1], y.v[1], kTolerance);
}

const double kStep = 1e-8;
const double kNumericalTolerance = 1e-6;  // Numeric derivation is quite inexact

// Differentiate using Jet and confirm results with numerical derivation.
template <typename Function>
void NumericalTest(const char* name, const Function& f, const double x) {
  const double exact_dx = f(MakeJet(x, 1.0, 0.0)).v[0];
  const double estimated_dx =
      (f(J(x + kStep)).a - f(J(x - kStep)).a) / (2.0 * kStep);
  VL << name << "(" << x << "), exact dx: " << exact_dx
     << ", estimated dx: " << estimated_dx;
  ExpectClose(exact_dx, estimated_dx, kNumericalTolerance);
}

// Same as NumericalTest, but given a function taking two arguments.
template <typename Function>
void NumericalTest2(const char* name,
                    const Function& f,
                    const double x,
                    const double y) {
  const J exact_delta = f(MakeJet(x, 1.0, 0.0), MakeJet(y, 0.0, 1.0));
  const double exact_dx = exact_delta.v[0];
  const double exact_dy = exact_delta.v[1];

  // Sanity check - these should be equivalent:
  EXPECT_EQ(exact_dx, f(MakeJet(x, 1.0, 0.0), MakeJet(y, 0.0, 0.0)).v[0]);
  EXPECT_EQ(exact_dx, f(MakeJet(x, 0.0, 1.0), MakeJet(y, 0.0, 0.0)).v[1]);
  EXPECT_EQ(exact_dy, f(MakeJet(x, 0.0, 0.0), MakeJet(y, 1.0, 0.0)).v[0]);
  EXPECT_EQ(exact_dy, f(MakeJet(x, 0.0, 0.0), MakeJet(y, 0.0, 1.0)).v[1]);

  const double estimated_dx =
      (f(J(x + kStep), J(y)).a - f(J(x - kStep), J(y)).a) / (2.0 * kStep);
  const double estimated_dy =
      (f(J(x), J(y + kStep)).a - f(J(x), J(y - kStep)).a) / (2.0 * kStep);
  VL << name << "(" << x << ", " << y << "), exact dx: " << exact_dx
     << ", estimated dx: " << estimated_dx;
  ExpectClose(exact_dx, estimated_dx, kNumericalTolerance);
  VL << name << "(" << x << ", " << y << "), exact dy: " << exact_dy
     << ", estimated dy: " << estimated_dy;
  ExpectClose(exact_dy, estimated_dy, kNumericalTolerance);
}

}  // namespace

TEST(Jet, Jet) {
  // Pick arbitrary values for x and y.
  J x = MakeJet(2.3, -2.7, 1e-3);
  J y = MakeJet(1.7, 0.5, 1e+2);

  VL << "x = " << x;
  VL << "y = " << y;

  {  // Check that log(exp(x)) == x.
    J z = exp(x);
    J w = log(z);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, x);
  }

  {  // Check that (x * y) / x == y.
    J z = x * y;
    J w = z / x;
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, y);
  }

  {  // Check that sqrt(x * x) == x.
    J z = x * x;
    J w = sqrt(z);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, x);
  }

  {  // Check that sqrt(y) * sqrt(y) == y.
    J z = sqrt(y);
    J w = z * z;
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, y);
  }

  NumericalTest("sqrt", sqrt<double, 2>, 0.00001);
  NumericalTest("sqrt", sqrt<double, 2>, 1.0);

  {  // Check that cos(2*x) = cos(x)^2 - sin(x)^2
    J z = cos(J(2.0) * x);
    J w = cos(x) * cos(x) - sin(x) * sin(x);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, z);
  }

  {  // Check that sin(2*x) = 2*cos(x)*sin(x)
    J z = sin(J(2.0) * x);
    J w = J(2.0) * cos(x) * sin(x);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, z);
  }

  {  // Check that cos(x)*cos(x) + sin(x)*sin(x) = 1
    J z = cos(x) * cos(x);
    J w = sin(x) * sin(x);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z + w, J(1.0));
  }

  {  // Check that atan2(r*sin(t), r*cos(t)) = t.
    J t = MakeJet(0.7, -0.3, +1.5);
    J r = MakeJet(2.3, 0.13, -2.4);
    VL << "t = " << t;
    VL << "r = " << r;

    J u = atan2(r * sin(t), r * cos(t));
    VL << "u = " << u;

    ExpectJetsClose(u, t);
  }

  {  // Check that tan(x) = sin(x) / cos(x).
    J z = tan(x);
    J w = sin(x) / cos(x);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z, w);
  }

  {  // Check that tan(atan(x)) = x.
    J z = tan(atan(x));
    J w = x;
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z, w);
  }

  {  // Check that cosh(x)*cosh(x) - sinh(x)*sinh(x) = 1
    J z = cosh(x) * cosh(x);
    J w = sinh(x) * sinh(x);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z - w, J(1.0));
  }

  {  // Check that tanh(x + y) = (tanh(x) + tanh(y)) / (1 + tanh(x) tanh(y))
    J z = tanh(x + y);
    J w = (tanh(x) + tanh(y)) / (J(1.0) + tanh(x) * tanh(y));
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z, w);
  }

  {  // Check that pow(x, 1) == x.
    VL << "x = " << x;

    J u = pow(x, 1.);
    VL << "u = " << u;

    ExpectJetsClose(x, u);
  }

  {  // Check that pow(x, 1) == x.
    J y = MakeJet(1, 0.0, 0.0);
    VL << "x = " << x;
    VL << "y = " << y;

    J u = pow(x, y);
    VL << "u = " << u;

    ExpectJetsClose(x, u);
  }

  {  // Check that pow(e, log(x)) == x.
    J logx = log(x);

    VL << "x = " << x;
    VL << "y = " << y;

    J u = pow(kE, logx);
    VL << "u = " << u;

    ExpectJetsClose(x, u);
  }

  {  // Check that pow(e, log(x)) == x.
    J logx = log(x);
    J e = MakeJet(kE, 0., 0.);
    VL << "x = " << x;
    VL << "log(x) = " << logx;

    J u = pow(e, logx);
    VL << "u = " << u;

    ExpectJetsClose(x, u);
  }

  {  // Check that pow(e, log(x)) == x.
    J logx = log(x);
    J e = MakeJet(kE, 0., 0.);
    VL << "x = " << x;
    VL << "logx = " << logx;

    J u = pow(e, logx);
    VL << "u = " << u;

    ExpectJetsClose(x, u);
  }

  {  // Check that pow(x,y) = exp(y*log(x)).
    J logx = log(x);
    J e = MakeJet(kE, 0., 0.);
    VL << "x = " << x;
    VL << "logx = " << logx;

    J u = pow(e, y * logx);
    J v = pow(x, y);
    VL << "u = " << u;
    VL << "v = " << v;

    ExpectJetsClose(v, u);
  }

  {  // Check that pow(0, y) == 0 for y > 1, with both arguments Jets.
    // This tests special case handling inside pow().
    J a = MakeJet(0, 1, 2);
    J b = MakeJet(2, 3, 4);
    VL << "a = " << a;
    VL << "b = " << b;

    J c = pow(a, b);
    VL << "a^b = " << c;
    ExpectJetsClose(c, MakeJet(0, 0, 0));
  }

  {  // Check that pow(0, y) == 0 for y == 1, with both arguments Jets.
    // This tests special case handling inside pow().
    J a = MakeJet(0, 1, 2);
    J b = MakeJet(1, 3, 4);
    VL << "a = " << a;
    VL << "b = " << b;

    J c = pow(a, b);
    VL << "a^b = " << c;
    ExpectJetsClose(c, MakeJet(0, 1, 2));
  }

  {  // Check that pow(0, <1) is not finite, with both arguments Jets.
    for (int i = 1; i < 10; i++) {
      J a = MakeJet(0, 1, 2);
      J b = MakeJet(i * 0.1, 3, 4);  // b = 0.1 ... 0.9
      VL << "a = " << a;
      VL << "b = " << b;

      J c = pow(a, b);
      VL << "a^b = " << c;
      EXPECT_EQ(c.a, 0.0);
      EXPECT_FALSE(IsFinite(c.v[0]));
      EXPECT_FALSE(IsFinite(c.v[1]));
    }
    for (int i = -10; i < 0; i++) {
      J a = MakeJet(0, 1, 2);
      J b = MakeJet(i * 0.1, 3, 4);  // b = -1,-0.9 ... -0.1
      VL << "a = " << a;
      VL << "b = " << b;

      J c = pow(a, b);
      VL << "a^b = " << c;
      EXPECT_FALSE(IsFinite(c.a));
      EXPECT_FALSE(IsFinite(c.v[0]));
      EXPECT_FALSE(IsFinite(c.v[1]));
    }

    {
      // The special case of 0^0 = 1 defined by the C standard.
      J a = MakeJet(0, 1, 2);
      J b = MakeJet(0, 3, 4);
      VL << "a = " << a;
      VL << "b = " << b;

      J c = pow(a, b);
      VL << "a^b = " << c;
      EXPECT_EQ(c.a, 1.0);
      EXPECT_FALSE(IsFinite(c.v[0]));
      EXPECT_FALSE(IsFinite(c.v[1]));
    }
  }

  {  // Check that pow(<0, b) is correct for integer b.
    // This tests special case handling inside pow().
    J a = MakeJet(-1.5, 3, 4);

    // b integer:
    for (int i = -10; i <= 10; i++) {
      J b = MakeJet(i, 0, 5);
      VL << "a = " << a;
      VL << "b = " << b;

      J c = pow(a, b);
      VL << "a^b = " << c;
      ExpectClose(c.a, pow(-1.5, i), kTolerance);
      EXPECT_TRUE(IsFinite(c.v[0]));
      EXPECT_FALSE(IsFinite(c.v[1]));
      ExpectClose(c.v[0], i * pow(-1.5, i - 1) * 3.0, kTolerance);
    }
  }

  {  // Check that pow(<0, b) is correct for noninteger b.
    // This tests special case handling inside pow().
    J a = MakeJet(-1.5, 3, 4);
    J b = MakeJet(-2.5, 0, 5);
    VL << "a = " << a;
    VL << "b = " << b;

    J c = pow(a, b);
    VL << "a^b = " << c;
    EXPECT_FALSE(IsFinite(c.a));
    EXPECT_FALSE(IsFinite(c.v[0]));
    EXPECT_FALSE(IsFinite(c.v[1]));
  }

  {
    // Check that pow(0,y) == 0 for y == 2, with the second argument a
    // Jet.  This tests special case handling inside pow().
    double a = 0;
    J b = MakeJet(2, 3, 4);
    VL << "a = " << a;
    VL << "b = " << b;

    J c = pow(a, b);
    VL << "a^b = " << c;
    ExpectJetsClose(c, MakeJet(0, 0, 0));
  }

  {
    // Check that pow(<0,y) is correct for integer y. This tests special case
    // handling inside pow().
    double a = -1.5;
    for (int i = -10; i <= 10; i++) {
      J b = MakeJet(i, 3, 0);
      VL << "a = " << a;
      VL << "b = " << b;

      J c = pow(a, b);
      VL << "a^b = " << c;
      ExpectClose(c.a, pow(-1.5, i), kTolerance);
      EXPECT_FALSE(IsFinite(c.v[0]));
      EXPECT_TRUE(IsFinite(c.v[1]));
      ExpectClose(c.v[1], 0, kTolerance);
    }
  }

  {
    // Check that pow(<0,y) is correct for noninteger y. This tests special
    // case handling inside pow().
    double a = -1.5;
    J b = MakeJet(-3.14, 3, 0);
    VL << "a = " << a;
    VL << "b = " << b;

    J c = pow(a, b);
    VL << "a^b = " << c;
    EXPECT_FALSE(IsFinite(c.a));
    EXPECT_FALSE(IsFinite(c.v[0]));
    EXPECT_FALSE(IsFinite(c.v[1]));
  }

  {  // Check that 1 + x == x + 1.
    J a = x + 1.0;
    J b = 1.0 + x;
    J c = x;
    c += 1.0;
    J d = J{x} + 1.0;
    J e = 1.0 + J{x};

    ExpectJetsClose(a, b);
    ExpectJetsClose(a, c);
    ExpectJetsClose(a, d);
    ExpectJetsClose(a, e);
  }

  {  // Check that 1 - x == -(x - 1).
    J a = 1.0 - x;
    J b = -(x - 1.0);
    J c = x;
    c -= 1.0;
    J d = -(J{x} - 1.0);
    J e = 1.0 - J{x};

    ExpectJetsClose(a, b);
    ExpectJetsClose(a, -c);
    ExpectJetsClose(a, d);
    ExpectJetsClose(a, e);
  }

  {  // Check that (x/s)*s == (x*s)/s.
    J a = x / 5.0;
    J b = x * 5.0;
    J c = x;
    c /= 5.0;
    J d = x;
    d *= 5.0;
    J e = J{x} / 5.0;
    J f = J{x} * 5.0;
    J g = 1.0 / (5.0 / J{x});
    J h = 5.0 * J{x};

    ExpectJetsClose(5.0 * a, b / 5.0);
    ExpectJetsClose(a, c);
    ExpectJetsClose(b, d);
    ExpectJetsClose(5.0 * e, f / 5.0);
    ExpectJetsClose(5.0 * g, h / 5.0);
  }

  {  // Check that x / y == 1 / (y / x).
    J a = x / y;
    J b = 1.0 / (y / x);
    J c = J{x} / y;
    J d = x / J{y};
    VL << "a = " << a;
    VL << "b = " << b;

    ExpectJetsClose(a, b);
    ExpectJetsClose(a, c);
    ExpectJetsClose(a, d);
  }

  {  // Check that abs(-x * x) == sqrt(x * x).
    ExpectJetsClose(abs(-x), sqrt(x * x));
  }

  {  // Check that cos(acos(x)) == x.
    J a = MakeJet(0.1, -2.7, 1e-3);
    ExpectJetsClose(cos(acos(a)), a);
    ExpectJetsClose(acos(cos(a)), a);

    J b = MakeJet(0.6, 0.5, 1e+2);
    ExpectJetsClose(cos(acos(b)), b);
    ExpectJetsClose(acos(cos(b)), b);
  }

  {  // Check that sin(asin(x)) == x.
    J a = MakeJet(0.1, -2.7, 1e-3);
    ExpectJetsClose(sin(asin(a)), a);
    ExpectJetsClose(asin(sin(a)), a);

    J b = MakeJet(0.4, 0.5, 1e+2);
    ExpectJetsClose(sin(asin(b)), b);
    ExpectJetsClose(asin(sin(b)), b);
  }

  {
    J zero = J(0.0);

    // Check that J0(0) == 1.
    ExpectJetsClose(BesselJ0(zero), J(1.0));

    // Check that J1(0) == 0.
    ExpectJetsClose(BesselJ1(zero), zero);

    // Check that J2(0) == 0.
    ExpectJetsClose(BesselJn(2, zero), zero);

    // Check that J3(0) == 0.
    ExpectJetsClose(BesselJn(3, zero), zero);

    J z = MakeJet(0.1, -2.7, 1e-3);

    // Check that J0(z) == Jn(0,z).
    ExpectJetsClose(BesselJ0(z), BesselJn(0, z));

    // Check that J1(z) == Jn(1,z).
    ExpectJetsClose(BesselJ1(z), BesselJn(1, z));

    // Check that J0(z)+J2(z) == (2/z)*J1(z).
    // See formula http://dlmf.nist.gov/10.6.E1
    ExpectJetsClose(BesselJ0(z) + BesselJn(2, z), (2.0 / z) * BesselJ1(z));
  }

  {  // Check that floor of a positive number works.
    J a = MakeJet(0.1, -2.7, 1e-3);
    J b = floor(a);
    J expected = MakeJet(floor(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }

  {  // Check that floor of a negative number works.
    J a = MakeJet(-1.1, -2.7, 1e-3);
    J b = floor(a);
    J expected = MakeJet(floor(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }

  {  // Check that floor of a positive number works.
    J a = MakeJet(10.123, -2.7, 1e-3);
    J b = floor(a);
    J expected = MakeJet(floor(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }

  {  // Check that ceil of a positive number works.
    J a = MakeJet(0.1, -2.7, 1e-3);
    J b = ceil(a);
    J expected = MakeJet(ceil(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }

  {  // Check that ceil of a negative number works.
    J a = MakeJet(-1.1, -2.7, 1e-3);
    J b = ceil(a);
    J expected = MakeJet(ceil(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }

  {  // Check that ceil of a positive number works.
    J a = MakeJet(10.123, -2.7, 1e-3);
    J b = ceil(a);
    J expected = MakeJet(ceil(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }

  {  // Check that erf works.
    J a = MakeJet(10.123, -2.7, 1e-3);
    J b = erf(a);
    J expected = MakeJet(erf(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }
  NumericalTest("erf", erf<double, 2>, -1.0);
  NumericalTest("erf", erf<double, 2>, 1e-5);
  NumericalTest("erf", erf<double, 2>, 0.5);
  NumericalTest("erf", erf<double, 2>, 100.0);

  {  // Check that erfc works.
    J a = MakeJet(10.123, -2.7, 1e-3);
    J b = erfc(a);
    J expected = MakeJet(erfc(a.a), 0.0, 0.0);
    EXPECT_EQ(expected, b);
  }
  NumericalTest("erfc", erfc<double, 2>, -1.0);
  NumericalTest("erfc", erfc<double, 2>, 1e-5);
  NumericalTest("erfc", erfc<double, 2>, 0.5);
  NumericalTest("erfc", erfc<double, 2>, 100.0);

  {  // Check that cbrt(x * x * x) == x.
    J z = x * x * x;
    J w = cbrt(z);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, x);
  }

  {  // Check that cbrt(y) * cbrt(y) * cbrt(y) == y.
    J z = cbrt(y);
    J w = z * z * z;
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(w, y);
  }

  {  // Check that cbrt(x) == pow(x, 1/3).
    J z = cbrt(x);
    J w = pow(x, 1.0 / 3.0);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z, w);
  }
  NumericalTest("cbrt", cbrt<double, 2>, -1.0);
  NumericalTest("cbrt", cbrt<double, 2>, -1e-5);
  NumericalTest("cbrt", cbrt<double, 2>, 1e-5);
  NumericalTest("cbrt", cbrt<double, 2>, 1.0);

  {  // Check that exp2(x) == exp(x * log(2))
    J z = exp2(x);
    J w = exp(x * log(2.0));
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z, w);
  }
  NumericalTest("exp2", exp2<double, 2>, -1.0);
  NumericalTest("exp2", exp2<double, 2>, -1e-5);
  NumericalTest("exp2", exp2<double, 2>, -1e-200);
  NumericalTest("exp2", exp2<double, 2>, 0.0);
  NumericalTest("exp2", exp2<double, 2>, 1e-200);
  NumericalTest("exp2", exp2<double, 2>, 1e-5);
  NumericalTest("exp2", exp2<double, 2>, 1.0);

  {  // Check that log2(x) == log(x) / log(2)
    J z = log2(x);
    J w = log(x) / log(2.0);
    VL << "z = " << z;
    VL << "w = " << w;
    ExpectJetsClose(z, w);
  }
  NumericalTest("log2", log2<double, 2>, 1e-5);
  NumericalTest("log2", log2<double, 2>, 1.0);
  NumericalTest("log2", log2<double, 2>, 100.0);

  {  // Check that hypot(x, y) == sqrt(x^2 + y^2)
    J h = hypot(x, y);
    J s = sqrt(x * x + y * y);
    VL << "h = " << h;
    VL << "s = " << s;
    ExpectJetsClose(h, s);
  }

  {  // Check that hypot(x, x) == sqrt(2) * abs(x)
    J h = hypot(x, x);
    J s = sqrt(2.0) * abs(x);
    VL << "h = " << h;
    VL << "s = " << s;
    ExpectJetsClose(h, s);
  }

  {  // Check that the derivative is zero tangentially to the circle:
    J h = hypot(MakeJet(2.0, 1.0, 1.0), MakeJet(2.0, 1.0, -1.0));
    VL << "h = " << h;
    ExpectJetsClose(h, MakeJet(sqrt(8.0), std::sqrt(2.0), 0.0));
  }

  {  // Check that hypot(x, 0) == x
    J zero = MakeJet(0.0, 2.0, 3.14);
    J h = hypot(x, zero);
    VL << "h = " << h;
    ExpectJetsClose(x, h);
  }

  {  // Check that hypot(0, y) == y
    J zero = MakeJet(0.0, 2.0, 3.14);
    J h = hypot(zero, y);
    VL << "h = " << h;
    ExpectJetsClose(y, h);
  }

  {  // Check that hypot(x, 0) == sqrt(x * x) == x, even when x * x underflows:
    EXPECT_EQ(DBL_MIN * DBL_MIN, 0.0);  // Make sure it underflows
    J huge = MakeJet(DBL_MIN, 2.0, 3.14);
    J h = hypot(huge, J(0.0));
    VL << "h = " << h;
    ExpectJetsClose(h, huge);
  }

  {  // Check that hypot(x, 0) == sqrt(x * x) == x, even when x * x overflows:
    EXPECT_EQ(DBL_MAX * DBL_MAX, std::numeric_limits<double>::infinity());
    J huge = MakeJet(DBL_MAX, 2.0, 3.14);
    J h = hypot(huge, J(0.0));
    VL << "h = " << h;
    ExpectJetsClose(h, huge);
  }

  // clang-format off
  NumericalTest2("hypot", hypot<double, 2>,  0.0,   1e-5);
  NumericalTest2("hypot", hypot<double, 2>, -1e-5,  0.0);
  NumericalTest2("hypot", hypot<double, 2>,  1e-5,  1e-5);
  NumericalTest2("hypot", hypot<double, 2>,  0.0,   1.0);
  NumericalTest2("hypot", hypot<double, 2>,  1e-3,  1.0);
  NumericalTest2("hypot", hypot<double, 2>,  1e-3, -1.0);
  NumericalTest2("hypot", hypot<double, 2>, -1e-3,  1.0);
  NumericalTest2("hypot", hypot<double, 2>, -1e-3, -1.0);
  NumericalTest2("hypot", hypot<double, 2>,  1.0,   2.0);
  // clang-format on

  {
    J z = fmax(x, y);
    VL << "z = " << z;
    ExpectJetsClose(x, z);
  }
  {
    J z = fmax(y, x);
    VL << "z = " << z;
    ExpectJetsClose(x, z);
  }
  {
    J z = fmax(x, y.a);
    VL << "z = " << z;
    ExpectJetsClose(x, z);
  }
  {
    J z = fmax(y, x.a);
    VL << "z = " << z;
    ExpectJetsClose(J{x.a}, z);
  }
  {
    J z = fmax(x.a, y);
    VL << "z = " << z;
    ExpectJetsClose(J{x.a}, z);
  }
  {
    J z = fmax(y.a, x);
    VL << "z = " << z;
    ExpectJetsClose(x, z);
  }

  {
    J z = fmin(x, y);
    VL << "z = " << z;
    ExpectJetsClose(y, z);
  }
  {
    J z = fmin(y, x);
    VL << "z = " << z;
    ExpectJetsClose(y, z);
  }
  {
    J z = fmin(x, y.a);
    VL << "z = " << z;
    ExpectJetsClose(J{y.a}, z);
  }
  {
    J z = fmin(y, x.a);
    VL << "z = " << z;
    ExpectJetsClose(y, z);
  }
  {
    J z = fmin(x.a, y);
    VL << "z = " << z;
    ExpectJetsClose(y, z);
  }
  {
    J z = fmin(y.a, x);
    VL << "z = " << z;
    ExpectJetsClose(J{y.a}, z);
  }
}

TEST(Jet, JetsInEigenMatrices) {
  J x = MakeJet(2.3, -2.7, 1e-3);
  J y = MakeJet(1.7, 0.5, 1e+2);
  J z = MakeJet(5.3, -4.7, 1e-3);
  J w = MakeJet(9.7, 1.5, 10.1);

  Eigen::Matrix<J, 2, 2> M;
  Eigen::Matrix<J, 2, 1> v, r1, r2;

  M << x, y, z, w;
  v << x, z;

  // Check that M * v == (v^T * M^T)^T
  r1 = M * v;
  r2 = (v.transpose() * M.transpose()).transpose();

  ExpectJetsClose(r1(0), r2(0));
  ExpectJetsClose(r1(1), r2(1));
}

TEST(JetTraitsTest, ClassificationMixed) {
  Jet<double, 3> a(5.5, 0);
  a.v[0] = std::numeric_limits<double>::quiet_NaN();
  a.v[1] = std::numeric_limits<double>::infinity();
  a.v[2] = -std::numeric_limits<double>::infinity();
  EXPECT_FALSE(IsFinite(a));
  EXPECT_FALSE(IsNormal(a));
  EXPECT_TRUE(IsInfinite(a));
  EXPECT_TRUE(IsNaN(a));
}

TEST(JetTraitsTest, ClassificationNaN) {
  Jet<double, 3> a(5.5, 0);
  a.v[0] = std::numeric_limits<double>::quiet_NaN();
  a.v[1] = 0.0;
  a.v[2] = 0.0;
  EXPECT_FALSE(IsFinite(a));
  EXPECT_FALSE(IsNormal(a));
  EXPECT_FALSE(IsInfinite(a));
  EXPECT_TRUE(IsNaN(a));
}

TEST(JetTraitsTest, ClassificationInf) {
  Jet<double, 3> a(5.5, 0);
  a.v[0] = std::numeric_limits<double>::infinity();
  a.v[1] = 0.0;
  a.v[2] = 0.0;
  EXPECT_FALSE(IsFinite(a));
  EXPECT_FALSE(IsNormal(a));
  EXPECT_TRUE(IsInfinite(a));
  EXPECT_FALSE(IsNaN(a));
}

TEST(JetTraitsTest, ClassificationFinite) {
  Jet<double, 3> a(5.5, 0);
  a.v[0] = 100.0;
  a.v[1] = 1.0;
  a.v[2] = 3.14159;
  EXPECT_TRUE(IsFinite(a));
  EXPECT_TRUE(IsNormal(a));
  EXPECT_FALSE(IsInfinite(a));
  EXPECT_FALSE(IsNaN(a));
}

// The following test ensures that Jets have all the appropriate Eigen
// related traits so that they can be used as part of matrix
// decompositions.
TEST(Jet, FullRankEigenLLTSolve) {
  Eigen::Matrix<J, 3, 3> A;
  Eigen::Matrix<J, 3, 1> b, x;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      A(i, j) = MakeJet(0.0, i, j * j);
    }
    b(i) = MakeJet(i, i, i);
    x(i) = MakeJet(0.0, 0.0, 0.0);
    A(i, i) = MakeJet(1.0, i, i * i);
  }
  x = A.llt().solve(b);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(x(i).a, b(i).a);
  }
}

TEST(Jet, FullRankEigenLDLTSolve) {
  Eigen::Matrix<J, 3, 3> A;
  Eigen::Matrix<J, 3, 1> b, x;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      A(i, j) = MakeJet(0.0, i, j * j);
    }
    b(i) = MakeJet(i, i, i);
    x(i) = MakeJet(0.0, 0.0, 0.0);
    A(i, i) = MakeJet(1.0, i, i * i);
  }
  x = A.ldlt().solve(b);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(x(i).a, b(i).a);
  }
}

TEST(Jet, FullRankEigenLUSolve) {
  Eigen::Matrix<J, 3, 3> A;
  Eigen::Matrix<J, 3, 1> b, x;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      A(i, j) = MakeJet(0.0, i, j * j);
    }
    b(i) = MakeJet(i, i, i);
    x(i) = MakeJet(0.0, 0.0, 0.0);
    A(i, i) = MakeJet(1.0, i, i * i);
  }

  x = A.lu().solve(b);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(x(i).a, b(i).a);
  }
}

// ScalarBinaryOpTraits is only supported on Eigen versions >= 3.3
TEST(JetTraitsTest, MatrixScalarUnaryOps) {
  const J x = MakeJet(2.3, -2.7, 1e-3);
  const J y = MakeJet(1.7, 0.5, 1e+2);
  Eigen::Matrix<J, 2, 1> a;
  a << x, y;

  const J sum = a.sum();
  const J sum2 = a(0) + a(1);
  ExpectJetsClose(sum, sum2);
}

TEST(JetTraitsTest, MatrixScalarBinaryOps) {
  const J x = MakeJet(2.3, -2.7, 1e-3);
  const J y = MakeJet(1.7, 0.5, 1e+2);
  const J z = MakeJet(5.3, -4.7, 1e-3);
  const J w = MakeJet(9.7, 1.5, 10.1);

  Eigen::Matrix<J, 2, 2> M;
  Eigen::Vector2d v;

  M << x, y, z, w;
  v << 0.6, -2.1;

  // Check that M * v == M * v.cast<J>().
  const Eigen::Matrix<J, 2, 1> r1 = M * v;
  const Eigen::Matrix<J, 2, 1> r2 = M * v.cast<J>();

  ExpectJetsClose(r1(0), r2(0));
  ExpectJetsClose(r1(1), r2(1));

  // Check that M * a == M * T(a).
  const double a = 3.1;
  const Eigen::Matrix<J, 2, 2> r3 = M * a;
  const Eigen::Matrix<J, 2, 2> r4 = M * J(a);

  ExpectJetsClose(r3(0, 0), r4(0, 0));
  ExpectJetsClose(r3(1, 0), r4(1, 0));
  ExpectJetsClose(r3(0, 1), r4(0, 1));
  ExpectJetsClose(r3(1, 1), r4(1, 1));
}

TEST(JetTraitsTest, ArrayScalarUnaryOps) {
  const J x = MakeJet(2.3, -2.7, 1e-3);
  const J y = MakeJet(1.7, 0.5, 1e+2);
  Eigen::Array<J, 2, 1> a;
  a << x, y;

  const J sum = a.sum();
  const J sum2 = a(0) + a(1);
  ExpectJetsClose(sum, sum2);
}

TEST(JetTraitsTest, ArrayScalarBinaryOps) {
  const J x = MakeJet(2.3, -2.7, 1e-3);
  const J y = MakeJet(1.7, 0.5, 1e+2);

  Eigen::Array<J, 2, 1> a;
  Eigen::Array2d b;

  a << x, y;
  b << 0.6, -2.1;

  // Check that a * b == a * b.cast<T>()
  const Eigen::Array<J, 2, 1> r1 = a * b;
  const Eigen::Array<J, 2, 1> r2 = a * b.cast<J>();

  ExpectJetsClose(r1(0), r2(0));
  ExpectJetsClose(r1(1), r2(1));

  // Check that a * c == a * T(c).
  const double c = 3.1;
  const Eigen::Array<J, 2, 1> r3 = a * c;
  const Eigen::Array<J, 2, 1> r4 = a * J(c);

  ExpectJetsClose(r3(0), r3(0));
  ExpectJetsClose(r4(1), r4(1));
}

TEST(Jet, nested3x) {
  typedef Jet<J, 2> JJ;
  typedef Jet<JJ, 2> JJJ;

  JJJ x;
  x.a = JJ(J(1, 0), 0);
  x.v[0] = JJ(J(1));

  JJJ y = x * x * x;

  ExpectClose(y.a.a.a, 1, kTolerance);
  ExpectClose(y.v[0].a.a, 3., kTolerance);
  ExpectClose(y.v[0].v[0].a, 6., kTolerance);
  ExpectClose(y.v[0].v[0].v[0], 6., kTolerance);

  JJJ e = exp(x);

  ExpectClose(e.a.a.a, kE, kTolerance);
  ExpectClose(e.v[0].a.a, kE, kTolerance);
  ExpectClose(e.v[0].v[0].a, kE, kTolerance);
  ExpectClose(e.v[0].v[0].v[0], kE, kTolerance);
}

}  // namespace internal
}  // namespace ceres
