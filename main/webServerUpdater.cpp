#pragma once
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <FS.h>
#include <SPIFFS.h>
#include <functional>
//#include <Arduino_JSON.h>

#define U_PART U_SPIFFS

class WebServerUpdater {
public:
  WebServerUpdater(const char* hostSsid, const char* pass, const char* vers) {
    m_hostSsid = hostSsid;
    m_pass = pass;
    m_vers = vers;
  }

  template<typename F>
  void setDataReceiveCallBack(F f) {
    using namespace std::placeholders;
    m_datafunc = std::bind(f, _1, _2);
  }
  
  String replacer(String in) {
    in.replace("<deviceName>", m_hostSsid.c_str());
    in.replace("<vers>", m_vers.c_str());
    in.replace("<sn>", efuseMacStr.c_str());
    return in;
  }

  void init(HardwareSerial *dbg = nullptr) {
    if (server) {
      return; // если уже инициировали сервер, то все остальное тоже настроено и работает.
    }
    dbgSerial = dbg;
    SPIFFS.begin(true);
    //char efuseMac[12]; // snprintf(efuseMac, 12, "%llX", ESP.getEfuseMac());
    efuseMacStr = WiFi.macAddress();
    efuseMacStr.replace(":", "");
    m_softApSsid = String(m_hostSsid) + "-" + efuseMacStr;

    if (!WiFi.softAP(m_softApSsid, m_pass)) {
      if (dbgSerial) dbgSerial->println("Soft AP creation failed."); // IPAddress myIP = WiFi.softAPIP();
      while (1) {
        delay(1000);
      }
    }
    if (dbgSerial) dbgSerial->printf("IPAddresss: %s\n", WiFi.softAPIP().toString().c_str());
    if (!MDNS.begin(m_hostSsid)) {
      if (dbgSerial) dbgSerial->println("Error setting up MDNS responder!");
      while (1) {
        delay(1000);
      }
    }
    MDNS.addService("http", "tcp", 80);
    server = new AsyncWebServer(80);
    if (dbgSerial) dbgSerial->println("mDNS responder started");


    server->on("/", HTTP_GET, [&](AsyncWebServerRequest *request) { // [&] - если понадобится, что бы лямбда видела переменные "снаружи"
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", replacer(loginIndex));
      response->addHeader("Connection", "close");
      request->send(response);
    });

    server->on("/serverIndex", HTTP_GET, [&](AsyncWebServerRequest *request) {
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", replacer(serverIndex));
      response->addHeader("Connection", "close");
      request->send(response);
    });

    server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      response->addHeader("Connection", "close");
      request->send(response);
      //ESP.restart();
    }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if(!index) {
        if (dbgSerial) dbgSerial->printf("UploadStart: %s\n", filename.c_str());
        content_len = request->contentLength();
        int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
          if (dbgSerial) Update.printError(*dbgSerial);
        }
      }
      if (Update.write(data, len) != len) {
        if (dbgSerial) Update.printError(*dbgSerial);
      }
      if (final) {
        if (dbgSerial) dbgSerial->printf("UploadEnd: %s, %u B\n", filename.c_str(), index+len);
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
        response->addHeader("Refresh", "20");  
        response->addHeader("Location", "/");
        request->send(response);
        if (!Update.end(true)){
          if (dbgSerial) Update.printError(*dbgSerial);
        } else {
          if (dbgSerial) dbgSerial->printf("Update complete\n");
          if (dbgSerial) dbgSerial->flush();
          ESP.restart();
        }
      }
    });

    server->onNotFound([&](AsyncWebServerRequest *request) {
      request->send(404, "text/plain", "Page is Not found");
      if (dbgSerial) dbgSerial->printf("Page is Not found\n");
    });
    dnsServer.start(53, "*", WiFi.softAPIP());
    server->begin();
    Update.onProgress([&](size_t prg, size_t sz) {
      if (dbgSerial) dbgSerial->printf("Progress: %d%%\n", (prg*100)/content_len);
    });
  }

  String getSoftApSsid() {
    return m_softApSsid;
  }

  void deinit() {
    if (!server)
      return;
    server->end();
    server = nullptr;
    MDNS.end();
    SPIFFS.end();
    dbgSerial = nullptr;
  }

  void poll() {
    dnsServer.processNextRequest();
  }

