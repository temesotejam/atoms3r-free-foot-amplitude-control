#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include <cstring>
#include "../src/foot_zero.h"
#include "../src/white_marker_tracker.h"
#include "../src/foot_angle_estimator.h"
using Image = std::vector<uint8_t>;
static Image blank(uint8_t level=30){return Image(appcfg::kFrameWidth*appcfg::kFrameHeight,level);}
static void band(Image& image,int x,int y,int half_width=10,uint8_t level=230){
  for(int row=y-10;row<=y+10;++row)for(int col=x-half_width;col<=x+half_width;++col)
    if(row>=0&&row<240&&col>=0&&col<320)image[row*320+col]=level;
}
static void verticalRecovery(){
  WhiteMarker1DTracker a(0),b(1);
  // Every pixel offset, including those between templates; independently moving
  // lanes, the same and different X, and X outside calibration support.
  for(int dy=-32;dy<=32;++dy)for(int x:{40,80,160,210,260}){
    auto image=blank();band(image,x,66+dy);band(image,320-x,160-dy);
    const auto ma=a.process(image.data()),mb=b.process(image.data());
    assert(ma.valid&&mb.valid);
    assert(std::abs(ma.center_x_px-x)<0.01&&std::abs(mb.center_x_px-(320-x))<0.01);
    assert(ma.reason==MarkerDetectionReason::Detected&&mb.reason==MarkerDetectionReason::Detected);
    assert(ma.templates_tested<=appcfg::kWhiteMaxTemplates&&mb.templates_tested<=appcfg::kWhiteMaxTemplates);
    assert(std::abs(ma.center_y_px-66)<=32&&std::abs(mb.center_y_px-160)<=32);
    if(std::abs(dy)>=16)assert(ma.templates_tested==17&&mb.templates_tested==17);
    // An isolated opposite marker must not be assigned to this lane, even when
    // moved to the nearest edge of its supported search region.
    auto upper=blank();band(upper,x,66+dy);assert(!b.process(upper.data()).valid);
    auto lower=blank();band(lower,x,160+dy);assert(!a.process(lower.data()).valid);
  }
  // A diagonal stripe can move both vertically and horizontally; the detector
  // still has to recover the right feature, not the peak's left-hand edge.
  for(int dy:{-24,24}){
    auto image=blank();
    for(int row=66+dy-10;row<=66+dy+10;++row){
      const int x=160+(row-66-dy)/2;
      for(int col=x-10;col<=x+10;++col)image[row*320+col]=230;
    }
    const auto ma=a.process(image.data());
    assert(ma.valid&&std::abs(ma.center_x_px-160)<3);
  }
  auto image=blank();band(image,160,66);band(image,120,160);
  const auto ma=a.process(image.data()),mb=b.process(image.data());
  assert(ma.valid&&mb.valid&&ma.templates_tested==1&&mb.templates_tested==1);
  assert(ma.center_y_px==66&&mb.center_y_px==160); // Return to calibrated nominal rows.
  const auto missing=a.process(nullptr);
  assert(!missing.valid&&missing.reason==MarkerDetectionReason::NoFrame&&missing.templates_tested==0);
  assert(missing.success_count==ma.success_count&&missing.fail_count==ma.fail_count+1);
  assert(missing.center_x_px==0); // Never hold a previous valid coordinate as current.
  for(int level:{0,30,230,255}){
    image=blank(level);const auto empty=a.process(image.data());
    assert(!empty.valid&&empty.reason==MarkerDetectionReason::LowContrast&&empty.templates_tested==17);
  }
  image=blank();band(image,160,90,10,50); // A dim target must still fail the original threshold.
  assert(!a.process(image.data()).valid);
  image=blank();band(image,0,66,1);const auto edge=a.process(image.data());
  assert(!edge.valid&&edge.reason==MarkerDetectionReason::LowWeight);
  for(int dy:{-60,60}){
    image=blank();band(image,160,66+dy);assert(!a.process(image.data()).valid);
    image=blank();band(image,120,160+dy);assert(!b.process(image.data()).valid);
  }
  assert(!std::strcmp(markerDetectionReasonName(MarkerDetectionReason::LowContrast),"low_contrast"));
  assert(!std::strcmp(markerDetectionReasonName(MarkerDetectionReason::LowWeight),"low_weight"));
  assert(!std::strcmp(markerDetectionReasonName(MarkerDetectionReason::NoFrame),"no_frame"));
}
int main(){
  verticalRecovery();
  std::vector<uint8_t> image(320*240,30);
  for(int row:appcfg::kWhiteMarkerARows)for(int x=150;x<=170;++x)image[row*320+x]=230;
  for(int row:appcfg::kWhiteMarkerBRows)for(int x=110;x<=130;++x)image[row*320+x]=230;
  WhiteMarker1DTracker a(0),b(1);
  const auto ma=a.process(image.data()),mb=b.process(image.data());
  assert(ma.valid&&mb.valid&&std::abs(ma.center_x_px-160)<0.01&&std::abs(mb.center_x_px-120)<0.01);
  assert(ma.templates_tested==1&&mb.templates_tested==1); // Original sparse-row fixture, unchanged.
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
  std::cout<<"vertical recovery at every offset -32..32, lane isolation, low-quality rejection, nominal compatibility, signed angles and zero lock PASS\n";
}
