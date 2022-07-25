#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

namespace am0r
{
  namespace web
  {
    AsyncWebServer server(80);

    typedef void (*routeCallbackFunction)(AsyncWebServerRequest*);
    typedef void (*voidcb)(void);

    voidcb updated_cb = NULL;

    void handleRoot(AsyncWebServerRequest* request)
    {
      request->send(LittleFS, "/index.html");
    }

    void fs_status(AsyncWebServerRequest* request)
    {
      fs::FSInfo info;
      LittleFS.info(info);
      String body;
      body = "total: " + String(info.totalBytes);
      body += " used: " + String(info.usedBytes);

      request->send(200, "plain/text", body);
    }

    void image_get(AsyncWebServerRequest* request)
    {
      request->send(LittleFS, "/image.raw");
    }

    void image_upload_req(AsyncWebServerRequest* request)
    {
      request->send(200);
      
    }    

    void image_upload(AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      if(index == 0)
      {
        request->_tempFile = LittleFS.open("/image.raw", "w");
        if(!request->_tempFile)
        {
          request->send(500, "plain/text", "Failed to open file.");
          return;
        }
      }

      request->_tempFile.seek(index);
      size_t written = request->_tempFile.write(data, len);
      if(written != len)
      {
        request->send(500, "plain/text", "Write error.");
        return;
      }

      if (final)
      {
        request->_tempFile.close();
        if(updated_cb) updated_cb();
      }
    }

    void add_route(const char* route, routeCallbackFunction callback)
    {
      server.on(route, callback);
    }

    void add_updated_cb(voidcb callback)
    {
      updated_cb = callback;
    }

    void setup()
    {
      LittleFS.begin();
      server.on("/", HTTP_GET, handleRoot);
      server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest* request){
         request->send(LittleFS, "/main.js");
      });
      server.on("/image.raw", HTTP_GET, image_get);
      server.on("/image.raw", HTTP_POST, image_upload_req, image_upload);
      server.on("/fs_status", HTTP_GET, fs_status);
      server.begin();
    }
  }
}