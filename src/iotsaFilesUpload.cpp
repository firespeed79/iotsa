#include <ESP.h>
#include "FS.h"
#ifdef ESP32
#include <SPIFFS.h>
#endif
#include "iotsaFilesUpload.h"

void IotsaFilesUploadMod::setup() {
}


File _uploadFile;
bool _uploadOK;

void
IotsaFilesUploadMod::uploadHandler() {
  if (needsAuthentication()) return;
  HTTPUpload& upload = server.upload();
  _uploadOK = false;
  if(upload.status == UPLOAD_FILE_START){
    String _uploadfilename = "/data/" + upload.filename;
    if(SPIFFS.exists(_uploadfilename)) SPIFFS.remove(_uploadfilename);
    _uploadFile = SPIFFS.open(_uploadfilename, "w");
    //DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(_uploadFile) _uploadFile.write(upload.buf, upload.currentSize);
    //DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(_uploadFile) {
        _uploadFile.close();
        _uploadOK = true;
    }
    //DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void
IotsaFilesUploadMod::uploadOkHandler() {
  String message;
  if (_uploadOK) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(200, "text/plain", "FAIL");
  }
}

void IotsaFilesUploadMod::uploadFormHandler() {
  if (needsAuthentication()) return;
  String message = "<form method='POST' action='/upload' enctype='multipart/form-data'>Select file to upload:<input type='file' name='blob'><br>Filename:<input name='filename'><br><input type='submit' value='Update'></form>";
  server.send(200, "text/html", message);
}
void IotsaFilesUploadMod::serverSetup() {
  server.on("/upload", HTTP_POST, std::bind(&IotsaFilesUploadMod::uploadOkHandler, this), std::bind(&IotsaFilesUploadMod::uploadHandler, this));
  server.on("/upload", HTTP_GET, std::bind(&IotsaFilesUploadMod::uploadFormHandler, this));
}

String IotsaFilesUploadMod::info() {
  return "<p>See <a href=\"/upload\">/upload</a> for uploading new files.</p>";
}

void IotsaFilesUploadMod::loop() {
  
}
