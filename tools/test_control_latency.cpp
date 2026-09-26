#include <cassert>
#include <iostream>
#include "control_latency.h"
int main() {
  using namespace control_latency;
  Span span; span.begin(UINT32_MAX-20); assert(span.at(29)==50);
  span.end(39); assert(span.at(99)==60);
  Profile p;
  p.receive(true,1,1000,600,1100,{100,60,8});
  p.mark(Derived,1130,{100,60,8});
  p.mark(Mekf,1500,{300,230,9});
  p.mark(Ready,1600,{400,310,9});
  p.mark(Decision,1650,{400,310,9});
  p.finish(1800,{400,310,9});
  assert(p.worst[0].poll_to_sample_us==400);
  assert(p.worst[0].age[Ready]==600 && p.worst[0].age[Done]==800);
  assert(p.worst[0].poll_overlap_to_ready_us==300 && p.worst[0].io_overlap_to_ready_us==250);
  assert(p.age[Decision].count==1 && p.worst[0].valid==63);
  p.receive(true,2,2000,1600,2050,{400,310,9});
  p.mark(Ready,2100,{400,310,9}); p.finish(2200,{400,310,9});
  assert(p.worst[0].sequence==1 && p.age[Done].count==2 && p.age[Decision].count==1);
  PsramString out; assert(out.reserve(12000)); p.appendJson(out); assert(out.ok());
  assert(std::string(out.c_str()).find("not_cpu")!=std::string::npos);
  p.reset(); assert(!p.epoch && !p.worst[0].sequence && !p.age[Done].count);
  p.receive(false,3,2000,1600,2050,{}); p.finish(2200,{}); assert(!p.age[Done].count);
  std::cout << "Latency: clock wrap, overlap, independent counts, worst buckets, reset and idle exclusion PASS\n";
}
