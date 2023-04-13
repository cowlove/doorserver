#include "jimlib.h"
#include "RollingLeastSquares.h"
#include "TTGO_TS.h"
//#include <CAN.h>
//#include <esp_now.h>
#include "PubSubClient.h"


//WiFiClient wifiClient;
//PubSubClient mqttClient(wifiClient); 
int overCurrent = 0;
int overCurrentInhibit = 0;

JStuff j;

//void mqtt_callback(char* topic, byte* payload, unsigned int length);

//void setupMQTT() {
//}

struct { 
	int led = 19;
	int led2 = 02;
	int l_r1 = 26;
	int l_r2 = 32; 
	int amps = 33;
	int topButton = 36;
	int midButton = 37;
	int botButton = 39;
} pins;

int door = 0, dir = 0;
int tdir = 0; // true dir
int rssi = 0;

int counter=0, loraRecvCount = 0;

void doorDown() { 
	dir = 0;
	door = 1300;
	overCurrentInhibit = 200;
}
void doorUp() { 
	dir = 1;
	door = 1300;
	overCurrentInhibit = 200;
}
void doorStop() { 
	dir = 0; door = 0;
}

void parseCommandText(const char *s) { 
	if (strcmp(s, "door up") == 0) { doorUp(); }
	else if(strcmp(s, "door down") == 0) { doorDown(); } 
	else if(strcmp(s, "door stop") == 0) { doorStop(); } 
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
	//Serial.printf("MQTT recv: '%s' => ", topic);
	char buf[256];
	if (length < sizeof(buf)) { 
		for (int i = 0; i < length; i++) {
			buf[i] = payload[i];
		}
		buf[length] = 0;
		Serial.println(buf);

		if (strcasecmp(topic, "door/command") == 0) { 
			if (strcasecmp(buf, "OPEN") == 0) doorUp();
			if (strcasecmp(buf, "CLOSE") == 0) doorDown();
			if (strcasecmp(buf, "STOP") == 0) doorStop();
			if (strcasecmp(buf, "RESET") == 0) ESP.restart();
		}
		if (strcasecmp(topic, "door/amps") == 0) {
			esp_task_wdt_reset();	
		}
	}
}

EggTimer sec(100);
int led;
RollingAverage<int16_t, 200> avgAmps;
RollingAverage<int16_t, 200> doorAmps[3], da2;

void LongShortButtonISR();
class LongShortButton : public LongShortFilter { 
public:
	DigitalButton b;
	LongShortButton(int p) : b(0), LongShortFilter(1500, 600) {
		buttons.push_back(this);
		attachInterrupt(digitalPinToInterrupt(b.pin), LongShortButtonISR, CHANGE);
	}
	static std::vector<LongShortButton *> buttons;
	static void isr() {
		for (std::vector<LongShortButton *>::iterator it = buttons.begin(); it != buttons.end(); it++) { 
			(*it)->b.check();
			(*it)->check((*it)->b.duration());
		}
	}
};

std::vector <LongShortButton *> LongShortButton::buttons;
void LongShortButtonISR() { 
	LongShortButton::isr();
}

JDisplay jd(2, 0, 0, false);
JDisplayItem<int> disTitle(&jd,0,0,"DOORCOMMANDER     2.0", "");
JDisplayItem<int> disDir(&jd,10,30,"DIR :", "%d");
JDisplayItem<int> disTime(&jd,10,40,"TIME:", "%04d");
JDisplayItem<float> disAmps(&jd,10,50,"AMPS:", "%4.1f");

void setup() {
	esp_task_wdt_init(20, true);
	esp_task_wdt_add(NULL);
	//WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector   

	Serial.begin(921600, SERIAL_8N1);
	jd.begin();
	jd.clear();
	disTitle.color.lf = ST7735_RED;
	jd.tft.setRotation(3);
	while(0) { 
		delay(100);
		printPins();
	}

    //WiFi.mode(WIFI_STA);
	//WiFi.begin("ChloeNet3", "niftyprairie7");
    
	for(int n = 0; n < sizeof(doorAmps)/sizeof(doorAmps[0]); n++) { 
		doorAmps[n].reset();
	}
	//jw.onConnect([](void) {});
	//jw.onOTA([](void) {});
}

