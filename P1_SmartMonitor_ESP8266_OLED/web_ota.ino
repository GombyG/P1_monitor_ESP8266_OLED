// ================================================================================
//  WEBES OTA FRISSÍTŐ FELÜLET (web_ota.ino) - UTF-8 KÓDOLÁSSAL
// ================================================================================

void setupWebOTA() {
  
  // 1. A WEBOLDAL MEGJELENÍTÉSE (GET)
  server.on("/update", HTTP_GET, []() {
    if (!server.authenticate(savedOtaUser.c_str(), savedOtaPass.c_str())) {
    return server.requestAuthentication();
  }

    String html = R"====(
      <!DOCTYPE html>
      <html lang='hu'>
      <head>
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width, initial-scale=1.0'>
        <title>P1 Monitor - Firmware Frissítés</title>
        <style>
          body { font-family: sans-serif; background: #121212; color: #e0e0e0; margin: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
          .card { background: #1e1e1e; padding: 25px; border-radius: 12px; border: 1px solid #333; box-shadow: 0 4px 20px rgba(0,0,0,0.5); width: 60%; max-width: 400px; text-align: center; }
          h2 { color: #00e676; margin-top: 0; font-size: 18px; }
          p { font-size: 12px; color: #aaa; margin-bottom: 18px; }
          .file-input-wrapper { margin-bottom: 20px; }
          input[type=file] { display: none; }
          .file-label { display: block; background: #2a2a2a; color: #00e676; border: 1px dashed #00e676; padding: 12px; border-radius: 6px; cursor: pointer; transition: 0.2s; font-weight: bold; }
          .file-label:hover { background: #333; }
          .btn-submit { background: #00e676; color: #121212; border: none; padding: 12px 20px; border-radius: 6px; font-weight: bold; font-size: 16px; cursor: pointer; width: 100%; transition: 0.2s; }
          .btn-submit:hover { background: #00c853; }
          .btn-back { display: inline-block; margin-top: 15px; color: #aaa; text-decoration: none; font-size: 13px; }
          .btn-back:hover { color: #fff; }
        </style>
      </head>
      <body>
        <div class='card'>
          <h2>⚡ Firmware Frissítés</h2>
          <p>Töltsd fel az Arduino IDE által generált .bin fájlt</p>
          <form method='POST' action='/update' enctype='multipart/form-data'>
            <div class='file-input-wrapper'>
              <label for='firmware' class='file-label' id='file-label-text'>📁 .bin fájl kiválasztása</label>
              <input type='file' name='update' id='firmware' accept='.bin' required onchange='document.getElementById("file-label-text").innerText = this.files[0].name;'>
            </div>
            <input type='submit' class='btn-submit' value='Frissítés Indítása'>
          </form>
          <a href='/' class='btn-back'>← Vissza a főoldalra</a>
        </div>
      </body>
      </html>
    )====";
    
    // FONTOS: "text/html; charset=utf-8" beállítása a Header-ben
    server.send(200, "text/html; charset=utf-8", html);
  });

  // 2. A FÁJL FOGADÁSA ÉS BEÍRÁSA A FLASH-BE (POST)
  server.on("/update", HTTP_POST, []() {
    if (!server.authenticate("admin", OTA_PASSWORD)) {
      return server.requestAuthentication();
    }
    
    bool error = Update.hasError();
    String statusMsg = error ? "❌ Frissítési hiba történt!" : "⚡ SIKERES FRISSÍTÉS!<br>Az eszköz újraindul...";
    
    String html = "<!DOCTYPE html><html lang='hu'><head><meta charset='UTF-8'></head><body style='background:#121212;color:#e0e0e0;font-family:sans-serif;text-align:center;padding-top:50px;'>";
    html += "<h3>" + statusMsg + "</h3>";
    if (!error) html += "<p>Kérlek várj 10 másodpercet, majd nyisd meg újra a főoldalt.</p>";
    html += "<script>setTimeout(function(){ window.location.href='/'; }, 10000);</script>";
    html += "</body></html>";

    // ITT IS ITT VAN A "text/html; charset=utf-8"
    server.send(200, "text/html; charset=utf-8", html);
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      WiFiUDP::stopAll();
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & ~0xFFF;
      Update.begin(maxSketchSpace);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      Update.end(true);
    }
  });
}