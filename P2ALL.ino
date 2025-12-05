// --- BIBLIOTECAS ---
#include <Wire.h>     // Para I2C (OLED y RTC)
#include "RTClib.h"   // Para Reloj DS3231
#include <Adafruit_GFX.h>       // Gráficos OLED
#include <Adafruit_SSD1306.h>   // Driver OLED
#include <DHT.h>      // Para Sensor DHT11
#include <SPI.h>      // Para SD Card
#include <FS.h>       // Para sistema de archivos SD
#include <SD.h>       // Para SD Card
#include <math.h>     // Para cálculos psicrométricos

// --- DEFINICIONES DE PINES ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

#define DHTPIN 32       // Pin D32 (Sensor Tbs2/phi2)
#define DHTTYPE DHT11

// Pines Termistores (ADC1)
#define TBS_PIN 35      // Pin D35 (Bulbo Seco Principal)
#define TBH_PIN 34      // Pin D34 (Bulbo Húmedo)
#define TBS_PROTECT 39  // Pin D39 (Sensor VN) - Tbs con Escudo
#define TBS_UNPROTECT 36 // Pin D36 (Sensor VP) - Tbs al Sol directo

#define SD_CS_PIN 5     // Pin D5 para CS

// --- CONSTANTES DEL PROYECTO ---
#define ALTITUD_Z 2250.0    // Altitud en metros (Chapingo/Texcoco aprox)

// --- CONSTANTES DEL TERMISTOR ---
#define THERMISTORNOMINAL 100000.0
#define TEMPERATURENOMINAL 25.0
#define BCOEFFICIENT 4000.0
#define SERIESRESISTOR 370000.0
#define ADC_RESOLUTION 4095.0

// --- OBJETOS GLOBALES ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;
DHT dht(DHTPIN, DHTTYPE);

// --- VARIABLES GLOBALES ---
double total_tbs = 0.0;
double total_tbh = 0.0;
// Nuevos acumuladores
double total_tbs_protect = 0.0;
double total_tbs_unprotect = 0.0;
double total_t_dht = 0.0;
double total_h_dht = 0.0;
int sampleCount = 0;

double P_atm; // Presión atmosférica

unsigned long lastSampleTime = 0;
unsigned long lastLogTime = 0;
const long sampleInterval = 2000;  // 2 segundos
const long logInterval = 600000;   // 10 minutos

// Variables para lecturas actuales
double current_tbs, current_tbh, current_tbsP, current_tbsUP, current_t_dht, current_h_dht;

// =================================================================
// FUNCIÓN SETUP
// =================================================================
void setup() {
  Serial.begin(115200);

  // 1. Iniciar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("Fallo al iniciar SSD1306"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

  // 2. Iniciar RTC
  if (!rtc.begin()) {
    display.println("Error: RTC");
    Serial.println("No se encontro el RTC");
    display.display();
    for (;;);
  }
  
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // 3. Iniciar DHT
  dht.begin();
  
  // 4. Iniciar SD
  // SPI manual si es necesario, si no, usa el default
  if (!SD.begin(SD_CS_PIN)) {
    display.println("Error: SD Card");
    Serial.println("Fallo al iniciar SD Card");
    display.display();
    for (;;);
  }

  // --- ENCABEZADO DEL CSV ---
  // Se añaden las columnas para los nuevos sensores
  if (!SD.exists("/datalog.txt")) {
    File dataFile = SD.open("/datalog.txt", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Timestamp,Tbs_C,Tbh_C,Tbs2_C,phi2_%,TbsProtect_C,TbsUnprotect_C,P_atm_Pa,Pvs_Pa,Pv_Pa,W_kgkg,Ws_kgkg,phi_%,Veh_m3kg,Tpr_C,h_kJkg");
      dataFile.close();
    }
  }

  // Calculamos P_atm
  P_atm = 101325.0 * pow((1.0 - 2.25577e-5 * ALTITUD_Z), 5.2259);

  Serial.println("Sistema listo.");
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Sistema Listo");
  display.print("Altitud: "); display.print(ALTITUD_Z,0); display.println("m");
  display.display();
  delay(2000);
}

// =================================================================
// FUNCIÓN LOOP
// =================================================================
void loop() {
  unsigned long currentTime = millis();

  // --- Toma de muestras (cada 2 seg) ---
  if (currentTime - lastSampleTime >= sampleInterval) {
    lastSampleTime = currentTime; 
    readAllSensors();
    
    // Solo acumulamos si el DHT (que es digital) da lecturas válidas
    // Los termistores siempre dan lectura (aunque sea incorrecta si se desconectan)
    if (!isnan(current_t_dht) && !isnan(current_h_dht)) { 
      total_tbs += current_tbs;
      total_tbh += current_tbh;
      total_tbs_protect += current_tbsP;     // Acumular Tbs Protegido
      total_tbs_unprotect += current_tbsUP;  // Acumular Tbs No Protegido
      total_t_dht += current_t_dht;
      total_h_dht += current_h_dht;
      sampleCount++;
    }
    updateDisplay();
  }

  // --- Registro en SD (cada 10 min) ---
  if (currentTime - lastLogTime >= logInterval) {
    lastLogTime = currentTime; 
    logDataToSD();
  }
}

