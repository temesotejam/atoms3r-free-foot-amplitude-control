#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
using std::min;
inline uint32_t host_us = 1000;
inline uint32_t micros() { return host_us; }
inline uint32_t millis() { return host_us / 1000U; }
inline void delay(uint32_t ms) { host_us += ms*1000U; }
class String {
 public:
  std::string value;
  String() = default;
  String(const char* s):value(s ? s : "") {}
  String(const std::string& s):value(s) {}
  template<class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  String(T v) { std::ostringstream s; s << +v; value = s.str(); }
  String(double v, int digits) { std::ostringstream s; s<<std::fixed<<std::setprecision(digits)<<v; value=s.str(); }
  void reserve(size_t n) { value.reserve(n); }
  const char* c_str() const {return value.c_str();}
  String& operator+=(const String& s) {value+=s.value;return *this;}
  friend String operator+(const String& a,const String& b) {return a.value+b.value;}
  void replace(const char* a,const char* b) {size_t p=0;while((p=value.find(a,p))!=std::string::npos){value.replace(p,strlen(a),b);p+=strlen(b);}}
};
