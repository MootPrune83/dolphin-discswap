#include <M5Unified.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Library references:
//  - M5Unified by M5Stack / lovyan03: https://github.com/m5stack/M5Unified
//  - ESPAsyncWebServer by ESP32Async: https://github.com/ESP32Async/ESPAsyncWebServer
//  - TJpg_Decoder by Bodmer: https://github.com/Bodmer/TJpg_Decoder
//  - FreeRTOS kernel by Richard Barry: https://www.freertos.org/

// ——————— CONFIG ———————
static constexpr char WIFI_SSID[]   = "SSID";
static constexpr char WIFI_PASS[]   = "Pass";
static constexpr char BRIDGE_HOST[] = "192.168.0.0";  //set this to the IP of the device that is running bridge.py (aka the device that is running dolphin)
static constexpr int  BRIDGE_PORT   = 8000;
static constexpr char CONFIG_PATH[] = "/config.txt";

// Image layout
static constexpr int IMG_W   = 180;
static constexpr int IMG_H   = 242;  // 260 − 18px title bar
static constexpr int TOP_PAD = 18;   // title bar height
static constexpr int SPACING = IMG_W / 2;

// Timeouts
static constexpr unsigned SLEEP_TIMEOUT    = 30000;
static constexpr unsigned TITLEBAR_TIMEOUT = 3000;

struct Game { String name, img, path; };
static std::vector<Game> games;
static int currentIndex = 0;

AsyncWebServer server(80);
static unsigned long lastActivity     = 0;
static unsigned long titlebarUntil    = 0;
static bool          titlebarVisible  = false;
static bool          sleeping         = false;
static bool          noSleep          = false;
// track center image position for redraw
static int lastCX = 0;
static int lastY0 = 0;

static const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Game Manager</title>
<style>body{font-family:sans-serif;padding:10px;}textarea{width:100%;height:120px;margin-bottom:8px;}ul{list-style:none;padding:0;}li{margin:6px 0;}button,label{margin-right:8px;}</style>
</head><body>
<h2>Config & Box Art Manager</h2>
<div><h3>config.txt</h3><textarea id="cfg"></textarea><br><button onclick="saveCfg()">Save Config</button></div>
<div><h3>Upload File</h3><input type="file" id="f"><button onclick="up()">Upload</button></div>
<div>
  <label><input type="checkbox" id="noSleep" onchange="toggleSleep()"> No Sleep</label>
</div>
<div><h3>Files</h3><ul id="fl"></ul></div>
<script>
let cfgEl, flEl, noSleepEl;
window.onload=()=>{
  cfgEl=document.getElementById('cfg');
  flEl=document.getElementById('fl');
  noSleepEl=document.getElementById('noSleep');
  loadCfg(); listFiles(); loadSleep();
};
function loadCfg(){ fetch('/config').then(r=>r.text()).then(t=>cfgEl.value=t); }
function saveCfg(){ fetch('/config',{method:'POST',body:cfgEl.value}).then(()=>listFiles()); }
function up(){ let f=document.getElementById('f'); if(!f.files.length) return; let fd=new FormData(); fd.append('file',f.files[0]); fetch('/upload',{method:'POST',body:fd}).then(()=>listFiles()); }
function listFiles(){ fetch('/list').then(r=>r.json()).then(js=>{ flEl.innerHTML=''; js.forEach(fn=>{ let li=document.createElement('li'); li.textContent=fn; let btn=document.createElement('button'); btn.textContent='Delete'; btn.onclick=()=>fetch('/delete?file='+encodeURIComponent(fn)).then(()=>listFiles()); li.appendChild(btn); flEl.appendChild(li); }); }); }
// Sleep toggle
function loadSleep(){
  fetch('/sleep').then(r=>r.json()).then(js=>{ noSleepEl.checked = js.noSleep; });
}
function toggleSleep(){
  fetch('/sleep?noSleep='+(noSleepEl.checked?'1':'0'));
}
</script>
</body></html>
)rawliteral";

bool jpgRender(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  M5.Display.pushImage(x, y, w, h, bitmap);
  return true;
}

void loadConfig(){
  games.clear();
  File f=SPIFFS.open(CONFIG_PATH,FILE_READ);
  if(!f) return;
  while(f.available()){
    String line=f.readStringUntil('\n'); line.trim();
    if(line.isEmpty()||line.startsWith("#")) continue;
    int p1=line.indexOf('|'), p2=line.indexOf('|',p1+1);
    if(p1<0||p2<0) continue;
    games.push_back({ line.substring(0,p1),
                      line.substring(p1+1,p2),
                      line.substring(p2+1) });
  }
  f.close();
}

