#include <gd_ik/frame.hpp>
#include <gd_ik/goal.hpp>
#include <gd_ik/robot.hpp>

#include <cmath>
#include <moveit/kinematics_base/kinematics_base.h>
#include <moveit/robot_model/joint_model_group.h>
#include <moveit/robot_model/robot_model.h>
#include <moveit/robot_state/robot_state.h>
#include <vector>

namespace gd_ik {

auto make_pose_cost_fn(Frame goal, size_t goal_link_index,
                       double rotation_scale) -> CostFn {
  return [=](std::vector<Frame> const& tip_frames,
             std::vector<double> const&) -> double {
    auto const& frame = tip_frames[goal_link_index];
    return frame.pos.distance2(goal.pos) +
           fmin((frame.rot - goal.rot).length2(),
                (frame.rot + goal.rot).length2()) *
               (rotation_scale * rotation_scale);
  };
}

auto make_center_joints_cost_fn(
    Robot robot, std::vector<size_t> active_variable_indexes,
    std::vector<double> minimal_displacement_factors) -> CostFn {
  return [=](std::vector<Frame> const&,
             std::vector<double> const& active_positions) -> double {
    double sum = 0;
    assert(active_positions.size() == active_variable_indexes.size() &&
           active_positions.size() == minimal_displacement_factors.size());
    for (size_t i = 0; i < active_positions.size(); ++i) {
      auto const index = active_variable_indexes[i];
      if (get_clip_max(robot, index) == DBL_MAX) {
        continue;
      }

      auto const position = active_positions[i];
      auto const weight = minimal_displacement_factors[i];
      auto const min = get_min(robot, index);
      auto const max = get_max(robot, index);
      auto const mid = (min + max) * 0.5;
      sum += std::pow((position - mid) * weight, 2);
    }
    return sum;
  };
}

auto make_avoid_joint_limits_cost_fn(
    Robot robot, std::vector<size_t> active_variable_indexes,
    std::vector<double> minimal_displacement_factors) -> CostFn {
  return [=](std::vector<Frame> const&,
             std::vector<double> const& active_positions) -> double {
    double sum = 0;
    assert(active_positions.size() == active_variable_indexes.size() &&
           active_positions.size() == minimal_displacement_factors.size());
    for (size_t i = 0; i < active_positions.size(); ++i) {
      auto const index = active_variable_indexes[i];
      if (get_clip_max(robot, index) == DBL_MAX) {
        continue;
      }

      auto const position = active_positions[i];
      auto const weight = minimal_displacement_factors[i];
      auto const min = get_min(robot, index);
      auto const max = get_max(robot, index);
      auto const mid = (min + max) * 0.5;
      auto const span = get_span(robot, index);
      sum += std::pow(
          std::fmax(0.0, std::fabs(position - mid) * 2.0 - span * 0.5) * weight,
          2);
    }
    return sum;
  };
}

auto make_minimal_displacement_cost_fn(
    std::vector<double> initial_guess,
    std::vector<double> minimal_displacement_factors) -> CostFn {
  return [=](std::vector<Frame> const&,
             std::vector<double> const& active_positions) -> double {
    double sum = 0;
    assert(active_positions.size() == initial_guess.size() &&
           active_positions.size() == minimal_displacement_factors.size());
    for (size_t i = 0; i < active_positions.size(); ++i) {
      auto const guess = initial_guess[i];
      auto const position = active_positions[i];
      auto const weight = minimal_displacement_factors[i];
      sum += std::pow((position - guess) * weight, 2);
    }
    return sum;
  };
}

auto make_ik_cost_fn(geometry_msgs::msg::Pose pose,
                     kinematics::KinematicsBase::IKCostFn cost_fn,
                     std::shared_ptr<moveit::core::RobotModel> robot_model,
                     moveit::core::JointModelGroup* jmg,
                     std::vector<double> initial_guess) -> CostFn {
  return [=](std::vector<Frame> const&,
             std::vector<double> const& active_positions) -> double {
    auto robot_state = moveit::core::RobotState(robot_model);
    robot_state.setJointGroupPositions(jmg, active_positions);
    robot_state.update();
    return cost_fn(pose, robot_state, jmg, initial_guess);
  };
}

}  // namespace gd_ik
