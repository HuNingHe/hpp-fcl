/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
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

/** \author Jia Pan */


#include "fcl/traversal_node_bvhs.h"

namespace fcl
{

namespace details
{
template<typename BV>
static inline void meshCollisionOrientedNodeLeafTesting(int b1, int b2,
                                                        const BVHModel<BV>* model1, const BVHModel<BV>* model2,
                                                        Vec3f* vertices1, Vec3f* vertices2, 
                                                        Triangle* tri_indices1, Triangle* tri_indices2,
                                                        const Matrix3f& R, const Vec3f& T,
                                                        const SimpleTransform& tf1, const SimpleTransform& tf2,
                                                        bool enable_statistics,
                                                        FCL_REAL cost_density,
                                                        int& num_leaf_tests,
                                                        const CollisionRequest& request,
                                                        CollisionResult& result)
{
  if(enable_statistics) num_leaf_tests++;

  const BVNode<BV>& node1 = model1->getBV(b1);
  const BVNode<BV>& node2 = model2->getBV(b2);

  int primitive_id1 = node1.primitiveId();
  int primitive_id2 = node2.primitiveId();

  const Triangle& tri_id1 = tri_indices1[primitive_id1];
  const Triangle& tri_id2 = tri_indices2[primitive_id2];

  const Vec3f& p1 = vertices1[tri_id1[0]];
  const Vec3f& p2 = vertices1[tri_id1[1]];
  const Vec3f& p3 = vertices1[tri_id1[2]];
  const Vec3f& q1 = vertices2[tri_id2[0]];
  const Vec3f& q2 = vertices2[tri_id2[1]];
  const Vec3f& q3 = vertices2[tri_id2[2]];

  if(model1->isOccupied() && model2->isOccupied())
  {
    bool is_intersect = false;

    if(!request.enable_contact) // only interested in collision or not
    {
      if(Intersect::intersect_Triangle(p1, p2, p3, q1, q2, q3, R, T))
      {
        is_intersect = true;
        result.addContact(Contact(model1, model2, primitive_id1, primitive_id2));
      }
    }
    else // need compute the contact information
    {
      FCL_REAL penetration;
      Vec3f normal;
      unsigned int n_contacts;
      Vec3f contacts[2];

      if(Intersect::intersect_Triangle(p1, p2, p3, q1, q2, q3,
                                       R, T,
                                       contacts,
                                       &n_contacts,
                                       &penetration,
                                       &normal))
      {
        is_intersect = true;
        
        if(request.num_max_contacts < result.numContacts() + n_contacts)
          n_contacts = (request.num_max_contacts > result.numContacts()) ? (request.num_max_contacts - result.numContacts()) : 0;
        
        for(unsigned int i = 0; i < n_contacts; ++i)
        {
          result.addContact(Contact(model1, model2, primitive_id1, primitive_id2, tf1.transform(contacts[i]), tf1.getQuatRotation().transform(normal), penetration));
        }
      }
    }

    if(is_intersect && request.enable_cost)
    {
      AABB overlap_part;
      AABB(tf1.transform(p1), tf1.transform(p2), tf1.transform(p3)).overlap(AABB(tf2.transform(q1), tf2.transform(q2), tf2.transform(q3)), overlap_part);
      result.addCostSource(CostSource(overlap_part, cost_density));    
    }
  }
  else if((!model1->isFree() && !model2->isFree()) && request.enable_cost)
  {
    if(Intersect::intersect_Triangle(p1, p2, p3, q1, q2, q3, R, T))
    {
      AABB overlap_part;
      AABB(tf1.transform(p1), tf1.transform(p2), tf1.transform(p3)).overlap(AABB(tf2.transform(q1), tf2.transform(q2), tf2.transform(q3)), overlap_part);
      result.addCostSource(CostSource(overlap_part, cost_density));          
    }    
  }
}




template<typename BV>
static inline void meshDistanceOrientedNodeLeafTesting(int b1, int b2,
                                                       const BVHModel<BV>* model1, const BVHModel<BV>* model2,
                                                       Vec3f* vertices1, Vec3f* vertices2, 
                                                       Triangle* tri_indices1, Triangle* tri_indices2,
                                                       const Matrix3f& R, const Vec3f& T,
                                                       bool enable_statistics,
                                                       int& num_leaf_tests,
                                                       const DistanceRequest& request,
                                                       DistanceResult& result)
{
  if(enable_statistics) num_leaf_tests++;

  const BVNode<BV>& node1 = model1->getBV(b1);
  const BVNode<BV>& node2 = model2->getBV(b2);

  int primitive_id1 = node1.primitiveId();
  int primitive_id2 = node2.primitiveId();

  const Triangle& tri_id1 = tri_indices1[primitive_id1];
  const Triangle& tri_id2 = tri_indices2[primitive_id2];

  const Vec3f& t11 = vertices1[tri_id1[0]];
  const Vec3f& t12 = vertices1[tri_id1[1]];
  const Vec3f& t13 = vertices1[tri_id1[2]];

  const Vec3f& t21 = vertices2[tri_id2[0]];
  const Vec3f& t22 = vertices2[tri_id2[1]];
  const Vec3f& t23 = vertices2[tri_id2[2]];

  // nearest point pair
  Vec3f P1, P2;

  FCL_REAL d = TriangleDistance::triDistance(t11, t12, t13, t21, t22, t23,
                                             R, T,
                                             P1, P2);

  if(request.enable_nearest_points)
    result.update(d, model1, model2, primitive_id1, primitive_id2, P1, P2);
  else
    result.update(d, model1, model2, primitive_id1, primitive_id2);
}

}

MeshCollisionTraversalNodeOBB::MeshCollisionTraversalNodeOBB() : MeshCollisionTraversalNode<OBB>()
{
  R.setIdentity();
}

bool MeshCollisionTraversalNodeOBB::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshCollisionTraversalNodeOBB::leafTesting(int b1, int b2) const
{
  details::meshCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, 
                                                tri_indices1, tri_indices2, 
                                                R, T, 
                                                tf1, tf2,
                                                enable_statistics, cost_density,
                                                num_leaf_tests,
                                                request, *result);
}


