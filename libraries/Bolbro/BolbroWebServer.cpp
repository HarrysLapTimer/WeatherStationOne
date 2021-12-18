#include <BolbroWebServer.h>
#include <Bolbro.h>

#if HASSPIFFS
#	include <SPIFFS.h> // for access to image data
#endif

/* --------------------------------------------------------------------------------
	WebServer base class to be customized
   -------------------------------------------------------------------------------- */

BolbroWebServer::BolbroWebServer() : WebServer(80) {
}

void BolbroWebServer::begin() {

  on("/restart", [this]() { CHECKLOCALACCESS handleRestart(); });
  on("/reconnect", [this]() { CHECKLOCALACCESS handleReconnect(); });
  on("/time", [this]() { CHECKLOCALACCESS handleTime(); });
  onNotFound([this]() { CHECKLOCALACCESS handleNotFound(); });

  WebServer::begin();

#if HASSPIFFS
  //  Allow access to files
  if (SPIFFS.begin())
    LOG->println("SPIFFS mount succeeded");
#endif
}

#if HASSPIFFS

//  read a file from SPIFFS and optionally replace substrings
void BolbroWebServer::returnFile(const char *path, const char *mimeType,
  int numReplacements, const char **subStrings, const char **replacements)
{
  File file = SPIFFS.open(path);
  if (!file)
  {
    LOG->printf("failed to open file %s for reading\n", path);
    handleNotFound();
  }
  else
  {
    String out = "";

    while(file.available())
      out += file.readString();
    file.close();

    for (int i = 0; i<numReplacements; i++)
      out.replace(subStrings[i], replacements[i]);

    send(200, mimeType, out);

    LOG->printf("file %s read, patched, and sent\n", path);
  }
}

bool BolbroWebServer::loadFromSpiffs(String path)
{
  String dataType = "text/plain";
  bool result = true;

  if (path.endsWith(".src"))
    path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html"))
    dataType = "text/html";
  else if(path.endsWith(".htm"))
    dataType = "text/html";
  else if(path.endsWith(".css"))
    dataType = "text/css";
  else if(path.endsWith(".js"))
    dataType = "application/javascript";
  else if(path.endsWith(".png"))
    dataType = "image/png";
  else if(path.endsWith(".gif"))
    dataType = "image/gif";
  else if(path.endsWith(".jpg"))
    dataType = "image/jpeg";
  else if(path.endsWith(".ico"))
    dataType = "image/x-icon";
  else if(path.endsWith(".xml"))
    dataType = "text/xml";
  else if(path.endsWith(".pdf"))
    dataType = "application/pdf";
  else if(path.endsWith(".zip"))
    dataType = "application/zip";

  if (SPIFFS.exists(path.c_str())) {
    File dataFile = SPIFFS.open(path.c_str(), "r");

    if (hasArg("download"))
      dataType = "application/octet-stream";

    sendHeader("Cache-Control", "max-age=1000");
    setContentLength(dataFile.size());

    result = streamFile(dataFile, dataType) == dataFile.size();

    if (result)
      LOG->printf("file %s read and sent\n", path.c_str());
    else
      LOG->printf("sending file %s failed...\n", path.c_str());

    dataFile.close();
  } else {
      LOG->printf("file %s does not exist...\n", path.c_str());
      result = false;
  }

  return result;
}

#endif

String BolbroWebServer::messageToString(String linePrefix) {

  String message = linePrefix + "URI: ";

  message += uri();

  message += "\n";
  message += linePrefix + "Method: ";
  message += (method() == HTTP_GET) ? "GET" : "POST";
  message += "\n";

  message += linePrefix + "Arguments: ";
  message += args();
  message += "\n";
  for (uint8_t i = 0; i < args(); i++) {
    message += linePrefix + " " + argName(i) + ": " + arg(i) + "\n";
  }
  message += linePrefix + "Headers: ";
  message += headers();
  message += "\n";
  for (uint8_t i = 0; i < headers(); i++) {
    message += linePrefix + " " + headerName(i) + ": " + header(i) + "\n";
  }

  message += linePrefix + "Host Header: ";
  message += hostHeader();

  return message;
}

void BolbroWebServer::handleNotFound()  {

  String message = "File Not Found\n";

  message += messageToString();

  String lineMessage = message;

  lineMessage.replace("\n", ", ");
  LOG->println("handling not found: " + lineMessage);
  send(404, "text/plain", message);
}

void BolbroWebServer::handleTime() {
  bool requestValid = args()==2
          &&((argName(0).equalsIgnoreCase("GMTOffset")
              &&argName(1).equalsIgnoreCase("DaylightOffset"))
            ||(argName(0).equalsIgnoreCase("DaylightOffset")
              &&argName(1).equalsIgnoreCase("GMTOffset")));

  if (requestValid) {
    int gmtOffset_sec = 0, daylightOffset_sec = 0;
    for (uint8_t i = 0; i < args(); i++)
      if (argName(i).equalsIgnoreCase("GMTOffset"))
        gmtOffset_sec = arg(i).toInt();
      else
        daylightOffset_sec = arg(i).toInt();

    Bolbro.setTimezone(gmtOffset_sec, daylightOffset_sec);
  }

  if (requestValid)
    send(200, "text/plain", "OK");
  else
    send(404, "text/plain", "invalid arguments");
}

void BolbroWebServer::handleRestart() {
    send(200, "text/plain", "OK");
    delay(500); // make sure the reply is sent

	ESP.restart();
}

void BolbroWebServer::handleReconnect() {
    send(200, "text/plain", "OK");
    delay(500); // make sure the reply is sent

    Bolbro.disconnectWiFi();
    delay(1000);
    Bolbro.connectToWiFi(); // trigger direct re-connect
}