void drawBoxArt(const String &fn,int x,int y,bool darken=false){
  String path="/"+fn;
  if(!SPIFFS.exists(path)) return;
  TJpgDec.setCallback(jpgRender);
  TJpgDec.drawFsJpg(x,y,path.c_str(),SPIFFS);
  if(darken){
    for(int yy=y;yy<y+IMG_H;yy++){
      for(int xx=x;xx<x+IMG_W;xx++){
        if((((xx-x)&3)+((yy-y)&3))<3)
          M5.Display.drawPixel(xx,yy,TFT_BLACK);
      }
    }
  }
}

struct DrawTaskArgs {
  const String*      img;
  int                x;
  int                y;
  bool               darken;
  SemaphoreHandle_t  done;
};

void drawBoxArtTask(void* p){
  DrawTaskArgs* a = (DrawTaskArgs*)p;
  drawBoxArt(*(a->img), a->x, a->y, a->darken);
  xSemaphoreGive(a->done);
  vTaskDelete(nullptr);
}

void renderMenu(){
  M5.Display.clear();
  if(games.empty()){
    M5.Display.setCursor(10,10);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.print("No games loaded");
    return;
  }
  int dispW=M5.Display.width();
  int dispH=M5.Display.height();
  int cx=(dispW-IMG_W)/2;
  int availH=dispH-TOP_PAD;
  int y0=TOP_PAD+(availH-IMG_H)/2-10;
  lastCX=cx; lastY0=y0;
  int left=(currentIndex-1+games.size())%games.size();
  int right=(currentIndex+1)%games.size();

  SemaphoreHandle_t semL = xSemaphoreCreateBinary();
  SemaphoreHandle_t semR = xSemaphoreCreateBinary();
  DrawTaskArgs leftArgs{&games[left].img,  cx-SPACING, y0, true, semL};
  DrawTaskArgs rightArgs{&games[right].img, cx+SPACING, y0, true, semR};

  xTaskCreatePinnedToCore(drawBoxArtTask, "drawL", 4096, &leftArgs, 1, nullptr, 0);
  xTaskCreatePinnedToCore(drawBoxArtTask, "drawR", 4096, &rightArgs, 1, nullptr, 1);

  xSemaphoreTake(semL, portMAX_DELAY);
  xSemaphoreTake(semR, portMAX_DELAY);
  vSemaphoreDelete(semL);
  vSemaphoreDelete(semR);

  drawBoxArt(games[currentIndex].img,cx,y0,false);

  // draw title bar
  M5.Display.fillRect(0,0,dispW,TOP_PAD,TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.drawString("30s",2,TOP_PAD/2);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.drawString(games[currentIndex].name,dispW/2,TOP_PAD/2);
  int pct = M5.Power.getBatteryLevel();  // updated
  M5.Display.setTextDatum(TR_DATUM);
  M5.Display.drawString(String(pct) + "%", dispW-2, TOP_PAD/2);

  titlebarUntil=millis()+TITLEBAR_TIMEOUT;
  titlebarVisible=true;
}

String urlEncode(const String &s){
  String e; char h[]="0123456789ABCDEF";
  for(char c:s){
    if(isalnum(c)||c=='/'||c=='.'||c=='_') e+=c;
    else{ e+='%', e+=h[(c>>4)&0xF], e+=h[c&0xF]; }
  }
  return e;
}

struct AudioTaskArgs {
  String             path;
  SemaphoreHandle_t  done;
};

void audioTask(void* p){
  AudioTaskArgs* a = (AudioTaskArgs*)p;
  File f = SPIFFS.open(a->path, FILE_READ);
  if(f){
    size_t len = f.size();
    uint8_t* buf = (uint8_t*)malloc(len);
    if(buf){
      f.read(buf, len);
      f.close();
      M5.Speaker.playWav(buf, len);
      while(M5.Speaker.isPlaying()) vTaskDelay(10);
      free(buf);
    }else{
      f.close();
    }
  }
  if(a->done) xSemaphoreGive(a->done);
  delete a;
  vTaskDelete(nullptr);
}

void playAudio(const char* path, bool wait=false){
  SemaphoreHandle_t done = wait ? xSemaphoreCreateBinary() : nullptr;
  AudioTaskArgs* args = new AudioTaskArgs{String(path), done};
  xTaskCreatePinnedToCore(audioTask, "aud", 4096, args, 1, nullptr, 1);
  if(wait){
    xSemaphoreTake(done, portMAX_DELAY);
    vSemaphoreDelete(done);
  }
}

void nextTitle(){ if(!games.empty()){ currentIndex=(currentIndex+1)%games.size(); renderMenu(); lastActivity=millis(); }}
void prevTitle(){ if(!games.empty()){ currentIndex=(currentIndex-1+games.size())%games.size(); renderMenu(); lastActivity=millis(); }}

void sendSelect(){
  if(games.empty()) return;
  lastActivity=millis();
  playAudio("/sound1.wav", false);
  String url=String("http://")+BRIDGE_HOST+":"+BRIDGE_PORT+
             "/swap?path="+urlEncode(games[currentIndex].path);
  HTTPClient http; http.begin(url); http.GET(); http.end();
}

String listFilesJson(){
  String j="[";
  File root=SPIFFS.open("/");
  File f=root.openNextFile();
  bool first=true;
  while(f){
    String n=f.name(); if(n.startsWith("/")) n=n.substring(1);
    if(!first) j+=',';
    j+="\""+n+"\""; first=false;
    f=root.openNextFile();
  }
  return j+"]";
}

void controllerLoop(void*){
  for(;;){
    M5.update();
    if(!sleeping){
      if(M5.BtnA.wasPressed()) prevTitle();
      if(M5.BtnC.wasPressed()) nextTitle();
      if(M5.BtnB.wasPressed()) sendSelect();

      unsigned long now=millis();
      if(titlebarVisible && now>titlebarUntil){
        M5.Display.fillRect(0,0,M5.Display.width(),TOP_PAD,TFT_BLACK);
        titlebarVisible=false;
        drawBoxArt(games[currentIndex].img,lastCX,lastY0,false);
      }
      if(!noSleep && now-lastActivity>SLEEP_TIMEOUT){
        sleeping=true;
        M5.Display.setBrightness(0);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println("Entering soft sleep...");
      }
    } else {
      if(M5.BtnA.wasPressed()||M5.BtnB.wasPressed()||M5.BtnC.wasPressed()){
        ESP.restart();
      }
    }
    vTaskDelay(10/portTICK_PERIOD_MS);
  }
}

void setupWebServer(){
  server.on("/",HTTP_GET,[](auto*req){req->send_P(200,"text/html",index_html);});

  server.on("/config",HTTP_GET,[](auto*req){
    File f=SPIFFS.open(CONFIG_PATH,FILE_READ);
    String t=f?f.readString():String(); if(f)f.close();
    req->send(200,"text/plain",t);
  });
  server.on("/config",HTTP_POST,
    [](auto*req){loadConfig();renderMenu();req->send(200,"text/plain","OK");},
    nullptr,
    [](auto*req,uint8_t*d,size_t l,size_t idx,size_t tot){
      if(idx==0) SPIFFS.remove(CONFIG_PATH);
      File f=SPIFFS.open(CONFIG_PATH,FILE_APPEND);
      if(f){f.write(d,l);f.close();}
    }
  );

  server.on("/sleep", HTTP_GET, [](auto* req){
    if(req->hasParam("noSleep")){
      noSleep = req->getParam("noSleep")->value() == "1";
    }
    req->send(200,"application/json", String("{\"noSleep\":") + (noSleep?"true":"false") + "}");
  });

  server.on("/list",HTTP_GET,[](auto*req){req->send(200,"application/json",listFilesJson());});
  server.on("/upload",HTTP_POST,
    [](auto*req){req->send(200);},
    [](auto*req,String fn,size_t idx,uint8_t*d,size_t l,bool fin){
      static File uf;
      if(idx==0) uf=SPIFFS.open("/"+fn,FILE_WRITE);
      uf.write(d,l);
      if(fin) uf.close();
    }
  );
  server.on("/delete",HTTP_GET,[](auto*req){
    if(req->hasParam("file")){
      String fn="/"+req->getParam("file")->value();
      SPIFFS.remove(fn);
      loadConfig();renderMenu();
      req->send(200,"text/plain","OK");
    } else req->send(400);
  });
  server.begin();
}

void setup(){
  auto cfg=M5.config();cfg.serial_baudrate=115200;
  M5.begin(cfg);Serial.begin(115200);
  M5.Display.setRotation(1);
  SPIFFS.begin(true);
  TJpgDec.setSwapBytes(true);
  M5.Speaker.begin();

  WiFi.begin(WIFI_SSID,WIFI_PASS);
  M5.Display.clear();M5.Display.setCursor(0,0);M5.Display.print("Wi-Fi...");
  while(WiFi.status()!=WL_CONNECTED){delay(200);M5.Display.print('.');}
  M5.Display.println(" OK");

  setupWebServer();
  loadConfig();
  renderMenu();

  playAudio("/sound2.wav", true);

  lastActivity=millis();

  xTaskCreatePinnedToCore(controllerLoop, "ctrl", 4096, nullptr, 1, nullptr, 0);
}

void loop(){
  vTaskDelay(portMAX_DELAY);
}
