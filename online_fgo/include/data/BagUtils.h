//  Copyright 2022 Institute of Automatic Control RWTH Aachen University
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//  Author: Haoming Zhang (h.zhang@irt.rwth-aachen.de)
//
//

//
// Created by haoming on 15.04.24.
//

#ifndef ONLINE_FGO_BAGUTILS_H
#define ONLINE_FGO_BAGUTILS_H
#pragma once

#include <any>
#include <irt_nav_msgs/msg/pva_geodetic.hpp>
#include <irt_nav_msgs/msg/gnss_obs_pre_processed.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <ublox_msgs/msg/nav_pvt.hpp>
#include <ublox_msgs/msg/nav_clock.hpp>

#include <novatel_oem7_msgs/msg/bestpos.hpp>
#include <novatel_oem7_msgs/msg/bestvel.hpp>
#include <novatel_oem7_msgs/msg/dualantennaheading.hpp>
#include <novatel_oem7_msgs/msg/clockmodel.hpp>
#include <novatel_oem7_msgs/msg/inspvax.hpp>
#include <novatel_oem7_msgs/msg/inscov.hpp>
#include "sensor/GNSS/GNSSDataParser.h"
#include "data/DataTypes.h"
#include "utils/GNSSUtils.h"


namespace fgo::data {
  enum class DataType : unsigned int {
    IMU = 1,
    IRTPVAGeodetic = 2,
    NavFix = 3,
    Odometry = 4,
    NovAtelINSPVA = 5,
    IRTGNSSObsPreProcessed = 6
  };

  const std::map<DataType, std::string> TypeMessageDict =
    {
      {DataType::IRTPVAGeodetic,         "IRTPVAGeodetic"},
      {DataType::Odometry,               "Odometry"},
      {DataType::NovAtelINSPVA,          "NovAtelINSPVA"},
      {DataType::IRTGNSSObsPreProcessed, "IRTGNSSObsPreProcessed"},
      {DataType::IMU,                    "IMU"},

    };

  const std::map<std::string, DataType> MessageTypeDict =
    {
      {"IMU",                    DataType::IMU},
      {"IRTPVAGeodetic",         DataType::IRTPVAGeodetic},
      {"Odometry",               DataType::Odometry},
      {"NovAtelINSPVA",          DataType::NovAtelINSPVA},
      {"IRTGNSSObsPreProcessed", DataType::IRTGNSSObsPreProcessed},

    };

  template<DataType Msg>
  struct ROSMessageTypeTranslator;

  template<typename T>
  static auto FGODataTimeGetter = [](const T &data) -> rclcpp::Time {
    return data.timestamp;
  };

  static auto StateTimeGetter = FGODataTimeGetter<State>;
  static auto PVADataTimeGetter = FGODataTimeGetter<PVASolution>;
  static auto IMUDataTimeGetter = FGODataTimeGetter<IMUMeasurement>;

  static auto GNSSDataTimeGetter = [](const GNSSMeasurement &data) -> rclcpp::Time {
    return data.measMainAnt.timestamp;
  };

  static auto onPVASolutionData = [](const std::vector<PVASolution> &pvas) {

  };

  inline IMUMeasurement msg2IMUMeasurement(const sensor_msgs::msg::Imu &imuMsg,
                                           const rclcpp::Time &timestamp,
                                           const gtsam::Matrix33 &trans = gtsam::Matrix33::Identity()) {
    static bool first_msg = true;
    static auto last_timestamp = timestamp;
    static auto last_gyro = gtsam::Vector3();
    IMUMeasurement imuMeasurement;
    imuMeasurement.timestamp = timestamp;
    imuMeasurement.accLin.x() = imuMsg.linear_acceleration.x;
    imuMeasurement.accLin.y() = imuMsg.linear_acceleration.y;
    imuMeasurement.accLin.z() = imuMsg.linear_acceleration.z;

    imuMeasurement.accLin = trans * imuMeasurement.accLin;

    imuMeasurement.accLinCov = gtsam::Matrix33(imuMsg.linear_acceleration_covariance.data());
    imuMeasurement.AHRSOri.w() = imuMsg.orientation.w;
    imuMeasurement.AHRSOri.x() = imuMsg.orientation.x;
    imuMeasurement.AHRSOri.y() = imuMsg.orientation.y;
    imuMeasurement.AHRSOri.z() = imuMsg.orientation.z;
    imuMeasurement.gyro.x() = imuMsg.angular_velocity.x;
    imuMeasurement.gyro.y() = imuMsg.angular_velocity.y;
    imuMeasurement.gyro.z() = imuMsg.angular_velocity.z;

    imuMeasurement.gyro = trans * imuMeasurement.gyro;
    imuMeasurement.gyroCov = gtsam::Matrix33(imuMsg.angular_velocity_covariance.data());
    imuMeasurement.AHRSOriCov = gtsam::Matrix33(imuMsg.orientation_covariance.data());
    //dt and accrot
    if (!first_msg) {
      imuMeasurement.dt = imuMeasurement.timestamp.seconds() - last_timestamp.seconds();
      imuMeasurement.accRot = (imuMeasurement.gyro - last_gyro) / imuMeasurement.dt;
    } else {
      first_msg = false;
      imuMeasurement.dt = 0.005;
      imuMeasurement.accRot = gtsam::Vector3();
    }
    last_timestamp = timestamp;
    last_gyro = imuMeasurement.gyro;
    return imuMeasurement;
  }

  template<>
  struct ROSMessageTypeTranslator<DataType::IMU> {
    using Type = sensor_msgs::msg::Imu;
  };
  template<>
  struct ROSMessageTypeTranslator<DataType::NavFix> {
    using Type = sensor_msgs::msg::NavSatFix;
  };
  template<>
  struct ROSMessageTypeTranslator<DataType::IRTPVAGeodetic> {
    using Type = irt_nav_msgs::msg::PVAGeodetic;
  };
  template<>
  struct ROSMessageTypeTranslator<DataType::NovAtelINSPVA> {
    using Type = novatel_oem7_msgs::msg::INSPVAX;
  };
  template<>
  struct ROSMessageTypeTranslator<DataType::Odometry> {
    using Type = nav_msgs::msg::Odometry;
  };
  template<>
  struct ROSMessageTypeTranslator<DataType::IRTGNSSObsPreProcessed> {
    using Type = irt_nav_msgs::msg::GNSSObsPreProcessed;
  };


}


#endif //ONLINE_FGO_BAGUTILS_H