// =================================================================
// --- FUNCIONES AUXILIARES ---
// =================================================================

// Declaración anticipada de funciones psicrométricas
void MPvs(double T, double* coef);
double Pvs(double Tk, double* coef);
double Ws(double P, double Pvs_in);
double h(double T, double W_in);
double W_from_h_Tbs(double h_in, double Tbs);
double Pv_from_W_P(double W_in, double P);
double Tpr(double T, double Pv_in);
double Veh(double Tk, double P, double W_in);

/**
 * Lee todos los sensores
 */
void readAllSensors() {
  current_h_dht = dht.readHumidity();
  current_t_dht = dht.readTemperature();
  
  // Termistores principales
  current_tbs = readThermistor(TBS_PIN);
  current_tbh = readThermistor(TBH_PIN);
  
  // Termistores de comparación (Radiación)
  current_tbsP = readThermistor(TBS_PROTECT);
  current_tbsUP = readThermistor(TBS_UNPROTECT);

  // Debug rápido en Serial
   Serial.print("T:"); Serial.print(current_t_dht, 1);
  Serial.print(" H:"); Serial.print(current_h_dht, 1);
  Serial.print("Tbs:"); Serial.print(current_tbs, 1);
  Serial.print(" Tbh:"); Serial.print(current_tbh, 1);
  Serial.print(" Prot:"); Serial.print(current_tbsP, 1);
  Serial.print(" Sol:"); Serial.println(current_tbsUP, 1);
}

/**
 * Muestra datos en OLED
 */
void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // 1. HORA (Cabecera)
  DateTime now = rtc.now();
  if(now.hour() < 10) display.print('0'); display.print(now.hour()); display.print(':');
  if(now.minute() < 10) display.print('0'); display.print(now.minute()); display.print(':');
  if(now.second() < 10) display.print('0'); display.println(now.second());
  
  // Dibujar una línea horizontal debajo de la hora para separar
  // (x0, y0, x1, y1, color)
  display.drawLine(0, 8, 128, 8, WHITE); 
  
  // Movemos el cursor un poco más abajo para empezar a listar datos
  display.setCursor(0, 12);

  // 2. TERMISTORES PRINCIPALES (Psicrómetro)
  display.print("Tbs: "); display.print(current_tbs, 1); display.println(" C");
  display.print("Tbh: "); display.print(current_tbh, 1); display.println(" C");
  
  // 3. SENSORES DE RADIACIÓN (Comparación)
  // Prt = Protegido, Sol = No Protegido
  display.print("Prt: "); display.print(current_tbsP, 1); display.println(" C");
  display.print("Sol: "); display.print(current_tbsUP, 1); display.println(" C");
  
  // 4. SENSOR DHT11 (Referencia)
  // D_T = DHT Temperatura, D_H = DHT Humedad
  display.print("D_T: "); display.print(current_t_dht, 1); display.println(" C");
  display.print("D_H: "); display.print(current_h_dht, 1); display.println(" %");
  
  display.display();
}

/**
 * Calcula promedios, psicrometría y guarda en SD
 */
