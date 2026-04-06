// ============================================================
//  Leviagotchi — Pwnagotchi-style WiFi Monitor + Portal
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "esp_wifi.h"
#include <set>
#include <map>
#include <string>

// ─── config ───────────────────────────────────────────────
#define PORTAL_SSID   "leviagotchi friendly"
// Fixed channel for EVERYTHING — AP + monitor both on same channel
// Change this to the channel your router uses for best results
#define FIXED_CHAN     6
#define EPOCH_MS       30000UL
#define BORED_EPOCHS   3
#define SLEEP_EPOCHS   8
// ──────────────────────────────────────────────────────────

#define W      320
#define H      240
#define BG     TFT_WHITE
#define FG     TFT_BLACK
#define BAR_H  18
#define DIV_X  160
#define FX     78
#define FY     135
#define EYE_R  11
#define ESEP   30
#define BH     22

TFT_eSPI  tft;
WebServer srv(80);
DNSServer dns;

enum Exp {
  E_AWAKE,E_HAPPY,E_EXCITED,E_GRATEFUL,
  E_SLEEP,E_SLEEP2,E_BORED,E_DEMOT,
  E_SAD,E_LONELY,E_INTENSE,E_LOOKL,E_LOOKR
};

const char* SMSG[] = {
  "Looking around...",  "I made a new friend!",
  "So many networks!!", "Friends nearby!",
  "Zzz...  sleeping",   "Zzz...  deep sleep",
  "Nothing to do...",   "This place is dead...",
  "I miss my friends",  "All alone out here...",
  "Intense mode!",      "Scanning left...",
  "Scanning right..."
};

struct AP { char ssid[33]; int8_t rssi; uint8_t ch; };
std::map<std::string,AP> apMap;
std::set<std::string>    clients;

uint32_t handshakes=0,totalAPs=0,totalClients=0;
uint32_t epochs=0,idleEpochs=0,epochAct=0,friends=0;
bool     gotHS=false;
Exp      curExp=E_AWAKE;
unsigned long lastEpoch=0,bootMs=0;

// ═══════════════ DRAWING ═════════════════════════════════

// Base eye: circle outline (thick) + filled centre dot = ( ● )
void eyeBase(int x,int y,int dotX,int dotY){
  // thick ring
  tft.drawCircle(x,y,EYE_R,FG);
  tft.drawCircle(x,y,EYE_R-1,FG);
  tft.drawCircle(x,y,EYE_R-2,FG);
  // filled dot in centre (or offset for look L/R)
  tft.fillCircle(dotX,dotY,4,FG);
}
void eyeNormal(int x,int y){ eyeBase(x,y,x,y); }
void eyeR(int x,int y)     { eyeBase(x,y,x+5,y); }
void eyeL(int x,int y)     { eyeBase(x,y,x-5,y); }
void eyeBig(int x,int y)   { tft.drawCircle(x,y,EYE_R+2,FG);tft.drawCircle(x,y,EYE_R+1,FG);tft.drawCircle(x,y,EYE_R,FG);tft.fillCircle(x,y,5,FG); }
void eyeWide(int x,int y)  { tft.drawCircle(x,y,EYE_R+3,FG);tft.drawCircle(x,y,EYE_R+2,FG);tft.drawCircle(x,y,EYE_R+1,FG);tft.fillCircle(x,y,4,FG); }
// Happy eye: ^ arc
void eyeHappy(int x,int y) { for(int i=-EYE_R+1;i<=EYE_R-1;i++){int j=-(int)(sqrt((float)(EYE_R*EYE_R-i*i))*0.65f);tft.fillRect(x+i-1,y+j-1,3,3,FG);} }
// Sleep: flat line
void eyeSleep(int x,int y) { tft.fillRect(x-EYE_R+2,y,EYE_R*2-4,3,FG); }
// Sleepy: ring with line through middle
void eyeSleepy(int x,int y){ tft.drawCircle(x,y,EYE_R,FG);tft.drawCircle(x,y,EYE_R-1,FG);tft.fillRect(x-EYE_R+1,y-1,EYE_R*2-2,3,FG); }
// Flat/bored: dash
void eyeFlat(int x,int y)  { tft.fillRect(x-EYE_R+2,y-1,EYE_R*2-4,4,FG); }
// Sad: normal ● eye + sad brow
void eyeSad(int x,int y)   { eyeBase(x,y,x,y);for(int i=0;i<8;i++)tft.fillRect(x-4+i,y-EYE_R-6+i/2,2,2,FG); }

