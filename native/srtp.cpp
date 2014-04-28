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

#include "srtp.h"

#include <node_buffer.h>

#include "helper.h"

using namespace v8;

v8::Persistent<v8::Function> Srtp::constructor;
bool Srtp::initialized = false;

static void createSession(srtp_t *session, const char *key, ssrc_type_t direction) {
	srtp_policy_t policy;

	memset(&policy, 0, sizeof(policy));

	crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
	crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

	policy.ssrc.type = direction;
	policy.ssrc.value = 0;

	policy.key = (unsigned char*) key;

	policy.next = NULL;

	int res = srtp_create(session, &policy);

	DEBUG("creating session " << res);
}

Srtp::Srtp(const char *sendKey, const char *recvKey) {
	if(!initialized) {
		DEBUG("initializing srtp");
		srtp_init();
		initialized = true;
	}

	createSession(&_sendSession, sendKey, ssrc_any_outbound);
	createSession(&_recvSession, recvKey, ssrc_any_inbound);
}

Srtp::~Srtp() {
	srtp_dealloc(_sendSession);
	srtp_dealloc(_recvSession);
}

void Srtp::init(v8::Handle<v8::Object> exports) {
	// Prepare constructor template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("Srtp"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// protoype
	NODE_SET_PROTOTYPE_METHOD(tpl, "protectRtp", protectRtp);
	NODE_SET_PROTOTYPE_METHOD(tpl, "unprotectRtp", unprotectRtp);
	NODE_SET_PROTOTYPE_METHOD(tpl, "protectRtcp", protectRtcp);
	NODE_SET_PROTOTYPE_METHOD(tpl, "unprotectRtcp", unprotectRtcp);
	constructor = Persistent<Function>::New(tpl->GetFunction());
	// export
	exports->Set(String::NewSymbol("Srtp"), constructor);
}

v8::Handle<v8::Value> Srtp::New(const v8::Arguments& args) {
	HandleScope scope;

	if (args.IsConstructCall()) {
		// Invoked as constructor: `new MyObject(...)`
		if(!node::Buffer::HasInstance(args[0]) || !node::Buffer::HasInstance(args[1])) {
			return ThrowException(Exception::TypeError(String::New("Expected buffers")));
		}

		const char *sendKey = node::Buffer::Data(args[0]);
		const char *recvKey = node::Buffer::Data(args[1]);

		Srtp* obj = new Srtp(sendKey, recvKey);
		obj->Wrap(args.This());

		return args.This();
	} else {
		// Invoked as plain function `MyObject(...)`, turn into construct call.
		const int argc = 1;
		Local<Value> argv[1] = { argv[0] };
		return scope.Close(constructor->NewInstance(argc, argv));
	}
}

v8::Handle<v8::Value> Srtp::convert(const v8::Arguments& args, srtp_t session, convert_fun fun) {
	HandleScope scope;

	// type checking

	if(!node::Buffer::HasInstance(args[0])) {
		return ThrowException(Exception::TypeError(String::New("Expected buffer")));
	}

	// copy buffer for result

	int size = node::Buffer::Length(args[0]);
	char *in_buf = node::Buffer::Data(args[0]);

	Handle<Object> tmp = node::Buffer::New(size + 32)->handle_;
	char *out_buf = node::Buffer::Data(tmp);

	memcpy(out_buf, in_buf, size);

	// actual crypt stuff

	err_status_t err = fun(session, out_buf, &size);

	if(err != err_status_ok) {
		DEBUG("srtp error " << err);
	}

	// return slice of the right size

	Handle<Value> slice_v = tmp->Get(String::New("slice")); 

	if(!slice_v->IsFunction()) {
		return ThrowException(Exception::Error(String::New("Buffer does not have a slice function")));
	}

	Handle<v8::Function> slice_f = v8::Handle<v8::Function>::Cast(slice_v);

	const int argc = 2;
	Handle<Value> argv[argc] = {
		Integer::New(0),
		Integer::New(size),
	};

	Handle<Value> res = slice_f->Call(tmp, argc, argv);

	return scope.Close(res);
}

v8::Handle<v8::Value> Srtp::protectRtp(const v8::Arguments& args) {
	Srtp *srtp = node::ObjectWrap::Unwrap<Srtp>(args.This()->ToObject());

	return convert(args, srtp->_sendSession, srtp_protect);
}

v8::Handle<v8::Value> Srtp::unprotectRtp(const v8::Arguments& args) {
	Srtp *srtp = node::ObjectWrap::Unwrap<Srtp>(args.This()->ToObject());

	return convert(args, srtp->_recvSession, srtp_unprotect);
}

v8::Handle<v8::Value> Srtp::protectRtcp(const v8::Arguments& args) {
	Srtp *srtp = node::ObjectWrap::Unwrap<Srtp>(args.This()->ToObject());

	return convert(args, srtp->_sendSession, srtp_protect_rtcp);
}

v8::Handle<v8::Value> Srtp::unprotectRtcp(const v8::Arguments& args) {
	Srtp *srtp = node::ObjectWrap::Unwrap<Srtp>(args.This()->ToObject());

	return convert(args, srtp->_recvSession, srtp_unprotect_rtcp);
}

