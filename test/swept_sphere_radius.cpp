/*
 *  Software License Agreement (BSD License)
 *
 *  Copyright (c) 2024, INRIA
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Louis Montaut */

#define BOOST_TEST_MODULE SWEPT_SPHERE_RADIUS
#include <boost/test/included/unit_test.hpp>

#include <hpp/fcl/narrowphase/narrowphase.h>
#include <hpp/fcl/collision_utility.h>

#include <hpp/fcl/serialization/geometric_shapes.h>
#include <hpp/fcl/serialization/convex.h>
#include <hpp/fcl/serialization/transform.h>
#include <hpp/fcl/serialization/archive.h>

#include "utility.h"

using namespace hpp::fcl;

NODE_TYPE node1_type;
NODE_TYPE node2_type;
int line;

#define SET_LINE                     \
  node1_type = shape1.getNodeType(); \
  node2_type = shape2.getNodeType(); \
  line = __LINE__

#define HPP_FCL_CHECK(cond)                                                    \
  BOOST_CHECK_MESSAGE(                                                         \
      cond, "from line " << line << ", for collision pair: "                   \
                         << get_node_type_name(node1_type) << " - "            \
                         << get_node_type_name(node2_type)                     \
                         << " with inflation1 = "                              \
                         << shape1.getSweptSphereRadius() << ", inflation2 = " \
                         << shape2.getSweptSphereRadius() << ": " #cond)

#define HPP_FCL_CHECK_VECTOR_CLOSE(v1, v2, tol) \
  EIGEN_VECTOR_IS_APPROX(v1, v2, tol);          \
  HPP_FCL_CHECK(((v1) - (v2)).isZero(tol))

#define HPP_FCL_CHECK_REAL_CLOSE(v1, v2, tol) \
  FCL_REAL_IS_APPROX(v1, v2, tol);            \
  HPP_FCL_CHECK(std::abs((v1) - (v2)) < tol)

#define HPP_FCL_CHECK_CONDITION(cond) \
  BOOST_CHECK(cond);                  \
  HPP_FCL_CHECK(cond)

// Preambule: swept sphere radius allows to virually inflate geometric shapes
// by a positive value.
// Sweeping a shape by a sphere corresponds to doing a Minkowski addition of the
// shape with a sphere of radius r. Essentially, this rounds the shape's corners
// and edges, which can be useful to smooth collision detection algorithms.
//
// A nice mathematical property of GJK and EPA is that it is not
// necessary to take into account the swept sphere radius in the iterations of
// the algorithms. This is because the GJK and EPA algorithms are based on the
// Minkowski difference of the two objects.
// With spheres of radii r1 and r2 swept around the shapes s1 and s2 of a
// collision pair, the Minkowski difference is simply the Minkowski difference
// of s1 and s2 inflated by a sphere of radius r1 + r2.
// This means that running GJK and EPA on the swept-sphere shapes is equivalent
// to running GJK and EPA on the original shapes, and then inflating the
// distance by r1 + r2.
// This inflation does not modify the normal returned by GJK and EPA.
// So we can also easily recover the witness points of the swept sphere shapes.
//
// This suite of test is designed to verify that property and generally test for
// swept-sphere radius support in hpp-fcl.
// Notes:
//   - not all collision pairs use GJK/EPA, so this test makes sure that
//     swept-sphere radius is taken into account even for specialized collision
//     algorithms.
//   - when manually taking swept-sphere radius into account in GJK/EPA, we
//     strongly deteriorate the convergence properties of the algorithms. This
//     is because certain parts of the shapes become locally strictly convex,
//     which GJK/EPA are not designed to handle. This becomes particularly the
//     bigger the swept-sphere radius. So don't be surprised if the tests fail
//     for large radii.

struct SweptSphereGJKSolver : public GJKSolver {
  template <typename S1, typename S2>
  bool shapeDistance(const S1& s1, const Transform3f& tf1, const S2& s2,
                     const Transform3f& tf2, FCL_REAL& distance,
                     bool compute_penetration, Vec3f& p1, Vec3f& p2,
                     Vec3f& normal,
                     bool use_swept_sphere_radius_in_gjk_epa_iterations) const {
    if (use_swept_sphere_radius_in_gjk_epa_iterations) {
      return this->runGJKAndEPA<S1, S2, true>(
          s1, tf1, s2, tf2, distance, compute_penetration, p1, p2, normal);
    }

    // Default behavior of hppfcl's GJKSolver
    return this->runGJKAndEPA<S1, S2, false>(
        s1, tf1, s2, tf2, distance, compute_penetration, p1, p2, normal);
  }
};

template <typename S1, typename S2>
void test_gjksolver_swept_sphere_radius(S1& shape1, S2& shape2,
                                        const Transform3f& tf1,
                                        const Transform3f& tf2) {
  SweptSphereGJKSolver solver;
  // The swept sphere radius is detrimental to the convergence of GJK
  // and EPA. This gets worse as the radius of the swept sphere increases.
  // So we need to increase the number of iterations to get a good result.
  const FCL_REAL tol = 1e-6;
  solver.gjk_tolerance = tol;
  solver.epa_tolerance = tol;
  solver.epa_max_iterations = 1000;
  const bool compute_penetration = true;

  std::array<FCL_REAL, 2> distance;
  std::array<Vec3f, 2> p1;
  std::array<Vec3f, 2> p2;
  std::array<Vec3f, 2> normal;

  // Default hppfcl behavior - Don't take swept sphere radius into account
  // during GJK/EPA iterations. Correct the solution afterwards.
  solver.shapeDistance(shape1, tf1, shape2, tf2, distance[0],
                       compute_penetration, p1[0], p2[0], normal[0], false);

  // Take swept sphere radius into account during GJK/EPA iterations
  solver.shapeDistance(shape1, tf1, shape2, tf2, distance[1],
                       compute_penetration, p1[1], p2[1], normal[1], true);

  // Precision is dependent on the inflation.
  // The issue of precision does not come from the default behavior of hppfcl,
  // but from the result in which we manually take the swept sphere radius into
  // account in GJK/EPA iterations.
  const FCL_REAL precision =
      3 * sqrt(tol) + (1 / 100.0) * std::max(shape1.getSweptSphereRadius(),
                                             shape2.getSweptSphereRadius());

  // Check that the distance is the same
  HPP_FCL_CHECK_REAL_CLOSE(distance[0], distance[1], precision);

  // Check that the normal is the same
  HPP_FCL_CHECK_CONDITION(normal[0].dot(normal[1]) > 0);
  HPP_FCL_CHECK_CONDITION(std::abs(1 - normal[0].dot(normal[1])) < precision);

  // Check that the witness points are the same
  HPP_FCL_CHECK_VECTOR_CLOSE(p1[0], p1[1], precision);
  HPP_FCL_CHECK_VECTOR_CLOSE(p2[0], p2[1], precision);
  if (!((p1[0] - p1[1]).isZero(precision))) {
    std::cout << "(p1[0] - p1[1]).norm() = " << (p1[0] - p1[1]).norm()
              << std::endl;
  }
  if (!((p2[0] - p2[1]).isZero(precision))) {
    std::cout << "(p2[0] - p2[1]).norm() = " << (p2[0] - p2[1]).norm()
              << std::endl;
  }
}

static const FCL_REAL min_shape_size = 0.1;
static const FCL_REAL max_shape_size = 0.5;
static const std::array<FCL_REAL, 4> inflations = {0, 0.1, 1., 10.};

BOOST_AUTO_TEST_CASE(ssr_mesh_mesh) {
  Convex<Triangle> shape1 = makeRandomConvex(min_shape_size, max_shape_size);
  Convex<Triangle> shape2 = makeRandomConvex(min_shape_size, max_shape_size);

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_mesh_ellipsoid) {
  Convex<Triangle> shape1 = makeRandomConvex(min_shape_size, max_shape_size);
  Ellipsoid shape2 = makeRandomEllipsoid(min_shape_size, max_shape_size);

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_box_box) {
  Box shape1 = makeRandomBox(min_shape_size, max_shape_size);
  Box shape2 = makeRandomBox(min_shape_size, max_shape_size);

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_ellipsoid_ellipsoid) {
  Ellipsoid shape1 = makeRandomEllipsoid(min_shape_size, max_shape_size);
  Ellipsoid shape2 = makeRandomEllipsoid(min_shape_size, max_shape_size);

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_ellipsoid_box) {
  Ellipsoid shape1 = makeRandomEllipsoid(min_shape_size, max_shape_size);
  Box shape2 = makeRandomBox(min_shape_size, max_shape_size);

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_cone_cone) {
  Cone shape1 = makeRandomCone({min_shape_size / 2, min_shape_size},
                               {max_shape_size, max_shape_size});
  Cone shape2 = makeRandomCone({min_shape_size / 2, min_shape_size},
                               {max_shape_size, max_shape_size});

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_cone_ellipsoid) {
  Cone shape1 = makeRandomCone({min_shape_size / 2, min_shape_size},
                               {max_shape_size, max_shape_size});
  Ellipsoid shape2 = makeRandomEllipsoid(min_shape_size, max_shape_size);

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_capsule_capsule) {
  Capsule shape1 = makeRandomCapsule({min_shape_size / 2, min_shape_size},
                                     {max_shape_size, max_shape_size});
  Capsule shape2 = makeRandomCapsule({min_shape_size / 2, min_shape_size},
                                     {max_shape_size, max_shape_size});

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_capsule_cone) {
  Capsule shape1 = makeRandomCapsule({min_shape_size / 2, min_shape_size},
                                     {max_shape_size, max_shape_size});
  Cone shape2 = makeRandomCone({min_shape_size / 2, min_shape_size},
                               {max_shape_size, max_shape_size});

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(ssr_cylinder_cylinder) {
  Cylinder shape1 = makeRandomCylinder({min_shape_size / 2, min_shape_size},
                                       {max_shape_size, max_shape_size});
  Cylinder shape2 = makeRandomCylinder({min_shape_size / 2, min_shape_size},
                                       {max_shape_size, max_shape_size});

  FCL_REAL extents[] = {-2, -2, -2, 2, 2, 2};
  std::size_t n = 10;
  std::vector<Transform3f> tf1s;
  std::vector<Transform3f> tf2s;
  generateRandomTransforms(extents, tf1s, n);
  generateRandomTransforms(extents, tf2s, n);

  for (const FCL_REAL& inflation1 : inflations) {
    shape1.setSweptSphereRadius(inflation1);
    for (const FCL_REAL& inflation2 : inflations) {
      shape2.setSweptSphereRadius(inflation2);
      for (std::size_t i = 0; i < n; ++i) {
        Transform3f tf1 = tf1s[i];
        Transform3f tf2 = tf2s[i];
        SET_LINE;
        test_gjksolver_swept_sphere_radius(shape1, shape2, tf1, tf2);
      }
    }
  }
}
