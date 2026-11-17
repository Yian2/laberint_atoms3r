#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>

#include "secrets.h"

const int N = 15;
const int CEL = 8;
const int OFF = (128 - N * CEL) / 2;
const int FORATS = 4;
const int PARETS_EXTRA = 10;
char MAPA[N][N + 1];

const float RB = 2.8;
const float GRAVETAT = 260.0;
const float FREGAMENT = 0.985;
const float REBOT = 0.3;
const float VMAX = 120.0;
const float SUAVITZAT = 0.6;
const unsigned long PERIODE_MS = 20;
const unsigned long TEMPS_MAX_MS = 180000;

M5Canvas canvas(&M5.Display);
M5Canvas fons(&M5.Display);
WebServer server(80);
WebSocketsServer ws(81);

enum Estat { ESPERANT, JUGANT, ENVIANT, FINAL };
Estat estat = ESPERANT;

float fx = 0, fy = 0;
float ax0 = 0, ay0 = 0;
float bx, by, vx = 0, vy = 0;
float sx, sy;
int   caigudes = 0;
bool  guanyat = false;
bool  enviatOk = false;
unsigned long inici = 0, tempsFinal = 0, ultimaLectura = 0;
bool  volComencar = false;

void generaLaberint();
void dibuixaFons();

const char PAGINA[] PROGMEM = R"rawliteral(
<!doctype html><html lang="ca"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Laberint</title>
<style>
body{margin:0;background:#1a1410;color:#f3e6d0;font-family:system-ui,sans-serif;text-align:center}
h1{font-size:1.6rem;margin:14px 0 4px;letter-spacing:.05em}
#marcador{display:flex;justify-content:center;gap:48px;font-size:1.1rem;color:#c9b89c}
.num{font-size:2.8rem;font-weight:700;color:#fff}
#marc{display:inline-block;padding:14px;border-radius:18px;background:linear-gradient(145deg,#a06a35,#6e4320);box-shadow:0 12px 40px #000a;margin-top:8px}
canvas{display:block;width:min(88vw,56vh);height:min(88vw,56vh);border-radius:8px}
#msg{font-size:1.6rem;min-height:2em;margin:10px 4px 0}
button{font-size:1.6rem;padding:16px 48px;border:0;border-radius:14px;background:#2e7dff;color:#fff;margin:6px;cursor:pointer}
</style></head><body>
<h1>LABERINT</h1>
<div id="marcador">
  <div><div class="num" id="temps">0.0</div>segons</div>
  <div><div class="num" id="caig">0</div>caigudes</div>
</div>
<div id="marc"><canvas id="c" width="640" height="640"></canvas></div>
<div id="msg"></div>
<button id="b" onclick="comencar()">Comencar</button>
<script>
const c=document.getElementById('c'),g=c.getContext('2d'),$=id=>document.getElementById(id);
let mapa=null,S=5,cel=8,d=null,pos=null,caigAnt=0,flash=0;
let ws;
function connecta(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onmessage=e=>{
    ultim=Date.now();
    const m=JSON.parse(e.data);
    if(m.files){ mapa=m.files; cel=m.cel; S=640/(m.files.length*cel); pos=null; return; }
    d=m;
    if(!pos||d.estat==='esperant'||d.caigudes!==caigAnt) pos={x:d.x,y:d.y};
    if(d.caigudes>caigAnt) flash=30;
    caigAnt=d.caigudes;
  };
  ws.onclose=()=>{ $('msg').textContent="Sense connexio amb l'Atom"; setTimeout(connecta,1000); };
}
function comencar(){ if(ws&&ws.readyState===1) ws.send('start'); else fetch('/start',{method:'POST'}); }
function tauler(){
  const k=cel*S;
  g.fillStyle='#e8c690'; g.fillRect(0,0,640,640);
  for(let i=0;i<14;i++){ g.strokeStyle='rgba(150,100,50,.12)'; g.lineWidth=3;
    g.beginPath(); g.moveTo(0,i*48+10); g.bezierCurveTo(200,i*48-10,440,i*48+30,640,i*48+5); g.stroke(); }
  for(let r=0;r<mapa.length;r++) for(let q=0;q<mapa[r].length;q++){
    const ch=mapa[r][q], x=q*k, y=r*k;
    if(ch==='#'){ g.fillStyle='rgba(0,0,0,.25)'; g.fillRect(x+4,y+4,k,k);
                  g.fillStyle='#8a5a2e'; g.fillRect(x,y,k,k); }
    if(ch==='O'){ const gr=g.createRadialGradient(x+k/2,y+k/2,2,x+k/2,y+k/2,k*.42);
                  gr.addColorStop(0,'#000'); gr.addColorStop(1,'#3b2614');
                  g.fillStyle=gr; g.beginPath(); g.arc(x+k/2,y+k/2,k*.42,0,7); g.fill(); }
    if(ch==='G'){ g.fillStyle='#2ecc71'; g.beginPath(); g.arc(x+k/2,y+k/2,k*.42,0,7); g.fill();
                  g.fillStyle='#fff'; g.font='bold '+(k*.3)+'px system-ui'; g.textAlign='center';
                  g.textBaseline='middle'; g.fillText('META',x+k/2,y+k/2); }
  }
}
function bola(x,y){
  const r=2.8*S*1.1;
  g.fillStyle='rgba(0,0,0,.3)'; g.beginPath(); g.arc(x+4,y+5,r,0,7); g.fill();
  const gr=g.createRadialGradient(x-r*.4,y-r*.4,1,x,y,r);
  gr.addColorStop(0,'#fff'); gr.addColorStop(.4,'#c8ccd2'); gr.addColorStop(1,'#5b6068');
  g.fillStyle=gr; g.beginPath(); g.arc(x,y,r,0,7); g.fill();
}
function frame(){
  requestAnimationFrame(frame);
  if(mapa&&d){
    if(!pos) pos={x:d.x,y:d.y};
    pos.x+=(d.x-pos.x)*.6; pos.y+=(d.y-pos.y)*.6;
    tauler(); bola(pos.x*S,pos.y*S);
    if(flash>0){ g.fillStyle='rgba(231,76,60,'+(flash/60)+')'; g.fillRect(0,0,640,640); flash--; }
    $('temps').textContent=d.temps.toFixed(1);
    $('caig').textContent=d.caigudes;
    const parat=d.estat==='esperant'||d.estat==='final';
    $('b').style.display=parat?'inline-block':'none';
    $('b').textContent=d.estat==='final'?'Tornar a jugar':'Comencar';
    $('msg').textContent=
      d.estat==='esperant'?"Deixa l'Atom pla i prem Comencar":
      d.estat==='jugant'?(flash>0?'Oh! Al forat. Torna a provar':'Inclina per portar la bola a la META'):
      d.estat==='enviant'?'Guardant la partida...':
      (d.guanyat?'Ho has aconseguit en '+d.temps.toFixed(1)+' s!':'Temps esgotat')+(d.enviat?' (guardat)':'');
  }
}
let ultim=Date.now();
setInterval(()=>{ if(Date.now()-ultim>3000&&ws){ ws.close(); ultim=Date.now(); } },1000);
connecta(); frame();
</script></body></html>
)rawliteral";

char casella(float x, float y) {
  int q = (int)floorf(x / CEL), r = (int)floorf(y / CEL);
  if (q < 0 || r < 0 || q >= N || r >= N) return '#';
  return MAPA[r][q];
}
bool xoca(float x, float y) {
  return casella(x - RB, y - RB) == '#' || casella(x + RB, y - RB) == '#' ||
         casella(x - RB, y + RB) == '#' || casella(x + RB, y + RB) == '#';
}
void tornaSortida() { bx = sx; by = sy; vx = 0; vy = 0; }

const char* nomEstat() {
  switch (estat) {
    case JUGANT:  return "jugant";
    case ENVIANT: return "enviant";
    case FINAL:   return "final";
    default:      return "esperant";
  }
}
float tempsJoc() {
  if (estat == JUGANT) return (millis() - inici) / 1000.0;
  if (estat == ESPERANT) return 0;
  return tempsFinal / 1000.0;
}

void connectaWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
  WiFi.setSleep(false);
  if (WiFi.status() == WL_CONNECTED) Serial.println("WiFi OK: http://" + WiFi.localIP().toString());
  else Serial.println("WiFi NO connectat (revisa nom i contrasenya)");
}

bool enviaPartida() {
  if (WiFi.status() != WL_CONNECTED) connectaWifi();
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5);
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(client, String(FIREBASE_URL) + "/sessions.json")) return false;
  http.addHeader("Content-Type", "application/json");
  String json = "{";
  json += "\"exercici\":\"laberint\",";
  json += "\"completat\":" + String(guanyat ? "true" : "false") + ",";
  json += "\"temps_s\":" + String(tempsFinal / 1000.0, 1) + ",";
  json += "\"caigudes\":" + String(caigudes) + ",";
  json += "\"dispositiu\":\"atoms3r-1\",";
  json += "\"timestamp\":{\".sv\":\"timestamp\"}";
  json += "}";
  int codi = http.POST(json);
  Serial.printf("Firebase -> codi %d: %s\n", codi, http.getString().c_str());
  http.end();
  return codi == 200;
}

