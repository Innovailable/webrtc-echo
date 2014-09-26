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

BIND_PORT = process.env.BIND_PORT ? 3000
BIND_HOST = process.env.BIND_HOST ? "0.0.0.0"

# require stuff

express = require 'express'
connect = require 'connect'
serve_static = require 'connect'

PalavaRoom = require('./palava').PalavaRoom

# initalize express

app = express()

app.configure () =>
  app.set 'views', __dirname + '/../views'
  app.use require('connect-assets')()
  app.use serve_static __dirname + '/../public'
  app.use serve_static __dirname + '/../support/public'
  app.use express.bodyParser()

# test page

app.get '/', (req, res) =>
  res.render 'index.jade'

app.post '/invite.json', (req, res) =>
  room = req.body.room

  if room
    new PalavaRoom(room, 10 * 60 * 1000)
    res.send { success: true }
  else
    res.status 500
    res.send { error: "no room given" }

# actually start

console.log "Listening on " + BIND_HOST + ":" + BIND_PORT
app.listen BIND_PORT, BIND_HOST

#doGcStuff = () =>
  #log 'doing gc stuff'
  #gc()

#setInterval doGcStuff, 5000

