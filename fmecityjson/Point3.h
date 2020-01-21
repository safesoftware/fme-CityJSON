
#include <string>
#include <array>

class Point3 {
public:
  Point3(double x, double y, double z);
  std::string           get_key(int precision = 6);
  void                  translate(double x, double y, double z);
  double                x();
  double                y();
  double                z();
private:
  std::string _id;
  double      _x;
  double      _y;
  double      _z;

};