void novaPartida() {
  generaLaberint();
  ax0 = fx; ay0 = fy;
  tornaSortida();
  caigudes = 0; guanyat = false; enviatOk = false;
  inici = millis();
  estat = JUGANT;
}

void paginaInici() { server.send_P(200, "text/html", PAGINA); }

String jsonMapa() {
  String json = "{\"cel\":" + String(CEL) + ",\"files\":[";
  for (int r = 0; r < N; r++) { json += "\""; json += MAPA[r]; json += (r < N - 1) ? "\"," : "\""; }
  json += "]}";
  return json;
}

bool hiHaCami() {
  static uint8_t cua[N * N][2];
  static bool vist[N][N];
  memset(vist, 0, sizeof(vist));
  int ini = 0, fi = 0;
  cua[fi][0] = 1; cua[fi][1] = 1; fi++; vist[1][1] = true;
  const int DR[4] = {-1, 1, 0, 0}, DC[4] = {0, 0, -1, 1};
  while (ini < fi) {
    int r = cua[ini][0], c = cua[ini][1]; ini++;
    if (MAPA[r][c] == 'G') return true;
    for (int k = 0; k < 4; k++) {
      int nr = r + DR[k], nc = c + DC[k];
      char ch = MAPA[nr][nc];
      if (!vist[nr][nc] && (ch == '.' || ch == 'G')) {
        vist[nr][nc] = true; cua[fi][0] = nr; cua[fi][1] = nc; fi++;
      }
    }
  }
  return false;
}