void mSmile(int y){int m=y+14;tft.fillRect(FX-22,m,4,4,FG);tft.fillRect(FX-18,m+4,4,4,FG);tft.fillRect(FX-14,m+8,4,4,FG);tft.fillRect(FX-10,m+10,24,4,FG);tft.fillRect(FX+14,m+8,4,4,FG);tft.fillRect(FX+18,m+4,4,4,FG);tft.fillRect(FX+22,m,4,4,FG);}
void mFlat(int y){tft.fillRect(FX-14,y+20,28,4,FG);}
void mSleep(int y){int m=y+14;tft.fillRect(FX-14,m+8,28,3,FG);tft.fillRect(FX-16,m+6,4,3,FG);tft.fillRect(FX+12,m+6,4,3,FG);}
void mSad(int y){int m=y+14;tft.fillRect(FX-22,m+8,4,4,FG);tft.fillRect(FX-18,m+4,4,4,FG);tft.fillRect(FX-14,m,4,4,FG);tft.fillRect(FX-10,m-2,24,4,FG);tft.fillRect(FX+14,m,4,4,FG);tft.fillRect(FX+18,m+4,4,4,FG);tft.fillRect(FX+22,m+8,4,4,FG);}
void mOpen(int y){int m=y+14;tft.fillRect(FX-22,m,4,4,FG);tft.fillRect(FX-18,m+4,4,4,FG);tft.fillRect(FX-14,m+8,4,4,FG);tft.fillRect(FX-10,m+10,8,4,FG);tft.fillRect(FX+6,m+10,8,4,FG);tft.fillRect(FX+14,m+8,4,4,FG);tft.fillRect(FX+18,m+4,4,4,FG);tft.fillRect(FX+22,m,4,4,FG);}

void lv_drawFace(Exp e) {
  tft.fillRect(0,BAR_H+17,DIV_X-1,H-BAR_H*2-17,BG);
  int lx=FX-ESEP,rx=FX+ESEP,ey=FY;
  for(int t=-BH;t<=BH;t++){
    float n=(float)t/BH;int b=(int)(10.0f*(1.0f-n*n));
    int lbx=FX-ESEP-EYE_R-14,rbx=FX+ESEP+EYE_R+14;
    for(int k=0;k<4;k++){tft.drawPixel(lbx-b+k,FY-4+t,FG);tft.drawPixel(rbx+b-k,FY-4+t,FG);}
  }
  tft.fillRect(FX-2,FY+2,5,3,FG);
  switch(e){
    case E_AWAKE:   eyeNormal(lx,ey);eyeNormal(rx,ey);mSmile(ey);break;
    case E_HAPPY:   eyeHappy(lx,ey); eyeHappy(rx,ey); mSmile(ey);break;
    case E_EXCITED: eyeBig(lx,ey);   eyeBig(rx,ey);   mOpen(ey); break;
    case E_GRATEFUL:eyeHappy(lx,ey); eyeHappy(rx,ey); mOpen(ey); break;
    case E_SLEEP:   eyeSleep(lx,ey); eyeSleep(rx,ey); mSleep(ey);break;
    case E_SLEEP2:  eyeSleepy(lx,ey);eyeSleepy(rx,ey);mSleep(ey);break;
    case E_BORED:   eyeFlat(lx,ey);  eyeFlat(rx,ey);  mFlat(ey); break;
    case E_DEMOT:   eyeSleepy(lx,ey);eyeSleepy(rx,ey);mFlat(ey); break;
    case E_SAD:     eyeSad(lx,ey);   eyeSad(rx,ey);   mSad(ey);  break;
    case E_LONELY:  eyeSad(lx,ey);   eyeSad(rx,ey);   mFlat(ey); break;
    case E_INTENSE: eyeWide(lx,ey);  eyeWide(rx,ey);  mSmile(ey);break;
    case E_LOOKL:   eyeL(lx,ey);     eyeL(rx,ey);     mFlat(ey); break;
    case E_LOOKR:   eyeR(lx,ey);     eyeR(rx,ey);     mFlat(ey); break;
  }
}

