#include <ESP8266WiFi.h>

String ssid     = "Vieiras";
String password = "Welcome01";

const char* host = "iot.vieiras.xyz";
const char* deviceId   = "ESPGarden";

boolean tryToConnectWIFI() {
	WiFi.mode(WIFI_STA);

	digitalWrite(LED_BUILTIN, HIGH);
	for (byte i = 0; i < 5; i++) {
		Serial.print("CONATT:"); Serial.println(i);

		WiFi.disconnect();
		WiFi.begin(ssid.c_str(), password.c_str());
		if (WiFi.waitForConnectResult() != WL_CONNECTED) {
			WiFi.printDiag(Serial);
		} else {
			Serial.print("SUCCON:"); Serial.println(i);
			return true;
		}
	}
	digitalWrite(LED_BUILTIN, LOW);
	return false;
}

void captureCredentials() {
	Serial.print("REQSID:");
	while (!Serial.available()) {
		delay(100);
	}
	ssid = Serial.readString();

	Serial.print("REQPWD:");
	while (!Serial.available()) {
		delay(100);
	}
	password = Serial.readString();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
  delay(250);

  while (ssid.length() == 0 || password.length() == 0 || !tryToConnectWIFI()) {
	  captureCredentials();
  }
}

boolean post(String url) {
	  WiFiClient client;

	  const int httpPort = 80;
	  if (!client.connect(host, httpPort)) {
		Serial.println("ERRCON:");
		return false;
	  }

	  digitalWrite(LED_BUILTIN, HIGH);
	  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
				   "Host: " + host + "\r\n" +
				   "Authorization: Basic aW90OnhwdG8="
				   "Connection: close\r\n\r\n");

	  unsigned long timeout = millis();
	  while (client.available() == 0) {
		if (millis() - timeout > 5000) {
		  Serial.println("ERRTOT:"); Serial.println(millis() - timeout);
		  client.stop();
		  return false;
		}
	  }

	  /* DEBUG
	    while(client.available()){
		String line = client.readStringUntil('\r');
		Serial.print(line);
	  }*/

	  Serial.print("SUCPST:"); Serial.println(url);

	  return true;
}

boolean postLog(String details, unsigned long msAgo) {
	  String url = "/postLog?device=";
	  url += deviceId;
	  url += "&group=garden";
	  url += "&msAgo="; url.concat(msAgo);
	  url += "&details=" + details;
	  return post(url);
}

boolean postReading(String sensor, unsigned short value, unsigned long msAgo) {
	  String url = "/postReading?device=";
	  url += deviceId;
	  url += "&sensor=" + sensor;
	  url += "&value="; url.concat(value);
	  url += "&msAgo="; url.concat(msAgo);
	  return post(url);
}

void loop() {
  for (byte i = 0; i < 5; i++) {
	digitalWrite(LED_BUILTIN, HIGH);
	delay(50);
	digitalWrite(LED_BUILTIN, LOW);
	delay(50);
  }

  Serial.print("REQCMD:");
  while (!Serial.available()) {
	delay(100);
  }
  String command = Serial.readStringUntil(':');

  if (command.equals("PSTLOG")) {
	  String details = Serial.readStringUntil('&');
	  String msAgo = Serial.readString();
	  postLog(details, msAgo.toInt());
  } else if (command.equals("PSTRDG")) {
	  String sensor = Serial.readStringUntil('&');
	  String value = Serial.readStringUntil('&');
	  String msAgo = Serial.readString();
	  postReading(sensor, value.toInt(), msAgo.toInt());
  } else {
	  Serial.println("ERRCMD:" + command);
  }
}

