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


#ifndef ONLINE_FGO_GPSFACTOR_H
#define ONLINE_FGO_GPSFACTOR_H

#pragma once

#include <cmath>
#include <fstream>
#include <iostream>
#include <utility>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/base/Matrix.h>
#include <gtsam/base/Vector.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/navigation/ImuBias.h>
#include <gtsam/base/numericalDerivative.h>

#include "model/gp_interpolator/GPWNOAInterpolator_old.h"
#include "utils/NavigationTools.h"
#include "factor/FactorTypeID.h"

namespace fgo::factor {
  class GPSFactor : public gtsam::NoiseModelFactor1<gtsam::Pose3> {
  protected:
    gtsam::Point3 pos_;
    gtsam::Vector3 lb_;

    typedef GPSFactor This;
    typedef gtsam::NoiseModelFactor1<gtsam::Pose3> Base;
    bool useAutoDiff_ = false;


  public:
    GPSFactor() = default;

    GPSFactor(const gtsam::Key &poseKey,
              const gtsam::Point3 &positionMeasured,
              const gtsam::Vector3 &lb,
              const gtsam::SharedNoiseModel &model,
              bool useAutoDiff = false) : Base(model, poseKey),
                                          pos_(positionMeasured), lb_(lb), useAutoDiff_(useAutoDiff) {
      factorTypeID_ = FactorTypeID::GPS;
      factorName_ = "GPSFactor";
    }

    ~GPSFactor() override = default;

    /// @return a deep copy of this factor
    [[nodiscard]] gtsam::NonlinearFactor::shared_ptr clone() const override {
      return boost::static_pointer_cast<gtsam::NonlinearFactor>(
        gtsam::NonlinearFactor::shared_ptr(new This(*this)));
    }

    [[nodiscard]] gtsam::Vector evaluateError(const gtsam::Pose3 &pose,
                                              boost::optional<gtsam::Matrix &> H1 = boost::none) const override {
      if (useAutoDiff_) {
        if (H1)
          *H1 = gtsam::numericalDerivative11<gtsam::Vector3, gtsam::Pose3>(
            boost::bind(&This::evaluateError_, this, boost::placeholders::_1),
            pose, 1e-5);

        return evaluateError_(pose);
      } else {
        gtsam::Matrix Hpose, Hrot, Hrot2;
        const auto err = pose.translation(&Hpose) + pose.rotation(&Hrot).rotate(lb_, &Hrot2) - pos_;
        if (H1) *H1 = Hrot2 * Hrot + Hpose;
        return err;
      }
    }

    [[nodiscard]] gtsam::Vector evaluateError_(const gtsam::Pose3 &pose) const {
      const auto err = pose.translation() + pose.rotation().rotate(lb_) - pos_;
      return err;
    }

    /** lifting all related state values in a vector after the ordering for evaluateError **/
    gtsam::Vector liftValuesAsVector(const gtsam::Values &values) override {
      const auto pose = values.at<gtsam::Pose3>(key());
      const auto liftedStates = (gtsam::Vector(6) << pose.rotation().rpy(),
        pose.translation()).finished();
      return liftedStates;
    }

    gtsam::Values generateValuesFromStateVector(const gtsam::Vector &state) override {
      assert(state.size() != 6);
      gtsam::Values values;
      try {
        values.insert(key(), gtsam::Pose3(gtsam::Rot3::RzRyRx(state.block<3, 1>(0, 0)),
                                          gtsam::Point3(state.block<3, 1>(3, 0))));
      }
      catch (std::exception &ex) {
        std::cout << "Factor " << getName() << " cannot generate values from state vector " << state << " due to "
                  << ex.what() << std::endl;
      }
      return values;
    }

    /** return the measured */
    [[nodiscard]] gtsam::Point3 measured() const {
      return pos_;
    }

    /** equals specialized to this factor */
    [[nodiscard]] bool equals(const gtsam::NonlinearFactor &expected, double tol = 1e-9) const override {
      const This *e = dynamic_cast<const This *> (&expected);
      return e != nullptr && Base::equals(*e, tol)
             && gtsam::equal_with_abs_tol((gtsam::Point3() << this->pos_).finished(),
                                          (gtsam::Point3() << e->pos_).finished(), tol);
    }

    /** print contents */
    void print(const std::string &s = "",
               const gtsam::KeyFormatter &keyFormatter = gtsam::DefaultKeyFormatter) const override {
      std::cout << s << "GPSFactor" << std::endl;
      Base::print("", keyFormatter);
    }

  private:

    /** Serialization function */
    friend class boost::serialization::access;

    template<class ARCHIVE>
    void serialize(ARCHIVE &ar, const unsigned int version) {
      ar & boost::serialization::make_nvp("GPSFactor",
                                          boost::serialization::base_object<Base>(*this));
      ar & BOOST_SERIALIZATION_NVP(pos_);
    }

  };
}

/// traits
namespace gtsam {
  template<>
  struct traits<fgo::factor::GPSFactor> : public Testable<fgo::factor::GPSFactor> {
  };
}

#endif //ONLINE_FGO_GPSFACTOR_H
