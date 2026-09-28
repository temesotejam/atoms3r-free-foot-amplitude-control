#pragma once
#include <Arduino.h>
#include "bmm150_observation.h"
inline String magneticObservationJson(const bmm150_observation::RawReading& raw, uint32_t now) {
  const auto m=bmm150_observation::compensate(raw.aux, bmm150_observation::startupTrimStorage());
  const auto num=[](float v) { return isfinite(v)?String(v,6):String("null"); };
  const bool fresh=raw.sequence && uint32_t(now-raw.sample_us)<=250000u;
  String s; s.reserve(512);
  s="{\"role\":\"observation_only\",\"installation_calibrated\":false";
  s+=",\"factory_ok\":"+String(m.factory_ok?"true":"false");
  s+=",\"valid\":"+String(m.valid?"true":"false");
  s+=",\"fresh\":"+String(fresh?"true":"false");
  s+=",\"sample_us\":"+String(raw.sample_us)+",\"sequence\":"+String(raw.sequence);
  s+=",\"age_us\":"+(raw.sequence?String(uint32_t(now-raw.sample_us)):String("null"));
  s+=",\"body_uT\":["+num(m.x_uT)+","+num(m.y_uT)+","+num(m.z_uT)+"]";
  s+=",\"norm_uT\":"+num(m.norm_uT)+"}";
  return s;
}