private:
  std::function<void(String&,String)> m_datafunc = NULL;
  HardwareSerial *dbgSerial = nullptr;
  AsyncWebServer *server = nullptr;
  DNSServer dnsServer;
  String m_hostSsid, m_pass, m_vers, m_softApSsid;
  size_t content_len;
  String loginIndexStr; // та же loginIndex, с добавлением мак-адреса
  String efuseMacStr;
  const char* loginIndex =
    "<form name='loginForm'>"
      "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b><deviceName></br>(<vers>, SN-<sn>)</br>Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
      "</table>"
    "</form>"
  "<script>"
    "function check(form) {"
      "if(form.userid.value=='admin' && form.pwd.value=='admin') {"
        "window.open('/serverIndex')"
      "}"
      "else {"
        "alert('Error Password or Username')/*displays error message*/"
      "}"
    "}"
  "</script>";

  const char* serverIndex =
    "<!DOCTYPE html><html><head><style>"
      "body {background-color: #ffffff;}"
	    "form {background-color: #00CED1;border: 1px solid #000000;border-radius: 10px;padding: 15px;max-width: 300px;margin: auto;}"
	    ".button-row {display: flex;justify-content: space-around;}"
	    "input[type='file'] {display: none;}"
	    "label[for='fileInput'] {background-color: #4CAF50;padding: 5px;text-align: center;display: inline-block;cursor: pointer;border-radius: 5px;"
        "content: url(\"data:image/svg+xml;charset=UTF-8,%3csvg width='24px' height='24px' viewBox='0 0 16 16' xmlns='http://www.w3.org/2000/svg'%3e%3cg fill='%232e3436'%3e%3cpath d='m 6 7 c 0.675781 0.023438 1.035156 0.695312 1.507812 1.09375 c 0.453126 0.484375 0.980469 0.910156 1.378907 1.441406 c "
        "0.308593 0.539063 -0.039063 1.117188 -0.46875 1.460938 c -0.625 0.605468 -1.214844 1.253906 -1.863281 1.835937 c -0.472657 0.277344 -1.03125 0.132813 -1.554688 0.167969 c 0 -2 0 -4 0 -6 z m 0 0'/%3e%3cpath d='m 5.019531 4 v -3 c 0 -0.550781 -0.445312 -1 -1 -1 c -0.550781 0 -1 0.449219 -1 1 v 3 c 0 0.550781 0.449219 1 1 1 c 0.554688 0 1 -0.449219 1 -1 z m -2.019531 3.003906 c 0.003906 -0.882812 -0.058594 -1.347656 0.230469 -1.632812 c 0.011719 -0.011719 0.023437 -0.023438 0.035156 "
        "-0.035156 c 0.007813 -0.011719 0.019531 -0.027344 0.027344 -0.039063 c 0.210937 -0.253906 0.648437 -0.332031 1.308593 -0.296875 h 0.023438 h 0.027344 c 2.511718 0.007812 5.019531 -0.015625 7.523437 0.011719 l -0.0625 -0.003907 c 0.535157 0.039063 0.960938 0.613282 0.894531 1.160157 c -0.003906 0.015625 -0.003906 0.03125 -0.007812 0.046875 v 0.066406 c "
        "-0.015625 1.34375 0.027344 2.660156 -0.023438 3.964844 l 0.007813 -0.082032 c -0.0625 0.511719 -0.621094 0.90625 -1.160156 0.84375 c -0.015625 -0.003906 -0.03125 -0.003906 -0.046875 -0.007812 h -0.777344 c -0.550781 0 -1 0.449219 -1 1 s 0.449219 1 1 1 h 0.707031 l -0.117187 -0.007812 c 1.574218 0.1875 3.175781 -0.945313 3.378906 -2.582032 c 0.003906 "
        "-0.015625 0.003906 -0.03125 0.003906 -0.046875 c 0.003906 -0.011719 0.003906 -0.023437 0.003906 -0.035156 c 0.050782 -1.355469 0.007813 -2.707031 0.023438 -4.023437 l -0.007812 0.109374 c 0.203124 -1.632812 -1.035157 -3.277343 -2.734376 -3.402343 c -0.011718 0 -0.023437 0 -0.035156 0 c -0.007812 0 -0.015625 0 -0.027344 0 c -2.515624 -0.023438 -5.03125 "
        "-0.003907 -7.539062 -0.011719 h 0.050781 c -0.855469 -0.042969 -2.125 0.019531 -2.953125 1.023438 l 0.066406 -0.070313 c -0.964843 0.957031 -0.816406 2.320313 -0.820312 3.042969 c 0 0.359375 0.1875 0.691406 0.496094 0.871094 c 0.308594 0.179687 0.691406 0.179687 1.003906 0.003906 c 0.308594 -0.179688 0.5 -0.511719 0.5 -0.867188 z m 6 -3.003906 v -3 c 0 "
        "-0.550781 -0.449219 -1 -1 -1 s -1 0.449219 -1 1 v 3 c 0 0.550781 0.449219 1 1 1 s 1 -0.449219 1 -1 z m 4 0 v -3 c 0 -0.550781 -0.449219 -1 -1 -1 s -1 0.449219 -1 1 v 3 c 0 0.550781 0.449219 1 1 1 s 1 -0.449219 1 -1 z m -7 5 h -1 c -2.214844 0 -4 1.785156 -4 4 v 2 c 0 0.550781 0.449219 1 1 1 s 1 -0.449219 1 -1 v -2 c 0 -1.097656 0.902344 -2 2 -2 h 1 c 0.550781 0 1 -0.449219 1 -1 s -0.449219 -1 -1 -1 z m 0 0'/%3e%3c/g%3e%3c/svg%3e\");}"
	    "input[type='submit'] {background-color: #4CAF50;padding: 10px;text-align: center;display: block;cursor: pointer;border-radius: 5px;border: none;}"
    "</style></head><body>"
      "<form method='POST' action='' enctype='multipart/form-data' id='upload_form'>"
	      "device: <b><deviceName></b></br>"
	      "sn: <b><sn></b></br>"
	      "version: <b><vers></b></br></br>"
	      "<input type='radio' id='loadTypeFirmware' name='loadType' value='Firmware' checked/>"
	      "<label for='loadTypeFirmware'>Firmware</label><br/>"
	      "<input type='radio' id='loadTypeSdCard' name='loadType' value='SdCard' />"
	      "<label for='loadTypeSdCard'>SdCard</label><br/>"
	      "<br/>"
	      "<div id='op'>operation: wait</div>"
	      "<div id='prg'>progress: 0%</div>"
	      "<div id='sta'></div>"
	      "<br/>"
	      "<div class='button-row'>"
		      "<div class='file-input-button'>"
			      "<input type='file' name='update' id='fileInput'>"
			      "<label for='fileInput'>Выбрать файл</label>"
		      "</div>"
		      "<div class='update-button'>"
			      "<input type='submit' value='Update'>"
		      "</div>"
	      "</div>"
      "</form>"
      "<script>"
        "const upload_form = document.getElementById('upload_form');"
        "upload_form.addEventListener('submit', function(e) {"
          "e.preventDefault();"
          "var value = document.querySelector('input[name=\"loadType\"]:checked').value;"
          "document.getElementById('op').innerHTML='operation: ' + value;"
          "document.getElementById('sta').innerHTML='status: progress...';"
          "httpRequest = new window.XMLHttpRequest();"
          "httpRequest.open('POST', '/update', true);"
          "httpRequest.upload.addEventListener('progress', function(evt) {"
            "if (evt.lengthComputable) {"
              "var per = evt.loaded / evt.total;"
              "document.getElementById('prg').innerHTML='progress: ' + Math.round(per*100) + '%';"
            "}"
          "});"
          "httpRequest.upload.addEventListener('loadend', function(evt) {"
            "if (evt.loaded !== 0) {"
              "document.getElementById('sta').innerHTML='status: load success';"
              "console.log('load success');"
            "} else {"
              "document.getElementById('sta').innerHTML='status: load error';"
              "console.log('load error');"
            "}"
          "});"
          "httpRequest.upload.addEventListener('error', function(evt) {"
            "document.getElementById('sta').innerHTML='status: error!';"
            "console.log('error!');"
          "});"
          "httpRequest.send(new FormData(upload_form));"
        "});"
      "</script>"
    "</body></html>";
};