void lv_status(const char* msg){
  tft.fillRect(DIV_X+1,BAR_H+4,W-DIV_X-1,14,BG);
  tft.setTextFont(1);tft.setTextSize(1);tft.setTextColor(FG,BG);
  tft.setCursor(DIV_X+5,BAR_H+5);tft.print(msg);
}

void lv_topBar(){
  unsigned long s=(millis()-bootMs)/1000;
  char buf[24];snprintf(buf,sizeof(buf),"UP %02lu:%02lu:%02lu",s/3600,(s%3600)/60,s%60);
  tft.setTextFont(1);tft.setTextSize(1);tft.setTextColor(BG,FG);
  tft.fillRect(W-82,0,82,BAR_H,FG);tft.setCursor(W-79,5);tft.print(buf);
  // show fixed channel
  char ch[8];snprintf(ch,sizeof(ch),"CH%2d",FIXED_CHAN);
  tft.fillRect(0,0,40,BAR_H,FG);tft.setCursor(2,5);tft.print(ch);
}

void lv_apCount(){
  tft.setTextFont(1);tft.setTextSize(1);tft.setTextColor(BG,FG);
  tft.fillRect(40,0,80,BAR_H,FG);
  char buf[12];snprintf(buf,sizeof(buf),"APS %lu",totalAPs);
  tft.setCursor(45,5);tft.print(buf);
}

void lv_stats(){
  int sx=DIV_X+5,sy=BAR_H+22,lh=17;
  tft.fillRect(DIV_X+1,sy,W-DIV_X-1,H-BAR_H*2-sy+BAR_H,BG);
  tft.setTextFont(1);tft.setTextSize(1);tft.setTextColor(FG,BG);
  char b[28];
  snprintf(b,sizeof(b),"APs:       %lu",totalAPs);      tft.setCursor(sx,sy);      tft.print(b);
  snprintf(b,sizeof(b),"Clients:   %lu",totalClients);  tft.setCursor(sx,sy+lh);   tft.print(b);
  snprintf(b,sizeof(b),"Handshakes:%lu",handshakes);    tft.setCursor(sx,sy+lh*2); tft.print(b);
  snprintf(b,sizeof(b),"Friends:   %lu",friends);       tft.setCursor(sx,sy+lh*3); tft.print(b);
  snprintf(b,sizeof(b),"Epochs:    %lu",epochs);        tft.setCursor(sx,sy+lh*4); tft.print(b);
  snprintf(b,sizeof(b),"Activity:  %lu",epochAct);      tft.setCursor(sx,sy+lh*5); tft.print(b);
}

void lv_botBar(){
  tft.fillRect(0,H-BAR_H,W,BAR_H,FG);
  tft.setTextFont(1);tft.setTextSize(1);tft.setTextColor(BG,FG);
  char b[32];snprintf(b,sizeof(b),"PWND %lu  APS %lu  F %lu",handshakes,totalAPs,friends);
  tft.setCursor(3,H-BAR_H+5);tft.print(b);
  tft.setCursor(W-40,H-BAR_H+5);tft.print("AUTO");
}

