// Copyright 2022 The Autoware Contributors
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

#include "gnss_module.hpp"

#include <autoware/component_interface_specs/localization.hpp>

#include <autoware_adapi_v1_msgs/msg/response_status.hpp>

#include <memory>

namespace autoware::pose_initializer
{
GnssModule::GnssModule(rclcpp::Node * node)
: fitter_(node),
  clock_(node->get_clock()),
  timeout_(node->declare_parameter<double>("gnss_pose_timeout"))
{
  sub_gnss_pose_ = node->create_subscription<PoseWithCovarianceStamped>(
    "gnss_pose_cov", 1, std::bind(&GnssModule::on_pose, this, std::placeholders::_1));
}

void GnssModule::on_pose(PoseWithCovarianceStamped::ConstSharedPtr msg)
{
  pose_ = msg;
}

geometry_msgs::msg::PoseWithCovarianceStamped GnssModule::get_pose()
{
  using Initialize = autoware::component_interface_specs::localization::Initialize;

  if (!pose_) {
    autoware_adapi_v1_msgs::msg::ResponseStatus respose_status;
    respose_status.success = false;
    respose_status.code = Initialize::Service::Response::ERROR_GNSS;
    respose_status.message = "The GNSS pose has not arrived.";
    throw respose_status;
  }

  const auto elapsed = rclcpp::Time(pose_->header.stamp) - clock_->now();
  if (timeout_ < elapsed.seconds()) {
    autoware_adapi_v1_msgs::msg::ResponseStatus respose_status;
    respose_status.success = false;
    respose_status.code = Initialize::Service::Response::ERROR_GNSS;
    respose_status.message = "The GNSS pose is out of date.";
    throw respose_status;
  }

  PoseWithCovarianceStamped pose = *pose_;
  const auto fitted = fitter_.fit(pose.pose.pose.position, pose.header.frame_id);
  if (fitted) {
    pose.pose.pose.position = fitted.value();
  }
  return pose;
}
}  // namespace autoware::pose_initializer
