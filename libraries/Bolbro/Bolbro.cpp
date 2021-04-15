#include <Bolbro.h>

/* --------------------------------------------------------------------------------
	Global definitions
   -------------------------------------------------------------------------------- */

#include <RemoteDebug.h>

Print *LOG = &Serial;
RemoteDebug Debug;

BolbroClass Bolbro;

BolbroClass::BolbroClass() {

	mLastBedtimeCheck = 0;
	mDebug = false;
	mAppName = mAppNameLowerCase = NULL;
	mSignalStrengthItem = NULL;
	mLastStartItem = NULL;
	mLastStartItemRetryCount = 0;
	mLastSignalStrength = 0;
	mLastSetSignalStrength = 0;
	mConfigureTimeStatus = ConfigureTimeNotRequested;
	mStartSeconds = 0;
	mSerialBaud = 115200;
	mOpenHABHost = NULL;
	mNetworks[0].ssid = NULL;
	mUnresolvedWAN[0] = NULL;
	mWANs[0] = IPAddress();
}

void BolbroClass::addWiFi(const char *ssid, const char *password) {

	for (int i = 0; i<MAX_NETWORKS; i++)
		if (!mNetworks[i].ssid) {
			mNetworks[i].ssid = ssid; // assume const memory, no copy
			mNetworks[i].password = password; // assume const memory, no copy
			mNetworks[i+1].ssid = NULL; // end marker
			return;
		}
}

void BolbroClass::setSerialBaud(long baudRate) {
	mSerialBaud = baudRate;
}

void BolbroClass::setOpenHABHost(const char *hostname) {
	mOpenHABHost = hostname; // assume const memory, no copy
}

void BolbroClass::setup(const char *appname, boolean debug, boolean useRemoteDebug) {

	//  Prepare logging / communication
	Serial.begin(mSerialBaud);
	delay(10);

	Serial.printf("\nBolbroClass::setup(appname: %s, debug: %s, useRemoteDebug: %s)\n",
		appname, debug?"true":"false", useRemoteDebug?"true":"false");

	mAppName = appname;

	mAppNameLowerCase = (char *)malloc(strlen(appname)+1);

	const char *source = appname;
	char *writer = mAppNameLowerCase;

	while(*source)
		*writer++ = tolower(*source++);
	*writer = '\0';

	if (debug) {
		mDebug = true;
		Serial.println("running Bolbro library in debug mode");
	}

	//  Set language, doesn't work for de_DE currently
	setlocale(LC_ALL, "de_DE");

	if (useRemoteDebug&&connectToWiFi()) {
		LOG = &Debug;
		Debug.begin(mAppName);
		Debug.setSerialEnabled(false);
		Serial.println("RemoteDebug started");
	}
}

unsigned long BolbroClass::uptime() {

	if (mStartSeconds) {
		time_t seconds = time(NULL);

		return seconds-mStartSeconds;
	} else
		return 0;
}

String BolbroClass::openHABTime(time_t utcTime) {
	char tBuffer [80];

	strftime (tBuffer, 80, "%Y-%m-%dT%H:%M:%S.000%z", localtime (&utcTime));

	return String(tBuffer);
}

String BolbroClass::startTime(bool humanReadable) {
	if (mStartSeconds) {
		if (humanReadable) {
			String timeStr(ctime(&mStartSeconds));

			return timeStr.substring(0, timeStr.length()-1);
		} else
			return openHABTime(mStartSeconds);
	} else
		return "";
}

static int mapSignalStrength(int dbm) {

	int strength;

	if (dbm > -60)
		strength = 4;
	else if (dbm > -70)
		strength = 3;
	else if (dbm > -80)
		strength = 2;
	else if (dbm > -90)
		strength = 1;
	else
		strength = 0;

	return strength;
}

void BolbroClass::updateOHItems() {

	if (WiFi.status() == WL_CONNECTED) {
		if (mSignalStrengthItem) {

			int signalStrength = mapSignalStrength(WiFi.RSSI());
			time_t now = time(NULL);

			// send if changed and not sent for 30 seconds or after 10 minutes
			if ((signalStrength != mLastSignalStrength && mLastSetSignalStrength+1*60 < now)|| mLastSetSignalStrength+10*60 < now) {
				mLastSignalStrength = signalStrength;
				mLastSetSignalStrength = now;
				updateItem(mSignalStrengthItem, String(signalStrength));
			}
		}

		if (mLastStartItem && mLastStartItemRetryCount<10) {
			String startTime = Bolbro.startTime(false);
			if (startTime!="") {
				if (updateItem(mLastStartItem, startTime))
					mLastStartItem = NULL; // success
				else
					// try again?
					mLastStartItemRetryCount++;
			}
		}
	}
}

