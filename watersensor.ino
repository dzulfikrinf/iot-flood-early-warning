/*
 * ==============================================================
 * PROJECT: FLOOD EARLY WARNING SYSTEM (FINAL FIXED)
 * ==============================================================
 */

#define BLYNK_TEMPLATE_ID "TMPL623TgZqsa"
#define BLYNK_TEMPLATE_NAME "Flood Early Warning System"
#define BLYNK_AUTH_TOKEN "Bv4nHKTsFMlZwXf2p15n8Q6p6womgCG_" 
#define BLYNK_PRINT Serial 

#include <WiFi.h>
#include <WiFiClientSecure.h> 
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// --------------------------------------------------------------
// KONFIGURASI
// --------------------------------------------------------------
String botToken = "8304293045:AAHONJR2aNMpSOOns2RS-lTd8AcV2g3Xv1k"; 
String chatID   = "@awasadabanjir"; // Pastikan Bot sudah jadi ADMIN di channel ini

char ssid[] = "Atlas";
char pass[] = "anjaymabar7272";

#define PIN_WATER_LEVEL 34 
#define PIN_RAIN_SENSOR 35 
#define PIN_BUZZER      18 

#define V_PIN_WATER_PCT   V1 
#define V_PIN_RAIN_RAW    V2 
#define V_PIN_FLOOD_STAT  V5 
#define V_PIN_RAIN_STAT   V6 
#define V_PIN_SECURE_DATA V10 

const int SENSOR_MAX_VALUE = 1711; 
const int THRESHOLD_BAHAYA = 70;   
const int THRESHOLD_SIAGA  = 30;   

const int RAIN_MILD    = 2500; 
const int RAIN_HEAVY   = 1500; 

int extremeDurationCounter = 0; 
const int EXTREME_LIMIT = 10; 

int rainValidationCounter = 0;
const int VALIDATION_LIMIT = 2; 

BlynkTimer timer; 

bool isBahaya = false;      
bool isExtreme = false;     
bool isHeavyRain = false;   
bool isMildRain = false;    

// --------------------------------------------------------------
// FUNGSI TAMBAHAN: URL ENCODE (WAJIB ADA BIAR TELEGRAM SUKSES)
// --------------------------------------------------------------
// Fungsi ini mengubah spasi menjadi %20, enter jadi %0A, dll.
String urlEncode(String value) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < value.length(); i++) {
    c = value.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code0 = code0;
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// --------------------------------------------------------------
// FUNGSI PENGIRIM TELEGRAM (SUDAH DIPERBAIKI)
// --------------------------------------------------------------
void sendTelegram(String message) {
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); 
  
  // 1. Encode Pesan agar spasi & emoji aman dikirim
  String encodedMsg = urlEncode(message);
  
  // 2. Susun URL dengan Mode Markdown (Agar huruf tebal jalan)
  String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + encodedMsg + "&parse_mode=Markdown";
  
  http.begin(client, url); 
  int httpCode = http.GET(); 
  
  if (httpCode > 0) {
    Serial.println("✅ Telegram Terkirim.");
  } else {
    Serial.print("❌ Gagal Telegram. Error Code: ");
    Serial.println(httpCode);
  }
  http.end(); 
}

// --------------------------------------------------------------
// FUNGSI ENKRIPSI
// --------------------------------------------------------------
String encryptData(String input, int shift) {
  String output = "";
  for (int i = 0; i < input.length(); i++) {
    output += char(input[i] + shift); 
  }
  return output;
}

