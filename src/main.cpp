#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include "RGBdriver.cpp"

//---------------------------------------------------------------
// Value for sides
#define LEFT 0
#define RIGHT 1
#define BOTH 2

// Possible position
#define DOWN 0
#define MIDDLE 1
#define UP 2

// Ms to go to new position
#define DELAY_DOWN 800
#define DELAY_MIDDLE 400
#define DELAY_UP 0

#define GPIO_LEFT D5
#define GPIO_LEFT_UP_DOWN D6
//#define GPIO_LEFT_DASHBOARD D7
#define GPIO_LEFT_LIGHT D4
#define GPIO_RIGHT D0
#define GPIO_RIGHT_UP_DOWN D1
//#define GPIO_RIGHT_DASHBOARD D2
#define GPIO_RIGHT_LIGHT D3
#define GPIO_DASHBOARD D2

// Value to open first relay (open or close 12v)
#define EN_LEFT LOW
#define EN_RIGHT LOW
#define DIS_LEFT HIGH
#define DIS_RIGHT HIGH

// Value to open second relay to up
#define UP_LEFT LOW
#define UP_RIGHT LOW
#define DOWN_LEFT HIGH
#define DOWN_RIGHT HIGH

// Value to open circuit to dashbaord
//#define DIS_LEFT_DASHBOARD LOW
//#define DIS_RIGHT_DASHBOARD LOW
#define DIS_DASHBOARD LOW

// Light
#define EN_LEFT_LIGHT LOW
#define DIS_LEFT_LIGHT HIGH
#define EN_RIGHT_LIGHT LOW
#define DIS_RIGHT_LIGHT HIGH

// Led strip
#define CLK D8// CIN (Drv)
#define DIO D7// DIN

RGBdriver Driver(CLK,DIO);

// SSID and Password of your WiFi router
const char* ssid = "MIATA";
const char* password = "123456789";
const char* host = "OTA";

// Delimiter to split commands received by Websocket from client
const char* delimiter = ":";

ESP8266WebServer httpServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdater;

uint8_t left_pos;
uint8_t right_pos;
uint8_t left_direction;
uint8_t right_direction;

uint8_t left_lastPos;
uint8_t right_lastPos;

uint16_t pos_delay[3] = {DELAY_DOWN,DELAY_MIDDLE,DELAY_UP};

void light(uint8_t state, uint8_t side){
  //char param[10] = "light;";
  //strcat(param, state?"1":"0");
  //strcat(param, ";");
  //strcat(param, (char*)side);



  switch(side){
    case LEFT:
      if(state==1)   // Enable light
        digitalWrite(GPIO_LEFT_LIGHT, EN_LEFT_LIGHT);
      else        // Disable light
        digitalWrite(GPIO_LEFT_LIGHT, DIS_LEFT_LIGHT);
      break;
    case RIGHT:
      if(state==1)   // Enable light
        digitalWrite(GPIO_RIGHT_LIGHT, EN_RIGHT_LIGHT);
      else        // Disable light
        digitalWrite(GPIO_RIGHT_LIGHT, DIS_RIGHT_LIGHT);
      break;
    case BOTH:
      if(state==1){  // Enable light
        digitalWrite(GPIO_LEFT_LIGHT, EN_LEFT_LIGHT);
        digitalWrite(GPIO_RIGHT_LIGHT, EN_RIGHT_LIGHT);
      } else {    // Disable light
        digitalWrite(GPIO_LEFT_LIGHT, DIS_LEFT_LIGHT);
        digitalWrite(GPIO_RIGHT_LIGHT, DIS_RIGHT_LIGHT);
      }
      break;
  }

  //webSocket.broadcastTXT(param);
}


void moveOneSide(uint8_t direction, uint8_t side, uint8_t en_side_light, uint8_t side_direction, uint8_t gpio_side_up_down,
   uint8_t gpio_side, uint8_t en_side, uint8_t &side_pos){
  // If second click on up --> light on
  if(side_pos == UP && direction == UP){
      light(en_side_light,side);
    }
  if(side_pos != direction){
    // Set direction
    digitalWrite(gpio_side_up_down,side_direction);
    // Enable motor
    digitalWrite(gpio_side,en_side);
  }
}

void stopOneSide(uint8_t gpio_side,uint8_t dis_side, uint8_t direction, uint8_t &side_pos, uint8_t &side_lastPos){
  // Stop motor
  digitalWrite(gpio_side,dis_side);

  // Update position
  side_lastPos = side_pos;
  side_pos = direction;
}