void BolbroClass::loop() {

	// handle remote debugger if enabled
	if (LOG == &Debug)
		Debug.handle();

	// handle configure time / start time retrieval
	switch (mConfigureTimeStatus) {
		case ConfigureTimeNotRequested:
			// do nothing
			break;
		case ConfigureTimeRequested:
			//	requested, but former call did not succeed
			configureTime();
			break;
		case ConfigureTimeSucceeded:
			//	configuration succeeded, but start time not set
			if (!mStartSeconds) {
				time_t t = time(NULL);
				if (t>50ul*365*24*60*60) { // 2020 or later
					mStartSeconds = t;
					LOG->printf("start time found: %s", ctime(&mStartSeconds));
				}
			}
			break;
	}

	// update signal strength item and others based on WiFi RSSI
	updateOHItems();
}

/* --------------------------------------------------------------------------------
	time stuff
   -------------------------------------------------------------------------------- */

void BolbroClass::configureTime() {

	if (mConfigureTimeStatus!=ConfigureTimeSucceeded) {
		//	we are here, so this has been at least requested
		mConfigureTimeStatus = ConfigureTimeRequested;

		if (WiFi.status() == WL_CONNECTED) {
			LOG->println("before configTime...");
			//  Set time
			const char *ntpServer = "pool.ntp.org";
			const long gmtOffset_sec = prefGetInt("GMTOffset", 3600 /*Germany*/);
			const int daylightOffset_sec = prefGetInt("DaylightOffset",3600 /*Summer*/);

			configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
			LOG->println("time configured.");
			mConfigureTimeStatus = ConfigureTimeSucceeded;
		}
	}
}

void BolbroClass::setTimezone(int gmtOffset_sec, int daylightOffset_sec) {

	//	store values
	prefSetInt("GMTOffset", gmtOffset_sec);
	prefSetInt("DaylightOffset", daylightOffset_sec);

	//	void existing configTime
	mConfigureTimeStatus = ConfigureTimeNotRequested;

	//	configure time again
	configureTime();
}

bool BolbroClass::timeConfigured() {
	return mConfigureTimeStatus == ConfigureTimeSucceeded;
}


/* --------------------------------------------------------------------------------
	openHAB access
   -------------------------------------------------------------------------------- */

#include <HTTPClient.h> // for communication to openHAB
#include <ArduinoJson.h>

