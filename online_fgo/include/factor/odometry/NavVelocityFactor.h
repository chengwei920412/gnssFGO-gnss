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

#ifndef ONLINE_FGO_VELOCITYFACTOR_H
#define ONLINE_FGO_VELOCITYFACTOR_H

#pragma once

#include <utility>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/base/Matrix.h>
#include <gtsam/base/Vector.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Point3.h>
#include "include/factor/FactorTypes.h"
#include "utils/NavigationTools.h"
#include "factor/FactorTypeIDs.h"
#include "third_party/matlab_utils.h"

namespace fgo::factor {

  class NavVelocityFactor : public gtsam::NoiseModelFactor2<gtsam::Pose3, gtsam::Vector3> {
  protected:
    gtsam::Vector3 velMeasured_;
    gtsam::Vector3 lb_;
    gtsam::Vector3 angularVelocity_;
    MeasurementFrame measuredVelFrame_ = MeasurementFrame::BODY;
    VelocityType type_ = VelocityType::VEL3D;
    bool useAutoDiff_ = false;

    typedef NavVelocityFactor This;
    typedef gtsam::NoiseModelFactor2<gtsam::Pose3, gtsam::Vector3> Base;

  public:
    NavVelocityFactor() = default;

    NavVelocityFactor(gtsam::Key poseKey, gtsam::Key velKey, const gtsam::Vector3 &velMeasured,
                      const gtsam::Vector3 &angularVelocity,
                      const gtsam::Vector3 &lb, MeasurementFrame velFrame, VelocityType type,
                      const gtsam::SharedNoiseModel &model, bool useAutoDiff = true) :
      Base(model, poseKey, velKey), velMeasured_(velMeasured), angularVelocity_(angularVelocity), lb_(lb),
      measuredVelFrame_(velFrame), type_(type), useAutoDiff_(useAutoDiff) {
      factorTypeID_ = FactorTypeID::NavVelocity;
      factorName_ = "NavVelocityFactor";
    }

    ~NavVelocityFactor() override = default;

    /// @return a deep copy of this factor
    [[nodiscard]] gtsam::NonlinearFactor::shared_ptr clone() const override {
      return boost::static_pointer_cast<gtsam::NonlinearFactor>(
        gtsam::NonlinearFactor::shared_ptr(new This(*this)));
    }

