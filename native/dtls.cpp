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

#include "dtls.h"

#include <sstream>
#include <iomanip>

#include <node_buffer.h>

#include <openssl/x509.h>
#include <openssl/evp.h>

#include "helper.h"

const int SRTP_KEY_LEN = 16;
const int SRTP_SALT_LEN = 14;

using namespace v8;

v8::Persistent<v8::Function> Dtls::constructor;

const BIO_METHOD Dtls::bioMethod = {
	BIO_TYPE_DGRAM,
	"node.js SSL buffer",
	Dtls::bioWrite,
	Dtls::bioRead,
	NULL,
	NULL,
	Dtls::bioCtrl,
	Dtls::bioNew,
	Dtls::bioFree,
	NULL
};

// instantiation

Dtls::Dtls(const char *cert_file, const char *key_file) : _buf(2048), _offset(0), _size(0), _connected(false), _closed(false) {
	OpenSSL_add_ssl_algorithms();
	SSL_load_error_strings();

	_ctx = SSL_CTX_new(DTLSv1_client_method());
	SSL_CTX_set_cipher_list(_ctx, "HIGH:!DSS:!aNULL@STRENGTH");

	if (!SSL_CTX_use_certificate_file(_ctx, cert_file, SSL_FILETYPE_PEM)) {
		DEBUG("no certificate found!");
	}

	if (!SSL_CTX_use_PrivateKey_file(_ctx, key_file, SSL_FILETYPE_PEM)) {
		DEBUG("no private key found!");
	}

	if (!SSL_CTX_check_private_key (_ctx)) {
		DEBUG("invalid private key!");
	}

	SSL_CTX_set_read_ahead(_ctx, 1);

	_ssl = SSL_new(_ctx);

	_bio = BIO_new(const_cast<BIO_METHOD *>(&bioMethod));
	_bio->ptr = this;

	SSL_set_tlsext_use_srtp(_ssl, "SRTP_AES128_CM_SHA1_80");

	SSL_set_bio(_ssl, _bio, _bio);
}

Dtls::~Dtls() {
	DEBUG("dtls destroyed");

	//BIO_free(_bio);
	SSL_free(_ssl);
	SSL_CTX_free(_ctx);
}

