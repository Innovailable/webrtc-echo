###############################################################################
#
#  webrtc-echo - A WebRTC echo server
#  Copyright (C) 2014  Stephan Thamm
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as
#  published by the Free Software Foundation, either version 3 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
###############################################################################

Srtp = require("./srtp").Srtp
Dtls = require("./dtls").Dtls
EventEmitter = require('events').EventEmitter
Buffer = require('buffer').Buffer

class exports.DtlsSrtp extends EventEmitter

  constructor: (@stream, cert_file, key_file, @rtcp_mux=false) ->
    @dtls = new Dtls(cert_file, key_file)

    @ready = false

    @initStream()
    @initDtls()

  initDtls: () ->
    @dtls.on 'encrypted', (data) =>
      @stream.send 1, data

    @dtls.on 'connected', () =>
      keys = @dtls.srtpKeys()

      catKey = (key) => Buffer.concat([key["key"], key["salt"]])

      sendKey = catKey(keys["client"])
      recvKey = catKey(keys["server"])

      console.log 'send: ' + sendKey.toString('hex')
      console.log 'recv: ' + recvKey.toString('hex')

      @srtp = new Srtp(sendKey, recvKey)

      clearInterval @dtls_timer
      delete @dtls_timer

      delete @dtls

    tick = () => if @ready then @dtls.tick()
    @dtls_timer = setInterval tick, 100

  initStream: () ->
    @stream.on 'receive', (component, data) =>
      if not @ready
        return

      if @dtls
        # dtls handshake
        @dtls.decrypt data
      else
        pt = data[1] & 0x7f

        if component == 1 and (not @rtpPayloads or @rtpPayloads[pt])
          @emit 'rtp', @srtp.unprotectRtp(data)
        else
          @emit 'rtcp', @srtp.unprotectRtcp(data)

    @stream.on 'stateChanged', (component, state) =>
      if component == 1 and state == 'ready'
        @ready = true
        @connect()

  setRtpPayloads: (payloads) ->
    @rtpPayloads = {}

    for payload in payloads
      @rtpPayloads[payload] = true

  fingerprint: () -> @dtls.fingerprint()

  connect: () ->
    if !@dtls?
      console.log "trying to connect but dtls does not exist"
      return

    @dtls.connect()

  rtp: (data) ->
    if !@srtp? then throw "dtls-srtp not ready to send"

    @stream.send 1, @srtp.protectRtp(data)

  rtcp: (data) ->
    if !@srtp? then throw "dtls-srtp not ready to send"

    if @rtcp_mux
      component = 1
    else
      component = 2

    @stream.send component, @srtp.protectRtcp(data)

  close: () ->
    if @dtls_timer? then clearInterval @dtls_timer