String BolbroClass::getItemStatus(const char *item) {

	String result = STRINGNOTINITIALIZED;

	if (connectToWiFi()&&mOpenHABHost) {
		//  Setup http request for openHAB REST API
		HTTPClient http;

		char httpURL[128];
		sprintf(httpURL, "http://%s:8080/rest/items/%s", mOpenHABHost, item);

		http.begin(httpURL);
		http.setConnectTimeout(200); // local network, should be fast
		http.setTimeout(200);

		int httpResponseCode = http.GET();

		if (httpResponseCode==HTTP_CODE_OK) {

			const String& jsonString = http.getString();

			if (mDebug)
				LOG->printf("openHAB GET received: %s\n", jsonString.c_str());

			//  Make sure the life time of document lasts up to return
			//  see https://arduinojson.org/v6/assistant/ for
			//  {"link":"http://openhabian:8080/rest/items/Electricity_Inverter_Yield_Range","state":"3.7 (-7.1) kWh",
			//  "stateDescription":{"pattern":"%s","readOnly":false,"options":[]},"editable":false,"type":"String",
			//  "name":"Electricity_Inverter_Yield_Range","label":"PV Generator","category":"solarplant","tags":[],"groupNames":[]}
			const size_t sizeForStrings = 1000;
			const size_t capacity = 3*JSON_ARRAY_SIZE(0) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(10) + sizeForStrings;
			StaticJsonDocument<capacity> jsonDoc;

			DeserializationError err = deserializeJson(jsonDoc, jsonString.c_str());

			if (err==DeserializationError::Ok) {
				const char *state = jsonDoc["state"];

				if (state)
					result = state; // creates a String

				if (mDebug)
					LOG->printf("item %s retrieved: %s\n", item, result.c_str());
			} else {
				LOG->printf("error accessing openHAB JSON: %s\n", err.c_str());
			}
		} else {
			const char *codeName = NULL;

			switch (httpResponseCode) {

				case HTTPC_ERROR_CONNECTION_REFUSED:

					codeName = "HTTPC_ERROR_CONNECTION_REFUSED";
					break;

				case HTTPC_ERROR_SEND_HEADER_FAILED:

					codeName = "HTTPC_ERROR_SEND_HEADER_FAILED";
					break;

				case HTTPC_ERROR_SEND_PAYLOAD_FAILED:

					codeName = "HTTPC_ERROR_SEND_PAYLOAD_FAILED";
					break;

				case HTTPC_ERROR_NOT_CONNECTED:

					codeName = "HTTPC_ERROR_NOT_CONNECTED";
					break;

				case HTTPC_ERROR_CONNECTION_LOST:

					codeName = "HTTPC_ERROR_CONNECTION_LOST";
					break;

				case HTTPC_ERROR_NO_STREAM:

					codeName = "HTTPC_ERROR_NO_STREAM";
					break;

				case HTTPC_ERROR_NO_HTTP_SERVER:

					codeName = "HTTPC_ERROR_NO_HTTP_SERVER";
					break;

				case HTTPC_ERROR_TOO_LESS_RAM:

					codeName = "HTTPC_ERROR_TOO_LESS_RAM";
					break;

				case HTTPC_ERROR_ENCODING:

					codeName = "HTTPC_ERROR_ENCODING";
					break;

				case HTTPC_ERROR_STREAM_WRITE:

					codeName = "HTTPC_ERROR_STREAM_WRITE";
					break;

				case HTTPC_ERROR_READ_TIMEOUT:

					codeName = "HTTPC_ERROR_READ_TIMEOUT";
					break;
			}

			if (codeName)
				LOG->printf("error requesting openHAB item '%s': %s\n", item, codeName);
			else
				LOG->printf("error requesting openHAB item '%s': HTTP response code %d\n", item, httpResponseCode);
		}

		http.end();
	}

	return result;
}

bool BolbroClass::sendItemCommand(const char *item, const String& status) {

	bool result = false;

	//  Connect to WiFi in case we are not connected already...
	if (connectToWiFi()&&mOpenHABHost)
	{
		//  Setup http request for openHAB REST API
		HTTPClient http;

		char httpURL[128];
		sprintf(httpURL, "http://%s:8080/rest/items/%s", mOpenHABHost, item);

		http.begin(httpURL);

		http.addHeader("Content-Type", "text/plain");
		http.addHeader("Accept", "application/json");

		//  Post state
		int httpResponseCode = http.POST(status.c_str());

		if (httpResponseCode>0) {
			LOG->printf("sent command %s to item %s (%d)\n", status.c_str(), item, httpResponseCode);
			result = true;
		} else
			LOG->printf("error on sending %s POST: %d\n", item, httpResponseCode);

		http.end();
	}

	return result;
}

bool BolbroClass::updateItem(const char *item, const String& status) {

	bool result = false;

	//  Connect to WiFi in case we are not connected already...
	if (connectToWiFi()&&mOpenHABHost)
	{
		//  Setup http request for openHAB REST API
		HTTPClient http;

		//	curl -X PUT --header "Content-Type: text/plain" --header "Accept: application/json"
		//		-d "OFF" "http://openhabian:8080/rest/items/Electricity_Dose1/state"
		char httpURL[128];
		sprintf(httpURL, "http://%s:8080/rest/items/%s/state", mOpenHABHost, item);

		http.begin(httpURL);

		http.addHeader("Content-Type", "text/plain");
		http.addHeader("Accept", "application/json");

		//  Post state
		int httpResponseCode = http.PUT(status.c_str());

		if (httpResponseCode==202) {
			LOG->printf("updated %s to %s (%d)\n", item, status.c_str(), httpResponseCode);
			result = true;
		} else
			LOG->printf("error on sending %s PUT: %d\n", item, httpResponseCode);

		http.end();
	}

	return result;
}

void BolbroClass::setSignalStrengthItem(const char *item) {

	mSignalStrengthItem = item;
}

void BolbroClass::setLastStartItem(const char *item) {

	mLastStartItem = item;
}

/* --------------------------------------------------------------------------------
	WiFi utilities
   -------------------------------------------------------------------------------- */

#include <WiFi.h>
#include <ESPmDNS.h> // to announce "appname.local"

