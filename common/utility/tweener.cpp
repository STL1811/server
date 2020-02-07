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


// The following code is based on Tweener for actionscript, http://code.google.com/p/tweener/
//
//Disclaimer for Robert Penner's Easing Equations license:
//
//TERMS OF USE - EASING EQUATIONS
//
//Open source under the BSD License.
//
//Copyright © 2001 Robert Penner
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
//    * Neither the name of the author nor the names of contributors may be used to endorse or promote products derived from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include "../stdafx.h"

#include "tweener.h"

#include <boost/assign/list_of.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <unordered_map>
#include <string>
#include <locale>
#include <functional>
#include <vector>

namespace caspar {

typedef std::function<double(double, double, double, double,int )> tweener_t;
			
static const double PI = std::atan(1.0)*4.0;
static const double H_PI = std::atan(1.0)*2.0;


// t : l'instant courant, d: la durée totale de la transition, c: position courante , b: la valeur de départ
//http://www.gizma.com/easing/
double ease_none (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*t/d + b;
}

double ease_in_quad (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*(t/=d)*t + b;
}
	
double ease_out_quad (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return -c *(t/=d)*(t-2) + b;
}	

double ease_in_out_quad (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	if ((t/=d/2) < 1) 
		return c/2*t*t + b;

	return -c/2 * ((--t)*(t-2) - 1) + b;
}	

double ease_out_in_quad (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	if (t < d/2) 
		return ease_out_quad (t*2, b, c/2, d,axis,  params);

	return ease_in_quad((t*2)-d, b+c/2, c/2, d,axis,  params);
}
	
double ease_in_cubic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*(t/=d)*t*t + b;
}	

double ease_out_cubic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*((t=t/d-1)*t*t + 1) + b;
}
	
double ease_in_out_cubic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if ((t/=d/2) < 1) 
		return c/2*t*t*t + b;

	return c/2*((t-=2)*t*t + 2) + b;
}
	
double ease_out_in_cubic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) return ease_out_cubic (t*2, b, c/2, d, axis, params);
	return ease_in_cubic((t*2)-d, b+c/2, c/2, d,axis,  params);
}
	
double ease_in_quart (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*(t/=d)*t*t*t + b;
}
	
double ease_out_quart (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return -c * ((t=t/d-1)*t*t*t - 1) + b;
}	

double ease_in_out_quart (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if ((t/=d/2) < 1)
		return c/2*t*t*t*t + b;

	return -c/2 * ((t-=2)*t*t*t - 2) + b;
}	

double ease_out_in_quart (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2)
		return ease_out_quart (t*2, b, c/2, d,axis,  params);

	return ease_in_quart((t*2)-d, b+c/2, c/2, d, axis, params);
}	

double ease_in_quint (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*(t/=d)*t*t*t*t + b;
}
	
double ease_out_quint (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c*((t=t/d-1)*t*t*t*t + 1) + b;
}
	
double ease_in_out_quint (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if ((t/=d/2) < 1) 
		return c/2*t*t*t*t*t + b;

	return c/2*((t-=2)*t*t*t*t + 2) + b;
}
	
double ease_out_in_quint (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) 
		return ease_out_quint (t*2, b, c/2, d,axis,  params);

	return ease_in_quint((t*2)-d, b+c/2, c/2, d,axis,  params);
}	

double ease_in_sine (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return -c * std::cos(t/d * (PI/2)) + c + b;
}	

double ease_out_sine (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c * std::sin(t/d * (PI/2)) + b;
}	

double ease_in_out_sine (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return -c/2 * (std::cos(PI*t/d) - 1) + b;
}	

double ease_out_in_sine (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	if (t < d/2) 
		return ease_out_sine (t*2, b, c/2, d,axis,  params);
	
	return ease_in_sine((t*2)-d, b+c/2, c/2, d,axis,  params);
}	