void move(uint8_t direction, uint8_t side){
  // If position is middle --> enable motor to end its movement
  // Motor work with circular method (is is not possible to inverse the direction)
  if(left_pos == MIDDLE && right_pos == MIDDLE){
    digitalWrite(GPIO_LEFT,EN_LEFT);
    digitalWrite(GPIO_RIGHT,EN_RIGHT);

    delay(DELAY_DOWN - DELAY_MIDDLE);
    digitalWrite(GPIO_LEFT,DIS_LEFT);
    digitalWrite(GPIO_RIGHT,DIS_RIGHT);

    // Update
    // if were up --> down
    // if were down --> up
    if(left_pos == MIDDLE) left_pos = 2-left_lastPos;
    if(right_pos == MIDDLE) right_pos = 2-right_lastPos;

  }



  // Set pin corresponding to direction
  if(direction == UP){           // UP
    left_direction = UP_LEFT;
    right_direction = UP_RIGHT;

  } else if(direction == DOWN){  // DOWN
    left_direction = DOWN_LEFT;
    right_direction = DOWN_RIGHT;
  } else if(direction == MIDDLE){ // MIDDLE

    if(left_pos == UP) left_direction = DOWN_LEFT;
    else left_direction = UP_LEFT;

    if(right_pos == UP) right_direction = DOWN_RIGHT;
    else right_direction = UP_RIGHT;
  }


  switch(side) {
    // LEFT
    case LEFT :
      moveOneSide(direction, LEFT, EN_LEFT_LIGHT, left_direction, GPIO_LEFT_UP_DOWN, GPIO_LEFT, EN_LEFT, left_pos);
      delay(abs(pos_delay[left_pos]-pos_delay[direction]));
      stopOneSide(GPIO_LEFT, DIS_LEFT, direction, left_pos, left_lastPos);
    break;

    // RIGHT
    case RIGHT  :
        moveOneSide(direction, RIGHT, EN_RIGHT_LIGHT, right_direction, GPIO_RIGHT_UP_DOWN, GPIO_RIGHT, EN_RIGHT, right_pos);
        delay(abs(pos_delay[right_pos]-pos_delay[direction]));
        stopOneSide(GPIO_RIGHT, DIS_RIGHT, direction, right_pos, right_lastPos);
      break;

    // LEFT & RIGHT
    case BOTH :
      uint16_t delayLeft = abs(pos_delay[left_pos]-pos_delay[direction]);
      uint16_t delayRight = abs(pos_delay[right_pos]-pos_delay[direction]);

      // Move
      moveOneSide(direction, LEFT, EN_LEFT_LIGHT, left_direction, GPIO_LEFT_UP_DOWN, GPIO_LEFT, EN_LEFT, left_pos);
      moveOneSide(direction, RIGHT, EN_RIGHT_LIGHT, right_direction, GPIO_RIGHT_UP_DOWN, GPIO_RIGHT, EN_RIGHT, right_pos);

      // Check which side to stop first
      if(delayLeft < delayRight){
        delay(delayLeft);
        stopOneSide(GPIO_LEFT, DIS_LEFT, direction, left_pos, left_lastPos);
        delay(delayRight-delayLeft);
        stopOneSide(GPIO_RIGHT, DIS_RIGHT, direction, right_pos, right_lastPos);
      } else {
        delay(delayRight);
        stopOneSide(GPIO_RIGHT, DIS_RIGHT, direction, right_pos, right_lastPos);
        delay(delayLeft-delayRight); // When delayLeft == delayRight --> delay(0)
        stopOneSide(GPIO_LEFT, DIS_LEFT, direction, left_pos, left_lastPos);
      }

      break;
  }

  // When light are down, disable light
  if (left_pos == DOWN) light(false, LEFT);
  if (right_pos == DOWN) light(false, RIGHT);
}

