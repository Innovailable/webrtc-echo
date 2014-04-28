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

# this test script is a echoing dtls client

dtls = require './dtls'
dgram = require 'dgram'

SOURCE_PORT = 3456
DEST_PORT = 6543
DEST_HOST = "localhost"

socket = dgram.createSocket('udp4')
enc = new dtls.Dtls()

console.log enc.fingerprint()

socket.bind SOURCE_PORT, () =>
  enc.on 'encrypted', (data) =>
    console.log 'sending ' + data.length + ' bytes'
    socket.send data, 0, data.length, DEST_PORT, DEST_HOST

  enc.on 'decrypted', (data) =>
    console.log 'recrypting ' + data.length + ' bytes'
    enc.encrypt data

  socket.on 'message', (data, info) =>
    console.log 'receiving ' + data.length + ' bytes'
    enc.decrypt data

  enc.connect()

