#include <Wire.h>
#include "paj7620.h"
#include <LOLIN_I2C_BUTTON.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LOLIN_I2C_MOTOR.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


#define GES_REACTION_TIME 500
#define GES_ENTRY_TIME 800
#define GES_QUIT_TIME 1000
#define I2C_ADDRESS 0x43
#define I2C_ADDRESS2 0x44

#define OLED_RESET -1

#define PWM_FREQUENCY 1000


#define LISTEN_PORT 80

MDNSResponder mdns;
ESP8266WebServer server(LISTEN_PORT);
String webPage = "<h1>WiFi control</h1>";

Adafruit_SSD1306 display(OLED_RESET);

I2C_BUTTON button(DEFAULT_I2C_BUTTON_ADDRESS); //I2C Address 0x31

LOLIN_I2C_MOTOR motor(DEFAULT_I2C_MOTOR_ADDRESS);

const char* ssid = "NDL_24G"; // name of local WiFi network in the NDL
const char* password = "RT-AC66U"; // the password of the WiFi network

bool gameover, motor_ran;
int x, y, fruit_x, fruit_y, score;
const int WIDTH = 64;
const int HEIGHT = 48;
int tail_x[100], tail_y[100];
int ntail;
enum directions {STOP=0, LEFT, RIGHT, UP, DOWN};
directions dir;
int GOAL = 1;
int DELAY1 = 200;
int DELAY2 = 1000;
unsigned long lastTime1 = 0; 
unsigned long lastTime2 = 0; 

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  Serial.begin(115200);

  webPage += "<p><a href=\"button1\">";
  webPage += "<button style=\"background-color:green;color:white;\">";
  webPage += "RESET</button></a></p>";


  webPage += "<p><a href=\"button2\">";
  webPage += "<button style=\"background-color:blue;color:white;\">";
  webPage += "Increase goal</button></a></p>";


  webPage += "<p><a href=\"button3\">";
  webPage += "<button style=\"background-color:blue;color:white;\">";
  webPage += "Decrease goal</button></a></p>";

  // webPage += "<label for="goal">GOAL (1-50):</label>"
  // webPage += "<input type="number" id="goal" name="goal" min="1" max="50" />"


  
  WiFi.begin(ssid, password); // make the WiFi connection
  Serial.println("Start connecting.");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  Serial.print("Connected to ");
  Serial.print(ssid);
  Serial.print(". IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
  server.on("/", [](){
    server.send(200, "text/html", webPage);
  });
  server.on("/button1", [](){
    server.send(200, "text/html", webPage);
    ntail = 0;
    motor_ran = false;
    gameover = false;
    dir = STOP;
    x = WIDTH / 2;
    y = HEIGHT / 2;
    fruit_x = random(1, WIDTH-1);
    fruit_y = random(1, HEIGHT-1);
    score = 0;
    display.clearDisplay();
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_STANDBY);
    
    Serial.print("reset");
  });
  server.on("/button2", [](){
    server.send(200, "text/html", webPage);
    if (GOAL<=50){
      GOAL++;
      Serial.print("goal increased");
    }
  });
    server.on("/button3", [](){
    server.send(200, "text/html", webPage);
    if(GOAL>1){
      GOAL--;
      Serial.print("goal decreased");
    }
  });
  
  server.begin(); // start the server for WiFi input
  Serial.println("HTTP server started");

  uint8_t error = paj7620Init(); // initialize Paj7620 registers
  if (error) {
    Serial.print("INIT ERROR,CODE: ");
    Serial.println(error);
  }

  motor_ran = false;
  gameover = false;
  dir = STOP;
  x = WIDTH / 2;
  y = HEIGHT / 2;
  fruit_x = random(1, WIDTH-1);
  fruit_y = random(1, HEIGHT-1);
  score = 0;
  

 //wait until motor shield ready.
 while (motor.PRODUCT_ID != PRODUCT_ID_I2C_MOTOR) {
 motor.getInfo();
 }
}


bool check_new_position(int x_new, int y_new){
  if (x_new<0||x_new>=WIDTH || y_new<0 || y_new>= HEIGHT){
    return false;
  } 
  for (int i = 0; i < ntail; i++){
    if (x_new == tail_x[ntail] && y_new == tail_y[ntail]){
      return false;
    }
  }
  return true;
}

void step(){
  int x_new , y_new;
  switch (dir){
    case LEFT:
      x_new = x-1;
      y_new = y;
      break;
    case RIGHT:
      x_new = x+1;
      y_new = y;
      break;
    case UP:
      x_new = x;
      y_new = y-1;
      break;
    case DOWN:
      x_new = x;
      y_new = y+1; 
      break;    
  }
  if (!check_new_position(x_new, y_new)){
    gameover = true;
  } else{
    if (fruit_x == x_new && fruit_y == y_new){
      ntail++;
      score++;
      fruit_x = random(1, WIDTH-1);
      fruit_y = random(1, HEIGHT-1);
      tail_x[ntail] = x;
      tail_y[ntail] = y;
    } else{
      for(int i = 0; i< ntail; i++){
        tail_x[i] = tail_x[i+1];
        tail_y[i] = tail_y[i+1];
      } 
      tail_x[ntail] = x;
      tail_y[ntail] = y;
    }
    x = x_new;
    y = y_new;
  }
}

directions change_dir(){
  uint8_t data = 0, data1 = 0, error;
  error = paj7620ReadReg(I2C_ADDRESS, 1, &data); // Read gesture result.
  if (!error) {
    switch (data) {
      case GES_RIGHT_FLAG:
        if(dir != LEFT){
          return RIGHT;
        }
      case GES_LEFT_FLAG:
        if(dir != RIGHT){
          return LEFT;
        }
      case GES_UP_FLAG:
        if(dir != DOWN){
          return UP;
        }
      case GES_DOWN_FLAG:
        if(dir != UP) {
          return DOWN;
        }
      default:
        return dir;
    }
  }
  return dir;
}


void loop() {
  server.handleClient();
  mdns.update();

  while(dir == STOP){
    dir = change_dir();
  }
  while(!gameover){
    if(millis() - DELAY1 > lastTime1){
      display.clearDisplay();

      dir = change_dir();
      step();
      display.drawRect(0,0,WIDTH,HEIGHT,1);
      display.drawPixel(fruit_x, fruit_y, 1);
      display.drawPixel(x,y,1);
      for (int i=0; i<ntail; i++){
        display.drawPixel(tail_x[i], tail_y[i],1);
      }
      display.display();
      lastTime1 = millis(); 
    }
  }
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.print("Game Over score: ");
  display.println(score);
  display.display();
  if (score >= GOAL && !motor_ran){
    motor.changeFreq(MOTOR_CH_BOTH, PWM_FREQUENCY);
    motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CCW);
    motor.changeDuty(MOTOR_CH_A, 50);
    lastTime2 = millis();
    while(!motor_ran){
      if(millis() - DELAY2 > lastTime2){
        motor.changeStatus(MOTOR_CH_A, MOTOR_STATUS_STANDBY);
        motor_ran = true;
      }
    }
   }
}