void loop() {
    esp_task_wdt_reset();
	pinMode(pins.l_r1, OUTPUT);
	pinMode(pins.l_r2, OUTPUT);
	pinMode(pins.amps, INPUT);
	pinMode(pins.topButton, INPUT_PULLUP);
	pinMode(pins.midButton, INPUT_PULLUP);
	pinMode(pins.botButton, INPUT_PULLUP);
	pinMode(pins.amps, INPUT_PULLDOWN);


#if 1
	int amps = analogRead(pins.amps);
	avgAmps.add(amps);
	if (overCurrentInhibit > 0) overCurrentInhibit--;
	
	int cLimit = 1150;
	if (doorAmps[1].full() && doorAmps[2].full()) { 
		cLimit = doorAmps[2].average() + (doorAmps[1].average() - doorAmps[2].average()) * 1.25;
	}
	cLimit = min(1150, cLimit);

	if (door && overCurrentInhibit <= 0 && (avgAmps.average() > cLimit || avgAmps.average() < 500)) {
		Serial.println("Overcurrent stop");
		door = dir = 0;
	}
	if (digitalRead(pins.topButton) == 0) { 
		//espnowSend("door up");
		doorUp();
	} else if (digitalRead(pins.botButton) == 0) {
		doorDown();
		//espnowSend("door down");
	} else if (digitalRead(pins.midButton) == 0) { 
		doorStop();
		//Serial.println("Door stop");
		//espnowSend("door stop");
	}
#endif

	if (door > 0) { 
		if (dir == 0) { 
			int t = door % 100;
			if (t < 1) { 
				digitalWrite(pins.l_r1, 0);
				digitalWrite(pins.l_r2, 0);
				overCurrentInhibit = 200;
			} else if (t < 3) { 
				int stop = (doorAmps[0].average() - doorAmps[2].average()) * 1.4 + doorAmps[2].average();
				Serial.printf("Bottom sense stop %d/%d\n", (int)avgAmps.average(), stop);
				if (avgAmps.average() < stop) { 
					door = dir = 0;
				}
			} else if (t < 13) {
				digitalWrite(pins.l_r1, 1);
				digitalWrite(pins.l_r2, 0);
			} else if (t < 15) {
				digitalWrite(pins.l_r1, 0);
				digitalWrite(pins.l_r2, 0);
				overCurrentInhibit = 200;
			} else { 
				digitalWrite(pins.l_r1, 0);
				digitalWrite(pins.l_r2, 1);
			}
		} else { 
				digitalWrite(pins.l_r1, 1);
				digitalWrite(pins.l_r2, 0);
		}	
	} else {
		digitalWrite(pins.l_r1, 0);
		digitalWrite(pins.l_r2, 0);
	}

	if (sec.tick()) {
		pinMode(pins.led, OUTPUT);
		digitalWrite(pins.led, !digitalRead(pins.led));
		pinMode(pins.led2, OUTPUT);
		digitalWrite(pins.led2, !digitalRead(pins.led2));
		if (door > 0) {
			door -= 1;
		}
		
		amps = avgAmps.average();
		if (overCurrentInhibit <= 0) { 
			int index = (digitalRead(pins.l_r1) == digitalRead(pins.l_r2)) ? 2 : digitalRead(pins.l_r1); // 0 == down, 1 == up, 2 == off
			doorAmps[index].add(amps);
		}
		std::string s = strfmt("%07.2f door %d dir %d amps %05d/%05d d0:%05d d1:%05d d2:%05d", 
			millis() / 1000.0, door, dir, amps, cLimit, 
			(int)doorAmps[0].average(), (int)doorAmps[1].average(), (int)doorAmps[2].average());
		Serial.println(s.c_str());
		//mqttClient.publish("door/debug", s.c_str());

		disDir = dir;
		disAmps = max(0.0, (amps - 00) / 10.0);
		disTime = door;
		jd.update(false, true);
		static int loopCount = 0;
		if (loopCount++ % 10 == 0) { 
			//mqttClient.publish("door/amps", strfmt("%d", amps).c_str());
			//loraSend(strfmt("test %d", loopCount % 100).c_str());
			//espnowSend(strfmt("test %d", loopCount % 100).c_str());
		}
	}

	j.run();

	delay(1);
}

