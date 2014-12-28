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

child_process = require('child_process')
fs = require('fs')
async = require('async')

exports.get_version = (cb) ->
  res = {}

  funs = [
    # read version from file
    (cb) ->
      fs.readFile __dirname + '/../package.json', 'utf-8', (err, data) ->
        if err
          cb(err)
        else
          res.version = JSON.parse(data).version
          cb()
    # get commit from git call
    (cb) ->
      child_process.exec 'git rev-parse HEAD', {cwd: __dirname + '/../'}, (err, stdout, stderr) ->
        if err
          cb(err)
        else
          res.commit = stdout
          cb()
  ]

  async.parallel funs, (err) ->
    cb(err, res)

