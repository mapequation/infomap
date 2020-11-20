//
// Created by Christopher Blöcker on 2020-11-12.
//

#ifndef INFOMAP_DOUBLEPAIR_H
#define INFOMAP_DOUBLEPAIR_H

#include <utility>
#include <ostream>

std::pair<double, double> operator-(const std::pair<double, double> p);
std::pair<double, double> operator*(const std::pair<double, double> p, double s);
std::pair<double, double> operator*(double s, const std::pair<double, double> p);
std::pair<double, double> operator/(const std::pair<double, double> p, const double s);
std::pair<double, double> operator+(const std::pair<double, double> p1, const std::pair<double, double> p2);
std::pair<double, double> operator-(const std::pair<double, double> p1, const std::pair<double, double> p2);
std::pair<double, double> operator/(const std::pair<double, double> p1, const std::pair<double, double> p2);
std::pair<double, double> operator*(const std::pair<double, double> p1, const std::pair<double, double> p2);
std::pair<double, double>& operator+=(std::pair<double, double>& self, const std::pair<double, double> other);
std::pair<double, double>& operator-=(std::pair<double, double>& self, const std::pair<double, double> other);
std::pair<double, double>& operator*=(std::pair<double, double>& self, const std::pair<double, double> other);
double total(std::pair<double, double>);

namespace infomap
{
  std::ostream& operator<<(std::ostream& out, const std::pair<double, double>& data);
}
#endif //INFOMAP_DOUBLEPAIR_H
