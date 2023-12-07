// Copyright 2023 Autoware Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lanelet2_extension/localization/landmark.hpp"

#include "lanelet2_extension/utility/message_conversion.hpp"

#include <Eigen/Core>
#include <tf2_eigen/tf2_eigen.hpp>

#include <lanelet2_core/LaneletMap.h>
#include <lanelet2_core/primitives/Polygon.h>

namespace lanelet::localization
{

std::vector<Landmark> parseLandmarks(
  const autoware_auto_mapping_msgs::msg::HADMapBin::ConstSharedPtr & msg,
  const std::string & target_subtype, const double volume_threshold)
{
  lanelet::LaneletMapPtr lanelet_map_ptr{std::make_shared<lanelet::LaneletMap>()};
  lanelet::utils::conversion::fromBinMsg(*msg, lanelet_map_ptr);

  std::vector<Landmark> landmarks;

  for (const auto & poly : lanelet_map_ptr->polygonLayer) {
    const std::string type{poly.attributeOr(lanelet::AttributeName::Type, "none")};
    if (type != "pose_marker") {
      continue;
    }
    const std::string subtype{poly.attributeOr(lanelet::AttributeName::Subtype, "none")};
    if (subtype != target_subtype) {
      continue;
    }

    // Get landmark_id
    const std::string landmark_id = poly.attributeOr("marker_id", "none");

    // Get 4 vertices
    const auto & vertices = poly.basicPolygon();
    if (vertices.size() != 4) {
      continue;
    }

    // 4 vertices represent the landmark vertices in counterclockwise order
    // Calculate the volume by considering it as a tetrahedron
    const auto & v0 = vertices[0];
    const auto & v1 = vertices[1];
    const auto & v2 = vertices[2];
    const auto & v3 = vertices[3];
    const double volume = (v1 - v0).cross(v2 - v0).dot(v3 - v0) / 6.0;
    if (volume > volume_threshold) {
      continue;
    }

    // Calculate the center of the quadrilateral
    const auto center = (v0 + v1 + v2 + v3) / 4.0;

    // Define axes
    const auto x_axis = (v1 - v0).normalized();
    const auto y_axis = (v2 - v1).normalized();
    const auto z_axis = x_axis.cross(y_axis).normalized();

    // Construct rotation matrix and convert it to quaternion
    Eigen::Matrix3d rotation_matrix;
    rotation_matrix << x_axis, y_axis, z_axis;
    const Eigen::Quaterniond q{rotation_matrix};

    // Create Pose
    geometry_msgs::msg::Pose pose;
    pose.position.x = center.x();
    pose.position.y = center.y();
    pose.position.z = center.z();
    pose.orientation.x = q.x();
    pose.orientation.y = q.y();
    pose.orientation.z = q.z();
    pose.orientation.w = q.w();

    // Add
    landmarks.push_back(Landmark{landmark_id, pose});
  }

  return landmarks;
}

}  // namespace lanelet::localization
