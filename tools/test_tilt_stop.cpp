#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include "tilt_stop.h"
using mekf6::Quaternion;
Quaternion multiply(Quaternion a, Quaternion b) {
  return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z, a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
      a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x, a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};
}
int main() {
  const mekf6::Vec3 upright{-0.021626f,0.033568f,0.999202f};
  mekf6::Mekf6 m; assert(m.initializeFromAccel(upright));
  for (float yaw : {-170.f,0.f,135.f}) {
    float y=mekf6::degToRad(yaw)*.5f;
    for (float angle : {-180.f,-100.f,-90.001f,-90.f,-89.99f,-20.f,0.f,20.f,89.99f,90.f,90.001f,100.f,180.f}) {
      float r=mekf6::degToRad(angle)*.5f;
      Quaternion q=multiply(multiply({cosf(y),0,0,sinf(y)}, {cosf(r),sinf(r),0,0}),m.quaternion());
      assert((tilt_stop::reason(q,true,2500,upright)!=nullptr)==(fabsf(angle)>=90.f));
      q={-q.w,-q.x,-q.y,-q.z};
      assert((tilt_stop::reason(q,true,2500,upright)!=nullptr)==(fabsf(angle)>=90.f));
    }
  }
  assert(!tilt_stop::reason(m.quaternion(),true,10000,upright));
  assert(!strcmp(tilt_stop::reason(m.quaternion(),true,10001,upright),"tilt_stale_mekf"));
  assert(tilt_stop::reason({NAN,0,0,0},true,0,upright));
  assert(tilt_stop::reason({0,0,0,0},true,0,upright));
  assert(tilt_stop::reason(m.quaternion(),false,0,upright));
  std::cout << "mechanical upright reference, +/-90 boundary, yaw/quaternion-sign invariance, stale/invalid attitude STOP PASS\n";
}