void generaLaberint() {
  for (int r = 0; r < N; r++) { memset(MAPA[r], '#', N); MAPA[r][N] = '\0'; }

  static uint8_t pila[N * N][2];
  int top = 0;
  MAPA[1][1] = '.'; pila[0][0] = 1; pila[0][1] = 1; top = 1;
  const int DR[4] = {-2, 2, 0, 0}, DC[4] = {0, 0, -2, 2};
  while (top > 0) {
    int r = pila[top - 1][0], c = pila[top - 1][1];
    int opcions[4], n = 0;
    for (int k = 0; k < 4; k++) {
      int nr = r + DR[k], nc = c + DC[k];
      if (nr > 0 && nr < N - 1 && nc > 0 && nc < N - 1 && MAPA[nr][nc] == '#') opcions[n++] = k;
    }
    if (n == 0) { top--; continue; }
    int k = opcions[random(n)];
    MAPA[r + DR[k] / 2][c + DC[k] / 2] = '.';
    MAPA[r + DR[k]][c + DC[k]] = '.';
    pila[top][0] = r + DR[k]; pila[top][1] = c + DC[k]; top++;
  }

  for (int i = 0; i < PARETS_EXTRA; i++) {
    int r = random(1, N - 1), c = random(1, N - 1);
    if ((r % 2) != (c % 2)) MAPA[r][c] = '.';
  }

  MAPA[1][1] = 'S';
  MAPA[N - 2][N - 2] = 'G';

  for (int intent = 0; intent < 50; intent++) {
    int fr[FORATS], fc[FORATS], posats = 0;
    int proves = 0;
    while (posats < FORATS && proves++ < 500) {
      int r = random(1, N - 1), c = random(1, N - 1);
      bool lluny = (r + c > 5) && (abs(r - (N - 2)) + abs(c - (N - 2)) > 2);
      if (MAPA[r][c] == '.' && lluny) { MAPA[r][c] = 'O'; fr[posats] = r; fc[posats] = c; posats++; }
    }
    if (hiHaCami()) break;
    for (int i = 0; i < posats; i++) MAPA[fr[i]][fc[i]] = '.';
  }

  sx = 1 * CEL + CEL / 2.0;  sy = 1 * CEL + CEL / 2.0;
  tornaSortida();
  dibuixaFons();
  String m = jsonMapa();
  ws.broadcastTXT(m);
}

void enviaDades() {
  char json[200];
  snprintf(json, sizeof(json),
    "{\"estat\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"temps\":%.1f,\"caigudes\":%d,\"guanyat\":%s,\"enviat\":%s}",
    nomEstat(), bx, by, tempsJoc(), caigudes, guanyat ? "true" : "false", enviatOk ? "true" : "false");
  ws.broadcastTXT(json);
}

