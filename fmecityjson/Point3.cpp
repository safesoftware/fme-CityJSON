
#include "Point3.h"
#include "iostream"
#include <sstream>
#include <cmath>

Point3::Point3(double x, double y, double z) {
  _x = x;
  _y = y;
  _z = z;
}

std::string Point3::get_key(int precision) {
  char* buf = new char[100];
  std::stringstream ss;
  ss << "%." << precision << "f " << "%." << precision << "f " << "%." << precision << "f";
  std::sprintf(buf, ss.str().c_str(), _x, _y, _z);
  return buf;
}

void Point3::translate(double x, double y, double z) {
  _x += x;
  _y += y;
  _z += z;
}

double Point3::x() {
  return _x;
}

double Point3::y() {
  return _y;
}

double Point3::z() {
  return _z;
}