bool MeshCollisionTraversalNodeOBB::BVTesting(int b1, int b2, const Matrix3f& Rc, const Vec3f& Tc) const
{
  if(enable_statistics) num_bv_tests++;
  return OBB::obbDisjoint(Rc, Tc, model1->getBV(b1).bv.extent, model2->getBV(b2).bv.extent);
}

void MeshCollisionTraversalNodeOBB::leafTesting(int b1, int b2, const Matrix3f& Rc, const Vec3f& Tc) const
{
  details::meshCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, 
                                                tri_indices1, tri_indices2, 
                                                R, T, 
                                                tf1, tf2,
                                                enable_statistics, cost_density,
                                                num_leaf_tests,
                                                request, *result);
}



MeshCollisionTraversalNodeRSS::MeshCollisionTraversalNodeRSS() : MeshCollisionTraversalNode<RSS>()
{
  R.setIdentity();
}

bool MeshCollisionTraversalNodeRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshCollisionTraversalNodeRSS::leafTesting(int b1, int b2) const
{
  details::meshCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, 
                                                tri_indices1, tri_indices2, 
                                                R, T, 
                                                tf1, tf2,
                                                enable_statistics, cost_density,
                                                num_leaf_tests,
                                                request, *result);
}




MeshCollisionTraversalNodekIOS::MeshCollisionTraversalNodekIOS() : MeshCollisionTraversalNode<kIOS>()
{
  R.setIdentity();
}

bool MeshCollisionTraversalNodekIOS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshCollisionTraversalNodekIOS::leafTesting(int b1, int b2) const
{
  details::meshCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, 
                                                tri_indices1, tri_indices2, 
                                                R, T, 
                                                tf1, tf2,
                                                enable_statistics, cost_density,
                                                num_leaf_tests,
                                                request, *result);
}



MeshCollisionTraversalNodeOBBRSS::MeshCollisionTraversalNodeOBBRSS() : MeshCollisionTraversalNode<OBBRSS>()
{
  R.setIdentity();
}

bool MeshCollisionTraversalNodeOBBRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshCollisionTraversalNodeOBBRSS::leafTesting(int b1, int b2) const
{
  details::meshCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, 
                                                tri_indices1, tri_indices2, 
                                                R, T, 
                                                tf1, tf2,
                                                enable_statistics, cost_density,
                                                num_leaf_tests,
                                                request,*result);
}


#if USE_SVMLIGHT


namespace details
{
template<typename BV>
static inline void pointCloudCollisionOrientedNodeLeafTesting(int b1, int b2, 
                                                              const BVHModel<BV>* model1, const BVHModel<BV>* model2,
                                                              Vec3f* vertices1, Vec3f* vertices2,
                                                              const Matrix3f& R, const Vec3f& T,
                                                              bool enable_statistics,
                                                              FCL_REAL collision_prob_threshold,
                                                              const boost::shared_arry<Uncertainty>& uc1, const boost::shared_array<Uncertainty>& uc2,
                                                              const CloudClassifierParam classifier_param,
                                                              int& num_leaf_tests,
                                                              FCL_REAL& max_collision_prob,
                                                              std::vector<BVHPointCollisionPair>& pairs)
{
  if(enable_statistics) num_leaf_tests++;
  
  const BVNode<BV>& node1 = model1->getBV(b1);
  const BVNode<BV>& node2 = model2->getBV(b2);
  
  FCL_REAL collision_prob = Intersect::intersect_PointClouds(vertices1 + node1.first_primitive, uc1.get() + node1.first_primitive,
                                                             node1.num_primitives,
                                                             vertices2 + node2.first_primitive, uc2.get() + node2.first_primitive,
                                                             node2.num_primitives,
                                                             R, T,
                                                             classifier_param);

  if(collision_prob > collision_prob_threshold)
    pairs.push_back(BVHPointCollisionPair(node1.first_primitive, node1.num_primitives, node2.first_primitive, node2.num_primitives, collision_prob));


  if(collision_prob > max_collision_prob)
    max_collision_prob = collision_prob;
}

}

PointCloudCollisionTraversalNodeOBB::PointCloudCollisionTraversalNodeOBB() : PointCloudCollisionTraversalNode<OBB>()
{
  R.setIdentity();
  // default T is 0
}

bool PointCloudCollisionTraversalNodeOBB::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void PointCloudCollisionTraversalNodeOBB::leafTesting(int b1, int b2) const
{
  details::pointCloudCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2,
                                                      R, T, 
                                                      enable_statistics, 
                                                      collision_prob_threshold,
                                                      uc1, uc2,
                                                      classifier_param,
                                                      num_leaf_tests,
                                                      max_collision_prob,
                                                      pairs);
}

PointCloudCollisionTraversalNodeRSS::PointCloudCollisionTraversalNodeRSS() : PointCloudCollisionTraversalNode<RSS>()
{
  R.setIdentity();
  // default T is 0
}

bool PointCloudCollisionTraversalNodeRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void PointCloudCollisionTraversalNodeRSS::leafTesting(int b1, int b2) const
{
  details::pointCloudCollisionOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2,
                                                      R, T, 
                                                      enable_statistics, 
                                                      collision_prob_threshold,
                                                      uc1, uc2,
                                                      classifier_param,
                                                      num_leaf_tests,
                                                      max_collision_prob,
                                                      pairs);
}


