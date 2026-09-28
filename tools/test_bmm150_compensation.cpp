#include "bmm150_observation.h"
#include <cassert>
#include <cmath>
#include <iostream>
struct Case {int16_t x,y,z;uint16_t hall;float X,Y,Z;};
const Case cases[]={
#include "fixtures/bmm150_bosch_float_vectors.inc"
};
int main(){
 using namespace bmm150_observation;
 const uint8_t a[]={251,4},b[]={232,3,253,251},c[]={104,197,245,127,168,97,12,254,254,40};
 auto trim=decodeTrim(a,b,c);
 assert(trim.x1==-5&&trim.y1==4&&trim.x2==-3&&trim.y2==-5&&trim.xy1==40&&trim.xy2==-2&&trim.z1==32757&&trim.z2==-15000&&trim.z3==-500&&trim.z4==1000&&trim.xyz1==25000);
 for(const auto& v:cases){
  uint8_t p[8];auto put=[&](int i,uint16_t n){p[i]=n&255;p[i+1]=n>>8;};
  put(0,uint16_t(v.x*8));put(2,uint16_t(v.y*8));put(4,uint16_t(v.z*2));put(6,uint16_t(v.hall*4)|1);
  auto r=compensate(p,trim);assert(r.valid&&r.raw_x==v.x&&r.raw_y==v.y&&r.raw_z==v.z&&r.rhall==v.hall);
  auto same=[](float x,float y){return fabsf(x-y)<=1e-5f*fmaxf(1.f,fabsf(y));};
  assert(same(r.x_uT,v.X)&&same(r.y_uT,v.Y)&&same(r.z_uT,v.Z));
 }
 uint8_t p[8]={0,128,0,0,0,0,1,4};assert(!compensate(p,trim).valid);
 p[1]=0;p[5]=128;assert(!compensate(p,trim).valid);
 std::cout<<"BMM150: 64 Bosch float vectors, signed decoding, trim map, overflow PASS\n";
}
