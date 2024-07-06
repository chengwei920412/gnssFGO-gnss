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

#ifndef ONLINE_FGO_GPINTERPOLATEDDRFACTOR_H
#define ONLINE_FGO_GPINTERPOLATEDDRFACTOR_H

#pragma once

#include <utility>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/base/Matrix.h>
#include <gtsam/base/Vector.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/navigation/ImuBias.h>
#include "model/gp_interpolator/GPInterpolatorBase.h"
#include "include/factor/FactorTypes.h"
#include "factor/FactorTypeIDs.h"

/*Inputs:
* Keys: pose of time i&j X(i)&X(j), velocity of time i&j V(i)&V(j), clock bias drift of time i C(i)
* Pseudorange measurement and doppler measurement: measRho and measdRho
* Position of the satellite and velocity of satellite: satXYZ, satVEL
* Position of sensor with respect to the Body: lb
 *delta_t: timedifference of state i & j tau: timedifference of state i & time of measurement
 * omega1&2: angularvelocity of time i and j
* Covariance Matrix of the measurement/s: model, qcModel*/
/* measurement equations used:
 * Pseudorange = Distance of Satellite and Receiver + Range through clock bias,
 * Doppler = Velocity between Satellite and Receiver (in direction) + Velocity through clock drift */
/*Jacobian: for X(i/j) = (e_RS * R_eb * skrew(lb_) , e_RS * R_eb), V(i/j) = 0, e_RS, C(i) = 1,tau,0,1
 * NOTE every Jacobian is multiplied with its designated GPSLAMJacobian at the end
 * */

namespace fgo::factor {

  class GPInterpolatedDrFactor : public NoiseModelFactor7<gtsam::Pose3, gtsam::Vector3, gtsam::Vector3,
    gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Vector2> {
  private:
    /** measurement */
    double measdRho_{};
    gtsam::Point3 lb_; ///< The pose of the sensor in the body frame
    gtsam::Vector3 satXYZ_;
    gtsam::Vector3 satVEL_;
    double tau_{};
    bool useAutoDiff_ = false;

    typedef GPInterpolatedDrFactor This;
    typedef NoiseModelFactor7<gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Vector2> Interpolator;
    typedef std::shared_ptr<fgo::models::GPInterpolator> GPBase;

    // interpolator
    GPBase GPbase_;
  public:

    GPInterpolatedDrFactor() = default; /* Default constructor */

    /**
     * Constructor
     * @param body_P_sensor transformation from body to sensor
     */
    GPInterpolatedDrFactor(gtsam::Key pose_i, gtsam::Key vel_i, gtsam::Key omega_i,
                           gtsam::Key pose_j, gtsam::Key vel_j, gtsam::Key omega_j,
                           gtsam::Key cbd_i, const double &measdRho,
                           const gtsam::Vector3 &satXYZ, const gtsam::Vector3 &satVEL, const gtsam::Vector3 &lb,
                           const gtsam::SharedNoiseModel &model,
                           const std::shared_ptr<fgo::models::GPInterpolator> &interpolator, bool useAutoDiff = false) :
      Interpolator(model, pose_i, vel_i, omega_i, pose_j, vel_j, omega_j, cbd_i),
      measdRho_(measdRho), lb_(lb),
      satXYZ_(satXYZ), satVEL_(satVEL), tau_(interpolator->getTau()), useAutoDiff_(useAutoDiff),
      GPbase_(interpolator) {
      factorTypeID_ = FactorTypeID::GPDR;
      factorName_ = "GPInterpolatedDrFactor";
    }

    ~GPInterpolatedDrFactor() override = default;

    /// @return a deep copy of this factor
    [[nodiscard]] gtsam::NonlinearFactor::shared_ptr clone() const override {
      return boost::static_pointer_cast<gtsam::NonlinearFactor>(
        gtsam::NonlinearFactor::shared_ptr(new This(*this)));
    }

    /** factor error */
    [[nodiscard]] gtsam::Vector
    evaluateError(const gtsam::Pose3 &pose1, const gtsam::Vector3 &vel1, const gtsam::Vector3 &omega1,
                  const gtsam::Pose3 &pose2, const gtsam::Vector3 &vel2, const gtsam::Vector3 &omega2,
                  const gtsam::Vector2 &cbd1,
                  boost::optional<gtsam::Matrix &> H1 = boost::none,
                  boost::optional<gtsam::Matrix &> H2 = boost::none,
                  boost::optional<gtsam::Matrix &> H3 = boost::none,
                  boost::optional<gtsam::Matrix &> H4 = boost::none,
                  boost::optional<gtsam::Matrix &> H5 = boost::none,
                  boost::optional<gtsam::Matrix &> H6 = boost::none,
                  boost::optional<gtsam::Matrix &> H7 = boost::none) const override {
      using namespace gtsam;

      if (useAutoDiff_) {
        if (H1)
          *H1 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Pose3>(
            boost::bind(&This::evaluateError_, this, boost::placeholders::_1, vel1, omega1, pose2, vel2, omega2,
                        cbd1), pose1);

        if (H2)
          *H2 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            boost::bind(&This::evaluateError_, this, pose1, boost::placeholders::_1, omega1, pose2, vel2, omega2,
                        cbd1), vel1);

        if (H3)
          *H3 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            boost::bind(&This::evaluateError_, this, pose1, vel1, boost::placeholders::_1, pose2, vel2, omega2, cbd1),
            omega1);

        if (H4)
          *H4 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Pose3>(
            boost::bind(&This::evaluateError_, this, pose1, vel1, omega1, boost::placeholders::_1, vel2, omega2,
                        cbd1), pose2);

        if (H5)
          *H5 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            boost::bind(&This::evaluateError_, this, pose1, vel1, omega1, pose2, boost::placeholders::_1, omega2,
                        cbd1), vel2);

