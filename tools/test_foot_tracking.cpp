#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include <cstring>
#include "host_v46o/Arduino.h"
#include "../src/foot_zero.h"
#include "../src/white_marker_tracker.h"
#include "../src/foot_angle_estimator.h"
using Image = std::vector<uint8_t>;
static Image blank(uint8_t level=30){return Image(320*240,level);}
static void band(Image& image,int x,int y,int half_width=10,uint8_t level=230){
  for(int row=y-10;row<=y+10;++row)for(int col=x-half_width;col<=x+half_width;++col)
    if(row>=0&&row<240&&col>=0&&col<320)image[row*320+col]=level;
}
static WhiteMarkerObservation observe(WhiteMarker1DTracker& tracker,const Image& image){
  host_us+=100000;return tracker.process(image.data());
}
static void verticalRecovery(){
  for(int dy=-32;dy<=32;++dy)for(int x:{35,80,160,210,260}){
    WhiteMarker1DTracker a(0),b(1);
    auto image=blank();band(image,x,66+dy);band(image,320-x,160-dy);
    const auto ma=a.process(image.data()),mb=b.process(image.data());
    assert(ma.valid&&mb.valid);
    assert(std::abs(ma.center_x_px-x)<0.01&&std::abs(mb.center_x_px-(320-x))<0.01);
    assert(ma.candidate_count==1&&mb.candidate_count==1);
    assert(ma.templates_tested==17&&mb.templates_tested==17);
    assert(std::abs(ma.center_y_px-66)<=32&&std::abs(mb.center_y_px-160)<=32);
    auto upper=blank();band(upper,x,66+dy);WhiteMarker1DTracker isolatedB(1);
    assert(!isolatedB.process(upper.data()).valid);
    auto lower=blank();band(lower,x,160+dy);WhiteMarker1DTracker isolatedA(0);
    assert(!isolatedA.process(lower.data()).valid);
  }
  for(int dy:{-24,24}){
    auto image=blank();
    for(int row=66+dy-10;row<=66+dy+10;++row){
      const int x=160+(row-66-dy)/2;
      for(int col=x-10;col<=x+10;++col)image[row*320+col]=230;
    }
    WhiteMarker1DTracker a(0);const auto ma=a.process(image.data());
    assert(ma.valid&&std::abs(ma.center_x_px-160)<3);
  }
  for(int level:{0,30,230,255}){
    auto image=blank(level);WhiteMarker1DTracker a(0);const auto empty=a.process(image.data());
    assert(!empty.valid&&empty.reason==MarkerDetectionReason::LowContrast);
  }
  auto image=blank();band(image,160,90,10,50);WhiteMarker1DTracker dim(0);
  assert(!dim.process(image.data()).valid);
  image=blank();band(image,160,66,70);WhiteMarker1DTracker wide(0);
  assert(wide.process(image.data()).reason==MarkerDetectionReason::BadShape);
  image=blank();band(image,0,66,1);WhiteMarker1DTracker edge(0);
  assert(!edge.process(image.data()).valid);
}
static void wrongZeroReproduction(){
  WhiteMarker1DTracker a(0),b(1);FootZero zero;
  auto upright=blank();band(upright,172,66);band(upright,175,184);
  band(upright,102,160,10,140); // nominal-row reflection, not the left white marker
  for(uint32_t t=0;t<=2200;t+=100){
    host_us+=100000;const auto ma=a.process(upright.data()),mb=b.process(upright.data());
    assert(ma.valid&&mb.valid&&std::abs(mb.center_x_px-175)<.01);
    assert(mb.candidate_count==2&&std::abs(mb.alternate_x_px-102)<.01);
    zero.observe(t,1,true,ma.valid&&mb.valid,ma.center_x_px,mb.center_x_px);
  }
  assert(zero.ready&&std::abs(zero.a_zero-172)<.01&&std::abs(zero.b_zero-175)<.01);
  WhiteMarkerObservation ma,mb;
  for(int step=1;step<=20;++step){
    const float f=step/20.0f;auto image=blank();
    band(image,std::lround(172-137*f),std::lround(66+20*f));
    band(image,std::lround(175-140*f),std::lround(184-4*f));
    host_us+=100000;ma=a.process(image.data());mb=b.process(image.data());
    assert(ma.valid&&mb.valid);
    zero.observe(2300+step*100,2,false,true,ma.center_x_px,mb.center_x_px);
  }
  const auto right=estimateFootAngle(ma,zero.a_zero,true),left=estimateFootAngle(mb,zero.b_zero,true);
  assert(right.valid&&left.valid);
  // This synthetic image moves A by 137 px and B by 140 px. Verify each
  // independent displacement, not an artificial left/right angle constraint.
  assert(std::abs(right.angle_deg-21.2550454)<.001&&std::abs(left.angle_deg-21.7695462)<.001);
  std::cout<<"wrong-feature zero reproduction: right="<<right.angle_deg<<" left="<<left.angle_deg<<" deg\n";
}
static void identityAndAmbiguity(){
  auto image=blank();band(image,100,66);band(image,175,90);
  WhiteMarker1DTracker ambiguous(0);const auto amb=observe(ambiguous,image);
  assert(!amb.valid&&amb.reason==MarkerDetectionReason::Ambiguous&&amb.candidate_count==2);
  // Nearby separate peaks are never blended into a centroid in the dark gap.
  image=blank();band(image,145,66,4);band(image,172,66,4,140);
  WhiteMarker1DTracker islands(0);const auto island=observe(islands,image);
  assert(island.valid&&std::abs(island.center_x_px-145)<.01);
  // A stronger distant reflection cannot take over an established track.
  WhiteMarker1DTracker tracked(0);image=blank();band(image,60,66);
  assert(observe(tracked,image).valid);
  image=blank();band(image,65,70);band(image,230,66,10,255);
  const auto kept=observe(tracked,image);assert(kept.valid&&std::abs(kept.center_x_px-65)<.01);
  image=blank();band(image,230,66,10,255);const auto jump=observe(tracked,image);
  assert(!jump.valid&&jump.reason==MarkerDetectionReason::TrackJump);
  host_us+=600000;
  const auto first=observe(tracked,image),second=observe(tracked,image),third=observe(tracked,image);
  assert(!first.valid&&!second.valid&&third.valid);
  assert(first.reason==MarkerDetectionReason::Reacquiring);
  const auto missing=tracked.process(nullptr);
  assert(!missing.valid&&missing.reason==MarkerDetectionReason::NoFrame&&missing.center_x_px==0);
  // The two feet remain independent after zero; different actual angles must
  // not be forced to agree by a left/right constraint.
  WhiteMarkerObservation a,b;a.valid=b.valid=true;a.id=0;b.id=1;a.center_x_px=160;b.center_x_px=120;
  assert(std::abs(estimateFootAngle(a,170,true).angle_deg-1.55146317)<.00001);
  assert(std::abs(estimateFootAngle(b,170,true).angle_deg-7.77483791)<.00001);
  assert(!estimateFootAngle(a,170,false).valid);
  a.center_x_px=10;const auto outside=estimateFootAngle(a,170,true);
  assert(outside.valid&&!outside.in_calibration_range);
}
static void zeroGates(){
  FootZero z;
  for(uint32_t t=0;t<1800;t+=100)z.observe(t,1,true,true,169,174);
  assert(!z.ready);
  z.observe(1800,2,true,true,171,176);assert(z.count==1);
  for(uint32_t t=1900;t<=3800;t+=100)z.observe(t,2,true,true,171,176);
  assert(z.ready&&z.a_zero==171&&z.b_zero==176);
  z.observe(5000,3,false,false,0,0);assert(z.ready&&z.a_zero==171);
  FootZero wrong;
  for(uint32_t t=0;t<10000;t+=100)wrong.observe(t,1,true,true,172,101.6366);
  assert(!wrong.ready&&wrong.reason==FootZeroReason::PositionMismatch);
  FootZero drift;
  for(uint32_t t=0;t<10000;t+=100)drift.observe(t,1,true,true,172,165+((t/100)%20));
  assert(!drift.ready); // stationary body with a moving foot is not a zero
  FootZero missed;
  for(uint32_t t=0;t<1500;t+=100)missed.observe(t,1,true,true,169,174);
  missed.observe(1900,1,true,true,169,174);assert(missed.count==1);
  missed.observe(2000,1,true,false,169,174);assert(missed.count==0);
  FootZero slow;for(uint32_t t=0;t<10000;t+=500)slow.observe(t,1,true,true,169,174);assert(!slow.ready);
}
static void hardwareRangeReplay(){
  // Original 0.47.4 pixel positions remain accepted with the new v2 slopes.
  const float rows[][4]={{171.7313f,170.7977f,-.04427876f,-.12231375f},
                        {41.7336f,40.3504f,20.12438555f,20.16181853f}};
  for(const auto& row:rows)for(int side=0;side<2;++side){
    WhiteMarkerObservation m;m.id=side;m.valid=true;m.center_x_px=row[side];
    const auto estimate=estimateFootAngle(m,side==0?171.4459f:170.0111f,true);
    assert(estimate.valid&&estimate.in_calibration_range);
    assert(std::abs(estimate.angle_deg-row[side+2])<.001);
  }
  for(int side=0;side<2;++side){
    WhiteMarkerObservation m;m.id=side;m.valid=true;
    const float lo=side==0?40:39,hi=side==0?173:177.5f;
    for(float x:{lo,hi}){m.center_x_px=x;assert(estimateFootAngle(m,170,true).in_calibration_range);}
    for(float x:{lo-.01f,hi+.01f,35.f}){m.center_x_px=x;assert(!estimateFootAngle(m,170,true).in_calibration_range);}
  }
  std::cout<<"hardware frames 694/781: accepted pixel range preserved with v2 angle calibration PASS\n";
}
static void fixedPoseCalibrationReplay(){
  // Means from the supplied 0.47.6 file; first two rows fit, last two held out.
  // Expected residual = delta foot angle + delta body roll, in degrees.
  const float rows[][5]={{171.64306f,169.55292f,0,0,0},
    {40.95084f,39.15524f,-20.27641653f,0,0},
    {148.0924f,146.0271f,-3.33639326f,.31740489f,.32179548f},
    {115.27286f,113.29306f,-8.48587166f,.25975724f,.26235420f}};
  for(const auto& row:rows)for(int side=0;side<2;++side){
    WhiteMarkerObservation marker;marker.id=side;marker.valid=true;marker.center_x_px=row[side];
    const auto angle=estimateFootAngle(marker,side==0?171.64306f:169.55292f,true);
    assert(angle.valid&&angle.in_calibration_range);
    assert(std::abs(angle.angle_deg+row[2]-row[3+side])<.00002f);
    // A new measured boot zero changes the offset, not the calibrated scale.
    const auto shifted=estimateFootAngle(marker,(side==0?171.64306f:169.55292f)+1,true);
    assert(std::abs(shifted.angle_deg-angle.angle_deg-angle.deg_per_px)<.00001f);
  }
  std::cout<<"Fixed-pose scale fit and both held-out hand-supported pose residuals PASS\n";
}
int main(){verticalRecovery();wrongZeroReproduction();identityAndAmbiguity();zeroGates();hardwareRangeReplay();fixedPoseCalibrationReplay();
  std::cout<<"full-height candidates, separate peaks, identity gates, independent angles and neutral zero guards PASS\n";
}
