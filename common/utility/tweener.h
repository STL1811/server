/*
* Copyright 2013 Sveriges Television AB http://casparcg.com/
*
* This file is part of CasparCG (www.casparcg.com).
*
* CasparCG is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CasparCG is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
*
* Author: Robert Nagy, ronag89@gmail.com
*/

#pragma once

#include <functional>

namespace caspar {

typedef std::function<double(double, double, double, double, int)> tweener_t;
tweener_t get_tweener(std::wstring name = L"linear");
double bezier_linear(double t, double b, double c, double d, double x0, double x1);
double bezier_quadra(double t, double b, double c, double d, double x0, double x1, double x2);
double bezier_cube(double t, double b, double c, double d, double x0, double x1, double x2, double x3);

}