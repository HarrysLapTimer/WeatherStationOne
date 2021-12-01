/* --------------------------------------------------------------------------------
	BolbroWebServer library
	Base class for Bolbro web servers on ESP32
	Harald Schlangmann, March 2021
   -------------------------------------------------------------------------------- */

#ifndef BolbroWebServer_h
#define BolbroWebServer_h

#include <Arduino.h>

#ifdef ESP_PLATFORM // ESP32
#	define HASSPIFFS 1
#	include <WebServer.h>
#else
#	define HASSPIFFS 0
#	include <ESP8266WebServer.h>
#	define WebServer ESP8266WebServer
#endif

class BolbroWebServer : public WebServer
{
  public:

    BolbroWebServer();

    virtual void begin();

  protected:

#if HASSPIFFS
    //  support to return files stored in SPIFFS
    void returnFile(const char *path, const char *mimeType,
      int numReplacements = 0, const char **subStrings = NULL, const char **replacements = NULL);
    bool loadFromSpiffs(String path);
#endif

    //  debug support
    String messageToString(String linePrefix = "");

    //  standard reply
#define CHECKLOCALACCESS if ((/*LOG->println(messageToString()),*/ Bolbro.localAccess(client().remoteIP()))) // for server pages to provide to local users only
    void handleNotFound();

    // set Germany during summer: http://weatherbase.local/time?GMTOffset=3600&DaylightOffset=3600
	void handleTime();

	//	restart ESP
	void handleRestart();

	//	reconnect WiFi
	void handleReconnect();
};

#endif