    /** factor error */
    [[nodiscard]] gtsam::Vector evaluateError(const gtsam::Pose3 &pose, const gtsam::Vector3 &vel,
                                              boost::optional<gtsam::Matrix &> H1 = boost::none,
                                              boost::optional<gtsam::Matrix &> H2 = boost::none) const override {
      static const auto varTrick6D = (gtsam::Vector6() << 1e-9, 1e-9, 1e-9, 1e-9, 1e-9, 1e-9).finished();
      static const auto varTrick3D = (gtsam::Vector3() << 1e-9, 1e-9, 1e-9).finished();
      if (useAutoDiff_) {

        switch (type_) {
          case VelocityType::VEL3D: {
            if (H1) {
              *H1 = gtsam::numericalDerivative11<gtsam::Vector3, gtsam::Pose3>(
                boost::bind(&This::evaluateError_, this, boost::placeholders::_1, vel), pose, 1e-5);

            }
            if (H2) {
              *H2 = gtsam::numericalDerivative11<gtsam::Vector3, gtsam::Vector3>(
                boost::bind(&This::evaluateError_, this, pose, boost::placeholders::_1), vel, 1e-5);
            }
            break;
          }
          case VelocityType::VEL2D: {
            if (H1) {
              *H1 = gtsam::numericalDerivative11<gtsam::Vector2, gtsam::Pose3>(
                boost::bind(&This::evaluateError_, this, boost::placeholders::_1, vel), pose, 1e-5);
              //std::cout << "*H1: " << *H1 << std::endl;
            }
            if (H2) {
              *H2 = gtsam::numericalDerivative11<gtsam::Vector2, gtsam::Vector3>(
                boost::bind(&This::evaluateError_, this, pose, boost::placeholders::_1), vel, 1e-5);
            }
            break;
          }
          case VelocityType::VELX:
          case VelocityType::VELY: {

            if (H1) {
              *H1 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Pose3>(
                boost::bind(&This::evaluateError_, this, boost::placeholders::_1, vel), pose, 1e-5);
              //*H1 += varTrick6D;
              //std::cout << "NavVelocityFactor *H1 " << *H1<< std::endl;
            }
            if (H2) {
              *H2 = gtsam::numericalDerivative11<gtsam::Vector1, gtsam::Vector3>(
                boost::bind(&This::evaluateError_, this, pose, boost::placeholders::_1), vel, 1e-5);
              //*H2 += varTrick3D;
              //std::cout << "NavVelocityFactor *H2 " << *H2<< std::endl;
            }
            break;
          }
        }
        return evaluateError_(pose, vel);
      } else {
        gtsam::Matrix Hpos, Hrot1, Hrot2;
        const auto &pos = pose.translation(Hpos);
        const auto &rot = pose.rotation(Hrot1);
        const auto lbv_b = gtsam::skewSymmetric(-lb_) * angularVelocity_;
        const auto pose_sensor = pos + rot.rotate(lb_, Hrot2);
        gtsam::Vector3 error;

        /*
         * Shape
         * H1: (1-3) * 6
         * H2: (1-3) * 3
         */

        gtsam::Matrix HH1, HH2, HH3;

        switch (measuredVelFrame_) {
          case MeasurementFrame::NED: {
            gtsam::Matrix Hrot_vel;
            const auto lbv = rot.rotate(lbv_b);
            const auto nRe = gtsam::Rot3(fgo::utils::nedRe_Matrix(pose_sensor));
            const auto jac = matlab_utils::jacobianECEF2ENU(pose_sensor, vel + lbv);
            const auto veln = nRe.rotate(vel + lbv);
            if (H1) HH1 = jac.block<3, 3>(0, 0) * (Hpos + Hrot2 * Hrot1);
            if (H2) HH2 = jac.block<3, 3>(3, 0);  // should be NOT COMPLETELY CORRECT
            error = veln - velMeasured_;
            break;
          }
          case MeasurementFrame::ENU: {
            const auto lbv = rot.rotate(lbv_b);
            const auto nRe = gtsam::Rot3(fgo::utils::enuRe_Matrix(pos));
            const auto jac = matlab_utils::jacobianECEF2NED(pose_sensor, vel + lbv);
            const auto veln = nRe.rotate(vel + lbv);
            if (H1) HH1 = jac.block<3, 3>(0, 0) * (Hpos + Hrot2 * Hrot1);
            if (H2) HH2 = jac.block<3, 3>(3, 0);  // should be NOT COMPLETELY CORRECT
            error = veln - velMeasured_;
            break;
          }
          case MeasurementFrame::BODY: {
            gtsam::Matrix Hrot3, Hrot_vel;
            error = rot.unrotate(vel, Hrot3, Hrot_vel) - velMeasured_ + lbv_b;
            if (H1) HH1 = Hrot3 * Hrot1;
            if (H2) HH2 = Hrot_vel;  // should be NOT COMPLETELY CORRECT
            break;
          }
          default: {
            const auto lbv = rot.rotate(lbv_b);
            if (H1) HH1 = (gtsam::Matrix36() << gtsam::Z_3x3, gtsam::I_3x3).finished();
            if (H2) HH2 = gtsam::Matrix33::Identity();
            error = vel + lbv - velMeasured_;
            break;
          }
        }

        if (error.hasNaN())
          error = gtsam::Vector3::Zero();

        switch (type_) {
          case VEL3D: {
            if (H1) *H1 = HH1;
            if (H2) *H2 = HH2;
            return error;
          }
          case VEL2D: {
            if (H1) *H1 = HH1.block<2, 6>(0, 0);
            if (H2) *H2 = HH2.block<2, 3>(0, 0);
            return error.head(2);
          }
          case VELX: {
            if (H1) *H1 = HH1.block<1, 6>(0, 0);
            if (H2) *H2 = HH2.block<1, 3>(0, 0);

            std::cout << "H1 " << HH1.block<1, 6>(0, 0) << std::endl;
            return (gtsam::Vector1() << error.x()).finished();
          }
          case VELY: {
            if (H1) *H1 = HH1.block<1, 6>(1, 0);
            if (H2) *H2 = HH2.block<1, 3>(1, 0);
            return (gtsam::Vector1() << error.y()).finished();
          }
        }
      }

    }