void Dtls::init(v8::Handle<v8::Object> exports) {
	// Prepare constructor template
	Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
	tpl->SetClassName(String::NewSymbol("Dtls"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// protoype
	NODE_SET_PROTOTYPE_METHOD(tpl, "encrypt", encrypt);
	NODE_SET_PROTOTYPE_METHOD(tpl, "decrypt", decrypt);
	NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
	NODE_SET_PROTOTYPE_METHOD(tpl, "connect", tick);
	NODE_SET_PROTOTYPE_METHOD(tpl, "tick", tick);
	NODE_SET_PROTOTYPE_METHOD(tpl, "fingerprint", fingerprint);
	NODE_SET_PROTOTYPE_METHOD(tpl, "srtpKeys", srtpKeys);
	constructor = Persistent<Function>::New(tpl->GetFunction());
	// export
	exports->Set(String::NewSymbol("Dtls"), constructor);
}

v8::Handle<v8::Value> Dtls::New(const v8::Arguments& args) {
	HandleScope scope;

	if (args.IsConstructCall()) {
		// Invoked as constructor: `new MyObject(...)`
		String::Utf8Value cert_file(args[0]->ToString());
		String::Utf8Value key_file(args[1]->ToString());

		Dtls* obj = new Dtls(*cert_file, *key_file);
		obj->Wrap(args.This());

		return args.This();
	} else {
		// Invoked as plain function `MyObject(...)`, turn into construct call.
		const int argc = 1;
		Local<Value> argv[1] = { argv[0] };
		return scope.Close(constructor->NewInstance(argc, argv));
	}
}

// do stuff

void Dtls::tick() {
	char buf[2048];

	if(_closed) {
		DEBUG("trying to tick when closed");
		return;
	}

	if(!_connected) {
		int res = SSL_connect(_ssl);

		if(res == 0) {
			_closed = true;
			return;
		} else if(res < 0) {
			switch(SSL_get_error(_ssl, res)) {
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE:
					DEBUG("waiting for connect");
					return;
				case SSL_ERROR_SSL:
					//DEBUG(ERR_error_string(NULL));
					_closed = true;
					return;
				default:
					_closed = true;
					return;
			}
		} else {
			HandleScope scope;

			_connected = true;
			DEBUG("connected");

			const int argc = 1;
			Handle<Value> argv[argc] = {
				String::New("connected"),
			};

			node::MakeCallback(handle_, "emit", argc, argv);
		}
	}

	while(true) {
		int res = SSL_read(_ssl, buf, sizeof(buf));

		if(res > 0) {
			DEBUG("read " << res << " bytes");

			HandleScope scope;

			const int argc = 2;
			Handle<Value> argv[argc] = {
				String::New("decrypted"),
				node::Buffer::New(buf, res)->handle_,
			};

			node::MakeCallback(handle_, "emit", argc, argv);
		} else {
			switch (SSL_get_error(_ssl, res)) {
				case SSL_ERROR_WANT_READ:
					//DEBUG("wanting to read");
					break;
				case SSL_ERROR_ZERO_RETURN:
					DEBUG("zero return");
					break;
				case SSL_ERROR_SSL:
					DEBUG("SSL read error: ");
					break;
				default:
					DEBUG("unexpected");
					break;
			}
			
			break;
		}
	}
}

v8::Handle<v8::Value> Dtls::decrypt(const v8::Arguments& args) {
	HandleScope scope;

	Dtls *dtls = node::ObjectWrap::Unwrap<Dtls>(args.This()->ToObject());

	// remove clean space at start

	if(dtls->_offset) {
		dtls->_size -= dtls->_offset;
		memmove(dtls->_buf.data(), dtls->_buf.data(), dtls->_size);
		dtls->_offset = 0;
	}

	// get buffer

	if(!node::Buffer::HasInstance(args[0])) {
		return ThrowException(Exception::TypeError(String::New("Expected buffer")));
	}

	Local<Object> buffer = args[0]->ToObject();

	char* buf = node::Buffer::Data(buffer);
	size_t size = node::Buffer::Length(buffer);

	// append buffer

	int new_size = dtls->_size + size;

	if(new_size > dtls->_buf.size()) {
		dtls->_buf.resize(new_size);
	}

	memcpy(dtls->_buf.data() + dtls->_size, buf, size);
	dtls->_size = new_size;

	// try to get some decrypted data out

	dtls->tick();

	return scope.Close(Undefined());
}
v8::Handle<v8::Value> Dtls::encrypt(const v8::Arguments& args) {
	HandleScope scope;

	Dtls *dtls = node::ObjectWrap::Unwrap<Dtls>(args.This()->ToObject());

	if(!node::Buffer::HasInstance(args[0])) {
		return ThrowException(Exception::TypeError(String::New("Expected buffer")));
	}

	Local<Object> buffer = args[0]->ToObject();

	char* buf = node::Buffer::Data(buffer);
	size_t size = node::Buffer::Length(buffer);

	int res = SSL_write(dtls->_ssl, buf, size);

	if(res != size) {
		return ThrowException(Exception::TypeError(String::New("Unable to write")));
	}

	return scope.Close(Undefined());
}

v8::Handle<v8::Value> Dtls::close(const v8::Arguments& args) {
	HandleScope scope;

	Dtls *dtls = node::ObjectWrap::Unwrap<Dtls>(args.This()->ToObject());

	dtls->_closed = true;
	SSL_shutdown(dtls->_ssl);

	return scope.Close(Undefined());
}

v8::Handle<v8::Value> Dtls::tick(const v8::Arguments& args) {
	HandleScope scope;

	Dtls *dtls = node::ObjectWrap::Unwrap<Dtls>(args.This()->ToObject());

	dtls->tick();

	return scope.Close(Undefined());
}

v8::Handle<v8::Value> Dtls::fingerprint(const v8::Arguments& args) {
	HandleScope scope;

	Dtls *dtls = node::ObjectWrap::Unwrap<Dtls>(args.This()->ToObject());
	unsigned char buf[EVP_MAX_MD_SIZE];
	unsigned int size;

	X509* x = SSL_get_certificate(dtls->_ssl);

	const EVP_MD *digest = EVP_get_digestbyname("sha256");
	X509_digest(x, digest, buf, &size);

	std::ostringstream ss;

	ss << "sha-256 ";

	for(size_t i = 0; i < size; ++i) {
		if(i) {
			ss << ":";
		}

		ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int) buf[i];
	}

	return scope.Close(String::New(ss.str().c_str()));
}

v8::Handle<v8::Value> Dtls::srtpKeys(const v8::Arguments& args) {
	HandleScope scope;

	Dtls *dtls = node::ObjectWrap::Unwrap<Dtls>(args.This()->ToObject());

	char material[(SRTP_KEY_LEN + SRTP_SALT_LEN) * 2];

	if(!SSL_export_keying_material(dtls->_ssl, (unsigned char *) material, sizeof(material), "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0)) {
		return scope.Close(Undefined());
	}

	Local<Object> res = Object::New();
	Local<Object> client = Object::New();
	res->Set(String::New("client"), client);
	Local<Object> server = Object::New();
	res->Set(String::New("server"), server);

	size_t offset = 0;

	client->Set(String::New("key"), node::Buffer::New(material + offset, SRTP_KEY_LEN)->handle_);
	offset += SRTP_KEY_LEN;

	server->Set(String::New("key"), node::Buffer::New(material + offset, SRTP_KEY_LEN)->handle_);
	offset += SRTP_KEY_LEN;

	client->Set(String::New("salt"), node::Buffer::New(material + offset, SRTP_SALT_LEN)->handle_);
	offset += SRTP_SALT_LEN;

	server->Set(String::New("salt"), node::Buffer::New(material + offset, SRTP_SALT_LEN)->handle_);
	offset += SRTP_SALT_LEN;

	return scope.Close(res);
}

// bio stuff

int Dtls::bioNew(BIO* bio) {
	bio->shutdown = 1;
	bio->init = 1;
	bio->num = -1;

	return 1;
}

int Dtls::bioFree(BIO* bio) {
	if(bio == NULL) {
		return 0;
	}

	return 1;
}

int Dtls::bioRead(BIO* bio, char* out, int len) {
	Dtls *obj = (Dtls *) bio->ptr;

	if(obj->_offset >= obj->_size) {
		BIO_set_retry_read(bio);
		return -1;
	}

	int available = std::min(obj->_size - obj->_offset, len);

	DEBUG("bio reads " << available << " bytes");

	memcpy(out, obj->_buf.data() + obj->_offset, available);

	obj->_offset += available;

	return available;
}

int Dtls::bioWrite(BIO* bio, const char* data, int len) {
	HandleScope scope;

	Dtls *obj = (Dtls *) bio->ptr;

	DEBUG("bio writes " << len << " bytes");

	const int argc = 2;
	Handle<Value> argv[argc] = {
		String::New("encrypted"),
		node::Buffer::New(data, len)->handle_,
	};

	node::MakeCallback(obj->handle_, "emit", argc, argv);

	return len;
}

long Dtls::bioCtrl(BIO* bio, int cmd, long num, void* ptr) {
	Dtls *obj = (Dtls *) bio->ptr;

	switch (cmd) {
		case BIO_CTRL_EOF:
			return 0;
		case BIO_CTRL_GET_CLOSE:
			return bio->shutdown;
		case BIO_CTRL_SET_CLOSE:
			bio->shutdown = num;
			return 1;
		case BIO_CTRL_WPENDING:
			return 0;
		case BIO_CTRL_PENDING:
			return obj->_size - obj->_offset;
		case BIO_CTRL_DUP:
			return 1;
		case BIO_CTRL_FLUSH:
			DEBUG("flushed");
			return 1;
		case BIO_CTRL_RESET:
			obj->_offset = 0;
			obj->_size = 0;
			return 1;
		case BIO_CTRL_DGRAM_QUERY_MTU:
		case BIO_CTRL_DGRAM_GET_MTU:
			return 1500;
		case BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT:
			return 1;
		case BIO_CTRL_PUSH:
		case BIO_CTRL_POP:
		default:
			DEBUG("unknown ctrl " << cmd);
			return 0;
	}
}

