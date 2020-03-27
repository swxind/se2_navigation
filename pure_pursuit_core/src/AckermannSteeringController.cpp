/*
 * AckermannSteeringController.cpp
 *
 *  Created on: Mar 21, 2020
 *      Author: jelavice
 */

#include "pure_pursuit_core/heading_control/AckermannSteeringController.hpp"

#include <iostream>

#include "pure_pursuit_core/math.hpp"

namespace pure_pursuit {

bool AckermannSteeringController::advanceImpl() {
  std::cout << "Current Robot state: " << currentRobotState_ << std::endl;

  chooseActiveAnchorAndLookaheadDistance(parameters_);
  const auto drivingDirection = currentPathSegment_.drivingDirection_;
  const auto& robotPose = currentRobotState_.pose_;
  const Point anchorPoint = computeAnchorPoint(currentRobotState_, activeAnchorDistance_, drivingDirection);
  const unsigned int closestPointOnPathId = getIdOfTheClosestPointOnThePath(currentPathSegment_, robotPose.position_, lastClosestPointId_);

  std::cout << "Active lookahead: " << activeLookaheadDistance_ << std::endl;
  std::cout << "Active anchor: " << activeAnchorDistance_ << std::endl;
  std::cout << "Driving direction: " << static_cast<int>(drivingDirection) << std::endl;
  std::cout << "AnchorPoint: " << anchorPoint.transpose() << std::endl;
  std::cout << "closest id: " << closestPointOnPathId << std::endl;
  Point lookaheadPoint;
  if (!computeLookaheadPoint(closestPointOnPathId, activeLookaheadDistance_, currentRobotState_, drivingDirection, currentPathSegment_,
                             &lookaheadPoint)) {
    return false;
  }
  std::cout << "lookahead point: " << lookaheadPoint.transpose() << std::endl;
  double lookaheadAngle = 0.0;
  const auto heading = computeDesiredHeadingVector(robotPose.yaw_, drivingDirection);
  if (!computeLookaheadAngle(lookaheadPoint, anchorPoint, heading, drivingDirection, &lookaheadAngle)) {
    return false;
  }
  std::cout << "lookahead angle: " << lookaheadAngle << std::endl;

  const double steeringAngle =
      computeSteeringAngleCmd(lookaheadAngle, activeLookaheadDistance_, activeAnchorDistance_, parameters_.wheelBase_);

  std::cout << "Steering angle: " << steeringAngle << std::endl;
  if (std::isnan(steeringAngle) || std::isinf(steeringAngle)) {
    return false;
  }

  const double filtered = avgFilter_.filterInputValue(steeringAngle);
  const double deadZoned = deadZone(filtered, parameters_.deadZoneWidth_);
  const double rateLimited = rateLimiter_.limitRateOfChange(deadZoned);
  const double boundToRange = bindToRange(rateLimited, -parameters_.maxSteeringAngleMagnitude_, parameters_.maxSteeringAngleMagnitude_);
  steeringAngle_ = boundToRange;

  lastClosestPointId_ = closestPointOnPathId;
  return true;
}

bool AckermannSteeringController::initializeImpl() {
  lastClosestPointId_ = 0.0;
  appendPointAlongFinalApproachDirection(activeLookaheadDistance_ * 5.0, &currentPathSegment_);
  return true;
}

bool AckermannSteeringController::computeSteeringAngle() {
  return true;
}
bool AckermannSteeringController::computeYawRate() {
  yawRate_ = currentRobotState_.desiredLongitudinalVelocity_ / parameters_.wheelBase_ * std::tan(steeringAngle_);
  return true;
}
bool AckermannSteeringController::computeTurningRadius() {
  const double yawRate = currentRobotState_.desiredLongitudinalVelocity_ / parameters_.wheelBase_ * std::tan(steeringAngle_);
  turningRadius_ = currentRobotState_.desiredLongitudinalVelocity_ / yawRate;
  return true;
}

void AckermannSteeringController::setParameters(const AckermannSteeringCtrlParameters& parameters) {
  if (parameters.anchorDistanceBck_ < 0) {
    throw std::runtime_error("anchorDistanceBck_ is less than 0.");
  }

  if (parameters.anchorDistanceFwd_ < 0) {
    throw std::runtime_error("anchorDistanceFwd_ is less than 0.");
  }

  if (parameters.lookaheadDistanceBck_ < 0) {
    throw std::runtime_error("lookaheadDistanceBck_ is less than 0.");
  }

  if (parameters.lookaheadDistanceFwd_ < 0) {
    throw std::runtime_error("lookaheadDistanceFwd_ is less than 0.");
  }

  if (parameters.wheelBase_ < 0) {
    throw std::runtime_error("wheelBase_ is less than 0.");
  }

  if (parameters.maxSteeringAngleMagnitude_ < 0) {
    throw std::runtime_error("maxSteeringAngleMagnitude_ is less than 0.");
  }

  if (parameters.maxSteeringRateOfChange_ < 0) {
    throw std::runtime_error("maxSteeringRateOfChange_ is less than 0.");
  }

  if (parameters.deadZoneWidth_ < 0) {
    throw std::runtime_error("deadZoneWidth_ is less than 0.");
  }

  parameters_ = parameters;
  rateLimiter_.setTimestep(parameters.dt_);
  rateLimiter_.setFallingRate(-parameters.maxSteeringRateOfChange_);
  rateLimiter_.setRisingRate(parameters.maxSteeringRateOfChange_);

  avgFilter_.setWeightForMostRecentMeasurement(parameters.avgFilgerCurrentSampleWeight_);
}

AckermannSteeringCtrlParameters AckermannSteeringController::getParameters() const {
  return parameters_;
}

} /* namespace pure_pursuit */
