const fs = require('fs');
const http = require('http');
const WebSocket = require('ws');
var static = require('node-static');
var mqtt = require('mqtt')
var history = '';

var file = new(static.Server)(__dirname);
server = http.createServer(function (req, res) {
  console.log(req);
  file.serve(req, res);
});

const wss = new WebSocket.Server({ server });
var mq = mqtt.connect({ port: 1883, host: 'localhost', keepalive: 10000});
mq.subscribe('door/#')
mq.on('message', function (topic, message, packet) {
  const s = message.toString();
  console.log(s);
  wss.clients.forEach(function each(wsock) {
    if (wsock.readyState === WebSocket.OPEN) {
      wsock.send('MQTT message: ' + s);
    }
  });
})

wss.on('connection', function connection(ws) {
  ws.on('message', function incoming(message) {
    console.log('received: %s', message);
    if (message == "STOP") { 
      mq.publish('door/command', message);
    }
    if (history == '3512') {
      if (message == 'OPEN' || message == 'CLOSE') {
        mq.publish('door/command', message);
      }
    }
    history = history.concat(message);
    wss.clients.forEach(function each(wsock) {
      if (wsock.readyState === WebSocket.OPEN) {
        if (history.length > 4) {
          for(var n = 0; n < 10; n++) { 
            wsock.send("removeProperty " + n + " background-color");
          }
        }
        if (parseInt(message) >= 0 && parseInt(message) < 10) {
          wsock.send("setProperty" + " " + message + " background-color blue");
        }
        wsock.send((history == "3512" ? "removeAttribute" : "setAttribute") + " OPEN disabled disabled");
        wsock.send((history == "3512" ? "removeAttribute" : "setAttribute") + " CLOSE disabled disabled");
        wsock.send((history == "3512" ? "setProperty" : "removeProperty") + " OPEN background-color green");
        wsock.send((history == "3512" ? "setProperty" : "removeProperty") + " CLOSE background-color green");
      }
    });
    if (history.length > 4) {
      history = message;
    }
    console.log('history is ' + history);
  });
});

server.listen(8082);