static boolean connectNetwork(const char *ssid, const char *password) {

	Serial.printf("connecting to WiFi SSID: %s\n", ssid);

	WiFi.begin(ssid, password);

	const int numConnectAttempts = 10;

	int numAttempts = numConnectAttempts;

	while (WiFi.status() != WL_CONNECTED && numAttempts > 0)
	{
		numAttempts--;
		delay(500);
	}

	return WiFi.status() == WL_CONNECTED;
}

boolean BolbroClass::connectToWiFi() {

	boolean connected = true;

	if (WiFi.status() != WL_CONNECTED) {

		connected = false;

		Serial.printf("testing available networks...\n");

		byte numSsids = WiFi.scanNetworks();
		struct Network *bestNetworkP = NULL;
		long bestRSSI = -2000;

		for (byte ap = 0; ap<numSsids; ap++) {
			const char *ssid = WiFi.SSID(ap).c_str();
			long rssi = WiFi.RSSI(ap);
			Serial.printf("SSID: %s, RSSI: %ld", ssid, rssi);

			if (ssid!=0 && rssi>bestRSSI) {
				// test if this is a known network
				struct Network *networkP = mNetworks;

				while (networkP->ssid) {

					if (strcmp(networkP->ssid, ssid)==0) {
						//  known network, memorize
						bestNetworkP = networkP;
						bestRSSI = rssi;
						Serial.printf(" (best so far)\n");
						break;
					}

					networkP++;
				}

				if (!networkP->ssid)
					Serial.printf("\n");
			} else
				Serial.printf("\n");
		}

		if (bestNetworkP) {
			Serial.printf("connecting to best network...\n");
			connected = connectNetwork(bestNetworkP->ssid, bestNetworkP->password);
		}

		if (!connected) {
			Serial.printf("trying other networks...\n");
			struct Network *networkP = mNetworks;

			while (networkP->ssid!=NULL) {
				if (networkP!=bestNetworkP) {
					connected = connectNetwork(bestNetworkP->ssid, bestNetworkP->password);

					if (connected)
						break;
				}
				networkP++;
			}
		}

		if (connected) {
			Serial.printf("WiFi %s connected\n", WiFi.SSID().c_str());
			Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

			publishDNSName();

			updateOHItems();

			if (LOG == &Debug)
				Serial.printf("to monitor log output, use 'telnet %s' on command line\n", WiFi.localIP().toString().c_str());
		} else
			Serial.println("giving up on WiFi connect, no network connection established");
	}

	return connected;
}

void BolbroClass::disconnectWiFi() {

	if (WiFi.status() == WL_CONNECTED) {

		WiFi.disconnect();

		while(WiFi.status() == WL_CONNECTED)
			delay(20);

		Serial.println("WiFi disconnected");
	}
}

void BolbroClass::addWANGateway(const char *dynHostname) {
	for (int i = 0; i<MAX_UNRESOLVED_WANS; i++)
		if (!mUnresolvedWAN[i]) {
			mUnresolvedWAN[i] = dynHostname;
			mUnresolvedWAN[i+1] = NULL;
			return;
		}
}

void BolbroClass::addWANGateway(uint8_t firstOctet, uint8_t secondOctet, uint8_t thirdOctet, uint8_t fourthOctet) {
	for (int i = 0; i<MAX_WANS; i++)
		if (mWANs[i]==IPAddress()) {
			mWANs[i] = IPAddress(firstOctet, secondOctet, thirdOctet, fourthOctet);
			mWANs[i+1] = IPAddress();
			LOG->printf("added WAN gateway: %d.%d.%d.%d\n", firstOctet, secondOctet, thirdOctet, fourthOctet);
			return;
		}
}

