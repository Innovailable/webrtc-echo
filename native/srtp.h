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

#ifndef _SRTP_H
#define _SRTP_H 

#include <node.h>
#include <v8.h>

#include <srtp/srtp.h>

typedef err_status_t (*convert_fun)(srtp_t, void* buf, int* len);

class Srtp : public node::ObjectWrap {
	public:
		Srtp(const char *sendKey, const char *recvKey);
		~Srtp();

		static void init(v8::Handle<v8::Object> exports);

	private:
		static v8::Persistent<v8::Function> constructor;

		// js functions

		static v8::Handle<v8::Value> New(const v8::Arguments& args);
		static v8::Handle<v8::Value> protectRtp(const v8::Arguments& args);
		static v8::Handle<v8::Value> unprotectRtp(const v8::Arguments& args);
		static v8::Handle<v8::Value> protectRtcp(const v8::Arguments& args);
		static v8::Handle<v8::Value> unprotectRtcp(const v8::Arguments& args);

		// helper

		static v8::Handle<v8::Value> convert(const v8::Arguments& args, srtp_t session, convert_fun fun);

		// state

		srtp_t _sendSession;
		srtp_t _recvSession;

		static bool initialized;
};

#endif /* _SRTP_H */