double ease_in_expo (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return (t==0) ? b : c * std::pow(2, 10 * (t/d - 1)) + b - c * 0.001;
}	

double ease_out_expo (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return (t==d) ? b+c : c * 1.001 * (-std::pow(2, -10 * t/d) + 1) + b;
}
	
double ease_in_out_expo (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t==0) 
		return b;
	if (t==d) 
		return b+c;
	if ((t/=d/2) < 1) 
		return c/2 * std::pow(2, 10 * (t - 1)) + b - c * 0.0005;

	return c/2 * 1.0005 * (-std::pow(2, -10 * --t) + 2) + b;
}
	
double ease_out_in_expo (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) 
		return ease_out_expo (t*2, b, c/2, d,axis,  params);

	return ease_in_expo((t*2)-d, b+c/2, c/2, d,axis,  params);
}
	
double ease_in_circ (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return -c * (std::sqrt(1 - (t/=d)*t) - 1) + b;
}
	
double ease_out_circ (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	return c * std::sqrt(1 - (t=t/d-1)*t) + b;
}
	
double ease_in_out_circ (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if ((t/=d/2) < 1) 
		return -c/2 * (std::sqrt(1 - t*t) - 1) + b;

	return c/2 * (std::sqrt(1 - (t-=2)*t) + 1) + b;
}
	
double ease_out_in_circ (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) return ease_out_circ(t*2, b, c/2, d,axis,  params);
	return ease_in_circ((t*2)-d, b+c/2, c/2, d,axis,  params);
}
	
double ease_in_elastic (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	if (t==0) return b;
	if ((t/=d)==1) return b+c;
	//var p:Number = !Boolean(p_params) || isNaN(p_params.period) ? d*.3 : p_params.period;
	//var s:Number;
	//var a:Number = !Boolean(p_params) || isNaN(p_params.amplitude) ? 0 : p_params.amplitude;
	double p = params.size() > 0 ? params[0] : d*0.3;
	double s;
	double a = params.size() > 1 ? params[1] : 0.0;
	if (a == 0.0 || a < std::abs(c)) 
	{
		a = c;
		s = p/4;
	} 
	else 
		s = p/(2*PI) * std::asin (c/a);
	
	return -(a*std::pow(2,10*(t-=1)) * std::sin( (t*d-s)*(2*PI)/p )) + b;
}
	
double ease_out_elastic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t==0) 
		return b;
	if ((t/=d)==1) 
		return b+c;
	//var p:Number = !Boolean(p_params) || isNaN(p_params.period) ? d*.3 : p_params.period;
	//var s:Number;
	//var a:Number = !Boolean(p_params) || isNaN(p_params.amplitude) ? 0 : p_params.amplitude;
	double p = params.size() > 0 ? params[0] : d*0.3;
	double s;
	double a = params.size() > 1 ? params[1] : 0.0;
	if (a == 0.0 || a < std::abs(c))
	{
		a = c;
		s = p/4;
	} 
	else 
		s = p/(2*PI) * std::asin (c/a);
	
	return (a*std::pow(2,-10*t) * std::sin( (t*d-s)*(2*PI)/p ) + c + b);
}	

double ease_in_out_elastic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t==0)
		return b;
	if ((t/=d/2)==2) 
		return b+c;
	//var p:Number = !Boolean(p_params) || isNaN(p_params.period) ? d*(.3*1.5) : p_params.period;
	//var s:Number;
	//var a:Number = !Boolean(p_params) || isNaN(p_params.amplitude) ? 0 : p_params.amplitude;
	double p = params.size() > 0 ? params[0] : d*0.3*1.5;
	double s;
	double a = params.size() > 1 ? params[1] : 0.0;
	if (a == 0.0 || a < std::abs(c)) 
	{
		a = c;
		s = p/4;
	}
	else
		s = p/(2*PI) * std::asin (c/a);
	
	if (t < 1) 
		return -.5*(a*std::pow(2,10*(t-=1)) * std::sin( (t*d-s)*(2*PI)/p )) + b;

	return a*std::pow(2,-10*(t-=1)) * std::sin( (t*d-s)*(2*PI)/p )*.5 + c + b;
}
	
