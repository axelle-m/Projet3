// CODE FINAL - PROJET INTÉGRATEUR EN GÉNIE ÉLECTRIQUE //
// Auteur: Axelle Monnot (2125719)                     //
// Date : 25 avril 2025                                //

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_SDA 4
#define OLED_SCL 22
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


const char* json_url = "http://xxx:5000/api/gtfs-json";
const char* ssid = "xxx";
const char* password = "xxx";

#define S0 18
#define S1 19
#define S2 21
#define Z1 23
#define Z2 5
#define Z3 26
#define Z4 25


struct Arret {
  const char* nom;
  float latitude;
  float longitude;
  int indexY;
  int demux;
};


Arret sens1[] = {
  {"Outremont", 45.515899,-73.601653, 0, 3},
  {"Stuart", 45.515424,-73.605821, 1, 3},
  {"Les Hauvillier", 45.514894, -73.608391, 2, 3},
  {"Rockland", 45.514122, -73.611227, 3, 3},
  {"Claude Champagne", 45.513118, -73.613051, 4, 3},
  {"Vincent d'Indy", 45.511465, -73.615148, 5, 3},
  {"Station", 45.510360, -73.612756, 7, 3},
  {"Édouard Montpetit", 45.508690, -73.614048, 0, 4},
  {"de Stirling", 45.507417, -73.615209, 1, 4},
  {"Woodbury", 45.505141, -73.617049, 2, 4},
  {"Université-de-Montréal", 45.504315, -73.618006, 3, 4},
  {"Louis-Colin", 45.503404, -73.618841, 4, 4},
  {"McKenna", 45.501366, -73.620681, 5, 4},
  {"Decelles", 45.499491, -73.622369, 7, 4}
};
int nbSens1 = sizeof(sens1) / sizeof(sens1[0]);

Arret sens2[] = {
  {"Decelles", 45.499244, -73.622304, 0, 1},
  {"McKenna", 45.501108, -73.620699, 1, 1},
  {"Louis-Colin", 45.503105, -73.618898, 2, 1},
  {"Université-de-Montréal", 45.504021, -73.618080, 3, 1},
  {"Woodbury", 45.505141, -73.617049, 4, 1},
  {"de Stirling", 45.507133, -73.615253, 5, 1},
  {"Edouard Montpetit", 45.509010, -73.613579, 0, 2},
  {"Vincent d'Indy", 45.511592, -73.615160, 1, 2},
  {"Dunlop", 45.512565, -73.613766, 2, 2},
  {"Courcellette", 45.514075, -73.611003, 3, 2},
  {"Pagnuel", 45.514455, -73.609653, 4, 2},
  {"Stuart", 45.515199, -73.606519, 5, 2},
  {"Outremont", 45.515723, -73.602270, 6, 2},
  {"Querbes", 45.517357, -73.597738, 7, 2}
};
int nbSens2 = sizeof(sens2) / sizeof(sens2[0]);

// États des LEDS
int ledStatesSens2[14] = {0};
int ledStatesSens1[15] = {0};
bool clignote = false;
unsigned long dernierToggle = 0;

