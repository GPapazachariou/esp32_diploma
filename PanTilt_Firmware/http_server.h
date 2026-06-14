#include "web_page.h"

WebServer server(80);

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void webCtrlServer() {
  server.on("/", handleRoot);

  server.on("/js", []() {
    String jsonCmdWebString = server.arg(0);
    deserializeJson(jsonCmdReceive, jsonCmdWebString);
    jsonCmdReceiveHandler();
    serializeJson(jsonInfoHttp, jsonFeedbackWeb);
    server.send(200, "text/plain", jsonFeedbackWeb);  // Bug Fix 10: was "text/plane"
    jsonFeedbackWeb = "";
    jsonInfoHttp.clear();
    jsonCmdReceive.clear();
  });

  server.begin();
  Serial.println("Server Starts.");
}

void initHttpWebServer() {
  webCtrlServer();
}
