$ ->
  s4 = () -> Math.floor((1 + Math.random()) * 0x10000) .toString(16) .substring(1)
  guid = () -> s4() + s4() + '-' + s4() + '-' + s4() + '-' + s4() + '-' + s4() + s4() + s4()

  invite_echo = (room) ->
    $.ajax {
      url: "/invite.json"
      type: 'POST'
      data: {
        room: room
      }
      success: (data) ->
        console.log data
      error: (err) ->
        console.log err
    }

  room = guid()

  session = new palava.Session
    roomId: room
    channel: new palava.WebSocketChannel('wss://machine.palava.tv')

  session.on 'local_stream_ready', (stream) ->
    session.room.join()
    invite_echo(room)

  session.on 'peer_stream_ready', (peer) ->
    if !peer.isLocal() and $('#' + peer.id).length == 0
      element = $("<li id='#{peer.id}'><video width='800px' autoplay></video></li>")
      $('#video').append(element)

      palava.browser.attachMediaStream(element.children('video')[0], peer.getStream())

  session.on 'peer_left', (peer) ->
    $('#' + peer.id).remove()

  session.init
    identity: new palava.Identity
      userMediaConfig:
        audio: true
        video: true
    options:
      stun: 'stun:stun.palava.tv'
      joinTimeout: 500