double ease_out_in_elastic (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) return ease_out_elastic (t*2, b, c/2, d,axis,  params);
	return ease_in_elastic((t*2)-d, b+c/2, c/2, d, axis, params);
}
	
double ease_in_back (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	//var s:Number = !Boolean(p_params) || isNaN(p_params.overshoot) ? 1.70158 : p_params.overshoot;
	double s = params.size() > 0 ? params[0] : 1.70158;
	return c*(t/=d)*t*((s+1)*t - s) + b;
}
	
double ease_out_back (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	//var s:Number = !Boolean(p_params) || isNaN(p_params.overshoot) ? 1.70158 : p_params.overshoot;
	double s = params.size() > 0 ? params[0] : 1.70158;
	return c*((t=t/d-1)*t*((s+1)*t + s) + 1) + b;
}
	
double ease_in_out_back (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	//var s:Number = !Boolean(p_params) || isNaN(p_params.overshoot) ? 1.70158 : p_params.overshoot;
	double s = params.size() > 0 ? params[0] : 1.70158;
	if ((t/=d/2) < 1) return c/2*(t*t*(((s*=(1.525))+1)*t - s)) + b;
	return c/2*((t-=2)*t*(((s*=(1.525))+1)*t + s) + 2) + b;
}
	
double ease_out_int_back (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	if (t < d/2) return ease_out_back (t*2, b, c/2, d, axis, params);
	return ease_in_back((t*2)-d, b+c/2, c/2, d, axis, params);
}
	
double ease_out_bounce (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if ((t/=d) < (1/2.75))
		return c*(7.5625*t*t) + b;
	else if (t < (2/2.75))
		return c*(7.5625*(t-=(1.5/2.75))*t + .75) + b;
	else if (t < (2.5/2.75))
		return c*(7.5625*(t-=(2.25/2.75))*t + .9375) + b;
	else 
		return c*(7.5625*(t-=(2.625/2.75))*t + .984375) + b;	
}
	
double ease_in_bounce (double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	return c - ease_out_bounce (d-t, 0, c, d, axis, params) + b;
}

double ease_in_out_bounce (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) return ease_in_bounce (t*2, 0, c, d, axis, params) * .5 + b;
	else return ease_out_bounce (t*2-d, 0, c, d, axis, params) * .5 + c*.5 + b;
}
	

double ease_out_in_bounce (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/2) return ease_out_bounce (t*2, b, c/2, d, axis, params);
	return ease_in_bounce((t*2)-d, b+c/2, c/2, d,axis,  params);
}