namespace details
{

template<typename BV>
static inline void pointCloudMeshCollisionOrientedNodeLeafTesting(int b1, int b2,
                                                                  const BVHModel<BV>* model1, const BVHModel<BV>* model2,
                                                                  Vec3f* vertices1, Vec3f* vertices2,
                                                                  Triangle* tri_indices2,
                                                                  const Matrix3f& R, const Vec3f& T,
                                                                  bool enable_statistics,
                                                                  FCL_REAL collision_prob_threshold,
                                                                  const boost::shared_array<Uncertainty>& uc1,
                                                                  int& num_leaf_tests,
                                                                  FCL_REAL& max_collision_prob,
                                                                  std::vector<BVHPointCollisionPair>& pairs)
{
  if(enable_statistics) num_leaf_tests++;

  const BVNode<OBB>& node1 = model1->getBV(b1);
  const BVNode<OBB>& node2 = model2->getBV(b2);


  const Triangle& tri_id2 = tri_indices2[node2.primitiveId()];

  const Vec3f& q1 = vertices2[tri_id2[0]];
  const Vec3f& q2 = vertices2[tri_id2[1]];
  const Vec3f& q3 = vertices2[tri_id2[2]];

  FCL_REAL collision_prob = Intersect::intersect_PointCloudsTriangle(vertices1 + node1.first_primitive, uc1.get() + node1.first_primitive,
                                                                     node1.num_primitives,
                                                                     q1, q2, q3,
                                                                     R, T);

  if(collision_prob > collision_prob_threshold)
    pairs.push_back(BVHPointCollisionPair(node1.first_primitive, node1.num_primitives, node2.first_primitive, node2.num_primitives, collision_prob));

  if(collision_prob > max_collision_prob)
    max_collision_prob = collision_prob;
}

}


PointCloudMeshCollisionTraversalNodeOBB::PointCloudMeshCollisionTraversalNodeOBB() : PointCloudMeshCollisionTraversalNode<OBB>()
{
  R.setIdentity();
  // default T is 0
}

bool PointCloudMeshCollisionTraversalNodeOBB::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void PointCloudMeshCollisionTraversalNodeOBB::leafTesting(int b1, int b2) const
{
  details::pointCloudMeshCollisionOrientedNodeLeafTesting(b1, b2,
                                                          model1, model2,
                                                          vertices1, vertices2,
                                                          tri_indices2,
                                                          R, T,
                                                          enable_statistics, collision_prob_threshold, uc1,
                                                          num_leaf_tests, max_collision_prob, pairs);
}

PointCloudMeshCollisionTraversalNodeRSS::PointCloudMeshCollisionTraversalNodeRSS() : PointCloudMeshCollisionTraversalNode<RSS>()
{
  R.setIdentity();
  // default T is 0
}

bool PointCloudMeshCollisionTraversalNodeRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return !overlap(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void PointCloudMeshCollisionTraversalNodeRSS::leafTesting(int b1, int b2) const
{
  details::pointCloudMeshCollisionOrientedNodeLeafTesting(b1, b2,
                                                          model1, model2,
                                                          vertices1, vertices2,
                                                          tri_indices2,
                                                          R, T,
                                                          enable_statistics, collision_prob_threshold, uc1,
                                                          num_leaf_tests, max_collision_prob, pairs);
}

#endif

namespace details
{

template<typename BV>
static inline void distancePreprocessOrientedNode(const BVHModel<BV>* model1, const BVHModel<BV>* model2,
                                                  const Vec3f* vertices1, Vec3f* vertices2,
                                                  Triangle* tri_indices1, Triangle* tri_indices2,
                                                  int init_tri_id1, int init_tri_id2,
                                                  const Matrix3f& R, const Vec3f& T,
                                                  const DistanceRequest& request,
                                                  DistanceResult& result)
{
  const Triangle& init_tri1 = tri_indices1[init_tri_id1];
  const Triangle& init_tri2 = tri_indices2[init_tri_id2];

  Vec3f init_tri1_points[3];
  Vec3f init_tri2_points[3];

  init_tri1_points[0] = vertices1[init_tri1[0]];
  init_tri1_points[1] = vertices1[init_tri1[1]];
  init_tri1_points[2] = vertices1[init_tri1[2]];

  init_tri2_points[0] = vertices2[init_tri2[0]];
  init_tri2_points[1] = vertices2[init_tri2[1]];
  init_tri2_points[2] = vertices2[init_tri2[2]];

  Vec3f p1, p2;
  FCL_REAL distance = TriangleDistance::triDistance(init_tri1_points[0], init_tri1_points[1], init_tri1_points[2],
                                                    init_tri2_points[0], init_tri2_points[1], init_tri2_points[2],
                                                    R, T, p1, p2);

  if(request.enable_nearest_points)
    result.update(distance, model1, model2, init_tri_id1, init_tri_id2, p1, p2);
  else
    result.update(distance, model1, model2, init_tri_id1, init_tri_id2);
}

template<typename BV>
static inline void distancePostprocessOrientedNode(const BVHModel<BV>* model1, const BVHModel<BV>* model2,
                                                   const SimpleTransform& tf1, const DistanceRequest& request, DistanceResult& result)
{
  /// the points obtained by triDistance are not in world space: both are in object1's local coordinate system, so we need to convert them into the world space.
  if(request.enable_nearest_points && (result.o1 == model1) && (result.o2 == model2))
  {
    result.nearest_points[0] = tf1.transform(result.nearest_points[0]);
    result.nearest_points[1] = tf1.transform(result.nearest_points[1]);
  }
}

}

MeshDistanceTraversalNodeRSS::MeshDistanceTraversalNodeRSS() : MeshDistanceTraversalNode<RSS>()
{
  R.setIdentity();
}

void MeshDistanceTraversalNodeRSS::preprocess()
{
  details::distancePreprocessOrientedNode(model1, model2, vertices1, vertices2, tri_indices1, tri_indices2, 0, 0, R, T, request, *result);
}

void MeshDistanceTraversalNodeRSS::postprocess()
{
  details::distancePostprocessOrientedNode(model1, model2, tf1, request, *result);
}

FCL_REAL MeshDistanceTraversalNodeRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return distance(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshDistanceTraversalNodeRSS::leafTesting(int b1, int b2) const
{
  details::meshDistanceOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, tri_indices1, tri_indices2, 
                                               R, T, enable_statistics, num_leaf_tests, 
                                               request, *result);
}

MeshDistanceTraversalNodekIOS::MeshDistanceTraversalNodekIOS() : MeshDistanceTraversalNode<kIOS>()
{
  R.setIdentity();
}

void MeshDistanceTraversalNodekIOS::preprocess()
{
  details::distancePreprocessOrientedNode(model1, model2, vertices1, vertices2, tri_indices1, tri_indices2, 0, 0, R, T, request, *result);
}

void MeshDistanceTraversalNodekIOS::postprocess()
{
  details::distancePostprocessOrientedNode(model1, model2, tf1, request, *result);
}

FCL_REAL MeshDistanceTraversalNodekIOS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return distance(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshDistanceTraversalNodekIOS::leafTesting(int b1, int b2) const
{
  details::meshDistanceOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, tri_indices1, tri_indices2, 
                                               R, T, enable_statistics, num_leaf_tests, 
                                               request, *result);
}

MeshDistanceTraversalNodeOBBRSS::MeshDistanceTraversalNodeOBBRSS() : MeshDistanceTraversalNode<OBBRSS>()
{
  R.setIdentity();
}

void MeshDistanceTraversalNodeOBBRSS::preprocess()
{
  details::distancePreprocessOrientedNode(model1, model2, vertices1, vertices2, tri_indices1, tri_indices2, 0, 0, R, T, request, *result);
}

void MeshDistanceTraversalNodeOBBRSS::postprocess()
{
  details::distancePostprocessOrientedNode(model1, model2, tf1, request, *result);
}

FCL_REAL MeshDistanceTraversalNodeOBBRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  return distance(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv);
}

void MeshDistanceTraversalNodeOBBRSS::leafTesting(int b1, int b2) const
{
  details::meshDistanceOrientedNodeLeafTesting(b1, b2, model1, model2, vertices1, vertices2, tri_indices1, tri_indices2, 
                                               R, T, enable_statistics, num_leaf_tests, 
                                               request, *result);
}