void lv_chrome(){
  tft.fillScreen(BG);
  tft.fillRect(0,0,W,BAR_H,FG);
  tft.setTextFont(1);tft.setTextSize(1);tft.setTextColor(BG,FG);
  tft.setCursor(45,5);tft.print("APS 0");
  tft.drawFastHLine(0,BAR_H+16,W,FG);
  tft.setTextColor(FG,BG);tft.setCursor(3,BAR_H+5);tft.print("leviagotchi>");
  tft.drawFastVLine(DIV_X,BAR_H,H-BAR_H*2,FG);
  tft.fillRect(0,H-BAR_H,W,BAR_H,FG);
}

// ═══════════════ CAPTIVE PORTAL ══════════════════════════

const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html>
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>leviagotchi</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#000;color:#fff;font-family:monospace;display:flex;flex-direction:column;
align-items:center;justify-content:center;min-height:100vh;text-align:center;padding:20px}
.face{font-size:60px;margin-bottom:20px;animation:b 3s infinite}
@keyframes b{0%,90%,100%{opacity:1}95%{opacity:0}}
h1{font-size:26px;margin-bottom:10px;letter-spacing:2px}
p{font-size:15px;color:#aaa;margin-top:8px}
.tag{margin-top:30px;font-size:11px;color:#333;border-top:1px solid #222;padding-top:14px;width:100%}
</style></head>
<body>
<div class="face">( &#9679;_&#9679; )</div>
<h1>I MADE A NEW FRIEND!</h1>
<p>leviagotchi is happy to see you :]</p>
<p style="margin-top:14px;color:#444">There is no internet here.</p>
<p style="color:#444">But leviagotchi says hi!</p>
<div class="tag">leviagotchi v1.0</div>
</body></html>)HTML";

void portalPage(){
  friends++;
  Serial.printf("[PORTAL] New friend #%lu\n",friends);
  curExp=E_HAPPY;
  lv_drawFace(E_HAPPY);
  lv_status(SMSG[E_HAPPY]);
  lv_stats();
  lv_botBar();
  srv.send_P(200,"text/html",PAGE);
}

void portalInit(){
  // Start AP first, completely, before touching promiscuous
  WiFi.mode(WIFI_AP);
  delay(200);
  bool ok = WiFi.softAP(PORTAL_SSID, "", FIXED_CHAN, 0, 8);
  delay(1000);  // critical: give AP time to fully start
  Serial.printf("[PORTAL] softAP=%s  SSID=%s  IP=%s\n",
                ok?"OK":"FAIL", PORTAL_SSID,
                WiFi.softAPIP().toString().c_str());
  dns.start(53,"*",WiFi.softAPIP());
  srv.on("/",                    portalPage);
  srv.on("/generate_204",        portalPage);
  srv.on("/hotspot-detect.html", portalPage);
  srv.on("/connecttest.txt",     portalPage);
  srv.on("/fwlink",              portalPage);
  srv.on("/redirect",            portalPage);
  srv.onNotFound(portalPage);
  srv.begin();
  Serial.println("[PORTAL] Server ready");
}

// ═══════════════ WIFI MONITOR ════════════════════════════

void IRAM_ATTR pkt_cb(void* buf, wifi_promiscuous_pkt_type_t type){
  if(type!=WIFI_PKT_MGMT && type!=WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* pkt=(wifi_promiscuous_pkt_t*)buf;
  const uint8_t* p=pkt->payload;
  uint16_t len=pkt->rx_ctrl.sig_len;
  if(len<24) return;
  epochAct++;

  if(p[0]==0x80 && len>36){
    char bssid[18];
    snprintf(bssid,sizeof(bssid),"%02X:%02X:%02X:%02X:%02X:%02X",p[16],p[17],p[18],p[19],p[20],p[21]);
    std::string key(bssid);
    if(apMap.find(key)==apMap.end()){
      AP ap;ap.rssi=pkt->rx_ctrl.rssi;ap.ch=pkt->rx_ctrl.channel;
      uint8_t sl=p[37];if(sl>32)sl=32;memcpy(ap.ssid,p+38,sl);ap.ssid[sl]=0;
      apMap[key]=ap;totalAPs++;
      Serial.printf("[AP] %s  CH:%d  RSSI:%d\n",ap.ssid,ap.ch,ap.rssi);
    }
  }

  if((p[0]&0x0C)==0x08){
    char src[18];
    snprintf(src,sizeof(src),"%02X:%02X:%02X:%02X:%02X:%02X",p[10],p[11],p[12],p[13],p[14],p[15]);
    std::string key(src);
    if(clients.find(key)==clients.end()){
      clients.insert(key);totalClients++;
      Serial.printf("[CLIENT] %s\n",src);
    }
    for(int i=24;i<(int)len-1;i++){
      if(p[i]==0x88&&p[i+1]==0x8E){
        handshakes++;gotHS=true;
        Serial.printf("[EAPOL] Handshake #%lu from %s\n",handshakes,src);
        break;
      }
    }
  }
}

void monitorInit(){
  // Switch to AP+STA so we can run promiscuous AND keep the AP alive
  // Must do this AFTER softAP is already running
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  delay(100);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(pkt_cb);
  // Stay on same fixed channel as AP — no hopping!
  esp_wifi_set_channel(FIXED_CHAN, WIFI_SECOND_CHAN_NONE);
  Serial.printf("[MONITOR] Promiscuous on CH%d\n", FIXED_CHAN);
}

// ═══════════════ MOOD ════════════════════════════════════

Exp chooseMood(){
  if(gotHS)                       {gotHS=false;return E_HAPPY;}
  if(epochAct>200)                return E_EXCITED;
  if(totalAPs>10&&epochAct>50)    return E_GRATEFUL;
  if(epochAct>10)                 {static bool lr=false;lr=!lr;return lr?E_LOOKR:E_LOOKL;}
  if(idleEpochs>=SLEEP_EPOCHS)    return (epochs%2==0)?E_SLEEP:E_SLEEP2;
  if(idleEpochs>=BORED_EPOCHS)    return E_BORED;
  return E_AWAKE;
}

// ═══════════════ SETUP / LOOP ════════════════════════════

void setup(){
  Serial.begin(115200);
  bootMs=millis();
  pinMode(27,OUTPUT);digitalWrite(27,HIGH);
  SPI.begin(14,12,13,15);
  tft.init();tft.setRotation(1);
  lv_chrome();lv_drawFace(E_AWAKE);lv_topBar();
  lv_status("Starting portal...");lv_stats();lv_botBar();

  // 1. Start captive portal AP first
  portalInit();

  // 2. Enable monitor mode on top (same channel, AP stays alive)
  monitorInit();

  lv_status(SMSG[E_AWAKE]);
  lastEpoch=millis();
}

void loop(){
  unsigned long now=millis();

  // Always serve portal requests
  dns.processNextRequest();
  srv.handleClient();

  // Epoch tick (no channel hopping — fixed channel keeps AP alive)
  if(now-lastEpoch>=EPOCH_MS){
    lastEpoch=now;epochs++;
    if(epochAct<5)idleEpochs++;else idleEpochs=0;
    Exp mood=chooseMood();
    if(mood!=curExp){curExp=mood;lv_drawFace(mood);lv_status(SMSG[mood]);}
    epochAct=0;lv_stats();lv_botBar();lv_apCount();
  }

  // Update display every 2s
  static unsigned long lastFast=0;
  if(now-lastFast>=2000){
    lastFast=now;
    lv_stats();lv_topBar();lv_botBar();lv_apCount();
    if(gotHS){gotHS=false;curExp=E_HAPPY;lv_drawFace(E_HAPPY);lv_status(SMSG[E_HAPPY]);}
  }

  delay(50);
}