    [[nodiscard]] gtsam::Vector evaluateError_(const gtsam::Pose3 &pose, const gtsam::Vector3 &vel) const {

      gtsam::Vector3 error;
      const auto lbv_b = gtsam::skewSymmetric(-lb_) * angularVelocity_;
      const auto &rot = pose.rotation();
      const auto pose_sensor = pose.translation() + rot.rotate(lb_);

      switch (measuredVelFrame_) {
        case MeasurementFrame::NED: {
          const auto lbv = rot.rotate(lbv_b);
          const auto nRe = gtsam::Rot3(fgo::utils::nedRe_Matrix(pose_sensor));
          const auto veln = nRe.rotate(vel + lbv);
          error = veln - velMeasured_;
          break;
        }
        case MeasurementFrame::ENU: {
          const auto lbv = rot.rotate(lbv_b);
          const auto nRe = gtsam::Rot3(fgo::utils::enuRe_Matrix(pose_sensor));
          const auto veln = nRe.rotate(vel + lbv);
          error = veln - velMeasured_;
          break;
        }
        case MeasurementFrame::BODY: {
          if (rot.rpy().hasNaN())
            std::cout << "VELFACTOR ROTATION HAS NAN" << std::endl;

          if (vel.hasNaN())
            std::cout << "VELFACTOR VELOCITY HAS NAN" << std::endl;
          const auto velb = rot.unrotate(vel) + lbv_b;
          error = velb - velMeasured_;
          break;
        }
        default: {
          const auto lbv = rot.rotate(lbv_b);
          error = vel + lbv - velMeasured_;
          break;
        }
      }

      if (error.hasNaN())
        error = gtsam::Vector3::Zero();

      switch (type_) {
        case VEL3D:
          return error;
        case VEL2D: {
          //std::cout << "error 2d: " << error << std::endl;
          return error.head(2);
        }
        case VELX: {
          //std::cout << "error velx: " << error << std::endl;
          return (gtsam::Vector1() << error.x()).finished();
        }
        case VELY:
          return (gtsam::Vector1() << error.y()).finished();
      }
    }

    /** lifting all related state values in a vector after the ordering for evaluateError **/
    gtsam::Vector liftValuesAsVector(const gtsam::Values &values) override {
      const auto poseI = values.at<gtsam::Pose3>(key1());
      const auto vel = values.at<gtsam::Vector3>(key2());
      const auto liftedStates = (gtsam::Vector(9) <<
                                                  poseI.rotation().rpy(),
        poseI.translation(), vel).finished();
      return liftedStates;
    }

    gtsam::Values generateValuesFromStateVector(const gtsam::Vector &state) override {
      assert(state.size() != 9);
      gtsam::Values values;
      try {
        values.insert(key1(), gtsam::Pose3(gtsam::Rot3::RzRyRx(state.block<3, 1>(0, 0)),
                                           gtsam::Point3(state.block<3, 1>(3, 0))));
        values.insert(key2(), gtsam::Vector3(state.block<3, 1>(6, 0)));
      }
      catch (std::exception &ex) {
        std::cout << "Factor " << getName() << " cannot generate values from state vector " << state << " due to "
                  << ex.what() << std::endl;
      }
      return values;
    }

/** return the measured */
    [[nodiscard]] gtsam::Vector3 measured() const {
      return velMeasured_;
    }

    /** equals specialized to this factor */
    [[nodiscard]] bool equals(const gtsam::NonlinearFactor &expected, double tol = 1e-9) const override {
      const This *e = dynamic_cast<const This *> (&expected);
      return e != nullptr && Base::equals(*e, tol)
             && gtsam::equal_with_abs_tol(this->velMeasured_,
                                          e->velMeasured_, tol);
    }

    /** print contents */
    void print(const std::string &s = "",
               const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const override {
      std::cout << s << "NavVelocityFactor" << std::endl;
      Base::print("", keyFormatter);
    }

  private:
    /** Serialization function */
    friend class boost::serialization::access;

    template<class ARCHIVE>
    void serialize(ARCHIVE &ar, const unsigned int version) {
      ar & boost::serialization::make_nvp("NavVelocityFactor",
                                          boost::serialization::base_object<Base>(*this));
      ar & BOOST_SERIALIZATION_NVP(velMeasured_);
    }

  };
}

/// traits
namespace gtsam {
  template<>
  struct traits<fgo::factor::NavVelocityFactor> :
    public Testable<fgo::factor::NavVelocityFactor> {
  };
}
#endif //ONLINE_FGO_VELOCITYFACTOR_H
