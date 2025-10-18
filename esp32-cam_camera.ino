#include "esp_camera.h"
#include "SD_MMC.h"

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// 📸 Fotoğraf isimlendirme
const char *photoPrefix = "/photo_";
int photoNumber = 0;

// ⚙️ Donanım pinleri
#define BUTTON_PIN 12      // Fotoğraf çekme butonu
#define FLASH_GPIO_NUM 4   // Flaş LED pini

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println("\n🚀 ESP32-CAM başlatılıyor...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  // 📷 Kamera konfigürasyonu
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // 🎥 Kamera başlat
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Kamera başlatma hatası! Hata kodu: 0x%x\n", err);
    return;
  }

  // 💾 SD kart başlat
  Serial.println("💾 SD kart başlatılıyor...");
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("❌ SD kart başlatılamadı!");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("❌ SD kart takılı değil!");
    return;
  }

  Serial.println("✅ SD kart başarıyla monte edildi!");

  // 📂 En son fotoğraf numarasını bul
  int lastPhoto = getLastPhotoNumber();
  photoNumber = lastPhoto + 1; // Yeni fotoğraf numarası
  Serial.printf("📷 Son fotoğraf numarası: %d → Yeni fotoğraf: photo_%d.jpg\n",
                lastPhoto, photoNumber);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("📸 Butona basıldı! Fotoğraf çekiliyor...");

    // 🔦 Flaş aç
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    delay(200); // ışığın oturması için

    // 📷 Fotoğraf al
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Kamera görüntüsü alınamadı!");
      digitalWrite(FLASH_GPIO_NUM, LOW);
      return;
    }

    // 📁 Dosya adı oluştur
    String photoFileName = String(photoPrefix) + String(photoNumber) + ".jpg";
    Serial.printf("📄 Dosya adı: %s\n", photoFileName.c_str());

    // 💾 SD karta yaz
    File file = SD_MMC.open(photoFileName, FILE_WRITE);
    if (!file) {
      Serial.println("❌ SD karta yazma hatası (FILE_WRITE başarısız)");
    } else {
      file.write(fb->buf, fb->len);
      file.close();
      Serial.printf("✅ Fotoğraf kaydedildi: %s (Boyut: %d bayt)\n", photoFileName.c_str(), fb->len);
      photoNumber++;
    }

    esp_camera_fb_return(fb);
    delay(500);
    digitalWrite(FLASH_GPIO_NUM, LOW);  // Flaş kapat
    delay(1000); // Titreşim engelleme
  }
}

// 📂 SD karttaki en son fotoğraf numarasını bulan fonksiyon
int getLastPhotoNumber() {
  int maxNum = -1;
  File root = SD_MMC.open("/");
  if (!root) {
    Serial.println("❌ SD kart dizini açılamadı!");
    return -1;
  }

  root.rewindDirectory();  // Tarama başından başlat
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    name.toLowerCase();

    if (name.startsWith("photo_") && name.endsWith(".jpg")) {
      int start = name.indexOf('_') + 1;
      int end = name.indexOf('.');
      if (start > 0 && end > start) {
        int num = name.substring(start, end).toInt();
        if (num > maxNum) maxNum = num;
      }
    }

    file.close(); // Dosyayı kapat
    file = root.openNextFile();
  }

  root.close();
  return maxNum >= 0 ? maxNum : -1;
}
