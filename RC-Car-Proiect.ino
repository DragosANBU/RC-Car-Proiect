#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <BH1750.h>
#include <ESP32Servo.h>

const char* ssid = "ESP32_RC_CAR";
const char* password = "12345678";

// ==================== LEDs ====================
#define POWER_LED_PIN 4
#define HEADLIGHT_PIN 5

// ================= Ultrasonic 1 (fata) =================
#define TRIG_PIN 18
#define ECHO1_PIN 19
#define BUZZER1_PIN 23

// ================= Ultrasonic 2 (spate) =================
#define TRIG2_PIN 16
#define ECHO2_PIN 17
#define BUZZER2_PIN 2

// ================= Motor Driver ===============
#define PWMA 25
#define AIN1 26
#define AIN2 27

#define PWMB 33
#define BIN1 32
#define BIN2 14

// ================= Servo Motor ===============
#define SERVO_PIN 13

BH1750 lightMeter;
Servo steeringServo;
WebServer server(80);

// ===================================================
// Stare globala
// ===================================================

int currentSpeed = 0;      // -255 .. 255
int currentSteer = 90;     // 0 .. 180 (90 = centru)
bool headlightManual = false;
bool headlightOn = false;

unsigned long lastCommandTime = 0;
const unsigned long COMMAND_TIMEOUT = 800; // ms - opreste masina daca nu mai primeste comenzi (siguranta)

float distanceFront = -1;
float distanceRear = -1;
float lux = 0;

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 60;

unsigned long lastLuxRead = 0;
const unsigned long LUX_INTERVAL = 300;

// Buzzer 1 (fata) - stare non-blocanta
bool buzzer1State = false;
unsigned long buzzer1LastToggle = 0;

// Buzzer 2 (spate) - stare non-blocanta
bool buzzer2State = false;
unsigned long buzzer2LastToggle = 0;

