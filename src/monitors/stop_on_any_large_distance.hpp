/*************************************************************************
 * Copyright (C) 2011 by Eric Ford and the Swarm-NG Development Team  *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 3 of the License.        *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ************************************************************************/
#pragma once

#include <limits>

namespace swarm {


struct stop_on_any_large_distance_params {
	double rmax;
	bool stop;
	stop_on_any_large_distance_params(const config &cfg)
	{
		rmax = cfg.optional("rmax",std::numeric_limits<float>::max());
		stop = cfg.optional("stop on rmax",false);
	}
};

/** Simple monitor that logs when any one body is separated from 
 *  both the origin and every other body by a distance of at least "rmax" 
 *  Optionally signals if "stop on rmax" is true
 *  This monitor may be useful in simple scattering experiments.
 *  WARNING: The code so that there is run time option of signaling to stop is untested.  
 *  TODO: Once it works, may want to port to other monitors.
 *  \ingroup monitors
 */
template<class log_t>
class stop_on_any_large_distance {
	public:
	typedef stop_on_any_large_distance_params params;

	private:
	params _params;

	ensemble::SystemRef& _sys;
	log_t& _log;
	int _counter;

	public:

	GPUAPI bool operator () () { 
	//	if(_counter % 1000 == 0)
	//		lprintf(_log,"Hello %g\n", _sys.time() );

		_counter++;

		bool is_any_body_far_from_origin = false;
		for(int b = 0 ; b < _sys.nbod(); b ++ ){
			if(_sys.radius(b) > _params.rmax )
				is_any_body_far_from_origin = true;
		}
		if(!is_any_body_far_from_origin) return false;
					
		for(int b = 0 ; b < _sys.nbod(); b ++ ){
			if(_sys.radius(b) <= _params.rmax ) continue;
			bool is_far_from_every_body = true;
			for(int bb = 0 ; bb < _sys.nbod(); bb ++ ){		
			   if(b == bb) continue;
			   if(_sys.distance_squared_between(b,bb) < _params.rmax*_params.rmax )
					{ is_far_from_every_body = false;  break; }
				}
			if(is_far_from_every_body) 
				{
				lprintf(_log, "Distance from all bodies exceeds rmax: _sys=%d, bod=%d, T=%lg r=%lg rmax=%lg.\n"
				, _sys.number(), b, _sys.time() , r, _params.rmax_squared);
				log::system(_log, _sys);
				if(stop) return true;
				}
		}
		return false; 
	}

	GPUAPI stop_on_any_large_distance(const params& p,ensemble::SystemRef& s,log_t& l)
		:_params(p),_sys(s),_log(l),_counter(0){}
	
};

}