/** for OBB and RSS, there is local coordinate of BV, so normal need to be transformed */
template<>
bool MeshConservativeAdvancementTraversalNode<OBB>::canStop(FCL_REAL c) const
{
  if((c >= w * (this->min_distance - this->abs_err)) && (c * (1 + this->rel_err) >= w * this->min_distance))
  {
    const ConservativeAdvancementStackData& data = stack.back();
    FCL_REAL d = data.d;
    Vec3f n;
    int c1, c2;

    if(d > c)
    {
      const ConservativeAdvancementStackData& data2 = stack[stack.size() - 2];
      d = data2.d;
      n = data2.P2 - data2.P1;
      c1 = data2.c1;
      c2 = data2.c2;
      stack[stack.size() - 2] = stack[stack.size() - 1];
    }
    else
    {
      n = data.P2 - data.P1;
      c1 = data.c1;
      c2 = data.c2;
    }

    assert(c == d);

    Vec3f n_transformed = model1->getBV(c1).bv.axis[0] * n[0] + model1->getBV(c1).bv.axis[1] * n[1] +  model1->getBV(c1).bv.axis[2] * n[2];

    FCL_REAL bound1 = motion1->computeMotionBound(this->model1->getBV(c1).bv, n_transformed);
    FCL_REAL bound2 = motion2->computeMotionBound(this->model2->getBV(c2).bv, n_transformed);

    FCL_REAL bound = bound1 + bound2;

    FCL_REAL cur_delta_t;
    if(bound <= c) cur_delta_t = 1;
    else cur_delta_t = c / bound;

    if(cur_delta_t < delta_t)
      delta_t = cur_delta_t;

    stack.pop_back();

    return true;
  }
  else
  {
    const ConservativeAdvancementStackData& data = stack.back();
    FCL_REAL d = data.d;

    if(d > c)
      stack[stack.size() - 2] = stack[stack.size() - 1];

    stack.pop_back();

    return false;
  }
}

template<>
bool MeshConservativeAdvancementTraversalNode<RSS>::canStop(FCL_REAL c) const
{
  if((c >= w * (this->min_distance - this->abs_err)) && (c * (1 + this->rel_err) >= w * this->min_distance))
  {
    const ConservativeAdvancementStackData& data = stack.back();
    FCL_REAL d = data.d;
    Vec3f n;
    int c1, c2;

    if(d > c)
    {
      const ConservativeAdvancementStackData& data2 = stack[stack.size() - 2];
      d = data2.d;
      n = data2.P2 - data2.P1;
      c1 = data2.c1;
      c2 = data2.c2;
      stack[stack.size() - 2] = stack[stack.size() - 1];
    }
    else
    {
      n = data.P2 - data.P1;
      c1 = data.c1;
      c2 = data.c2;
    }

    assert(c == d);

    Vec3f n_transformed = model1->getBV(c1).bv.axis[0] * n[0] + model1->getBV(c1).bv.axis[1] * n[1] +  model1->getBV(c1).bv.axis[2] * n[2];

    FCL_REAL bound1 = motion1->computeMotionBound(this->model1->getBV(c1).bv, n_transformed);
    FCL_REAL bound2 = motion2->computeMotionBound(this->model2->getBV(c2).bv, n_transformed);

    FCL_REAL bound = bound1 + bound2;

    FCL_REAL cur_delta_t;
    if(bound <= c) cur_delta_t = 1;
    else cur_delta_t = c / bound;

    if(cur_delta_t < delta_t)
      delta_t = cur_delta_t;

    stack.pop_back();

    return true;
  }
  else
  {
    const ConservativeAdvancementStackData& data = stack.back();
    FCL_REAL d = data.d;

    if(d > c)
      stack[stack.size() - 2] = stack[stack.size() - 1];

    stack.pop_back();

    return false;
  }
}


MeshConservativeAdvancementTraversalNodeRSS::MeshConservativeAdvancementTraversalNodeRSS(FCL_REAL w_) : MeshConservativeAdvancementTraversalNode<RSS>(w_)
{
  R.setIdentity();
  // default T is 0
}

FCL_REAL MeshConservativeAdvancementTraversalNodeRSS::BVTesting(int b1, int b2) const
{
  if(enable_statistics) num_bv_tests++;
  Vec3f P1, P2;
  FCL_REAL d = distance(R, T, model1->getBV(b1).bv, model2->getBV(b2).bv, &P1, &P2);

  stack.push_back(ConservativeAdvancementStackData(P1, P2, b1, b2, d));

  return d;
}


