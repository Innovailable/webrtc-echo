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

#ifndef DTLS_H
#define DTLS_H 

#include <vector>

#include <node.h>
#include <v8.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

class Dtls : public node::ObjectWrap {
	public:
		Dtls(const char *cert_file, const char *key_file);
		~Dtls();

		static void init(v8::Handle<v8::Object> exports);

		void tick();
		void flush();

	private:
		static v8::Persistent<v8::Function> constructor;

		// js functions

		static v8::Handle<v8::Value> New(const v8::Arguments& args);
		static v8::Handle<v8::Value> close(const v8::Arguments& args);
		static v8::Handle<v8::Value> encrypt(const v8::Arguments& args);
		static v8::Handle<v8::Value> decrypt(const v8::Arguments& args);
		static v8::Handle<v8::Value> tick(const v8::Arguments& args);
		static v8::Handle<v8::Value> fingerprint(const v8::Arguments& args);
		static v8::Handle<v8::Value> srtpKeys(const v8::Arguments& args);

		// bio functions

		static const BIO_METHOD bioMethod;

		static int bioNew(BIO* bio);
		static int bioFree(BIO* bio);
		static int bioRead(BIO* bio, char* out, int len);
		static int bioWrite(BIO* bio, const char* data, int len);
		static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);
		//static int bioPuts(BIO* bio, const char* str);
		//static int bioGets(BIO* bio, char* out, int size);

		// state

		SSL_CTX *_ctx;
		SSL *_ssl;
		BIO *_bio;

		std::vector<char> _buf;
		int _offset;
		int _size;

		bool _connected;
		bool _closed;
};

#endif /* DTLS_H */