//  check if ipAddr is considered a local access
bool BolbroClass::localAccess(const IPAddress &ipAddr) {

	bool result = false;

	//  check well known local address ranges
	if (ipAddr[0]==10)
		result = true;
	else if (ipAddr[0]==172 && ipAddr[1]>=16 && ipAddr[1]<32)
		result = true;
	else if (ipAddr[0]==192 && ipAddr[1]==168)
		result = true;

	if (!result) {
		//  resolve the unresolved...
		int i = 0;

		while (mUnresolvedWAN[i]) {

			IPAddress resolvedAddress;

			if (WiFi.status() == WL_CONNECTED && WiFi.hostByName(mUnresolvedWAN[i], resolvedAddress)) {
				LOG->printf("lookup to %s successful: %d.%d.%d.%d\n",
					mUnresolvedWAN[i],
					resolvedAddress[0], resolvedAddress[1], resolvedAddress[2], resolvedAddress[3]);
				addWANGateway(resolvedAddress[0], resolvedAddress[1], resolvedAddress[2], resolvedAddress[3]);

				//	remove unresolved entry...
				int j = i;
				while (mUnresolvedWAN[j]) {
					mUnresolvedWAN[j] = mUnresolvedWAN[j+1];
					j++;
				}
				//	i not incremented
			} else
				i++; // not successful for i, keep it
		}
	}

	if (!result)
		for (int i = 0; i<MAX_WANS; i++) {
			if (mWANs[i]==IPAddress())
				break;
			if (ipAddr==mWANs[i]) {
				result = true;
				break;
			}
		}

	if (result)
		LOG->printf("IP address %d.%d.%d.%d is considered local access.\n", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
	else
		LOG->printf("IP address %d.%d.%d.%d is considered external access.\n", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);

	return result;
}

void BolbroClass::publishDNSName() {

	if (mAppName) {
		char lowerAppName[128];

		strcpy(lowerAppName, mAppNameLowerCase);

		if (mDebug)
			strcat(lowerAppName, "debug");

		if (MDNS.begin(lowerAppName))
			Serial.printf("MDNS responder started: '%s.local'\n", lowerAppName);
	}
}

/* --------------------------------------------------------------------------------
	Preference handling
   -------------------------------------------------------------------------------- */

void BolbroClass::prefSetInt(const char *key, int value) {
	preferences.begin(mAppNameLowerCase);
	if (!preferences.putInt(key, value))
		LOG->printf("preference '%s/%s' write failed!\n", mAppNameLowerCase, key);
	else
		LOG->printf("preference '%s/%s' written: %d\n", mAppNameLowerCase, key, value);
	preferences.end();
}

int BolbroClass::prefGetInt(const char *key, int defaultValue) {
	preferences.begin(mAppNameLowerCase);
	int result = preferences.getInt(key,defaultValue);
	preferences.end();
	LOG->printf("preference '%s/%s' read: %d\n", mAppNameLowerCase, key, result);

	return result;
}

void BolbroClass::prefSetUnsignedLong(const char *key, unsigned long value) {
	preferences.begin(mAppNameLowerCase);
	if (!preferences.putULong(key, value))
		LOG->printf("preference '%s/%s' write failed!\n", mAppNameLowerCase, key);
	else
		LOG->printf("preference '%s/%s' written: %lu\n", mAppNameLowerCase, key, value);
	preferences.end();
}

unsigned long BolbroClass::prefGetUnsignedLong(const char *key, unsigned long defaultValue) {
	preferences.begin(mAppNameLowerCase);
	unsigned long result = preferences.getULong(key,defaultValue);
	preferences.end();
	LOG->printf("preference '%s/%s' read: %lu\n", mAppNameLowerCase, key, result);

	return result;
}

void BolbroClass::prefSetFloat(const char *key, float value) {
	preferences.begin(mAppNameLowerCase);
	if (!preferences.putFloat(key, value))
		LOG->printf("preference '%s/%s' write failed!\n", mAppNameLowerCase, key);
	else
		LOG->printf("preference '%s/%s' written: %f\n", mAppNameLowerCase, key, value);
	preferences.end();
}

float BolbroClass::prefGetFloat(const char *key, float defaultValue) {
	preferences.begin(mAppNameLowerCase);
	float result = preferences.getFloat(key,defaultValue);
	preferences.end();
	LOG->printf("preference '%s/%s' read: %f\n", mAppNameLowerCase, key, result);

	return result;
}

void BolbroClass::prefSetString(const char *key, const String &value) {
	preferences.begin(mAppNameLowerCase);
	if (!preferences.putString(key, value))
		LOG->printf("preference '%s/%s' write failed!\n", mAppNameLowerCase, key);
	else
		LOG->printf("preference '%s/%s' written: %s\n", mAppNameLowerCase, key, value.c_str());
	preferences.end();
}

String BolbroClass::prefGetString(const char *key, String defaultValue) {
	preferences.begin(mAppNameLowerCase);
	String result = preferences.getString(key,defaultValue);
	preferences.end();
	LOG->printf("preference '%s/%s' read: %s\n", mAppNameLowerCase, key, result.c_str());

	return result;
}






