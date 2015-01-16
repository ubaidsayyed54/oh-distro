#ifndef STATEESTIMATORUTILITIES_H_
#define STATEESTIMATORUTILITIES_H_

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <assert.h>

#include <Eigen/Dense>

#include <lcm/lcm-cpp.hpp>
#include "lcmtypes/bot_core.hpp"
#include "lcmtypes/drc_lcmtypes.hpp"

#include <drc_utils/joint_utils.hpp>

#include <bot_frames/bot_frames.h>
#include <leg-odometry/BotTransforms.hpp>

#include <leg-odometry/TwoLegOdometry.h>
#include <leg-odometry/sharedUtilities.hpp>
#include <leg-odometry/QuaternionLib.h>

#include <inertial-odometry/InertialOdometry_Types.hpp>
#include <inertial-odometry/Odometry.hpp>

namespace StateEstimate {

struct Joints { 
  std::vector<float> position;
  std::vector<float> velocity;
  std::vector<float> effort;
  std::vector<std::string> name;
};

// Equivalent to bot_core_pose contents
struct PoseT { 
  int64_t utime;
  Eigen::Vector3d pos;
  Eigen::Vector3d vel;
  Eigen::Vector4d orientation;
  Eigen::Vector3d rotation_rate;
  Eigen::Vector3d accel;
};

class RobotModel {
 public:
   lcm::LCM lcm;
   std::string robot_name;
   std::string urdf_xml_string;
   std::vector<std::string> joint_names_;
 };

// BDI POSE============================================================================
// Returns false if Pose BDI is old or hasn't appeared yet
bool convertBDIPose_ERS(const bot_core::pose_t* msg, drc::robot_state_t& ERS_msg);


void extractBDIPose(const bot_core::pose_t* msg, PoseT &pose_BDI_);
bool insertPoseBDI(const PoseT &pose_BDI_, drc::robot_state_t& msg);

// Use forward kinematics to estimate the pelvis position as update to the KF
// Store result as StateEstimator:: state
void doLegOdometry(TwoLegs::FK_Data &_fk_data, const drc::atlas_state_t &atlasState, const bot_core::pose_t &_bdiPose, TwoLegs::TwoLegOdometry &_leg_odo, int firstpass, RobotModel* _robot);

//void stampInertialPoseERSMsg(const InertialOdometry::DynamicState &InerOdoEst, const Eigen::Isometry3d &IMU_to_body, drc::robot_state_t &msg);
void stampInertialPoseMsgs(const InertialOdometry::DynamicState &InerOdoEst,
		const Eigen::Isometry3d &IMU_to_body,
		drc::robot_state_t& msg,
		bot_core::pose_t &_msg,
		Eigen::Isometry3d *_mArrowTransform,
		const Eigen::Quaterniond &alignq_out);
//void stampInertialPoseBodyMsg(const InertialOdometry::DynamicState &InerOdoEst, const Eigen::Isometry3d &IMU_to_body, bot_core::pose_t &_msg, Eigen::Isometry3d *_mArrowTransform); // to be depreciated


// DATA FUSION UTILITIES ==============================================================

//void packDFUpdateRequestMsg(InertialOdometry::Odometry &inert_odo, TwoLegs::TwoLegOdometry &_leg_odo, drc::ins_update_request_t &msg);
void stampInertialPoseUpdateRequestMsg(const InertialOdometry::DynamicState &_insState, drc::ins_update_request_t &msg);
//void stampMatlabReferencePoseUpdateRequest(const drc::nav_state_t &matlabPose, drc::ins_update_request_t &msg);
void stampLegOdoPoseUpdateRequestMsg(TwoLegs::TwoLegOdometry &_leg_odo, drc::ins_update_request_t &msg);
void stampEKFReferenceMeasurementUpdateRequest(const Eigen::Vector3d &_ref, const Eigen::Quaterniond &refLegKinQ, const int type, drc::ins_update_request_t &msg);

// Utilities
void copyDrcVec3D(const Eigen::Vector3d &from, drc::vector_3d_t &to);
void onMessage(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::robot_urdf_t* msg, RobotModel* robot);
void detectIMUSampleTime(unsigned long long &prevImuPacketCount, unsigned long long &previous_imu_utime, int &receivedIMUPackets, double &previous_Ts_imu, const drc::atlas_raw_imu_t &imu);

Eigen::Quaterniond convertToOutQuaternion(const InertialOdometry::DynamicState &InerOdoEst, const Eigen::Isometry3d &IMU_to_body, const Eigen::Quaterniond &alignq_out);



} // namespace StateEstimate

#endif /*STATEESTIMATORUTILITIES_H_*/