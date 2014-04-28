# webrtc-echo

## Overview

This is a webrtc echo server. It sends all incoming packages back to the sender
without changing them. It implements ICE and DTLS-SRTP, makes some minor changes
to SDP, and relies on the ability of the peer to understand itself for the rest
of the WebRTC stack.

Signaling is done through [palava.tv](https://palava.tv), but can be easily
replaced with any other signaling server.

The project uses libsrtp, OpenSSL, and libnice. The business logic is written in
coffeescript, which also offers an HTTP interface to invite an echo instance
into a room and a test page.

## Setup

You need the following native libraries and their headers:

* libnice (>=1.4)
* OpenSSL
* libsrtp

To compile the native bindings and install the node dependencies run

    npm install

## Usage

The requires a certificate and a key file for the DTLS handshake. The
certificate does not have to be signed. You can configure the path to those
files with

    export CERT_FILE=cert.pem
    export KEY_FILE=key.pem

The values in the example are also the default values.

To start the server run

    coffee src/main.coffee

You can now either visit the test page on

    http://localhost:3000/

or invite the echo bot into any palava room with a post request to

    http://localhost:3000/invite.json

passing the desired room as the `room` value.