double ease_lora (double t, double b, double c, double d,int axis, const std::vector<double>& params) 
{
	if (t < d/3) return c*t/d + b;
	else if (t < 2*d/3) return c/3 + b;
	else  return c*t/d + b;
}
// t : l'instant courant, d: la durée totale de la transition, c: position courante , b: la valeur de départ
double ease_delayed(double t, double b, double c, double d, int axis, const std::vector<double>& params)
{
	if (t >= d - 2)
	{
		return c * t / d + b;
	}
	else  return  b;
}
double ease_bezier(double t, double b, double c, double d,int axis, const std::vector<double>& params)
{
	if(axis == 0) //x
	{
		double x0 = params.size() > 0 ? params[0] : 0.0;
		//double y0 = params.size() > 1 ? params[1] : 0.0;
		double x1 = params.size() > 2 ? params[2] : 0.0;
		//double y1 = params.size() > 3 ? params[3] : 0.0;
		double x2 = params.size() > 4 ? params[4] : 0.0;
		//double y2 = params.size() > 5 ? params[5] : 0.0;
		double x3 = params.size() > 6 ? params[6] : 0.0;
		//double y3 = params.size() > 7 ? params[7] : 0.0;
		if (params.size() > 7)
		{
			return bezier_cube(t, b, c, d, x0, x1, x2, x3);
		}
		else if (params.size() > 5)
		{
			return bezier_quadra(t, b, c, d, x0, x1, x2);
		}
		else if (params.size() > 3)
		{
			return bezier_linear(t, b, c, d, x0, x1);
		}
	}
	else if (axis == 1)
	{
		//double x0 = params.size() > 0 ? params[0] : 0.0;
		double y0 = params.size() > 1 ? params[1] : 0.0;
		//double x1 = params.size() > 2 ? params[2] : 0.0;
		double y1 = params.size() > 3 ? params[3] : 0.0;
		//double x2 = params.size() > 4 ? params[4] : 0.0;
		double y2 = params.size() > 5 ? params[5] : 0.0;
		//double x3 = params.size() > 6 ? params[6] : 0.0;
		double y3 = params.size() > 7 ? params[7] : 0.0;
		
		if (params.size() > 7)
		{
			return bezier_cube(t, b, c, d, y0, y1, y2, y3);
		}
		else if (params.size() > 5)
		{
			return bezier_quadra(t, b, c, d, y0, y1,y2 );
		}
		else if (params.size() > 3)
		{
			return bezier_linear(t, b, c, d, y0, y1);
		}	

	}
	return ease_none(t, b, c, d, axis, params);
}
double bezier_linear(double t, double b, double c, double d, double a0,double a1)
{

	return  (1 - (t / d)) * a0 + (t / d)*a1;
	//return  (1 - (t / d)) * y0 + (t / d)*y1;
}
double bezier_quadra (double t, double b, double c, double d, double a0, double a1, double a2)
{
	double t1 = t / d;
	return  (1 - t1)*(1 - t1) * a0 + 2*t1*(1-t1)*a1 + t1*t1*a2;
	//return  (1 - t1)*(1 - t1) * y0 + 2 * t1*(1 - t1)*y1 + t1 * t1*y2;
}