void logDataToSD() {
  if (sampleCount > 0) {
    // 1. Calcular Promedios
    double avg_tbs = total_tbs / sampleCount;
    double avg_tbh = total_tbh / sampleCount;
    double avg_tbsP = total_tbs_protect / sampleCount;     // Promedio Protegido
    double avg_tbsUP = total_tbs_unprotect / sampleCount;  // Promedio Sin Protección
    double avg_t_dht = total_t_dht / sampleCount;
    double avg_h_dht = total_h_dht / sampleCount;

    // --- CÁLCULOS PSICROMÉTRICOS (Con Tbs y Tbh principales) ---
    double coef[7];
    
    // Propiedades en Tbh (saturado)
    double Tbh_K = avg_tbh + 273.15;
    MPvs(avg_tbh, coef);
    double Pvs_bh = Pvs(Tbh_K, coef); 
    double Ws_bh = Ws(P_atm, Pvs_bh); 
    double h_real = h(avg_tbh, Ws_bh); // Isentalpía

    // Propiedades en Tbs
    double Tbs_K = avg_tbs + 273.15;
    MPvs(avg_tbs, coef);
    double Pvs_real = Pvs(Tbs_K, coef); 
    double Ws_real = Ws(P_atm, Pvs_real); 

    // W Real y Pv Real
    double W_real = W_from_h_Tbs(h_real, avg_tbs);
    double Pv_real = Pv_from_W_P(W_real, P_atm);

    // Finales
    double phi_real = (Pv_real / Pvs_real) * 100.0; 
    double Tpr_real = Tpr(avg_tbs, Pv_real); 
    double Veh_real = Veh(Tbs_K, P_atm, W_real); 

    // 2. Timestamp
    DateTime now = rtc.now();
    char timestamp[32];
    sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());

    // 3. Crear línea CSV
    // IMPORTANTE: El orden debe coincidir con el header del setup
    String dataString = String(timestamp) + ", " +
                        String(avg_tbs, 2) + ", " +
                        String(avg_tbh, 2) + ", " +
                        String(avg_t_dht, 2) + ", " +
                        String(avg_h_dht, 2) + ", " +
                        // Aquí agregamos los nuevos promedios
                        String(avg_tbsP, 2) + ", " +
                        String(avg_tbsUP, 2) + ", " +
                        // Datos calculados
                        String(P_atm, 2) + ", " +
                        String(Pvs_real, 2) + ", " +
                        String(Pv_real, 2) + ", " +
                        String(W_real, 6) + ", " +
                        String(Ws_real, 6) + ", " +
                        String(phi_real, 2) + ", " +
                        String(Veh_real, 3) + ", " +
                        String(Tpr_real, 2) + ", " +
                        String(h_real, 2);

    // 4. Guardar
    File dataFile = SD.open("/datalog2.txt", FILE_APPEND);
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      Serial.println(F("DATOS GUARDADOS EN SD."));
      Serial.println(dataString);
    } else {
      Serial.println(F("Error escritura SD"));
      display.setCursor(100,0); display.print("ERR"); display.display(); // Indicador visual
    }

    // 5. Resetear
    total_tbs = 0.0; total_tbh = 0.0;
    total_tbs_protect = 0.0; total_tbs_unprotect = 0.0;
    total_t_dht = 0.0; total_h_dht = 0.0;
    sampleCount = 0;

  } else {
    Serial.println(F("Sin muestras."));
  }
}

/**
 * Lee termistor y convierte a Celsius
 */
double readThermistor(int pin) {
  int adcVal = analogRead(pin);
  if (adcVal == 0 || adcVal == 4095) return -273.15; // Error de desconexión

  double resistance = SERIESRESISTOR * ( (ADC_RESOLUTION / (double)adcVal) - 1.0 );
  double steinhart;
  steinhart = resistance / THERMISTORNOMINAL;     
  steinhart = log(steinhart);                  
  steinhart /= BCOEFFICIENT;                   
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); 
  steinhart = 1.0 / steinhart;                 
  steinhart -= 273.15;                         

  return steinhart;
}

// =================================================================
// --- FUNCIONES PSICROMÉTRICAS (Sin Cambios) ---
// =================================================================
void MPvs(double T, double* coef) {
  if (-100.0 <= T && T < 0.0) {
    coef[0] = -5.6745359e3; coef[1] = 6.3925247; coef[2] = -9.677843e-3;
    coef[3] = 6.2215701e-7; coef[4] = 2.0747825e-9; coef[5] = -9.484024e-13; coef[6] = 4.1635019;
  } else if (0.0 <= T && T < 200.0) {
    coef[0] = -5.8002206e3; coef[1] = 1.3914993; coef[2] = -4.8640239e-2;
    coef[3] = 4.1764768e-5; coef[4] = -1.4452093e-8; coef[5] = 0.0; coef[6] = 6.5459673;
  }
}
double Pvs(double Tk, double* coef) {
  return exp(coef[0]/Tk + coef[1] + coef[2]*Tk + coef[3]*pow(Tk,2) + coef[4]*pow(Tk,3) + coef[5]*pow(Tk,4) + coef[6]*log(Tk));
}
double Ws(double P, double Pvs_in) { return 0.621945 * (Pvs_in / (P - Pvs_in)); }
double h(double T, double W_in) { return 1.006 * T + W_in * (2501.0 + 1.805 * T); }
double W_from_h_Tbs(double h_in, double Tbs) { return (h_in - 1.006 * Tbs) / (2501.0 + 1.805 * Tbs); }
double Pv_from_W_P(double W_in, double P) { return (W_in * P) / (0.621945 + W_in); }
double Tpr(double T, double Pv_in) {
  if (Pv_in <= 0) return -273.15;
  double log_Pv = log(Pv_in);
  if (-60.0 <= T && T < 0.0) return (-60.45 + 7.0322 * log_Pv + 0.37 * pow(log_Pv, 2));
  else if (0.0 <= T && T < 70.0) return (-35.957 - 1.8726 * log_Pv + 1.1689 * pow(log_Pv, 2));
  return -273.15;
}
double Veh(double Tk, double P, double W_in) { return ((287.055 * (Tk)) / P) * ((1.0 + 1.6078 * W_in) / (1.0 + W_in)); }