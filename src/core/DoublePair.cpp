//
// Created by Christopher Blöcker on 2020-11-12.
//

#include "DoublePair.h"

std::pair<double, double> operator-(const std::pair<double, double> p)
{
  return { -p.first, -p.second };
}

std::pair<double, double> operator*(const std::pair<double, double> p, double s)
{
  return { s * p.first, s * p.second };
}

std::pair<double, double> operator*(double s, const std::pair<double, double> p)
{
  return { s * p.first, s * p.second };
}

std::pair<double, double> operator/(const std::pair<double, double> p, const double s)
{
  return { p.first / s, p.second / s };
}

std::pair<double, double> operator+(const std::pair<double, double> p1, const std::pair<double, double> p2)
{
  return { p1.first + p2.first, p1.second + p2.second };
}

std::pair<double, double> operator-(const std::pair<double, double> p1, const std::pair<double, double> p2)
{
  return { p1.first - p2.first, p1.second - p2.second };
}

std::pair<double, double> operator/(const std::pair<double, double> p1, const std::pair<double, double> p2)
{
  return { p1.first / p2.first, p1.second / p2.second };
}

std::pair<double, double> operator*(const std::pair<double, double> p1, const std::pair<double, double> p2)
{
  return std::make_pair(p1.first * p2.first, p1.second * p2.second);
}

std::pair<double, double>& operator+=(std::pair<double, double>& self, const std::pair<double, double> other)
{
  self.first += other.first;
  self.second += other.second;
  return self;
}

std::pair<double, double>& operator-=(std::pair<double, double>& self, const std::pair<double, double> other)
{
  self.first -= other.first;
  self.second -= other.second;
  return self;
}

std::pair<double, double>& operator*=(std::pair<double, double>& self, const std::pair<double, double> other)
{
  self.first *= other.first;
  self.second *= other.second;
  return self;
}

double total(const std::pair<double, double> p)
{
  return p.first + p.second;
}

namespace infomap
{
  std::ostream& operator<<(std::ostream& out, const std::pair<double, double>& data)
  {
    return out << "(" << data.first << "," << data.second << ")";
  }
}