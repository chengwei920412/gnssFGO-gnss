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

#ifndef ONLINE_FGO_GPINTERPOLATORBASE_H
#define ONLINE_FGO_GPINTERPOLATORBASE_H

#pragma once

#include <iostream>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/base/Matrix.h>
#include <gtsam/base/numericalDerivative.h>

#include "utils/NavigationTools.h"
#include "utils/Pose3Utils.h"
#include "utils/GPUtils.h"

namespace fgo::models {

  class GPInterpolator {
  protected:
    gtsam::Matrix6 Qc_;
    gtsam::Matrix6 Ad_;
    double delta_t_{};
    double tau_{};

    bool useAutoDiff_ = false;
    bool calcJacobian_ = false;

    GPInterpolator() = default;

    //for WNOA and WNOJ
    explicit GPInterpolator(const gtsam::Matrix6 &Qc, double delta_t = 0.0, double tau = 0.0, bool useAutoDiff = false,
                            bool calcJacobian = true) {
      Qc_ = Qc;
      delta_t_ = delta_t;
      tau_ = tau;
      useAutoDiff_ = useAutoDiff;
      calcJacobian_ = calcJacobian;
    }

    //for Singer
    explicit GPInterpolator(const gtsam::Matrix6 &Qc, const gtsam::Matrix6 &Ad, double delta_t = 0.0, double tau = 0.0,
                            bool useAutoDiff = false, bool calcJacobian = true) {
      Qc_ = Qc;
      Ad_ = Ad;
      delta_t_ = delta_t;
      tau_ = tau;
      useAutoDiff_ = useAutoDiff;
      calcJacobian_ = calcJacobian;
    }

    virtual ~GPInterpolator() = default;

    void update(double delta_t, double tau) {
      tau_ = tau;
      delta_t_ = delta_t;
    }

    void update(double delta_t, double tau, const gtsam::Matrix66 &Ad) {
      tau_ = tau;
      delta_t_ = delta_t;
      Ad_ = Ad;
    }

  public:

    [[nodiscard]] virtual double getTau() const {
      return tau_;
    }

    [[nodiscard]] double getDeltat() const {
      return delta_t_;
    }

    [[nodiscard]] gtsam::Matrix6 getQc() const {
      return Qc_;
    }

    [[nodiscard]] gtsam::Matrix6 getAd() const {
      return Ad_;
    }

    virtual void recalculate(const double &delta_t, const double &tau,
                             const gtsam::Vector6 &accI = gtsam::Vector6(),
                             const gtsam::Vector6 &accJ = gtsam::Vector6()) {};

    virtual void recalculate(const double &delta_t, const double &tau, const gtsam::Matrix66 &Ad,
                             const gtsam::Vector6 &accI = gtsam::Vector6(),
                             const gtsam::Vector6 &accJ = gtsam::Vector6()) {};

    virtual gtsam::Pose3
    interpolatePose(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                    const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                    boost::optional<gtsam::Matrix &> H1 = boost::none,
                    boost::optional<gtsam::Matrix &> H2 = boost::none,
                    boost::optional<gtsam::Matrix &> H3 = boost::none,
                    boost::optional<gtsam::Matrix &> H4 = boost::none,
                    boost::optional<gtsam::Matrix &> H5 = boost::none,
                    boost::optional<gtsam::Matrix &> H6 = boost::none) const {};

    virtual gtsam::Pose3
    interpolatePose(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                    const gtsam::Vector6 &acc1,
                    const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                    const gtsam::Vector6 &acc2,
                    boost::optional<gtsam::Matrix &> H1 = boost::none,
                    boost::optional<gtsam::Matrix &> H2 = boost::none,
                    boost::optional<gtsam::Matrix &> H3 = boost::none,
                    boost::optional<gtsam::Matrix &> H4 = boost::none,
                    boost::optional<gtsam::Matrix &> H5 = boost::none,
                    boost::optional<gtsam::Matrix &> H6 = boost::none,
                    boost::optional<gtsam::Matrix &> H7 = boost::none,
                    boost::optional<gtsam::Matrix &> H8 = boost::none) const {};


    virtual gtsam::Pose3
    interpolatePose_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                     const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b) const {};

    virtual gtsam::Pose3
    interpolatePose_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                     const gtsam::Vector6 &acc1,
                     const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                     const gtsam::Vector6 &acc2) const {};

    virtual gtsam::Vector6
    interpolateVelocity(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                        const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                        boost::optional<gtsam::Matrix &> H1 = boost::none,
                        boost::optional<gtsam::Matrix &> H2 = boost::none,
                        boost::optional<gtsam::Matrix &> H3 = boost::none,
                        boost::optional<gtsam::Matrix &> H4 = boost::none,
                        boost::optional<gtsam::Matrix &> H5 = boost::none,
                        boost::optional<gtsam::Matrix &> H6 = boost::none) const {};

    virtual gtsam::Vector6
    interpolateVelocity_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                         const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n,
                         const gtsam::Vector3 &omega2_b) const {};

    virtual gtsam::Vector6
    interpolateVelocity(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                        const gtsam::Vector6 &acc1,
                        const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                        const gtsam::Vector6 &acc2,
                        boost::optional<gtsam::Matrix &> H1 = boost::none,
                        boost::optional<gtsam::Matrix &> H2 = boost::none,
                        boost::optional<gtsam::Matrix &> H3 = boost::none,
                        boost::optional<gtsam::Matrix &> H4 = boost::none,
                        boost::optional<gtsam::Matrix &> H5 = boost::none,
                        boost::optional<gtsam::Matrix &> H6 = boost::none,
                        boost::optional<gtsam::Matrix &> H7 = boost::none,
                        boost::optional<gtsam::Matrix &> H8 = boost::none) const {};

    virtual gtsam::Vector6
    interpolateVelocity_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                         const gtsam::Vector6 &acc1,
                         const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                         const gtsam::Vector6 &acc2) const {};


    virtual gtsam::Vector6
    interpolateAcceleration(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                            const gtsam::Vector6 &acc1,
                            const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                            const gtsam::Vector6 &acc2,
                            boost::optional<gtsam::Matrix &> H1 = boost::none,
                            boost::optional<gtsam::Matrix &> H2 = boost::none,
                            boost::optional<gtsam::Matrix &> H3 = boost::none,
                            boost::optional<gtsam::Matrix &> H4 = boost::none,
                            boost::optional<gtsam::Matrix &> H5 = boost::none,
                            boost::optional<gtsam::Matrix &> H6 = boost::none,
                            boost::optional<gtsam::Matrix &> H7 = boost::none,
                            boost::optional<gtsam::Matrix &> H8 = boost::none) const {};

    virtual gtsam::Vector6
    interpolateAcceleration_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &v1_n, const gtsam::Vector3 &omega1_b,
                             const gtsam::Vector6 &acc1,
                             const gtsam::Pose3 &pose2, const gtsam::Vector3 &v2_n, const gtsam::Vector3 &omega2_b,
                             const gtsam::Vector6 &acc2) const {};


    virtual void print(const std::string &s = "GPIntegratorBase") const = 0;

  };
}

#endif //ONLINE_FGO_GPINTERPOLATORBASE_H