float distanceGPS(float lat1, float lon1, float lat2, float lon2) {
  float R = 6371e3;
  float phi1 = radians(lat1);
  float phi2 = radians(lat2);
  float deltaPhi = radians(lat2 - lat1);
  float deltaLambda = radians(lon2 - lon1);
  float a = sin(deltaPhi / 2) * sin(deltaPhi / 2) +
            cos(phi1) * cos(phi2) *
            sin(deltaLambda / 2) * sin(deltaLambda / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

void afficherSurOLED(float secondes, int codeAchalandage) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ARRET: DE STIRLING");

  display.setCursor(0, 20);
  if (secondes < 60) display.println("Bus IMMINENT");
  else if (secondes < 999) {
    display.print("Bus dans ");
    display.print((int)(secondes / 60));
    display.print(" min ");
    display.print((int)(secondes) % 60);
    display.println(" sec");
  } else {
    display.println("Bus inconnu");
  }

  display.setCursor(0, 40);
  display.print("Achalandage: ");
  if (codeAchalandage == 1) display.println("Faible");
  else if (codeAchalandage == 2) display.println("Modere");
  else if (codeAchalandage == 3) display.println("Eleve");
  else if (codeAchalandage >= 4) display.println("Tres eleve");
  else display.println("N/A");

  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(Z1, OUTPUT);
  pinMode(Z2, OUTPUT);
  pinMode(Z3, OUTPUT);
  pinMode(Z4, OUTPUT);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED non détecté");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("OLED prêt");
  display.display();
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connexion Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("Connecté !");
}

void afficherDELs() {
  // Mise à jour clignotement
  if (millis() - dernierToggle > 500) {
    clignote = !clignote;
    dernierToggle = millis();
  }

  for (int repeat = 0; repeat < 10; repeat++) {
    for (int i = 0; i < nbSens2; i++) {
      digitalWrite(S0, sens2[i].indexY & 0x01);
      digitalWrite(S1, (sens2[i].indexY >> 1) & 0x01);
      digitalWrite(S2, (sens2[i].indexY >> 2) & 0x01);
      delayMicroseconds(5);
      if (ledStatesSens2[i] == 1 || (ledStatesSens2[i] == 2 && clignote)) {
        digitalWrite(sens2[i].demux == 1 ? Z1 : Z2, HIGH);
        delayMicroseconds(2000);                                                //clignotement très rapide (2ms) qui donne l'effet continu
        digitalWrite(sens2[i].demux == 1 ? Z1 : Z2, LOW);
      }
    }

    for (int i = 0; i < nbSens1; i++) {
      digitalWrite(S0, sens1[i].indexY & 0x01);
      digitalWrite(S1, (sens1[i].indexY >> 1) & 0x01);
      digitalWrite(S2, (sens1[i].indexY >> 2) & 0x01);
      delayMicroseconds(5);
      if (ledStatesSens1[i] == 1 || (ledStatesSens1[i] == 2 && clignote)) {
        digitalWrite(sens1[i].demux == 3 ? Z3 : Z4, HIGH);
        delayMicroseconds(2000);
        digitalWrite(sens1[i].demux == 3 ? Z3 : Z4, LOW);
      }
    }
  }
}

void loop() {
  afficherDELs();  // affichage multiplexé et clignotement

  static unsigned long lastUpdate = 0;
  static float distanceMinStirling = 9999;
  static int codeAchalandage = 0;

  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    distanceMinStirling = 9999;
    codeAchalandage = 0;

    int nouveauxEtats[14] = {0};
    int nouveauxEtatsSens1[15] = {0};

    bool busTrouve = false;

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(json_url);

      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<8192> doc;
        deserializeJson(doc, payload);
        JsonArray bus_positions = doc["bus_positions"].as<JsonArray>();

        for (JsonObject bus : bus_positions) {
          busTrouve = true;  // bus détecté

          int directionBus = bus["direction"];
          float latitudeBus = bus["latitude"];
          float longitudeBus = bus["longitude"];
          int achalandage = bus["achalandage"] | 0;

          if (directionBus == 2) {
            for (int i = 0; i < nbSens2; i++) {
              float dist = distanceGPS(latitudeBus, longitudeBus, sens2[i].latitude, sens2[i].longitude);
              if (dist < 20) {
                nouveauxEtats[i] = 1;
                if (i > 0) nouveauxEtats[i - 1] = 0;
              } else if (i > 0 &&latitudeBus > (sens2[i - 1].latitude ) && (latitudeBus) < sens2[i].latitude) {
                nouveauxEtats[i] = max(nouveauxEtats[i], 2);
                if (i > 0) nouveauxEtats[i - 1] = 0;
              }
            }

            float distStirling = distanceGPS(latitudeBus, longitudeBus, sens2[5].latitude, sens2[5].longitude);
            if (distStirling < distanceMinStirling && latitudeBus < sens2[5].latitude) {
              distanceMinStirling = distStirling;
              codeAchalandage = achalandage;
            }
          } else if (directionBus == 1) {
            for (int i = 0; i < nbSens1; i++) {
              float dist = distanceGPS(latitudeBus, longitudeBus, sens1[i].latitude, sens1[i].longitude);
              if (dist < 20) {
                nouveauxEtatsSens1[i] = 1;
                if (i > 0) nouveauxEtatsSens1[i - 1] = 0;
              } else if (i > 0 && latitudeBus < (sens1[i - 1].latitude) && (latitudeBus) > sens1[i].latitude) {
                nouveauxEtatsSens1[i] = max(nouveauxEtatsSens1[i], 2);
                if (i > 0) nouveauxEtatsSens1[i - 1] = 0;
              }
            }
          }
        }
      } else {
        Serial.print("Erreur HTTP: ");
        Serial.println(httpCode);
      }
      http.end();
    }

    if (busTrouve) {
      for (int i = 0; i < nbSens2; i++) ledStatesSens2[i] = nouveauxEtats[i];
      for (int i = 0; i < nbSens1; i++) ledStatesSens1[i] = nouveauxEtatsSens1[i];
    }

    float tempsEstime = (distanceMinStirling < 5000) ? distanceMinStirling / 5.0 : 9999;
    afficherSurOLED(tempsEstime, codeAchalandage);
  }
}





