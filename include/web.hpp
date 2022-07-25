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

    String processor(const String& var)
    {
      fs::FSInfo info;
      LittleFS.info(info);
      if(var == "TOTAL_SIZE")
        return String(info.totalBytes / 1024);
      else if(var == "ALLOCATED_SIZE")
        return String(info.usedBytes / 1024);
      else
        return String();
    }

    void setup()
    {
      LittleFS.begin();
      server.on("/", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        request->send(LittleFS, "/index.html", String(), false, processor);
      });
      server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/main.js.gz", "text/javascript", false, nullptr);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      });
      server.on("/image.raw", HTTP_GET, [](AsyncWebServerRequest* request)
      {      
        request->send(LittleFS, "/image.raw", "application/octet-stream");
      });
      server.on("/image.raw", HTTP_POST, image_upload_req, image_upload);
      server.begin();
    }
  }
}