// ===================================================
// Pagina web (HTML + CSS + JS, totul intr-un singur fisier,
// fara resurse externe - telefonul conectat la AP nu are internet)
// ===================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ro">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>RC Car Control</title>
<style>
  :root{
    --bg:#0b0d10;
    --panel:#15181d;
    --panel-border:#262b33;
    --accent:#ffb300;
    --accent-dim:#8a6300;
    --danger:#ff3b30;
    --good:#3ecf8e;
    --text:#e8e6e1;
    --text-dim:#8b92a0;
    --mono: 'SF Mono', 'Courier New', monospace;
    --sans: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
  }
  * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; user-select:none; }
  html,body{
    margin:0; padding:0; height:100%;
    background:var(--bg); color:var(--text);
    font-family:var(--sans);
    overscroll-behavior:none;
    touch-action:none;
  }
  body{
    display:flex; flex-direction:column;
    min-height:100vh;
    padding: max(12px, env(safe-area-inset-top)) 14px max(14px, env(safe-area-inset-bottom));
  }

  /* ---------- Header ---------- */
  header{
    display:flex; align-items:center; justify-content:space-between;
    padding: 4px 2px 12px;
  }
  .title{
    font-family:var(--mono);
    font-size:15px; letter-spacing:3px; font-weight:700;
    color:var(--accent);
  }
  .conn{
    display:flex; align-items:center; gap:6px;
    font-family:var(--mono); font-size:11px; color:var(--text-dim);
  }
  .dot{
    width:8px; height:8px; border-radius:50%;
    background:var(--danger);
  }
  .dot.on{ background:var(--good); box-shadow:0 0 6px var(--good); }

  /* ---------- Telemetry strip ---------- */
  .telemetry{
    display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px;
    margin-bottom:14px;
  }
  .tile{
    background:var(--panel); border:1px solid var(--panel-border);
    border-radius:10px; padding:8px 6px; text-align:center;
    transition:border-color .2s, background .2s;
  }
  .tile.warn{ border-color:var(--danger); background:#1f1113; }
  .tile .label{ font-size:9px; color:var(--text-dim); letter-spacing:1.5px; text-transform:uppercase; }
  .tile .value{ font-family:var(--mono); font-size:18px; font-weight:700; margin-top:2px; }

  /* ---------- Mode toggle ---------- */
  .modeToggle{
    display:flex; background:var(--panel); border:1px solid var(--panel-border);
    border-radius:10px; padding:3px; margin-bottom:12px;
  }
  .modeToggle button{
    flex:1; padding:9px; border:none; border-radius:8px;
    background:transparent; color:var(--text-dim);
    font-family:var(--sans); font-size:13px; font-weight:600;
  }
  .modeToggle button.active{
    background:var(--accent); color:#1a1300;
  }

  /* ---------- Control area ---------- */
  .controlArea{
    flex:1; display:flex; align-items:center; justify-content:center;
    min-height:280px;
  }

  /* Joystick */
  .joyBase{
    position:relative;
    width:min(70vw, 260px); height:min(70vw, 260px);
    border-radius:50%;
    background:radial-gradient(circle at 50% 40%, #1c2027, #101317 70%);
    border:1px solid var(--panel-border);
    box-shadow: inset 0 0 30px rgba(0,0,0,.5);
  }
  .joyRing{
    position:absolute; inset:0; border-radius:50%;
    border:2px solid var(--accent-dim);
    transition:border-color .15s, box-shadow .15s;
  }
  .joyRing.active{
    border-color:var(--accent);
    box-shadow:0 0 24px rgba(255,179,0,.35);
  }
  .joyStick{
    position:absolute; width:64px; height:64px; border-radius:50%;
    background:linear-gradient(160deg,#2a2f38,#15181d);
    border:1px solid #3a4150;
    top:50%; left:50%;
    transform:translate(-50%,-50%);
    box-shadow:0 4px 10px rgba(0,0,0,.5);
  }
  .joyCross{
    position:absolute; left:50%; top:50%; width:1px; height:100%;
    background:var(--panel-border); transform:translateX(-50%);
  }
  .joyCross2{
    position:absolute; top:50%; left:50%; height:1px; width:100%;
    background:var(--panel-border); transform:translateY(-50%);
  }

  /* D-pad */
  .dpad{
    display:grid;
    grid-template-columns: 78px 78px 78px;
    grid-template-rows: 78px 78px 78px;
    gap:8px;
  }
  .dpad button{
    border:none; border-radius:14px;
    background:var(--panel); border:1px solid var(--panel-border);
    color:var(--text); font-size:26px;
    display:flex; align-items:center; justify-content:center;
  }
  .dpad button:active, .dpad button.pressed{
    background:var(--accent); color:#1a1300; border-color:var(--accent);
  }
  .dpad .up{ grid-column:2; grid-row:1; }
  .dpad .left{ grid-column:1; grid-row:2; }
  .dpad .stopBtn{ grid-column:2; grid-row:2; background:var(--danger); color:#fff; font-size:11px; font-weight:700; letter-spacing:1px;}
  .dpad .right{ grid-column:3; grid-row:2; }
  .dpad .down{ grid-column:2; grid-row:3; }

  /* readout under control area */
  .readout{
    display:flex; justify-content:center; gap:22px;
    font-family:var(--mono); font-size:12px; color:var(--text-dim);
    margin-top:10px;
  }
  .readout b{ color:var(--accent); font-size:13px; }

  /* ---------- Bottom bar ---------- */
  .bottomBar{
    display:flex; gap:8px; margin-top:14px; align-items:center;
  }
  .sliderWrap{
    flex:1; background:var(--panel); border:1px solid var(--panel-border);
    border-radius:10px; padding:8px 12px;
  }
  .sliderWrap .label{ font-size:9px; color:var(--text-dim); letter-spacing:1.5px; text-transform:uppercase; margin-bottom:4px; }
  input[type=range]{
    width:100%; accent-color:var(--accent);
  }
  .lightBtn{
    width:58px; height:58px; border-radius:12px;
    background:var(--panel); border:1px solid var(--panel-border);
    color:var(--text-dim); font-size:22px;
    display:flex; align-items:center; justify-content:center;
  }
  .lightBtn.on{
    background:var(--accent); color:#1a1300; border-color:var(--accent);
    box-shadow:0 0 16px rgba(255,179,0,.4);
  }
  .stopWide{
    width:100%; margin-top:8px; padding:14px;
    background:var(--danger); color:#fff; border:none; border-radius:12px;
    font-family:var(--sans); font-weight:700; letter-spacing:2px; font-size:14px;
  }
  .stopWide:active{ filter:brightness(0.85); }

  .banner{
    display:none; position:fixed; top:0; left:0; right:0;
    background:var(--danger); color:#fff; text-align:center;
    padding:8px; font-size:12px; font-family:var(--mono); z-index:50;
  }
  .banner.show{ display:block; }
</style>
</head>
<body>

<div class="banner" id="banner">CONEXIUNE PIERDUTA - masina oprita automat</div>

<header>
  <div class="title">RC CAR</div>
  <div class="conn"><span class="dot" id="connDot"></span><span id="connText">conectare...</span></div>
</header>

<div class="telemetry">
  <div class="tile" id="tileFront"><div class="label">Fata</div><div class="value" id="distFront">-- cm</div></div>
  <div class="tile" id="tileRear"><div class="label">Spate</div><div class="value" id="distRear">-- cm</div></div>
  <div class="tile"><div class="label">Lumina</div><div class="value" id="luxVal">-- lx</div></div>
</div>

<div class="modeToggle">
  <button id="btnModeJoy" class="active">Joystick</button>
  <button id="btnModeDpad">Butoane</button>
</div>

<div class="controlArea">
  <div id="joyMode">
    <div class="joyBase" id="joyBase">
      <div class="joyCross"></div><div class="joyCross2"></div>
      <div class="joyRing" id="joyRing"></div>
      <div class="joyStick" id="joyStick"></div>
    </div>
  </div>
  <div id="dpadMode" style="display:none;">
    <div class="dpad">
      <button class="up" data-dir="fwd">&#9650;</button>
      <button class="left" data-dir="left">&#9668;</button>
      <button class="stopBtn" id="dpadStop">STOP</button>
      <button class="right" data-dir="right">&#9658;</button>
      <button class="down" data-dir="back">&#9660;</button>
    </div>
  </div>
</div>

<div class="readout">
  <div>VITEZA <b id="rSpeed">0</b></div>
  <div>DIRECTIE <b id="rSteer">90</b>&deg;</div>
</div>

<div class="bottomBar">
  <button class="lightBtn" id="lightBtn">&#128161;</button>
  <div class="sliderWrap">
    <div class="label">Limita viteza <span id="maxSpeedVal">255</span></div>
    <input type="range" id="maxSpeed" min="60" max="255" value="255">
  </div>
</div>

<button class="stopWide" id="bigStop">OPRIRE DE URGENTA</button>

<script>
(function(){
  var lastSendOk = true, missCount = 0;

  function sendControl(speed, steer){
    var url = '/control?speed=' + Math.round(speed) + '&steer=' + Math.round(steer);
    fetch(url).then(function(){ missCount = 0; setConn(true); })
              .catch(function(){ missCount++; if(missCount > 3) setConn(false); });
  }
  function sendStop(){
    fetch('/stop').catch(function(){});
  }
  function setConn(ok){
    var dot = document.getElementById('connDot');
    var txt = document.getElementById('connText');
    var banner = document.getElementById('banner');
    if(ok){ dot.classList.add('on'); txt.textContent='conectat'; banner.classList.remove('show'); }
    else{ dot.classList.remove('on'); txt.textContent='pierdut'; banner.classList.add('show'); }
  }

  // ---------- Mode switch ----------
  var joyMode = document.getElementById('joyMode');
  var dpadMode = document.getElementById('dpadMode');
  var btnJoy = document.getElementById('btnModeJoy');
  var btnDpad = document.getElementById('btnModeDpad');
  btnJoy.onclick = function(){
    joyMode.style.display='block'; dpadMode.style.display='none';
    btnJoy.classList.add('active'); btnDpad.classList.remove('active');
    sendStop();
  };
  btnDpad.onclick = function(){
    joyMode.style.display='none'; dpadMode.style.display='block';
    btnDpad.classList.add('active'); btnJoy.classList.remove('active');
    sendStop();
  };

  // ---------- Max speed slider ----------
  var maxSpeedInput = document.getElementById('maxSpeed');
  var maxSpeedVal = document.getElementById('maxSpeedVal');
  maxSpeedInput.oninput = function(){ maxSpeedVal.textContent = maxSpeedInput.value; };

  // ---------- Joystick ----------
  var base = document.getElementById('joyBase');
  var stick = document.getElementById('joyStick');
  var ring = document.getElementById('joyRing');
  var dragging = false;
  var sendTimer = null;
  var curSpeed = 0, curSteer = 90;

  function updateReadout(){
    document.getElementById('rSpeed').textContent = Math.round(curSpeed);
    document.getElementById('rSteer').textContent = Math.round(curSteer);
  }

  function pointerPos(e){
    var t = (e.touches && e.touches[0]) ? e.touches[0] : e;
    return {x:t.clientX, y:t.clientY};
  }

  function handleMove(e){
    if(!dragging) return;
    e.preventDefault();
    var rect = base.getBoundingClientRect();
    var cx = rect.left + rect.width/2;
    var cy = rect.top + rect.height/2;
    var p = pointerPos(e);
    var dx = p.x - cx, dy = p.y - cy;
    var radius = rect.width/2 - 32;
    var dist = Math.sqrt(dx*dx + dy*dy);
    if(dist > radius){ dx = dx/dist*radius; dy = dy/dist*radius; }
    stick.style.transform = 'translate(calc(-50% + ' + dx + 'px), calc(-50% + ' + dy + 'px))';

    var maxSpeed = parseInt(maxSpeedInput.value, 10);
    curSpeed = (-dy/radius) * maxSpeed;
    curSteer = 90 + (dx/radius) * 90;
    curSteer = Math.max(0, Math.min(180, curSteer));
    updateReadout();
  }

  function startDrag(e){
    dragging = true;
    ring.classList.add('active');
    handleMove(e);
    if(sendTimer) clearInterval(sendTimer);
    sendTimer = setInterval(function(){ sendControl(curSpeed, curSteer); }, 100);
  }
  function endDrag(){
    dragging = false;
    ring.classList.remove('active');
    stick.style.transform = 'translate(-50%,-50%)';
    curSpeed = 0; curSteer = 90;
    updateReadout();
    if(sendTimer){ clearInterval(sendTimer); sendTimer=null; }
    sendStop();
  }

  base.addEventListener('touchstart', startDrag, {passive:false});
  base.addEventListener('touchmove', handleMove, {passive:false});
  base.addEventListener('touchend', endDrag);
  base.addEventListener('touchcancel', endDrag);
  base.addEventListener('mousedown', startDrag);
  window.addEventListener('mousemove', handleMove);
  window.addEventListener('mouseup', function(){ if(dragging) endDrag(); });

  // ---------- D-pad ----------
  var dpadTimer = null;
  var dpadButtons = document.querySelectorAll('.dpad button[data-dir]');
  dpadButtons.forEach(function(btn){
    var dir = btn.getAttribute('data-dir');
    function press(e){
      e.preventDefault();
      btn.classList.add('pressed');
      var maxSpeed = parseInt(maxSpeedInput.value, 10);
      var speed = 0, steer = 90;
      if(dir === 'fwd'){ speed = maxSpeed; steer = 90; }
      if(dir === 'back'){ speed = -maxSpeed; steer = 90; }
      if(dir === 'left'){ speed = Math.round(maxSpeed*0.6); steer = 35; }
      if(dir === 'right'){ speed = Math.round(maxSpeed*0.6); steer = 145; }
      document.getElementById('rSpeed').textContent = speed;
      document.getElementById('rSteer').textContent = steer;
      if(dpadTimer) clearInterval(dpadTimer);
      sendControl(speed, steer);
      dpadTimer = setInterval(function(){ sendControl(speed, steer); }, 150);
    }
    function release(){
      btn.classList.remove('pressed');
      if(dpadTimer){ clearInterval(dpadTimer); dpadTimer=null; }
      document.getElementById('rSpeed').textContent = 0;
      document.getElementById('rSteer').textContent = 90;
      sendStop();
    }
    btn.addEventListener('touchstart', press, {passive:false});
    btn.addEventListener('touchend', release);
    btn.addEventListener('touchcancel', release);
    btn.addEventListener('mousedown', press);
    btn.addEventListener('mouseup', release);
    btn.addEventListener('mouseleave', release);
  });
  document.getElementById('dpadStop').addEventListener('click', sendStop);
  document.getElementById('bigStop').addEventListener('click', function(){
    if(dpadTimer){ clearInterval(dpadTimer); dpadTimer=null; }
    if(sendTimer){ clearInterval(sendTimer); sendTimer=null; }
    curSpeed = 0; curSteer = 90;
    updateReadout();
    sendStop();
  });

  // ---------- Headlight ----------
  var lightBtn = document.getElementById('lightBtn');
  var lightOn = false;
  lightBtn.addEventListener('click', function(){
    lightOn = !lightOn;
    lightBtn.classList.toggle('on', lightOn);
    fetch('/light?state=' + (lightOn ? 'on' : 'off')).catch(function(){});
  });

  // ---------- Status polling ----------
  function poll(){
    fetch('/status').then(function(r){ return r.json(); }).then(function(d){
      setConn(true);
      document.getElementById('distFront').textContent = (d.front >= 0 ? d.front.toFixed(0) : '--') + ' cm';
      document.getElementById('distRear').textContent = (d.rear >= 0 ? d.rear.toFixed(0) : '--') + ' cm';
      document.getElementById('luxVal').textContent = d.lux.toFixed(0) + ' lx';
      document.getElementById('tileFront').classList.toggle('warn', d.front >= 0 && d.front < 30);
      document.getElementById('tileRear').classList.toggle('warn', d.rear >= 0 && d.rear < 30);
    }).catch(function(){ missCount++; if(missCount > 3) setConn(false); });
  }
  setInterval(poll, 500);
  poll();
})();
</script>
</body>
</html>
)rawliteral";

// ===================================================
// Setup
// ===================================================

void setup() {

  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("========================================");
  Serial.println("        ESP32 RC CAR - PORNIRE");
  Serial.println("========================================");

  // I2C
  Wire.begin(21, 22);
  bool luxOk = lightMeter.begin();
  Serial.print("[SENZOR LUMINA BH1750] initializare -> ");
  Serial.println(luxOk ? "OK" : "EROARE");

  // WiFi Access Point
  WiFi.softAP(ssid, password);

  Serial.println("[WIFI] Access Point pornit");
  Serial.print("[WIFI] SSID: ");
  Serial.println(ssid);
  Serial.print("[WIFI] IP Address: ");
  Serial.println(WiFi.softAPIP());

  // LEDs
  pinMode(POWER_LED_PIN, OUTPUT);
  pinMode(HEADLIGHT_PIN, OUTPUT);
  Serial.println("[LED] Power LED si Far configurate (OUTPUT)");

  // Ultrasonic 1 (fata)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO1_PIN, INPUT);
  Serial.println("[ULTRASONIC FATA] pini configurati (trig=18, echo=19)");

  // Buzzer 1
  pinMode(BUZZER1_PIN, OUTPUT);
  digitalWrite(BUZZER1_PIN, HIGH);   // OFF (active LOW)
  Serial.println("[BUZZER FATA] configurat, stare initiala OFF");

  // Ultrasonic 2 (spate)
  pinMode(TRIG2_PIN, OUTPUT);
  pinMode(ECHO2_PIN, INPUT);
  Serial.println("[ULTRASONIC SPATE] pini configurati (trig=16, echo=17)");

  // Buzzer 2
  pinMode(BUZZER2_PIN, OUTPUT);
  digitalWrite(BUZZER2_PIN, HIGH);   // OFF (active LOW)
  Serial.println("[BUZZER SPATE] configurat, stare initiala OFF");

  // Motor driver
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  Serial.println("[MOTOR DRIVER] pini AIN1/AIN2/BIN1/BIN2 configurati");

  // Servo motor
  steeringServo.setPeriodHertz(50);
  steeringServo.attach(SERVO_PIN, 500, 2500);
  steeringServo.write(90);   // Centru
  Serial.println("[SERVO] atasat pe pinul 13, pozitionat la centru (90 grade)");

  // PWM setup (ESP32)
  ledcAttach(PWMA, 1000, 8);
  ledcAttach(PWMB, 1000, 8);
  Serial.println("[PWM] canale motor configurate (1000Hz, 8-bit)");

  // Motoare oprite
  driveMotors(0);

  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(TRIG2_PIN, LOW);

  // Rute server web
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/stop", handleStop);
  server.on("/light", handleLight);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("[WEB SERVER] pornit pe portul 80");
  Serial.println("[WEB SERVER] rute active: / , /control , /stop , /light , /status");

  // Animatie pornire
  for (int i = 0; i < 3; i++) {
    digitalWrite(POWER_LED_PIN, HIGH);
    delay(200);
    digitalWrite(POWER_LED_PIN, LOW);
    delay(200);
  }
  digitalWrite(POWER_LED_PIN, HIGH);
  Serial.println("[LED] Power LED PORNIT");
  Serial.println("========================================");
  Serial.println("       TOATE COMPONENTELE SUNT GATA");
  Serial.println("========================================");

  lastCommandTime = millis();
}

// ===================================================
// Control motoare / servo
// ===================================================

void driveMotors(int speed) {
  // speed: -255 (spate, full) .. 255 (fata, full)
  speed = constrain(speed, -255, 255);
  bool forward = speed >= 0;
  int pwm = abs(speed);

  digitalWrite(AIN1, forward ? HIGH : LOW);
  digitalWrite(AIN2, forward ? LOW : HIGH);
  digitalWrite(BIN1, forward ? HIGH : LOW);
  digitalWrite(BIN2, forward ? LOW : HIGH);

  ledcWrite(PWMA, pwm);
  ledcWrite(PWMB, pwm);

  currentSpeed = speed;

  Serial.print("[MOTOR] speed=");
  Serial.print(speed);
  Serial.print(" dir=");
  Serial.print(forward ? "inainte" : "inapoi");
  Serial.print(" pwm=");
  Serial.println(pwm);
}

void setSteer(int angle) {
  angle = constrain(angle, 0, 180);
  steeringServo.write(angle);
  currentSteer = angle;

  Serial.print("[SERVO] unghi=");
  Serial.print(angle);
  Serial.println(" grade");
}

// ===================================================
// Handlere HTTP
// ===================================================

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleControl() {
  Serial.print("[WEB] /control primit -> ");
  if (server.hasArg("speed")) {
    Serial.print("speed=");
    Serial.print(server.arg("speed"));
    driveMotors(server.arg("speed").toInt());
  }
  if (server.hasArg("steer")) {
    Serial.print(" steer=");
    Serial.print(server.arg("steer"));
    setSteer(server.arg("steer").toInt());
  }
  Serial.println();
  lastCommandTime = millis();
  server.send(200, "text/plain", "ok");
}

void handleStop() {
  Serial.println("[WEB] /stop primit -> oprire completa");
  driveMotors(0);
  setSteer(90);
  lastCommandTime = millis();
  server.send(200, "text/plain", "ok");
}

void handleLight() {
  if (server.hasArg("state")) {
    headlightManual = true;
    headlightOn = (server.arg("state") == "on");
    digitalWrite(HEADLIGHT_PIN, headlightOn ? HIGH : LOW);

    Serial.print("[FAR] comanda manuala -> ");
    Serial.println(headlightOn ? "PORNIT" : "OPRIT");
  }
  server.send(200, "text/plain", "ok");
}

void handleStatus() {
  char buf[128];
  snprintf(buf, sizeof(buf),
    "{\"front\":%.1f,\"rear\":%.1f,\"lux\":%.1f,\"light\":%s}",
    distanceFront, distanceRear, lux, headlightOn ? "true" : "false");
  server.send(200, "application/json", buf);

  Serial.print("[WEB] /status trimis -> ");
  Serial.println(buf);
}

// ===================================================
// Ultrasonic (functie generica pentru ambii senzori)
// ===================================================

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return -1;
  return duration * 0.0343 / 2.0;
}

// ===================================================
// Buzzer non-blocant (functioneaza in fundal, fara sa
// blocheze serverul web sau restul loop-ului)
// ===================================================

// ===================================================
// Buzzer non-blocant - zona activa 30cm -> 5cm
//
// Functioneaza ca un senzor de parcare real: bipuri
// SCURTE (30ms) cu pauza intre ele. Pauza se micsoreaza
// neliniar (nu simetric) pe masura ce distanta scade,
// astfel incat lucrurile devin vizibil mai "dense" si
// mai usor de simtit chiar langa pragul periculos (5cm),
// nu doar liniar intre 30 si 5cm.
// ===================================================

#define BUZZER_FAR_CM     30.0   // peste asta -> tacere completa
#define BUZZER_NEAR_CM    5.0    // sub asta -> ON continuu (pericol)
#define BEEP_ON_TIME_MS   30     // durata unui bip (fix)
#define BEEP_GAP_MAX_MS   450    // pauza intre bipuri la 30cm (rar)
#define BEEP_GAP_MIN_MS   25     // pauza intre bipuri la 5cm (foarte des)
#define BEEP_CURVE_POWER  1.8    // >1 => se accelereaza mult spre 5cm (progresiv)

void updateBuzzer(float distance, int buzzerPin, bool &state, unsigned long &lastToggle, const char* label) {
  unsigned long now = millis();

  // --- Prea departe: tacere ---
  if (distance < 0 || distance > BUZZER_FAR_CM) {
    digitalWrite(buzzerPin, HIGH);  // OFF (active LOW)
    if (state) {
      Serial.print("[BUZZER ");
      Serial.print(label);
      Serial.println("] OPRIT (fara obstacol in raza 30cm)");
    }
    state = false;
    return;
  }

  // --- Foarte aproape: ON continuu, pericol iminent ---
  if (distance <= BUZZER_NEAR_CM) {
    digitalWrite(buzzerPin, LOW);   // ON continuu
    if (!state) {
      Serial.print("[BUZZER ");
      Serial.print(label);
      Serial.print("] ON CONTINUU - pericol iminent (<=");
      Serial.print(BUZZER_NEAR_CM);
      Serial.println("cm)");
    }
    state = true;
    return;
  }

  // --- Zona progresiva 30cm -> 5cm: bipuri scurte, pauza variabila ---
  float t = (distance - BUZZER_NEAR_CM) / (BUZZER_FAR_CM - BUZZER_NEAR_CM); // 0 (aproape) .. 1 (departe)
  t = constrain(t, 0.0, 1.0);
  float curved = pow(t, BEEP_CURVE_POWER);
  int gapTime = BEEP_GAP_MIN_MS + (int)(curved * (BEEP_GAP_MAX_MS - BEEP_GAP_MIN_MS));

  if (state) {
    // Bipul e in curs -> il oprim dupa BEEP_ON_TIME_MS
    if (now - lastToggle >= BEEP_ON_TIME_MS) {
      digitalWrite(buzzerPin, HIGH); // OFF intre bipuri
      state = false;
      lastToggle = now;
    }
  } else {
    // Suntem in pauza -> pornim urmatorul bip cand a trecut gapTime
    if (now - lastToggle >= (unsigned long)gapTime) {
      digitalWrite(buzzerPin, LOW);  // ON - incepe bipul
      state = true;
      lastToggle = now;

      Serial.print("[BUZZER ");
      Serial.print(label);
      Serial.print("] bip, distanta=");
      Serial.print(distance);
      Serial.print("cm pauza_urmatoare=");
      Serial.print(gapTime);
      Serial.println("ms");
    }
  }
}

// ===================================================
// Loop
// ===================================================

void loop() {

  server.handleClient();

  unsigned long now = millis();

  // --- Siguranta: opreste masina daca nu mai primeste comenzi ---
  if (now - lastCommandTime > COMMAND_TIMEOUT && currentSpeed != 0) {
    Serial.println("[SIGURANTA] Timeout comenzi - conexiune pierduta, opresc motoarele!");
    driveMotors(0);
  }

  // --- Senzori de distanta + buzzere (non-blocant) ---
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    distanceFront = getDistance(TRIG_PIN, ECHO1_PIN);
    distanceRear  = getDistance(TRIG2_PIN, ECHO2_PIN);

    Serial.print("[ULTRASONIC FATA] ");
    if (distanceFront < 0) Serial.println("fara ecou");
    else { Serial.print(distanceFront); Serial.println(" cm"); }

    Serial.print("[ULTRASONIC SPATE] ");
    if (distanceRear < 0) Serial.println("fara ecou");
    else { Serial.print(distanceRear); Serial.println(" cm"); }

    updateBuzzer(distanceFront, BUZZER1_PIN, buzzer1State, buzzer1LastToggle, "FATA");
    updateBuzzer(distanceRear,  BUZZER2_PIN, buzzer2State, buzzer2LastToggle, "SPATE");
  }

  // --- Senzor de lumina + faruri automate (daca nu sunt controlate manual) ---
  if (now - lastLuxRead >= LUX_INTERVAL) {
    lastLuxRead = now;
    lux = lightMeter.readLightLevel();

    Serial.print("[LUMINA] ");
    Serial.print(lux);
    Serial.println(" lx");

    if (!headlightManual) {
      if (lux < 15 && !headlightOn) {
        headlightOn = true;
        digitalWrite(HEADLIGHT_PIN, HIGH);
        Serial.println("[FAR] AUTO -> PORNIT (lumina scazuta)");
      }
      else if (lux > 25 && headlightOn) {
        headlightOn = false;
        digitalWrite(HEADLIGHT_PIN, LOW);
        Serial.println("[FAR] AUTO -> OPRIT (lumina suficienta)");
      }
    }
  }
}