void HTTPUpdateConnect() {
  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
    Serial.printf("[%u] Disconnected!\n", num);
    break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
    }
    break;
    case WStype_TEXT:                   // if new text data is received

    char *_payload = (char *) &payload[0];
    char *command = strtok(_payload, delimiter);
    if(strcmp(command,"move") == 0){
      char *direction = strtok(NULL, delimiter);
      char *side = strtok(NULL, delimiter);
      move(((int)direction[0]-48),((int)side[0]-48));
    }

    if(strcmp(command,"led") == 0){
      char *hexstring = strtok(NULL, delimiter);

      // Convert hexadecimal string to long
      long long number = strtoll( &hexstring[1], NULL, 16);
      // Split them up into r, g, b values
      long long r = number >> 16;
      long long g = number >> 8 & 0xFF;
      long long b = number & 0xFF;

      // Serial.printf("%d, %d, %d\n", (int)r, (int)g, (int)b);

      Driver.begin(); // begin
      Driver.SetColor((int)r, (int)g, (int)b); //Red. first node data
      //Driver.SetColor((int)r, (int)g, (int)b); //Red. second node data
      Driver.end();
    }

    if(strcmp(command,"light") == 0){
      char *state = strtok(NULL, delimiter);
      char *side = strtok(NULL, delimiter);
        light((int)state, (int)side);
    }
    break;
    }
}

//===============================================================
// This routine is executed when you open its IP in browser
//===============================================================
bool loadFromSpiffs(String path){
  String dataType = "text/plain";


  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (httpServer.hasArg("download")) dataType = "application/octet-stream";
  if (httpServer.streamFile(dataFile, dataType) != dataFile.size()) {
  }

  dataFile.close();
  return true;
}

void handleWebRequests(){
  if(loadFromSpiffs(httpServer.uri())) return;
  String message = "File Not Detected\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i=0; i<httpServer.args(); i++){
    message += " NAME:"+httpServer.argName(i) + "\n VALUE:" + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  Serial.println(message);
}



void handleRoot() {
  Serial.println("You called root page");
  //String s = MAIN_page; //Read HTML contents
  //httpServer.send(200, "text/html", s); //Send web page
  httpServer.sendHeader("Location", "/index.html",true);   //Redirect to our html web page
  httpServer.send(302, "text/plane","");
}

//===============================================================
//                  SETUP
//===============================================================
void setup(void){
  // Onboard port Direction output
  pinMode(GPIO_LEFT,OUTPUT);
  pinMode(GPIO_LEFT_UP_DOWN,OUTPUT);
  //pinMode(GPIO_LEFT_DASHBOARD, OUTPUT);
  pinMode(GPIO_LEFT_LIGHT, OUTPUT);
  pinMode(GPIO_RIGHT,OUTPUT);
  pinMode(GPIO_RIGHT_UP_DOWN,OUTPUT);
  //pinMode(GPIO_RIGHT_DASHBOARD, OUTPUT);
  pinMode(GPIO_RIGHT_LIGHT, OUTPUT);

  pinMode(GPIO_DASHBOARD, OUTPUT);

  // Power on state off
  digitalWrite(GPIO_LEFT,DIS_LEFT);
  digitalWrite(GPIO_RIGHT, DIS_RIGHT);

  // Open circuit to dashboard
  //digitalWrite(GPIO_LEFT_DASHBOARD, DIS_LEFT_DASHBOARD);
  //digitalWrite(GPIO_LEFT_DASHBOARD, DIS_RIGHT_DASHBOARD);
  pinMode(GPIO_DASHBOARD, DIS_DASHBOARD);

  // Close light circuit
  digitalWrite(GPIO_LEFT_LIGHT, EN_LEFT_LIGHT);
  digitalWrite(GPIO_RIGHT_LIGHT, EN_RIGHT_LIGHT);
  // Open serial
  Serial.begin(115200);
  Serial.println("");

  // Initialize File System
  SPIFFS.begin();
  Serial.println("File System Initialized");

  // WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP);           //Only Access point

  boolean result = WiFi.softAP(ssid, password);  //Start HOTspot removing password will disable security

  if(!result) Serial.println("Issue starting Wifi AP");
  //Serial.print(result);
  IPAddress myIP = WiFi.softAPIP(); //Get IP address
  Serial.print("HotSpt IP:");
  Serial.println(myIP);


  if (MDNS.begin("miata")) {
    Serial.println("MDNS responder started");
  }


  //Initialize Webserver
  httpServer.on("/",handleRoot);
  httpServer.onNotFound(handleWebRequests); //Set setver all paths are not found so we can handle as per URI
  httpServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("HTTP server started");

  HTTPUpdateConnect();

  Driver.begin(); // begin
  Driver.SetColor(0, 0, 0); //Red. first node data
  //Driver.SetColor((int)r, (int)g, (int)b); //Red. second node data
  Driver.end();

  move(DOWN, BOTH);
}



//===============================================================
//                     LOOP
//===============================================================
void loop(void){
  webSocket.loop();
  httpServer.handleClient();
}
