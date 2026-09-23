#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include "../src/foot_zero.h"
#include "../src/white_marker_tracker.h"
#include "../src/foot_angle_estimator.h"
int main(){
  std::vector<uint8_t> image(320*240,30);
  for(int row:appcfg::kWhiteMarkerARows)for(int x=150;x<=170;++x)image[row*320+x]=230;
  for(int row:appcfg::kWhiteMarkerBRows)for(int x=110;x<=130;++x)image[row*320+x]=230;
  WhiteMarker1DTracker a(0),b(1);
  const auto ma=a.process(image.data()),mb=b.process(image.data());
  assert(ma.valid&&mb.valid&&std::abs(ma.center_x_px-160)<0.01&&std::abs(mb.center_x_px-120)<0.01);
  assert(!a.process(nullptr).valid);
  std::fill(image.begin(),image.end(),230);assert(!a.process(image.data()).valid); // common bright background
  const auto right=estimateFootAngle(ma,170,true),left=estimateFootAngle(mb,170,true);
  assert(std::abs(right.angle_deg-1.67779119)<0.00001);
  assert(std::abs(left.angle_deg-8.13226525)<0.00001);
  assert(!estimateFootAngle(ma,170,false).valid);
  auto outside=ma;outside.center_x_px=10;const auto extrap=estimateFootAngle(outside,170,true);
  assert(extrap.valid&&!extrap.in_calibration_range); // validity and support are distinct
  FootZero z;
  for(uint32_t t=0;t<1800;t+=100)z.observe(t,1,true,true,169,174);
  assert(!z.ready);
  z.observe(1800,2,true,true,171,176);assert(z.count==1); // movement between frames
  for(uint32_t t=1900;t<=3800;t+=100)z.observe(t,2,true,true,171,176);
  assert(z.ready&&z.a_zero==171&&z.b_zero==176);
  z.observe(5000,3,false,false,0,0);assert(z.ready&&z.a_zero==171); // boot lock is permanent
  FootZero missed;
  for(uint32_t t=0;t<1500;t+=100)missed.observe(t,1,true,true,169,174);
  missed.observe(1900,1,true,true,169,174);assert(missed.count==1);
  missed.observe(2000,1,true,false,169,174);assert(missed.count==0);
  FootZero slow;for(uint32_t t=0;t<10000;t+=500)slow.observe(t,1,true,true,169,174);assert(!slow.ready);
  std::cout<<"marker lanes, signed angles, support flags, continuous zero, dropouts and epoch reset PASS\n";
}
