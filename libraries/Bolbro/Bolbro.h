/* --------------------------------------------------------------------------------
	Bolbro library
	Common utilities for Home Automation gadgets based on ESP32
	Harald Schlangmann, June 2020

	prepared for
		ESP32 Dev Model
		LOLIN (WEMOS) D1 R2 & mini

   -------------------------------------------------------------------------------- */

#ifndef Bolbro_h
#define Bolbro_h

#include <Arduino.h>

#ifdef ESP_PLATFORM
#	define HASPREFERENCES 1
#	define HASREMOTEDEBUG 1
#	include <WiFi.h>
#else
#	define HASPREFERENCES 0
#	define HASREMOTEDEBUG 0
#	include <ESP8266WiFi.h>
#endif

#if HASPREFERENCES
#	include <Preferences.h>
#endif

#define STRINGNOTINITIALIZED  "-"

class BolbroClass
{
	public:

		BolbroClass();

		//	BolbroClass needs to be configured before calling Bolbro::setup() from from setup()
		void addWiFi(const char *ssid, const char *password); // call up to MAX_NETWORKS times, best will be selected
		void setSerialBaud(long bauRate); // for Serial monitor, default is 115200
		void setOpenHABHost(const char *hostname); // call in case openHAB communication is required

		//	utility to limit access to BolbroWebServer to local, or local initiated requests; see
		//	CHECKLOCALACCESS in class BolbroWebServer
		void addWANGateway(const char *dynHostname);
		void addWANGateway(uint8_t firstOctet, uint8_t secondOctet, uint8_t thirdOctet, uint8_t fourthOctet);

		//	Start everything, includes Serial configuration
		void setup(const char *appname, // app name used for reporting, RemoteDebug and DNS
			boolean debug = false, // turn additional debugging output on
			boolean useRemoteDebug = false); // switch between Serial and RemoteDebug interface

		unsigned long uptime(); // returns number of seconds since start of program (call to setup())
		String startTime(bool humanReadable = true);

		//	call from loop()
		void loop();

		//	in case program wants to use time_t functions, this function prepares it by pulling
		//	the current time from a NTP server, setTimezone allows to set a zone other than
		//	the default (Germany)
		void configureTime();
		void setTimezone(int gmtOffset_sec, int daylightOffset_sec);
		bool timeConfigured();

		//	openHAB access

		//	generic access to openHAB items, use STRINGNOTINITIALIZED for undefined / NULL values
		String getItemStatus(const char *item);
		bool sendItemCommand(const char *item, const String& status);
		bool updateItem(const char *item, const String& status);

		//	Utility updating the signalStrength item (Number) whenever the WiFi RSSI changes
		void setSignalStrengthItem(const char *item);

		//	Utility to log start time to DateTime item
		void setLastStartItem(const char *item);

		//	Utility to format time in a way acceptable by openHAB
		String openHABTime(time_t utcTime);

		//	WiFi utilities, connectToWiFi() includes a flexible algorithm to select the best SSID available
		boolean connectToWiFi();
		void disconnectWiFi();

		//	Check if requester ip address given is considered a local access; this applies if either the
		//	IP is a well known local address range, or one of the WAN gateways used at Bolbro-Haus
		bool localAccess(const IPAddress &ipAddr);

#if HASPREFERENCES
		//	Preference handling
		void prefSetInt(const char *key, int value);
		int prefGetInt(const char *key, int defaultValue = 0);

		void prefSetUnsignedLong(const char *key, unsigned long value);
		unsigned long prefGetUnsignedLong(const char *key, unsigned long defaultValue = 0);

		void prefSetString(const char *key, const String &value);
		String prefGetString(const char *key, String defaultValue = String());

		void prefSetFloat(const char *key, float value);
		float prefGetFloat(const char *key, float defaultValue = 0.0f);
#endif

	private:

#if !HASPREFERENCES
		int mGMTOffsetSec;
		int mDaylightOffsetSec;
#endif

#define MAX_NETWORKS 10
		struct Network {
			const char *ssid;
			const char *password;
		} mNetworks [MAX_NETWORKS+1];

		const char *mOpenHABHost;
		long mSerialBaud;

#define MAX_UNRESOLVED_WANS 10
#define MAX_WANS 10
		const char *mUnresolvedWAN[MAX_UNRESOLVED_WANS+1];
		IPAddress mWANs[MAX_WANS+1];

		const char *mAppName;
		char *mAppNameLowerCase;
		boolean mDebug; // generates extra debug prints

		enum {
			ConfigureTimeNotRequested,
			ConfigureTimeRequested,
			ConfigureTimeSucceeded
		} mConfigureTimeStatus;
		time_t mStartSeconds;

		time_t mLastBedtimeCheck;
		boolean mLastIsBedtime;
		boolean mLastIsMorningTime;

		void updateIsBedtime();
		void publishDNSName();

		void onWiFiEvent(WiFiEvent_t event);

		friend void onWiFiEventFctn(WiFiEvent_t event);

		const char *mSignalStrengthItem;
		int mLastSignalStrength;
		time_t mLastSetSignalStrength;
		const char *mLastStartItem;
		int mLastStartItemRetryCount;
		void updateOHItems();

#if HASPREFERENCES
		Preferences preferences;
#endif
};

extern BolbroClass Bolbro;
extern Print *LOG;

#endif

