#!/usr/bin/env coffee
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

RTC_ADDRESS = process.env.RTC_ADDRESS ? "wss://machine.palava.tv"

WebSocketClient = require('websocket').client

echo = require './echo'

log = (msg) => console.log '[palava] ' + msg

objectSize = (obj) =>
  count = 0

  for _, _ of obj
    count += 1

  return count

class PalavaSignaling

  constructor: (@id, @room) ->

  send: (data) ->
    @room.send {
      event: 'send_to_peer'
      peer_id: @id
      data: data
    }

  sendAnswer: (sdp) ->
    @send {
      event: 'answer'
      sdp:
        sdp: sdp
        type: 'answer'
    }

  sendCandidate: (id, index, candidate) ->
    @send {
      event: 'ice_candidate'
      sdpmid: id
      sdpmlineindex: index
      candidate: candidate
    }

class PalavaRoom

  constructor: (room, timeout) ->
    log "joining room" + room + " ..."

    @peers = {}

    closeCb = () =>
      log 'closing because of timeout'
      @close()

    @timer = setTimeout closeCb, timeout

    ws = new WebSocketClient()

    ws.on 'connectFailed', (error) =>
      log error

    ws.on 'connect', (connection) =>
      log "connected to websocket"

      @connection = connection

      connection.on 'message', (data, flags) =>
        if data.type != 'utf8'
          return

        @receive data.utf8Data

      connection.on 'close', () =>
        @close()

      @send {
        event: 'join_room'
        room_id: room
        status:
          name: "Echo"
          user_agent: 'chrome'
      }

    log "conencting to " + RTC_ADDRESS
    ws.connect RTC_ADDRESS

  receive: (raw) ->
    data = JSON.parse raw

    event = data.event

    #console.log '<<<'
    #console.log data

    if data.sender_id?
      peer = @peers[data.sender_id]

      if not peer?
        log 'message from unknown peer'
        return

      switch event
        when 'offer'
          peer.offer data.sdp?.sdp
        when 'ice_candidate'
          peer.addIceCandidate data.sdpmid, data.sdpmlineindex, data.candidate
        when 'peer_left'
          peer.close()
          delete @peers[data.sender_id]

          if objectSize(@peers) == 0
            log 'closing because last peer left'
            @close()
    else
      switch event
        when 'joined_room'
          if data.peers.length == 0
            log 'entered empty room, leaving immediately'
            @close()

          for peer in data.peers
            @addPeer peer.peer_id
        when 'new_peer'
          @addPeer data.peer_id

  send: (data) ->
    #console.log '>>>'
    #console.log data
    @connection?.send(JSON.stringify(data))

  addPeer: (peer_id) ->
    signaling = new PalavaSignaling(peer_id, this)
    @peers[peer_id] = new echo.EchoPeer(signaling)

  close: ->
    # clean up peers

    for _, peer of @peers
      peer.close()

    @peers = []

    clearTimeout @timer

    # close websocket

    @connection?.close()
    delete @connection

exports.PalavaRoom = PalavaRoom

