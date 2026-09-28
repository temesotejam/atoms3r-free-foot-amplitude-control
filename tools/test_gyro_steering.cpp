#include "gyro_steering.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
  using namespace steering;
  // True ZYX trajectories -> body rates. Heading remains independent of tilt.
  for (float turn : {0.0f,84.57f,-16.38f,370.0f}) {
    GyroHeading h; assert(h.reset(0,0,1,0xffff0000U));
    for (unsigned i=1;i<=12000;++i) {
      const float t=i*.0025f, f=2*3.14159265f*1.1f;
      const float r=2.5f*kRad*sinf(f*t), p=12*kRad*sinf(f*t);
      const float rd=2.5f*kRad*f*cosf(f*t), pd=12*kRad*f*cosf(f*t), yd=turn/30*kRad;
      h.update((rd-yd*sinf(p))*kDeg,
               (pd*cosf(r)+yd*sinf(r)*cosf(p))*kDeg,
               (-pd*sinf(r)+yd*cosf(r)*cosf(p))*kDeg,0xffff0000U+i*2500);
    }
    assert(h.valid()); assert(fabsf(h.yaw()-turn)<.04f);
    const float previous=h.yaw();
    h.update(NAN,0,0,0xffff0000U+12000*2500); // duplicate isn't a new sample
    assert(h.yaw()==previous);
    h.update(0,0,0,0xffff0000U+12000*2500+10001);
    assert(!h.valid() && !std::isfinite(h.yaw()));
  }
  GyroHeading bad; assert(!bad.reset(0,0,0,1)); assert(!bad.reset(NAN,0,1,1));
  assert(bad.reset(0,0,1,1)); bad.update(INFINITY,0,0,2501); assert(!bad.valid());
  // Deterministic illustrative plant. Not a claim of physical closed-loop stability.
  for (float disturbance : {-2.f,0.f,2.f}) {
    Controller c; float yaw=0,rate=disturbance,delta=0;
    for (unsigned cycle=0;cycle<40;++cycle) {
      const uint32_t ms=500+1000*cycle;
      // Geometric asymmetry can remain at zero yaw.
      const float difference=-1.2f+2*c.state().delta_deg;
      rate=disturbance-1.5f*(difference+1.2f);
      yaw+=rate;
      c.peak(1,10+difference/2,yaw-rate/2,ms,false,false);
      c.peak(-1,10-difference/2,yaw,ms+500,false,false);
      const auto& s=c.state();
      assert(fabsf(s.delta_deg)<=kLimitDeg && fabsf(s.delta_deg-delta)<=kStepDeg+1e-6f);
      assert(fabsf((c.target(10,1)+c.target(10,-1))/2-10)<1e-6f);
      if(ms+500<kSettleMs) assert(s.delta_deg==0);
      delta=s.delta_deg;
    }
    assert(fabsf(rate)<.45f);
    if(disturbance==0) { assert(fabsf(delta)<1e-6f); assert(fabsf(c.state().actual_difference_deg+1.2f)<1e-5f); }
  }
  // Both required actuator directions blocked: no windup. Reverse rotation releases.
  Controller c;float yaw=0;
  for(unsigned i=0;i<40;++i) {
    yaw+=3;
    c.peak(1,9,yaw-1.5f,i*1000+500,true,false);
    c.peak(-1,11,yaw,i*1000+1000,false,true);
  }
  assert(c.state().delta_deg==0 && c.state().reason==Reason::Saturated);
  for(unsigned i=40;i<50;++i) {
    yaw-=3;c.peak(1,9,yaw+1.5f,i*1000+500,true,false);
    c.peak(-1,11,yaw,i*1000+1000,false,true);
  }
  assert(c.state().delta_deg<0);
  const float held=c.state().delta_deg;
  c.peak(-1,11,NAN,51000,false,false);
  assert(c.state().delta_deg==held && c.state().reason==Reason::Invalid);
  c.reset(); assert(c.state().delta_deg==0 && c.state().cycles==0);
  puts("Independent 3D gyro, zero-yaw sway, wrap/gap faults, both steering signs, mean/step bounds, nonzero straight asymmetry and saturation/reversal PASS");
}