double bezier_cube(double t, double b, double c, double d,double a0, double a1, double a2,  double a3)
{
	double t1 = t / d;
	return  (1 - t1)*(1 - t1) *(1 - t1) * a0 + 3*t1*(1-t1)*(1-t1)*a1 + 3*t1*t1*(1-t1)*a2 + t1*t1*t1*a3;
}
tweener_t get_tweener(std::wstring name)
{
	std::transform(name.begin(), name.end(), name.begin(), std::tolower);

	if(name == L"linear")
		return [](double t, double b, double c, double d, int axis){return ease_none(t, b, c, d,axis,  std::vector<double>());};
	
	std::vector<double> params;
	// il faut écrire : easeinelastic:0.1:0.2 et partir d'un coin
	static const boost::wregex expr(L"(?<NAME>\\w*)(:(?<V0>\\d+\\.?\\d?))?(:(?<V1>\\d+\\.?\\d?))?(:(?<V2>\\d+\\.?\\d?))?(:(?<V3>\\d+\\.?\\d?))?(:(?<V4>\\d+\\.?\\d?))?(:(?<V5>\\d+\\.?\\d?))?(:(?<V6>\\d+\\.?\\d?))?(:(?<V7>\\d+\\.?\\d?))?"); // boost::regex has no repeated captures?
	boost::wsmatch what;
	if(boost::regex_match(name, what, expr))
	{
		name = what["NAME"].str();

		/*params.push_back(0.0);
		params.push_back(0.0);
		params.push_back(1.0);
		params.push_back(1.0);
		params.push_back(1.0);
		params.push_back(0.0);*/
		if(what["V0"].matched)
			params.push_back(boost::lexical_cast<double>(what["V0"].str()));
		if(what["V1"].matched)
			params.push_back(boost::lexical_cast<double>(what["V1"].str()));
		if (what["V2"].matched)
			params.push_back(boost::lexical_cast<double>(what["V2"].str()));
		if (what["V3"].matched)
			params.push_back(boost::lexical_cast<double>(what["V3"].str()));
		if (what["V4"].matched)
			params.push_back(boost::lexical_cast<double>(what["V4"].str()));
		if (what["V5"].matched)
			params.push_back(boost::lexical_cast<double>(what["V5"].str()));
		if (what["V6"].matched)
			params.push_back(boost::lexical_cast<double>(what["V6"].str()));
		if (what["V7"].matched)
			params.push_back(boost::lexical_cast<double>(what["V7"].str()));
			
	}
		
	typedef std::function<double(double, double, double, double,int,  const std::vector<double>&)> tween_t;	
	static const std::unordered_map<std::wstring, tween_t> tweens = boost::assign::map_list_of	
		(L"",					ease_none		   )	
		(L"linear",				ease_none		   )	
		(L"easenone",			ease_none		   )
		(L"easeinquad",			ease_in_quad	   )
		(L"easeoutquad",		ease_out_quad	   )
		(L"easeinoutquad",		ease_in_out_quad   )
		(L"easeoutinquad",		ease_out_in_quad   )
		(L"easeincubic",		ease_in_cubic	   )
		(L"easeoutcubic",		ease_out_cubic	   )
		(L"easeinoutcubic",		ease_in_out_cubic  )
		(L"easeoutincubic",		ease_out_in_cubic  )
		(L"easeinquart", 		ease_in_quart 	   )
		(L"easeoutquart",		ease_out_quart	   )
		(L"easeinoutquart",		ease_in_out_quart  )
		(L"easeoutinquart",		ease_out_in_quart  )
		(L"easeinquint",		ease_in_quint	   )
		(L"easeoutquint",		ease_out_quint	   )
		(L"easeinoutquint",		ease_in_out_quint  )
		(L"easeoutinquint",		ease_out_in_quint  )
		(L"easeinsine",			ease_in_sine	   )
		(L"easeoutsine",		ease_out_sine	   )
		(L"easeinoutsine",		ease_in_out_sine   )
		(L"easeoutinsine",		ease_out_in_sine   )
		(L"easeinexpo",			ease_in_expo	   )
		(L"easeoutexpo",		ease_out_expo	   )
		(L"easeinoutexpo",		ease_in_out_expo   )
		(L"easeoutinexpo",		ease_out_in_expo   )
		(L"easeincirc",			ease_in_circ	   )
		(L"easeoutcirc",		ease_out_circ	   )
		(L"easeinoutcirc",		ease_in_out_circ   )
		(L"easeoutincirc",		ease_out_in_circ   )
		(L"easeinelastic",		ease_in_elastic	   )
		(L"easeoutelastic",		ease_out_elastic   )
		(L"easeinoutelastic",	ease_in_out_elastic)
		(L"easeoutinelastic",	ease_out_in_elastic)
		(L"easeinback",			ease_in_back	   )
		(L"easeoutback",		ease_out_back	   )
		(L"easeinoutback",		ease_in_out_back   )
		(L"easeoutintback",		ease_out_int_back  )
		(L"easeoutbounce",		ease_out_bounce	   )
		(L"easeinbounce",		ease_in_bounce	   )
		(L"easeinoutbounce",	ease_in_out_bounce )
		(L"easeoutinbounce",	ease_out_in_bounce )
		(L"easebezier", ease_bezier)
		(L"bezier", ease_bezier)
		(L"easelora", ease_lora)
		(L"cutdelayed", ease_delayed);

	auto it = tweens.find(name);
	if(it == tweens.end())
		it = tweens.find(L"linear");
	
	return [=](double t, double b, double c, double d, int axis)
	{
		return it->second(t, b, c, d, axis,params);
	};
};

}