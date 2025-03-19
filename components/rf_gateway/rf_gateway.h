#pragma once

#include "esphome.h"

class RFGateway : public Component {
 public:
  void setup() override;
  void send_rf_data(const std::string &payload);
};
