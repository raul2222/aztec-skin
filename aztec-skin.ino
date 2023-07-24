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

// Agrega esto al inicio de tu archivo
#include <ArduinoJson.h>
#define ADC1_GPIO34_CHANNEL ADC1_CHANNEL_6
// Este es el tamaño máximo esperado del JSON que estás recibiendo. Ajusta según sea necesario.
#define MAX_JSON_SIZE 4096

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

struct Particle {
  int x, y; // Posición de la partícula
  int r, g, b; // Color de la partícula
  int speed; // Velocidad de la partícula
};
#define PARTICLE_THRESHOLD 200
#define MAX_PARTICLES 1000  
Particle particles[MAX_PARTICLES];
int numParticles = 0;


void drawBatteryBar(float batteryPercentage) {
  if(VAR==0){
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

}



void setup() {
  delay(10000);
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

  server.on("/skeleton", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    VAR=7;
    StaticJsonDocument<MAX_JSON_SIZE> doc;
    DeserializationError err = deserializeJson(doc, data);

    if (err) {
      request->send(400, "application/json", "{\"message\":\"Invalid JSON\"}");
      return;
    }

    // Clear the matrix at the start of each frame
    //clearMatrix();

    // Assume the JSON structure you receive is as follows:
    // [
    //   [[x1, y1], [x2, y2], [r, g, b]],
    //   ...
    // ]
    // Print the whole document
    //String docAsString;
    //serializeJsonPretty(doc, docAsString);
    //Serial.println(docAsString);
    JsonArray lines = doc.as<JsonArray>();
    for (JsonVariant line : lines) {
      JsonArray pos1 = line[0].as<JsonArray>();
      JsonArray pos2 = line[1].as<JsonArray>();
      JsonArray color = line[2].as<JsonArray>();
      spawnParticle(pos2[0], pos2[1], color[0], color[1], color[2]);
      spawnParticle(pos1[0], pos1[1], color[0], color[1], color[2]);
      // Now you have to use pos1, pos2, and color as you wish. Here I am just drawing them on the matrix
      // drawLineOnMatrix(pos1[0], pos1[1], pos2[0], pos2[1], color[0], color[1], color[2]);
    }
    request->send(200, "application/json", "{\"message\":\"Received skeleton lines\"}");
    
    // Update the matrix
    //matrix.show();

    
  });


  server.begin();


  if(xTaskCreatePinnedToCore( task_battery , "task_battery", 4096, NULL, 2, NULL,1) != pdPASS){
      Serial.println("Error en creacion tarea task_loopcontr");
      exit(-1);
  }
}


void spawnParticle(int pos1_x, int pos1_y, int r, int g, int b) {
  // Comprobar si ya se ha alcanzado el número máximo de partículas
  if (numParticles >= MAX_PARTICLES) {
    return;
  }

  // Convertir las coordenadas a la escala de la matriz
  int pos1_x_scaled = 63 - ((pos1_x * (63 - 0) / 320) + 0);

  int pos1_y_scaled = pos1_y * mh / 320;

  // Crear una nueva partícula
  particles[numParticles].x = pos1_x_scaled;
  particles[numParticles].y = pos1_y_scaled;
  particles[numParticles].r = r;
  particles[numParticles].g = g;
  particles[numParticles].b = b;
  particles[numParticles].speed = random(1, 3);

  // Incrementar el contador de partículas
  numParticles++;
}

// Función para determinar si el usuario está haciendo bien el ejercicio
bool isDoingWell() {
  if (numParticles > PARTICLE_THRESHOLD) {  // PARTICLE_THRESHOLD es un valor que tú defines
    return true;
  } else {
    return false;
  }
}

void displayFeedback() {
  // Colores para el feedback
  int colorRed = matrix.Color(255, 0, 0);  // Rojo
  int colorBlue = matrix.Color(0, 0, 255);  // Azul

  // Rellenar los bordes con los colores
  for (int i = 0; i < matrix.width(); i++) {
    int color = (i % 2 == 0) ? colorRed : colorBlue;
    matrix.drawPixel(i, 0, color);  // Borde superior
    matrix.drawPixel(i, matrix.height() - 1, color);  // Borde inferior
  }
  for (int i = 0; i < matrix.height(); i++) {
    int color = (i % 2 == 0) ? colorRed : colorBlue;
    matrix.drawPixel(0, i, color);  // Borde izquierdo
    matrix.drawPixel(matrix.width() - 1, i, color);  // Borde derecho
  }

  // Actualizar la matriz
  matrix.show();
}

void updateMatrix() {
  // Borrar la matriz
  matrix.fillScreen(0);
  displayFeedback();
  // Para cada partícula
  for (int i = 0; i < numParticles; i++) {
    // Mover la partícula hacia abajo
    particles[i].y += particles[i].speed;

    // Cambiar el color de la partícula aleatoriamente
    int color = random(1, 3); // Genera un número entre 1 y 3
    switch (color) {
      case 1: // Rojo
        particles[i].r = 255;
        particles[i].g = 0;
        particles[i].b = 0;
        break;
      case 2: // Azul
        particles[i].r = 0;
        particles[i].g = 0;
        particles[i].b = 255;
        break;
      case 3: // Blanco
        particles[i].r = 255;
        particles[i].g = 255;
        particles[i].b = 255;
        break;
    }

    // Comprobar si la partícula ha salido de la matriz
    if (particles[i].x < 0 || particles[i].x >= matrix.width() || particles[i].y < 0 || particles[i].y >= matrix.height()) {
      // Si es así, mover la partícula al final del array y reducir numParticles
      particles[i] = particles[numParticles - 1];
      numParticles--;
      i--;  // Volver a verificar este índice en la próxima iteración
      continue;
    }

    // Dibujar la partícula en la matriz
    matrix.drawPixel(particles[i].x, particles[i].y, matrix.Color(particles[i].r, particles[i].g, particles[i].b));
  }

  // Actualizar la matriz
  matrix.show();
}



/*
int map_to_pixels(float value, float* data_range, int* pixel_range){
  return round((value - data_range[0]) / (data_range[1] - data_range[0]) * (pixel_range[1] - pixel_range[0]));
}

void drawLineOnMatrix(int pos1_x, int pos1_y, int pos2_x, int pos2_y, int r, int g, int b) {
  // Convertir las coordenadas a la escala de la matriz

  int pos1_x_scaled = 61 - ((pos1_x * (46 - 20) / 320) + 20);
  int pos1_y_scaled = pos1_y * mh / 320;
  int pos2_x_scaled = 61 - ((pos2_x * (46 - 20) / 320) + 20);
  int pos2_y_scaled = pos2_y * mh / 320;

  // Dibujar la línea en la matriz
  matrix.drawLine(pos1_x_scaled, pos1_y_scaled, pos2_x_scaled, pos2_y_scaled, matrix.Color(r, g, b));
}

void clearMatrix() {
  // Borrar la matriz
  matrix.fillScreen(0);
}

*/

void loop() {

  if (VAR==0 ){
    matrix.setBrightness(30);
    aztec();
  } 
  if (VAR==2 ){
    
    aztec2();
  } 
  if (VAR==6 ){
    
    waitingAnimation();
  } 
  if (VAR==7 ){
    matrix.setBrightness(255);
    updateMatrix();
    
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