// --------------------------------------------------------------
// LOGIKA UTAMA
// --------------------------------------------------------------
void sendSensorData() {
  int waterRaw = analogRead(PIN_WATER_LEVEL);
  int rainRaw = analogRead(PIN_RAIN_SENSOR);

  int waterPercent = map(waterRaw, 0, SENSOR_MAX_VALUE, 0, 100);
  waterPercent = constrain(waterPercent, 0, 100);

  // --- ANALISIS HUJAN ---
  bool isRaining = (rainRaw < RAIN_MILD); 
  bool isPouring = (rainRaw < RAIN_HEAVY); 

  if (isRaining) {
    rainValidationCounter++; 
  } else {
    rainValidationCounter = 0;
    extremeDurationCounter = 0;
    isExtreme = false;
  }
  
  bool isValidRain = (rainValidationCounter >= VALIDATION_LIMIT);

  if (isValidRain && isPouring) {
    extremeDurationCounter++;
  } else {
    if (!isPouring) extremeDurationCounter = 0; 
  }

  bool isExtremeCondition = (extremeDurationCounter >= EXTREME_LIMIT);

  // --- STATUS & NOTIFIKASI ---
  String txtStatus = "AMAN";
  String txtHujan  = "CERAH";

  // PRIORITAS 1: BANJIR
  if (waterPercent >= THRESHOLD_BAHAYA) {
    digitalWrite(PIN_BUZZER, HIGH); 
    txtStatus = "BAHAYA (BANJIR)!";
    txtHujan  = isValidRain ? "HUJAN" : "CERAH";
    
    if (!isBahaya) {
      // Gunakan \n untuk enter (karena sudah ada urlEncode)
      // Gunakan *text* untuk Bold (karena sudah ada parse_mode=Markdown)
      String msg = "🔴 **PERINGATAN DARURAT (EMERGENCY)** 🔴\n";
      msg += "\n----------------------------------------\n";
      msg += "⚠️ **Awas Potensi Banjir Tinggi!**\n";
      msg += "🌊 Ketinggian Air: *" + String(waterPercent) + "% (BAHAYA)*\n";
      msg += "🌧️ Kondisi Cuaca: " + String(isValidRain ? "Hujan Turun" : "Tidak Hujan") + "\n";
      msg += "----------------------------------------\n";
      msg += "❗ **INSTRUKSI:**\n";
      msg += "Segera lakukan evakuasi ke tempat tinggi. Amankan dokumen penting dan matikan aliran listrik rumah.";
      
      sendTelegram(msg); 
      Blynk.logEvent("peringatan_banjir", "DARURAT! Segera Evakuasi!");
      
      isBahaya = true; 
      isExtreme = false; isHeavyRain = false; isMildRain = false;
    }
  }

  // PRIORITAS 2: CUACA EKSTREM
  else if (isExtremeCondition) {
    txtStatus = "SIAGA (CUACA EKSTREM)";
    txtHujan  = "BADAI / EKSTREM";

    if (!isExtreme) {
      String msg = "🟠 **PERINGATAN CUACA EKSTREM** 🟠\n";
      msg += "\n----------------------------------------\n";
      msg += "⛈️ **Hujan Intensitas Tinggi Terdeteksi**\n";
      msg += "⏱️ Durasi: Terus-menerus (Sangat Lama)\n";
      msg += "🌊 Status Air: " + String(waterPercent) + "% (Stabil)\n";
      msg += "----------------------------------------\n";
      msg += "❗ **POTENSI BAHAYA:**\n";
      msg += "Tanah jenuh air. Risiko tinggi banjir bandang mendadak. Harap warga di bantaran sungai bersiap Siaga 1.";
      
      sendTelegram(msg);
      Blynk.logEvent("info_hujan", "Cuaca Ekstrem! Waspada Banjir Bandang.");
      
      isExtreme = true; 
      isHeavyRain = false; isMildRain = false;
    }
  }

  // PRIORITAS 3: HUJAN DERAS
  else if (isValidRain && isPouring) {
    txtStatus = "WASPADA (HUJAN DERAS)";
    txtHujan  = "DERAS";

    if (!isHeavyRain && !isExtreme) {
       String msg = "🟡 **INFO PERINGATAN DINI** 🟡\n";
       msg += "\n🌧️ Hujan Deras Mulai Turun\n";
       msg += "Intensitas hujan meningkat drastis dalam waktu singkat. Jarak pandang terbatas. Waspada genangan air di jalan raya.";
       
       sendTelegram(msg);
       isHeavyRain = true;
       isMildRain = false;
    }
  }

  // PRIORITAS 4: HUJAN BIASA
  else if (isValidRain) {
    digitalWrite(PIN_BUZZER, LOW); 
    txtStatus = "WASPADA (GERIMIS)";
    txtHujan  = "RINGAN";

    if (!isMildRain && !isHeavyRain && !isExtreme) {
       String msg = "⚪ **INFO CUACA TERKINI** ⚪\n";
       msg += "\n🌦️ Hujan Ringan/Gerimis\n";
       msg += "Terpantau hujan intensitas ringan. Siapkan payung/jas hujan jika beraktivitas luar ruangan.";
       
       sendTelegram(msg);
       isMildRain = true;
    }
  }

  // AMAN
  else {
    digitalWrite(PIN_BUZZER, LOW); 
    if (waterPercent >= THRESHOLD_SIAGA) txtStatus = "SIAGA (AIR NAIK)";
    else txtStatus = "AMAN";
    isBahaya = false; isExtreme = false; isHeavyRain = false; isMildRain = false;
  }

  // --- DEBUG & ENKRIPSI ---
  String dataAsli = "W:" + String(waterPercent) + "|R:" + String(rainRaw) + "|St:" + txtStatus;
  String dataTerenkripsi = encryptData(dataAsli, 5);

  Serial.println("\n--- [MONITOR CUACA & BAHAYA] ---");
  Serial.print("Rain Raw       : "); Serial.println(rainRaw);
  Serial.print("Durasi Ekstrem : "); Serial.print(extremeDurationCounter); Serial.println(" / 10");
  Serial.print("Status Sistem  : "); Serial.println(txtStatus);
  Serial.println("[ENKRIPSI DATA TELEMETRI]");
  Serial.println(dataTerenkripsi);
  Serial.println("--------------------------------");

  Blynk.virtualWrite(V_PIN_WATER_PCT, waterPercent);
  Blynk.virtualWrite(V_PIN_FLOOD_STAT, txtStatus);
  Blynk.virtualWrite(V_PIN_RAIN_STAT, txtHujan);
  Blynk.virtualWrite(V_PIN_RAIN_RAW, rainRaw);
  Blynk.virtualWrite(V_PIN_SECURE_DATA, dataTerenkripsi);
}

void setup() {
  Serial.begin(115200); 
  pinMode(PIN_WATER_LEVEL, INPUT);
  pinMode(PIN_RAIN_SENSOR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW); 
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(2000L, sendSensorData);
}

void loop() {
  Blynk.run(); 
  timer.run(); 
}