/*
 *  webrtc-echo - A WebRTC echo server
 *  Copyright (C) 2014  Stephan Thamm
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HELPER_H
#define HELPER_H 

#define STRINGIFY(x)	#x
#define TOSTRING(x)	STRINGIFY(x)

#define AT()		__FILE__ ":" TOSTRING(__LINE__)

#ifdef DO_DEBUG
#include <iostream>
#include <mutex>
extern std::mutex log_mutex;
#define DEBUG(x) do { std::lock_guard<std::mutex> guard(log_mutex); std::cerr << "[dtls] " << x << " (@" << AT() << ")" << std::endl; } while (0)
#else
#define DEBUG(x)
#endif

#endif /* HELPER_H */