void onWs(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type == WStype_CONNECTED) { String m = jsonMapa(); ws.sendTXT(num, m); }
  if (type == WStype_TEXT && len == 5 && memcmp(payload, "start", 5) == 0)
    volComencar = true;
}

void comencaDesDeWeb() {
  volComencar = true;
  server.send(200, "text/plain", "ok");
}

void dibuixaFons() {
  fons.fillSprite(0x8B26);
  for (int r = 0; r < N; r++) for (int q = 0; q < N; q++) {
    char ch = MAPA[r][q];
    int x = q * CEL, y = r * CEL;
    if (ch == '#') fons.fillRect(x, y, CEL, CEL, 0x4A02);
    if (ch == 'O') fons.fillCircle(x + CEL / 2, y + CEL / 2, 3, TFT_BLACK);
    if (ch == 'G') fons.fillCircle(x + CEL / 2, y + CEL / 2, 3, TFT_GREEN);
  }
}

void dibuixaAtom() {
  canvas.fillSprite(TFT_BLACK);
  fons.pushSprite(&canvas, OFF, OFF);
  canvas.fillCircle(OFF + (int)bx, OFF + (int)by, 3, TFT_WHITE);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  if (estat == ESPERANT) {
    canvas.setCursor(2, 2); canvas.print("Prem per comencar");
    canvas.setCursor(2, 119);
    canvas.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("Sense WiFi"));
  } else if (estat == FINAL) {
    canvas.setCursor(2, 2);   canvas.print(guanyat ? "META!" : "Temps esgotat");
    canvas.setCursor(2, 119); canvas.printf("%.1fs  %d caig.", tempsFinal / 1000.0, caigudes);
  } else if (estat == ENVIANT) {
    canvas.setCursor(2, 2); canvas.print("Enviant...");
  }
  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.setTextSize(1);

  randomSeed(esp_random());
  fons.createSprite(N * CEL, N * CEL);

  canvas.fillSprite(TFT_BLACK);
  canvas.setCursor(4, 60); canvas.print("Connectant WiFi...");
  canvas.pushSprite(0, 0);
  connectaWifi();

  MDNS.begin("entrenador");
  server.on("/", paginaInici);
  server.on("/start", HTTP_POST, comencaDesDeWeb);
  server.begin();
  ws.begin();
  ws.onEvent(onWs);
  generaLaberint();
}

void loop() {
  M5.update();
  server.handleClient();
  ws.loop();

  if (millis() - ultimaLectura < PERIODE_MS) return;
  ultimaLectura = millis();
  const float dt = PERIODE_MS / 1000.0;

  float ax, ay, az;
  M5.Imu.getAccel(&ax, &ay, &az);
  fx = SUAVITZAT * fx + (1 - SUAVITZAT) * ax;
  fy = SUAVITZAT * fy + (1 - SUAVITZAT) * ay;

  if (M5.BtnA.wasPressed()) volComencar = true;
  if (volComencar) {
    volComencar = false;
    if (estat == ESPERANT || estat == FINAL) novaPartida();
  }

  if (estat == JUGANT) {
    vx += -(fx - ax0) * GRAVETAT * dt;
    vy +=  (fy - ay0) * GRAVETAT * dt;
    vx *= FREGAMENT; vy *= FREGAMENT;
    vx = constrain(vx, -VMAX, VMAX);
    vy = constrain(vy, -VMAX, VMAX);

    float nx = bx + vx * dt;
    if (xoca(nx, by)) vx = -vx * REBOT; else bx = nx;
    float ny = by + vy * dt;
    if (xoca(bx, ny)) vy = -vy * REBOT; else by = ny;

    char ch = casella(bx, by);
    float cxCas = floorf(bx / CEL) * CEL + CEL / 2.0, cyCas = floorf(by / CEL) * CEL + CEL / 2.0;
    float dCentre = sqrtf((bx - cxCas) * (bx - cxCas) + (by - cyCas) * (by - cyCas));
    if (ch == 'O' && dCentre < CEL * 0.4) { caigudes++; tornaSortida(); }
    if (ch == 'G') { guanyat = true; }

    if (guanyat || millis() - inici >= TEMPS_MAX_MS) {
      tempsFinal = millis() - inici;
      estat = ENVIANT;
    }
  }

  dibuixaAtom();
  enviaDades();

  if (estat == ENVIANT) {
    enviatOk = enviaPartida();
    estat = FINAL;
  }
}
