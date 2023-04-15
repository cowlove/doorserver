sudo apt-get install mosquitto npm nodejs
npm install
node ./doorserver.js
echo listener 1883 | sudo tee -a /etc/mosquitto/mosquitto.conf

echo allow_anonymous true | sudo tee -a /etc/mosquitto/mosquitto.conf

 