        if (H6)
          *H6 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            boost::bind(&This::evaluateError_, this, pose1, vel1, omega1, pose2, vel2, boost::placeholders::_1, cbd1),
            omega2);
        if (H7)
          *H7 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector2>(
            boost::bind(&This::evaluateError_, this, pose1, vel1, omega1, pose2, vel2, omega2,
                        boost::placeholders::_1), cbd1);

        return evaluateError_(pose1, vel1, omega1, pose2, vel2, omega2, cbd1);
      } else {
        gtsam::Matrix Hint1_P, Hint2_P, Hint3_P, Hint4_P, Hint5_P, Hint6_P;
        gtsam::Matrix Hint1_V, Hint2_V, Hint3_V, Hint4_V, Hint5_V, Hint6_V;
        gtsam::Pose3 pose;
        gtsam::Vector6 vel_b;

        if (H1 || H2 || H3 || H4 || H5 || H6) {
          pose = GPbase_->interpolatePose(pose1, vel1, omega1, pose2, vel2, omega2,
                                          Hint1_P, Hint2_P, Hint3_P, Hint4_P, Hint5_P, Hint6_P);
          vel_b = GPbase_->interpolateVelocity(pose1, vel1, omega1, pose2, vel2, omega2,
                                               Hint1_V, Hint2_V, Hint3_V, Hint4_V, Hint5_V, Hint6_V);
        } else {
          pose = GPbase_->interpolatePose(pose1, vel1, omega1, pose2, vel2, omega2);
          vel_b = GPbase_->interpolateVelocity(pose1, vel1, omega1, pose2, vel2, omega2);
        }

        gtsam::Matrix Hpose_r;
        gtsam::Matrix3 Hdrho_p, Hdrho_t;
        gtsam::Matrix13 Hd;

        gtsam::Vector3 lin_vel_b(vel_b(3), vel_b(4), vel_b(5));
        gtsam::Point3 lbv = gtsam::skewSymmetric((-lb_)) * vel_b.head(3);
        gtsam::Point3 vel_eA_e = pose.rotation(Hpose_r).rotate(lbv + lin_vel_b, &Hdrho_p, &Hdrho_t);
        double vel_delta = (Hd * (vel_eA_e - satVEL_))(0);

        if (H1 || H2 || H3 || H4 || H5 || H6) {
          gtsam::Matrix tmp_drho = Hdrho_p * Hpose_r;
          if (H1) *H1 = Hd * (tmp_drho * Hint1_P + Hdrho_t * Hint1_V.block<3, 6>(3, 0));
          if (H2) *H2 = Hd * (tmp_drho * Hint2_P + Hdrho_t * Hint2_V.block<3, 3>(3, 0));
          if (H3) *H3 = Hd * (tmp_drho * Hint3_P + Hdrho_t * Hint3_V.block<3, 3>(3, 0));
          if (H4) *H4 = Hd * (tmp_drho * Hint4_P + Hdrho_t * Hint4_V.block<3, 6>(3, 0));
          if (H5) *H5 = Hd * (tmp_drho * Hint5_P + Hdrho_t * Hint5_V.block<3, 3>(3, 0));
          if (H6) *H6 = Hd * (tmp_drho * Hint6_P + Hdrho_t * Hint6_V.block<3, 3>(3, 0));
        }
        if (H7) *H7 = (gtsam::Matrix12() << 0, 1).finished();
        return (gtsam::Vector1() << vel_delta + cbd1(1) - measdRho_).finished();
      }
    }

    [[nodiscard]] gtsam::Vector
    evaluateError_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &vel1, const gtsam::Vector3 &omega1,
                   const gtsam::Pose3 &pose2, const gtsam::Vector3 &vel2, const gtsam::Vector3 &omega2,
                   const gtsam::Vector2 &cbd1) const {
      // get position
      gtsam::Pose3 pose = GPbase_->interpolatePose(pose1, vel1, omega1, pose2, vel2, omega2);
      gtsam::Point3 positionReceiver = pose.translation() + pose.rotation() * lb_;

      //get velocity
      gtsam::Vector6 vel = GPbase_->interpolateVelocity(pose1, vel1, omega1, pose2, vel2, omega2);
      gtsam::Point3 lbv = gtsam::skewSymmetric(-lb_) * vel.head(3); //velocity through rotation of antenna
      gtsam::Point3 velocityReceiver = pose.rotation().rotate(vel.block<3, 1>(3, 0) + lbv);

      //error pos
      gtsam::Matrix13 e;
      double distance = gtsam::distance3(positionReceiver, satXYZ_, e); //distance between receiver and sat
      // error vel
      gtsam::Point3 velocityDifference = velocityReceiver - satVEL_;
      double errd = e * velocityDifference + cbd1(1) - measdRho_;

      return gtsam::Vector1(errd);
    }

    /** lifting all related state values in a vector after the ordering for evaluateError **/
    gtsam::Vector liftValuesAsVector(const gtsam::Values &values) override {
      const auto poseI = values.at<gtsam::Pose3>(key1());
      const auto velI = values.at<gtsam::Vector3>(key2());
      const auto omegaI = values.at<gtsam::Vector3>(key3());
      const auto poseJ = values.at<gtsam::Pose3>(key4());
      const auto velJ = values.at<gtsam::Vector3>(key5());
      const auto omegaJ = values.at<gtsam::Vector3>(key6());
      const auto cbd1 = values.at<gtsam::Vector2>(key7());
      const auto liftedStates = (gtsam::Vector(26) << poseI.rotation().rpy(),
        poseI.translation(),
        velI, omegaI,
        poseJ.rotation().rpy(),
        poseJ.translation(),
        velJ, omegaJ, cbd1).finished();
      return liftedStates;
    }

    gtsam::Values generateValuesFromStateVector(const gtsam::Vector &state) override {
      assert(state.size() != 26);
      gtsam::Values values;
      try {
        values.insert(key1(), gtsam::Pose3(gtsam::Rot3::RzRyRx(state.block<3, 1>(0, 0)),
                                           gtsam::Point3(state.block<3, 1>(3, 0))));
        values.insert(key2(), gtsam::Vector3(state.block<3, 1>(6, 0)));
        values.insert(key3(), gtsam::Vector3(state.block<3, 1>(9, 0)));
        values.insert(key4(), gtsam::Pose3(gtsam::Rot3::RzRyRx(state.block<3, 1>(12, 0)),
                                           gtsam::Point3(state.block<3, 1>(15, 0))));
        values.insert(key5(), gtsam::Vector3(state.block<3, 1>(18, 0)));
        values.insert(key6(), gtsam::Vector3(state.block<3, 1>(21, 0)));
        values.insert(key7(), gtsam::Vector2(state.block<2, 1>(24, 0)));
      }
      catch (std::exception &ex) {
        std::cout << "Factor " << getName() << " cannot generate values from state vector " << state << " due to "
                  << ex.what() << std::endl;
      }
      return values;
    }

    /** return the measured */
    [[nodiscard]] gtsam::Vector1 measured() const {
      return gtsam::Vector1(measdRho_);
    }

    /** equals specialized to this factor */
    [[nodiscard]] bool equals(const gtsam::NonlinearFactor &expected, double tol = 1e-9) const override {
      const This *e = dynamic_cast<const This *> (&expected);
      return e != NULL && Base::equals(*e, tol)
             && gtsam::equal_with_abs_tol((gtsam::Vector1() << this->measdRho_).finished(),
                                          (gtsam::Vector1() << e->measdRho_).finished(), tol);
    }

    /** print contents */
    void print(const std::string &s = "",
               const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const override {
      std::cout << s << "GPInterpolatedDrFactor" << std::endl;
      Base::print("", keyFormatter);
    }

  private:
    /** Serialization function */
    friend class boost::serialization::access;

    template<class ARCHIVE>
    void serialize(ARCHIVE &ar, const unsigned int version) {
      ar & boost::serialization::make_nvp("GPInterpolatedDrFactor",
                                          boost::serialization::base_object<Base>(*this));
      ar & BOOST_SERIALIZATION_NVP(measdRho_);
    }
  };

  class GPInterpolatedDrFactorFull
    : public NoiseModelFactor9<gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Vector6,
      gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Vector6, gtsam::Vector2> {
  private:
    /** measurement */
    double measdRho_{};
    gtsam::Point3 lb_; ///< The pose of the sensor in the body frame
    gtsam::Vector3 satXYZ_;
    gtsam::Vector3 satVEL_;
    double tau_{};
    bool useAutoDiff_ = false;

    typedef GPInterpolatedDrFactorFull This;
    typedef NoiseModelFactor9<gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Vector6,
      gtsam::Pose3, gtsam::Vector3, gtsam::Vector3, gtsam::Vector6, gtsam::Vector2> Interpolator;
    typedef std::shared_ptr<fgo::models::GPInterpolator> GPBase;

    // interpolator
    GPBase GPbase_;
  public:

    GPInterpolatedDrFactorFull() = default; /* Default constructor */

    /**
     * Constructor
     * @param body_P_sensor transformation from body to sensor
     */
    GPInterpolatedDrFactorFull(const gtsam::Key &pose_i, const gtsam::Key &vel_i, const gtsam::Key &omega_i,
                               const gtsam::Key &acc_i,
                               const gtsam::Key &pose_j, const gtsam::Key &vel_j, const gtsam::Key &omega_j,
                               const gtsam::Key &acc_j, const gtsam::Key &cbd_i,
                               const double &measdRho,
                               const gtsam::Vector3 &satXYZ, const gtsam::Vector3 &satVEL, const gtsam::Vector3 &lb,
                               const gtsam::SharedNoiseModel &model,
                               const std::shared_ptr<fgo::models::GPInterpolator> &interpolator,
                               bool useAutoDiff = false) :
      Interpolator(model, pose_i, vel_i, omega_i, acc_i, pose_j, vel_j, omega_j, acc_j, cbd_i),
      measdRho_(measdRho), lb_(lb),
      satXYZ_(satXYZ), satVEL_(satVEL), tau_(interpolator->getTau()), useAutoDiff_(useAutoDiff),
      GPbase_(interpolator) {
      factorTypeID_ = FactorTypeID::GPDR;
      factorName_ = "GPInterpolatedDrFactorFull";
    }

    ~GPInterpolatedDrFactorFull() override = default;

    /// @return a deep copy of this factor
    [[nodiscard]] gtsam::NonlinearFactor::shared_ptr clone() const override {
      return boost::static_pointer_cast<gtsam::NonlinearFactor>(
        gtsam::NonlinearFactor::shared_ptr(new This(*this)));
    }

    /** factor error */
    [[nodiscard]] gtsam::Vector
    evaluateError(const gtsam::Pose3 &pose1, const gtsam::Vector3 &vel1, const gtsam::Vector3 &omega1,
                  const gtsam::Vector6 &acc1,
                  const gtsam::Pose3 &pose2, const gtsam::Vector3 &vel2, const gtsam::Vector3 &omega2,
                  const gtsam::Vector6 &acc2, const gtsam::Vector2 &cbd1,
                  boost::optional<gtsam::Matrix &> H1 = boost::none,
                  boost::optional<gtsam::Matrix &> H2 = boost::none,
                  boost::optional<gtsam::Matrix &> H3 = boost::none,
                  boost::optional<gtsam::Matrix &> H4 = boost::none,
                  boost::optional<gtsam::Matrix &> H5 = boost::none,
                  boost::optional<gtsam::Matrix &> H6 = boost::none,
                  boost::optional<gtsam::Matrix &> H7 = boost::none,
                  boost::optional<gtsam::Matrix &> H8 = boost::none,
                  boost::optional<gtsam::Matrix &> H9 = boost::none) const override {
      using namespace gtsam;

      if (useAutoDiff_) {
        if (H1)
          *H1 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Pose3>(
            std::bind(&This::evaluateError_, this, std::placeholders::_1, vel1, omega1, acc1, pose2, vel2,
                      omega2, acc2, cbd1), pose1);

        if (H2)
          *H2 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            std::bind(&This::evaluateError_, this, pose1, std::placeholders::_1, omega1, acc1, pose2,
                      vel2, omega2, acc2, cbd1), vel1);

        if (H3)
          *H3 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            std::bind(&This::evaluateError_, this, pose1, vel1, std::placeholders::_1, acc1, pose2, vel2,
                      omega2, acc2, cbd1), omega1);

        if (H4)
          *H4 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector6>(
            std::bind(&This::evaluateError_, this, pose1, vel1, omega1, std::placeholders::_1, pose2, vel2,
                      omega2, acc2, cbd1), acc1);

        if (H5)
          *H5 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Pose3>(
            std::bind(&This::evaluateError_, this, pose1, vel1, omega1, acc1, std::placeholders::_1, vel2,
                      omega2, acc2, cbd1), pose2);

        if (H6)
          *H6 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            std::bind(&This::evaluateError_, this, pose1, vel1, omega1, acc1, pose2,
                      std::placeholders::_1, omega2, acc2, cbd1), vel2);

        if (H7)
          *H7 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
            std::bind(&This::evaluateError_, this, pose1, vel1, omega1, acc1, pose2, vel2,
                      std::placeholders::_1, acc2, cbd1), omega2);

        if (H8)
          *H8 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector6>(
            std::bind(&This::evaluateError_, this, pose1, vel1, omega1, acc1, pose2, vel2, omega2,
                      std::placeholders::_1, cbd1), acc2);

        if (H9)
          *H9 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector2>(
            std::bind(&This::evaluateError_, this, pose1, vel1, omega1, acc1, pose2, vel2, omega2,
                      acc2, std::placeholders::_1), cbd1);

        return evaluateError_(pose1, vel1, omega1, acc1, pose2, vel2, omega2, acc2, cbd1);
      } else {
        gtsam::Matrix Hint1_P, Hint2_P, Hint3_P, Hint4_P, Hint5_P, Hint6_P, Hint7_P, Hint8_P;
        gtsam::Matrix Hint1_V, Hint2_V, Hint3_V, Hint4_V, Hint5_V, Hint6_V, Hint7_V, Hint8_V;
        gtsam::Pose3 pose;
        gtsam::Vector6 vel_b;

        if (H1 || H2 || H3 || H4 || H5 || H6 || H7 || H8) {
          pose = GPbase_->interpolatePose(pose1, vel1, omega1, acc1, pose2, vel2, omega2, acc2,
                                          Hint1_P, Hint2_P, Hint3_P, Hint4_P, Hint5_P, Hint6_P, Hint7_P, Hint8_P);
          vel_b = GPbase_->interpolateVelocity(pose1, vel1, omega1, acc1, pose2, vel2, omega2, acc2,
                                               Hint1_V, Hint2_V, Hint3_V, Hint4_V, Hint5_V, Hint6_V, Hint7_V, Hint8_V);
        } else {
          pose = GPbase_->interpolatePose(pose1, vel1, omega1, acc1, pose2, vel2, omega2, acc2);
          vel_b = GPbase_->interpolateVelocity(pose1, vel1, omega1, acc1, pose2, vel2, omega2, acc2);
        }

        gtsam::Matrix Hpose_r;
        gtsam::Matrix3 Hdrho_p, Hdrho_t;
        gtsam::Matrix13 Hd;

        gtsam::Vector3 lin_vel_b(vel_b(3), vel_b(4), vel_b(5));
        gtsam::Point3 lbv = gtsam::skewSymmetric((-lb_)) * vel_b.head(3);
        gtsam::Point3 vel_eA_e = pose.rotation(Hpose_r).rotate(lbv + lin_vel_b, &Hdrho_p, &Hdrho_t);
        double vel_delta = (Hd * (vel_eA_e - satVEL_))(0);

        if (H1 || H2 || H3 || H4 || H5 || H6) {
          gtsam::Matrix tmp_drho = Hdrho_p * Hpose_r;
          if (H1) *H1 = Hd * (tmp_drho * Hint1_P + Hdrho_t * Hint1_V.block<3, 6>(3, 0));
          if (H2) *H2 = Hd * (tmp_drho * Hint2_P + Hdrho_t * Hint2_V.block<3, 3>(3, 0));
          if (H3) *H3 = Hd * (tmp_drho * Hint3_P + Hdrho_t * Hint3_V.block<3, 3>(3, 0));
          if (H4) *H4 = Hd * (tmp_drho * Hint4_P + Hdrho_t * Hint4_V.block<3, 6>(3, 0));
          if (H5) *H5 = Hd * (tmp_drho * Hint5_P + Hdrho_t * Hint5_V.block<3, 3>(3, 0));
          if (H6) *H6 = Hd * (tmp_drho * Hint6_P + Hdrho_t * Hint6_V.block<3, 3>(3, 0));
          if (H7) *H7 = Hd * (tmp_drho * Hint7_P + Hdrho_t * Hint7_V.block<3, 3>(3, 0));
          if (H8) *H8 = Hd * (tmp_drho * Hint8_P + Hdrho_t * Hint8_V.block<3, 3>(3, 0));
        }
        if (H9) *H9 = (gtsam::Matrix12() << 0, 1).finished();
        return (gtsam::Vector1() << vel_delta + cbd1(1) - measdRho_).finished();
      }
    }

    [[nodiscard]] gtsam::Vector
    evaluateError_(const gtsam::Pose3 &pose1, const gtsam::Vector3 &vel1, const gtsam::Vector3 &omega1,
                   const gtsam::Vector6 &acc1,
                   const gtsam::Pose3 &pose2, const gtsam::Vector3 &vel2, const gtsam::Vector3 &omega2,
                   const gtsam::Vector6 &acc2,
                   const gtsam::Vector2 &cbd1) const {
      // get position
      gtsam::Pose3 pose = GPbase_->interpolatePose(pose1, vel1, omega1, acc1, pose2, vel2, omega2, acc2);
      gtsam::Point3 positionReceiver = pose.translation() + pose.rotation() * lb_;

      //get velocity
      gtsam::Vector6 vel = GPbase_->interpolateVelocity(pose1, vel1, omega1, pose2, vel2, omega2);
      gtsam::Point3 lbv = gtsam::skewSymmetric(-lb_) * vel.head(3); //velocity through rotation of antenna
      gtsam::Point3 velocityReceiver = pose.rotation().rotate(vel.block<3, 1>(3, 0) + lbv);

      //error pos
      gtsam::Matrix13 e;
      double distance = gtsam::distance3(positionReceiver, satXYZ_, e); //distance between receiver and sat
      // error vel
      gtsam::Point3 velocityDifference = velocityReceiver - satVEL_;
      double errd = e * velocityDifference + cbd1(1) - measdRho_;

      return gtsam::Vector1(errd);
    }

    /** lifting all related state values in a vector after the ordering for evaluateError **/
    gtsam::Vector liftValuesAsVector(const gtsam::Values &values) override {
      const auto poseI = values.at<gtsam::Pose3>(key1());
      const auto velI = values.at<gtsam::Vector3>(key2());
      const auto omegaI = values.at<gtsam::Vector3>(key3());
      const auto accI = values.at<gtsam::Vector6>(key4());

      const auto poseJ = values.at<gtsam::Pose3>(key5());
      const auto velJ = values.at<gtsam::Vector3>(key6());
      const auto omegaJ = values.at<gtsam::Vector3>(key7());
      const auto accJ = values.at<gtsam::Vector6>(key8());

      const auto cbd1 = values.at<gtsam::Vector2>(key9());

      const auto liftedStates = (gtsam::Vector(38) << poseI.rotation().rpy(),
        poseI.translation(),
        velI, omegaI, accI,
        poseJ.rotation().rpy(),
        poseJ.translation(),
        velJ, omegaJ, accJ, cbd1).finished();
      return liftedStates;
    }

    gtsam::Values generateValuesFromStateVector(const gtsam::Vector &state) override {
      assert(state.size() != 38);
      gtsam::Values values;
      try {
        values.insert(key1(), gtsam::Pose3(gtsam::Rot3::RzRyRx(state.block<3, 1>(0, 0)),
                                           gtsam::Point3(state.block<3, 1>(3, 0))));
        values.insert(key2(), gtsam::Vector3(state.block<3, 1>(6, 0)));
        values.insert(key3(), gtsam::Vector3(state.block<3, 1>(9, 0)));
        values.insert(key4(), gtsam::Vector6(state.block<6, 1>(12, 0)));

        values.insert(key5(), gtsam::Pose3(gtsam::Rot3::RzRyRx(state.block<3, 1>(18, 0)),
                                           gtsam::Point3(state.block<3, 1>(21, 0))));
        values.insert(key6(), gtsam::Vector3(state.block<3, 1>(24, 0)));
        values.insert(key7(), gtsam::Vector3(state.block<3, 1>(27, 0)));
        values.insert(key8(), gtsam::Vector6(state.block<6, 1>(30, 0)));
        values.insert(key9(), gtsam::Vector2(state.block<2, 1>(37, 0)));
      }
      catch (std::exception &ex) {
        std::cout << "Factor " << getName() << " cannot generate values from state vector " << state << " due to "
                  << ex.what() << std::endl;
      }
      return values;
    }

    /** return the measured */
    [[nodiscard]] gtsam::Vector1 measured() const {
      return gtsam::Vector1(measdRho_);
    }

    /** equals specialized to this factor */
    [[nodiscard]] bool equals(const gtsam::NonlinearFactor &expected, double tol = 1e-9) const override {
      const This *e = dynamic_cast<const This *> (&expected);
      return e != NULL && Base::equals(*e, tol)
             && gtsam::equal_with_abs_tol((gtsam::Vector1() << this->measdRho_).finished(),
                                          (gtsam::Vector1() << e->measdRho_).finished(), tol);
    }

    /** print contents */
    void print(const std::string &s = "",
               const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const override {
      std::cout << s << "GPInterpolatedDrFactorFull" << std::endl;
      Base::print("", keyFormatter);
    }

  private:
    /** Serialization function */
    friend class boost::serialization::access;

    template<class ARCHIVE>
    void serialize(ARCHIVE &ar, const unsigned int version) {
      ar & boost::serialization::make_nvp("GPInterpolatedDrFactorFull",
                                          boost::serialization::base_object<Base>(*this));
      ar & BOOST_SERIALIZATION_NVP(measdRho_);
    }
  };
} //namespace


/// traits
namespace gtsam {
  template<>
  struct traits<fgo::factor::GPInterpolatedDrFactor> :
    public Testable<fgo::factor::GPInterpolatedDrFactor> {
  };

  template<>
  struct traits<fgo::factor::GPInterpolatedDrFactorFull> :
    public Testable<fgo::factor::GPInterpolatedDrFactorFull> {
  };
}

#endif //ONLINE_FGO_GPINTERDRFACTORO_H