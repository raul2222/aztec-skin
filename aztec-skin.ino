#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include <Fonts/Picopixel.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "font5x.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "function.h"

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

int VAR=0;

const char* ssid = "Robot";
const char* password = "123456789";

AsyncWebServer server(80); // Create an instance of the server

float batteryPercentage = 0.0;
#define NUM_LINES 16

struct LoadingLine {
  int position;
  int direction;
  int length;
};

LoadingLine lines[NUM_LINES];



void drawBatteryBar(float batteryPercentage) {
  int totalPixels = 10; // Total de píxeles en la barra de progreso
  int litPixels = round(batteryPercentage / 100 * totalPixels); // Cantidad de píxeles que deben estar encendidos

  int startX = (matrix.width() - totalPixels) / 2; // Punto de inicio de la barra de progreso
  int startY = matrix.height() / 2; // Punto de inicio de la barra de progreso

  // Limpiar los píxeles de la barra de progreso
  for (int i = 0; i < totalPixels; i++) {
    matrix.drawPixel(startX + i, startY, matrix.Color(0, 0, 0));
  }

  // Encender los píxeles correspondientes al porcentaje de la batería
  for (int i = 0; i < litPixels; i++) {
    matrix.drawPixel(startX + i, startY, matrix.Color(0, 45, 0));
  }
}



void setup() {
  init();
  Serial.begin(115200);       
  matrix.begin();

  adc1_config_width(ADC_WIDTH_BIT_12); // Configurar la resolución del ADC a 12 bits
  adc1_config_channel_atten(ADC1_GPIO34_CHANNEL, ADC_ATTEN_DB_2_5); 
  matrix.setTextWrap(false);
  matrix.setBrightness(30);  // Reduced brightness.
  matrix.setFont(&Font5x7FixedMono);



  
  for (int i = 0; i < NUM_LINES; ++i) {
    lines[i].length = random(3, 10);
    lines[i].position = random(0, matrix.width() - lines[i].length);
    lines[i].direction = (i % 2 == 0) ? 1 : -1;
  }


  WiFi.begin(ssid, password);

  // Connect to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

 

    // Set a static IP address
  IPAddress ip(192, 168, 0, 196); // Set your desired IP
  IPAddress gateway(192, 168, 0, 1); // Set your network Gateway usually it is your Router IP
  IPAddress subnet(255, 255, 255, 0); // Set your network subnet mask
  IPAddress dns(192, 168, 0, 1); // Set your network DNS, usually it is your Router IP
  WiFi.config(ip, dns, gateway, subnet);

  Serial.println(WiFi.localIP());

  

  server.on("/aztec", HTTP_GET, [](AsyncWebServerRequest *request){
    VAR= 0;
    request->send(200, "text/plain", "AZTEC animation started");
  });

  // Add more endpoints for each animation you want to control
  server.on("/aztec2", HTTP_GET, [](AsyncWebServerRequest *request){
    VAR= 2;
    request->send(200, "text/plain", "AZTEC2 animation started");
  });

  server.on("/aztecl", HTTP_GET, [](AsyncWebServerRequest *request){
    VAR= 6;
    request->send(200, "text/plain", "AZTEC2 animation started");
  });

  server.begin();


  if(xTaskCreatePinnedToCore( task_battery , "task_battery", 4096, NULL, 2, NULL,1) != pdPASS){
      Serial.println("Error en creacion tarea task_loopcontr");
      exit(-1);
  }
}




void loop() {

  if (VAR==0 ){
    aztec();
  } 
  if (VAR==2 ){
    aztec2();
  } 
  if (VAR==6 ){
    waitingAnimation();
  } 
  //
  //
  //aztec3();
  //aztec4();
  //aztec5();
      if(WiFi.status() != WL_CONNECTED){
        Serial.println("WiFi connection lost. Trying to reconnect...");
        WiFi.begin(ssid, password);
        
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.println("Reconnecting to WiFi...");
        }
 
        Serial.println("Reconnected to WiFi");
    }
}


void waitingAnimation() {
  matrix.fillScreen(0);  // Clear the screen
  
  // Move and draw each loading line
  for (int i = 0; i < NUM_LINES; ++i) {
    LoadingLine &line = lines[i];
    
    // Draw the loading line
    for (int j = 0; j < line.length; ++j) {
      int x = line.position + j * line.direction;
      if (x >= 0 && x < matrix.width()) {
        matrix.drawPixel(x, i, matrix.Color(60, 0, 0));
      }
    }
    
    // Update the position of the loading line
    line.position += line.direction;
    if (line.position == matrix.width() - line.length || line.position == 0) {
      line.direction *= -1;
    }
  }
  
  matrix.show();  // Update the screen
  delay(100);  // Wait a bit before the next frame
}


void task_battery(void* arg) {

  while(1) {    
    int i = 0;
    voltage = 0;
    for (i=0;i<20;i++){ 
      int sensorValue = adc1_get_raw(ADC1_GPIO34_CHANNEL);

      // Convertir la lectura del sensor a voltaje
      voltage = voltage + sensorValue * (ADC_REF_VOLTAGE / ADC_RESOLUTION);
      delay(1);
    } 

    voltage = voltage / i;
    Serial.println(voltage);
      // Considerar el divisor de voltaje
    voltage = voltage / (R2 / (R1 + R2));
    Serial.println(voltage);
    // Asegurarse de que el voltaje no está fuera del rango esperado
    voltage = constrain(voltage, BATTERY_EMPTY, BATTERY_FULL);

    // Convertir el voltaje a un porcentaje de la batería
    batteryPercentage = (voltage - BATTERY_EMPTY) / (BATTERY_FULL - BATTERY_EMPTY) * 100;

    // Imprimir el porcentaje de la batería
    Serial.print("Battery: ");
    Serial.print(batteryPercentage);
    Serial.println(" %");


    vTaskDelay( portTICK_PERIOD_MS*2000);
  }
}


void aztec() {
  matrix.fillScreen(0);
  matrix.setCursor(0, 0);

  matrix.setTextColor(matrix.Color(60, 0, 0));  
  // Draw "AZTEC" in the center.
  int x = (matrix.width() - 15*2 - (6*5)) / 2 + 15;  // Calculate the starting x position for the text.
  
  matrix.setCursor(x, 7);
  matrix.println("AZTEC");
  
  // Draw "ROBOT" below "AZTEC".
  matrix.setCursor(x, 16);  // Change the y position to draw the text on the second row.
  matrix.println("ROBOT");
  
  // Draw the bouncing water drop animation.
  drawBouncingWaterDrops();

  // Draw the battery bar
  drawBatteryBar(batteryPercentage);

  matrix.show();
  delay(100);
}