void MeshConservativeAdvancementTraversalNodeRSS::leafTesting(int b1, int b2) const
{
  if(enable_statistics) num_leaf_tests++;

  const BVNode<RSS>& node1 = model1->getBV(b1);
  const BVNode<RSS>& node2 = model2->getBV(b2);

  int primitive_id1 = node1.primitiveId();
  int primitive_id2 = node2.primitiveId();

  const Triangle& tri_id1 = tri_indices1[primitive_id1];
  const Triangle& tri_id2 = tri_indices2[primitive_id2];

  const Vec3f& t11 = vertices1[tri_id1[0]];
  const Vec3f& t12 = vertices1[tri_id1[1]];
  const Vec3f& t13 = vertices1[tri_id1[2]];

  const Vec3f& t21 = vertices2[tri_id2[0]];
  const Vec3f& t22 = vertices2[tri_id2[1]];
  const Vec3f& t23 = vertices2[tri_id2[2]];

  // nearest point pair
  Vec3f P1, P2;

  FCL_REAL d = TriangleDistance::triDistance(t11, t12, t13, t21, t22, t23,
                                             R, T,
                                             P1, P2);

  if(d < min_distance)
  {
    min_distance = d;

    p1 = P1;
    p2 = P2;

    last_tri_id1 = primitive_id1;
    last_tri_id2 = primitive_id2;
  }


  /** n is the local frame of object 1, pointing from object 1 to object2 */
  Vec3f n = P2 - P1;
  /** turn n into the global frame, pointing from object 1 to object 2 */
  Matrix3f R0;
  motion1->getCurrentRotation(R0);
  Vec3f n_transformed = R0 * n;
  n_transformed.normalize();
  FCL_REAL bound1 = motion1->computeMotionBound(t11, t12, t13, n_transformed);
  FCL_REAL bound2 = motion2->computeMotionBound(t21, t22, t23, -n_transformed);

  FCL_REAL bound = bound1 + bound2;

  FCL_REAL cur_delta_t;
  if(bound <= d) cur_delta_t = 1;
  else cur_delta_t = d / bound;

  if(cur_delta_t < delta_t)
    delta_t = cur_delta_t;
}

bool MeshConservativeAdvancementTraversalNodeRSS::canStop(FCL_REAL c) const
{
  if((c >= w * (min_distance - abs_err)) && (c * (1 + rel_err) >= w * min_distance))
  {
    const ConservativeAdvancementStackData& data = stack.back();
    FCL_REAL d = data.d;
    Vec3f n;
    int c1, c2;

    if(d > c)
    {
      const ConservativeAdvancementStackData& data2 = stack[stack.size() - 2];
      d = data2.d;
      n = data2.P2 - data2.P1;
      c1 = data2.c1;
      c2 = data2.c2;
      stack[stack.size() - 2] = stack[stack.size() - 1];
    }
    else
    {
      n = data.P2 - data.P1;
      c1 = data.c1;
      c2 = data.c2;
    }

    assert(c == d);

    // n is in local frame of RSS c1, so we need to turn n into the global frame
    Vec3f n_transformed = model1->getBV(c1).bv.axis[0] * n[0] + model1->getBV(c1).bv.axis[1] * n[2] + model1->getBV(c1).bv.axis[2] * n[2];
    Matrix3f R0;
    motion1->getCurrentRotation(R0);
    n_transformed = R0 * n_transformed;
    n_transformed.normalize();

    FCL_REAL bound1 = motion1->computeMotionBound(model1->getBV(c1).bv, n_transformed);
    FCL_REAL bound2 = motion2->computeMotionBound(model2->getBV(c2).bv, -n_transformed);

    FCL_REAL bound = bound1 + bound2;

    FCL_REAL cur_delta_t;
    if(bound <= c) cur_delta_t = 1;
    else cur_delta_t = c / bound;

    if(cur_delta_t < delta_t)
      delta_t = cur_delta_t;

    stack.pop_back();

    return true;
  }
  else
  {
    const ConservativeAdvancementStackData& data = stack.back();
    FCL_REAL d = data.d;

    if(d > c)
      stack[stack.size() - 2] = stack[stack.size() - 1];

    stack.pop_back();

    return false;
  }
}





}
