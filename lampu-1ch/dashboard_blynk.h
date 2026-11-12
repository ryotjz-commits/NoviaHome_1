#pragma once
#include "config.h"
#include "relay.h"

BLYNK_WRITE(V0) { setRelay(0, param.asInt()); }
BLYNK_WRITE(V1) { setRelay(1, param.asInt()); }
BLYNK_WRITE(V2) { setRelay(2, param.asInt()); }
BLYNK_WRITE(V3) { setRelay(3, param.asInt()); }