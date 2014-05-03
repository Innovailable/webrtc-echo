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

# configuration

STUN_ADDRESS = process.env.STUN_ADDRESS ? "176.28.9.212"

CERT_FILE = process.env.CERT_FILE ? "cert.pem"
KEY_FILE = process.env.KEY_FILE ? "key.pem"

# init

NiceAgent = require('libnice').NiceAgent
DtlsSrtp = require('./dtls_srtp').DtlsSrtp

log = (msg) => console.log '[echo] ' + msg

nice = new NiceAgent "rfc5245"
nice.setStunServer(STUN_ADDRESS)
nice.setControlling(false)

class exports.EchoPeer

  constructor: (@signaling) ->
    @streams = {}

  offer: (sdp) ->
    lines = sdp.split('\r\n')

    # let's parse!

    stream = global = {}
    rtp_types = []

    i = 0
    mline = 0

    while i < lines.length
      line = lines[i]

      # helper functions

      remove_line = () =>
        lines.splice i, 1
        i--

      insert_line = (data) =>
        lines.splice i, 0, data
        i++

      if stream.needs_ice_cred and line.match(/a=.*/)
        credentials = stream.nice.getLocalCredentials()
        insert_line 'a=ice-ufrag:' + credentials.ufrag
        insert_line 'a=ice-pwd:' + credentials.pwd
        insert_line 'a=fingerprint:' + stream.dtls_srtp.fingerprint()
        delete stream.needs_ice_cred

      if m = line.match(/m=(\w+) \S+ \S+ (.*)/)
        # m-lines describe a media stream, create nice connections for them

        id = m[1]
        index = mline

        mline++

        nice_stream = nice.createStream(2)
        dtls_srtp = new DtlsSrtp(nice_stream, CERT_FILE, KEY_FILE)

        stream = {
          id: id
          index: index
          nice: nice_stream
          dtls_srtp: dtls_srtp
          needs_ice_cred: true
        }

        @streams[m[1]] = stream

        # send candidates when gathered

        gatheringDone = (stream) => (candidates) =>
          log stream.id + " gathering done"
          for candidate in candidates
            @signaling.sendCandidate stream.id, stream.index, candidate + '\r\n'

        nice_stream.on 'gatheringDone', gatheringDone stream

        # state stuff

        stateChanged = (stream) => (component, state) =>
          log stream.id + ":" + component + " is " + state

        nice_stream.on 'stateChanged', stateChanged stream

        # mirroring

        rtp = (stream) => (data) =>
          stream.dtls_srtp.rtp data

        dtls_srtp.on 'rtp', rtp(stream)

        rtcp = (stream) => (data) =>
          stream.dtls_srtp.rtcp data

        dtls_srtp.on 'rtcp', rtcp(stream)

        # save payload types for rtpmux

        rtp_types = rtp_types.concat(parseInt(type) for type in m[2].split(" "))

      else if m = line.match(/a=ice-ufrag:(.*)/)
        # replace and apply ufrag
        stream.ufrag = m[1]
        remove_line()

      else if m = line.match(/a=ice-pwd:(.*)/)
        # replace and apply pwd
        stream.pwd = m[1]
        remove_line()

      else if m = line.match(/a=ice-options:/)
        # line not needed
        remove_line()

      else if m = line.match(/a=setup:actpass/)
        # we are acting as dtls client
        lines[i] = 'a=setup:active'

      else if m = line.match(/a=candidate:(.*)/)
        # add incoming candidates to libnice
        stream.nice.addRemoteIceCandidate line
        remove_line()

      else if m = line.match(/a=crypto:(.*)/)
        # we are using dtls-srtp, so this line has to go
        remove_line()

      else if m = line.match(/a=fingerprint:/)
        # remove fingerprint, we add our own above
        remove_line()

      i++

    for id, stream of @streams
      stream.nice.setRemoteCredentials(stream.ufrag ? global.ufrag, stream.pwd ? global.pwd)
      stream.nice.gatherCandidates()

      stream.dtls_srtp.setRtpPayloads(rtp_types)

    answer = lines.join('\r\n')

    #console.log '========'
    #console.log answer

    @signaling.sendAnswer answer

  addIceCandidate: (id, index, candidate) ->
    @streams[id]?.nice?.addRemoteIceCandidate candidate

  close: () ->
    console.log 'closing echo'
    for _, stream of @streams
      stream.nice.close()
      stream.dtls_srtp.close()

