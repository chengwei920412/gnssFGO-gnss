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

#ifndef ONLINE_FGO_GRAPHGNSSCENTRIC_H
#define ONLINE_FGO_GRAPHGNSSCENTRIC_H

#pragma once

#include "GraphTimeCentric.h"
#include "data/DataTypes.h"
#include "integrator/GNSSTCIntegrator.h"
#include "integrator/GNSSLCIntegrator.h"
#include "integrator/LIOIntegrator.h"

namespace fgo::graph {
  // ToDo:
  //              NOT FINISHED YET!
  //************************************************
  //************************************************
  //************************************************

  class GraphSensorCentric : public GraphTimeCentric {
    GraphSensorCentricParamPtr paramPtr_;

  public:
    typedef std::shared_ptr<GraphSensorCentric> Ptr;

    explicit GraphSensorCentric(gnss_fgo::GNSSFGOLocalizationBase &node);

    ~GraphSensorCentric() override {
      if (pubResidualsThread_)
        pubResidualsThread_->join();
    };

    StatusGraphConstruction constructFactorGraphOnIMU(
      std::vector<fgo::data::IMUMeasurement> &dataIMU
    ) override;

    StatusGraphConstruction constructFactorGraphOnTime(
      const std::vector<double> &stateTimestamps,
      std::vector<fgo::data::IMUMeasurement> &dataIMU
    ) override;

  };


}


#endif //ONLINE_FGO_GRAPHGNSSCENTRIC_H
