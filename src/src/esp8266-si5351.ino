#define BUILD_VERSION "2.0.4s"
#define BUILD_DATE __DATE__ " " __TIME__

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include "si5351.h"
#include "Wire.h"

#include "index_html_data.h"
#include "version.h"
#include "config.h"

#include <stdint.h>


Si5351 si5351;


#define SDA_PIN 0
#define SCL_PIN 2

//#define SDA_PIN 1
//#define SCL_PIN 3

#define DEFAULT_FREQUENCY 0.0f

#define AP_MODE_PIN 3 // //Serial RX
#define RESTORE_FACTORY_DEFAULTS_PIN 1 // Serial TX

float current_clk0_freq = 0.0;
float current_clk1_freq = 0.0;
float current_clk2_freq = 0.0;


#define WDT_TIMEOUT_MS 30000

#ifndef STASSID
#define STASSID "mywifi"
#define STAPSK "12345678"
#endif

#define APSSID "si5351"
#define APPSK "12345678"

uint64_t seconds_online = 0;

#define DEFAULT_CONFIG_MAGIC '7'
#define CONFIG_MAX_STRING_LEN 128
#define CONFIG_EEPROM_SIZE 4096
#define CONFIG_EEPROM_ADDR 512

#define EXPECTED_FACTORY_MAGIC 0xe1955ee
#define FACTORY_MAGIC_EEPROM_ADDR 0
#define CALIB_EEPROM_ADDR 10
#define MIN_FREQ_EEPROM_ADDR 20
#define MAX_FREQ_EEPROM_ADDR 30
#define DEF_POWER_EEPROM_ADDR 40

#define SIMODE_NORMAL 0
#define SIMODE_CLK0_CLK1_INVERT 1
#define SIMODE_CLK0_CLK2_PHASE_SHIFT 2

struct Config {
  char magic;
  char wifi_ssid[CONFIG_MAX_STRING_LEN];
  char wifi_pass[CONFIG_MAX_STRING_LEN];
  char ap_ssid[CONFIG_MAX_STRING_LEN];
  char ap_pass[CONFIG_MAX_STRING_LEN];
  int32_t si5351_cal_factor;
  float si5351_clk0_freq;
  bool si5351_clk0_enabled;
  unsigned si5351_clk0_power;
  float si5351_clk1_freq;
  bool si5351_clk1_enabled;
  unsigned si5351_clk1_power;
  float si5351_clk2_freq;
  bool si5351_clk2_enabled;
  unsigned si5351_clk2_power;
  uint64_t si5351_phase_offset_freq;
  uint64_t si5351_phase_offset_pll_freq;
  uint8_t si5351_phase_offset_even_divider;

  int mode;
  bool si5351_ok;
};

Config CONFIG = { 0 };

void CFG_Save(Config& cfg) {
  //Serial.println("CFG_Save()");
  EEPROM.put<Config>(CONFIG_EEPROM_ADDR, cfg);
  if (EEPROM.commit()) {
    //Serial.println("   EEPROM successfully committed");
    //Serial.println("   Saved config!");
    delay(1000);
  } else {
    //Serial.println("Fail writing EEPROM!!!!!!!!!!!!");
  }
  CFG_Load();
}


void CFG_SaveDefault() {
  //Serial.println("CFG_SaveDefault()!");
  uint32_t read_magic;
  int32_t def_calib_factor = 0;
  EEPROM.get<int32_t>(CALIB_EEPROM_ADDR, def_calib_factor);
  uint32_t def_power = 0;
  EEPROM.get<uint32_t>(DEF_POWER_EEPROM_ADDR, def_power);
  float def_min_freq = 0;
  float def_max_freq = 0;
  EEPROM.get<float>(MIN_FREQ_EEPROM_ADDR, def_min_freq);
  EEPROM.get<float>(MAX_FREQ_EEPROM_ADDR, def_max_freq);
  Config c;
  c.magic = DEFAULT_CONFIG_MAGIC;
  strncpy(c.wifi_ssid, STASSID, 32);
  strncpy(c.wifi_pass, STAPSK, 32);
  strncpy(c.ap_ssid, APSSID, 32);
  strncpy(c.ap_pass, APPSK, 32);
  c.si5351_cal_factor = 0;
  c.si5351_clk0_freq = DEFAULT_FREQUENCY;
  c.si5351_clk0_power = 3; // 0=SI5351_DRIVE_2MA .. 3=SI5351_DRIVE_8MA
  c.si5351_clk0_enabled = true;
  c.si5351_clk1_freq = DEFAULT_FREQUENCY;
  c.si5351_clk1_power = 3; // 0=SI5351_DRIVE_2MA .. 3=SI5351_DRIVE_8MA
  c.si5351_clk1_enabled = true;
  c.si5351_clk2_freq = DEFAULT_FREQUENCY;
  c.si5351_clk2_power = 3; // 0=SI5351_DRIVE_2MA .. 3=SI5351_DRIVE_8MA
  c.si5351_clk2_enabled = true;
  c.si5351_ok = false;
  c.si5351_phase_offset_freq = 2815000000ULL;
  c.si5351_phase_offset_pll_freq = 73190000000ULL;
  c.si5351_phase_offset_even_divider = 26;
  c.mode = SIMODE_NORMAL;
  CFG_Save(c);
  //Serial.println("reboot!");
  ESP.restart();
}

String CFG_toString(const Config& c) {
  String s = String("{ \"magic\": \"") + String((char)c.magic) +
  String("\", \"ap_ssid\": \"") + String(c.ap_ssid) +
  String("\", \"ap_pass\": \"") + String(c.ap_pass) +
  String("\", \"wifi_ssid\": \"") + String(c.wifi_ssid) +
  String("\", \"wifi_pass\": \"") + String("*****") + //String(c.wifi_pass) +
  String("\", \"si5351_cal_factor\": ") + String(c.si5351_cal_factor) +
  String(", \"si5351_clk0_freq\": ") + String(c.si5351_clk0_freq) +
  String(", \"si5351_clk0_enabled\": ") + String(c.si5351_clk0_enabled) +
  String(", \"si5351_clk0_power\": ") + String(c.si5351_clk0_power) +
  String(", \"si5351_clk1_freq\": ") + String(c.si5351_clk1_freq) +
  String(", \"si5351_clk1_enabled\": ") + String(c.si5351_clk1_enabled) +
  String(", \"si5351_clk1_power\": ") + String(c.si5351_clk1_power) +
  String(", \"si5351_clk2_freq\": ") + String(c.si5351_clk2_freq) +
  String(", \"si5351_clk2_enabled\": ") + String(c.si5351_clk2_enabled) +
  String(", \"si5351_clk2_power\": ") + String(c.si5351_clk2_power) +
  String(", \"si5351_phase_offset_freq\": ") + String(c.si5351_phase_offset_freq) +
  String(", \"si5351_phase_offset_pll_freq\": ") + String(c.si5351_phase_offset_pll_freq) +
  String(", \"si5351_phase_offset_even_divider\": ") + String(c.si5351_phase_offset_even_divider) +
  String(", \"mode\": ") + String(c.mode) +
  String(", \"si5351_ok\": ") + String(c.si5351_ok) +
  String(", \"version\": \"") + String(BUILD_VERSION) + String("\"") +
  String(", \"version_date\": \"") + String(BUILD_DATE) + String("\"") +
  String(", \"sizeof_config\": ") + String(sizeof(Config)) +
  String("}");
  return s;
}


void CFG_Load() {
  EEPROM.begin(CONFIG_EEPROM_SIZE);
  EEPROM.get<Config>(CONFIG_EEPROM_ADDR, CONFIG);
  //Serial.print("   rr:CONFIG magic is (int)");
  //Serial.print((int)CONFIG.magic);
  //Serial.print(" (char)");
  //Serial.println((char)CONFIG.magic);
  if(CONFIG.magic != DEFAULT_CONFIG_MAGIC) {
    //Serial.println("   Invalid magic, need to flash default config.");
    CFG_SaveDefault();
  } else {
    //Serial.println("   No need to flash default config.");
  }
  current_clk0_freq = CONFIG.si5351_clk0_freq;
  current_clk1_freq = CONFIG.si5351_clk1_freq;
  current_clk2_freq = CONFIG.si5351_clk2_freq;
  //Serial.print("Config loaded:");
  //Serial.println(CFG_toString(CONFIG));
}

void init_si5351() {
  Serial.println("init_si5351()");
  bool i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0, SDA_PIN, SCL_PIN);
  if(!i2c_found)
  {
    CONFIG.si5351_ok = false;
    for(int i=0; i<10; i++) {
      Serial.println("Device si5351 not found on I2C bus!"); // Rebooting in 5s...");
      delay(1000);
    }
    return;
  } else {
    Serial.println("OK!");
  }
  CONFIG.si5351_ok = true;
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 0);
  //Serial.print("Device si5355 - found, power (0..3, 3=8mA): "); //Serial.println(CONFIG.si5351_clk0_power);
  si5351.drive_strength(SI5351_CLK0, (si5351_drive)CONFIG.si5351_clk0_power);
  si5351_calibrate();
  si5351_update();
  si5351.output_enable(SI5351_CLK0, CONFIG.si5351_clk0_enabled);
  si5351.output_enable(SI5351_CLK1, CONFIG.si5351_clk1_enabled);
  si5351.output_enable(SI5351_CLK2, CONFIG.si5351_clk2_enabled);
}


void set_90deg_phase_offset(uint64_t freq = 2815000000ULL, uint64_t pll_freq = 73190000000ULL, unsigned even_divider = 26) {
/*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Setting the phase of a clock requires that you manually set the PLL and
 * take the PLL frequency into account when calculation the value to place
 * in the phase register. As shown on page 10 of Silicon Labs Application
 * Note 619 (AN619), the phase register is a 7-bit register, where a bit
 * represents a phase difference of 1/4 the PLL period. Therefore, the best
 * way to get an accurate phase setting is to make the PLL an even multiple
 * of the clock frequency, depending on what phase you need.
 *
 * If you need a 90 degree phase shift (as in many RF applications), then
 * it is quite easy to determine your parameters. Pick a PLL frequency that
 * is an even multiple of your clock frequency (remember that the PLL needs
 * to be in the range of 600 to 900 MHz). Then to set a 90 degree phase shift,
 * you simply enter that multiple into the phase register. Remember when
 * setting multiple outputs to be phase-related to each other, they each need
 * to be referenced to the same PLL.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */


  // Set CLK0 and CLK1 to use PLLA as the MS source.
  // This is not explicitly necessary in v2 of this library,
  // as these are already the default assignments.
  // si5351.set_ms_source(SI5351_CLK0, SI5351_PLLA);
  // si5351.set_ms_source(SI5351_CLK1, SI5351_PLLA);

  // Set CLK0 and CLK1 to output 14.1 MHz with a fixed PLL frequency
  si5351.set_freq_manual(freq, pll_freq, SI5351_CLK0);
  si5351.set_freq_manual(freq, pll_freq, SI5351_CLK1);

  // Now we can set CLK1 to have a 90 deg phase shift by entering
  // 50 in the CLK1 phase register, since the ratio of the PLL to
  // the clock frequency is 50.
  si5351.set_phase(SI5351_CLK0, 0);
  //si5351.set_phase(SI5351_CLK1, 50);
  si5351.set_phase(SI5351_CLK1, even_divider);

  // We need to reset the PLL before they will be in phase alignment
  si5351.pll_reset(SI5351_PLLA);

  // Query a status update and wait a bit to let the Si5351 populate the
  // status flags correctly.
  si5351.update_status();

}




void si5351_update() {


  if(CONFIG.mode == SIMODE_NORMAL) {
      Serial.println("Si5351 NORMAL mode!");
      si5351.set_freq(current_clk0_freq * SI5351_FREQ_MULT, SI5351_CLK0);
      si5351.set_freq(current_clk1_freq * SI5351_FREQ_MULT, SI5351_CLK1);
      si5351.set_freq(current_clk2_freq * SI5351_FREQ_MULT, SI5351_CLK2);
  } else if(CONFIG.mode == SIMODE_CLK0_CLK1_INVERT) {
      Serial.println("Si5351 CLK0 CLK1 invert mode!");
      si5351.set_freq(current_clk0_freq * SI5351_FREQ_MULT, SI5351_CLK0);
      si5351.set_freq(current_clk0_freq * SI5351_FREQ_MULT, SI5351_CLK1);
      si5351.set_freq(current_clk2_freq * SI5351_FREQ_MULT, SI5351_CLK2);
      si5351.set_clock_invert(SI5351_CLK1, 1);
  } if(CONFIG.mode == SIMODE_CLK0_CLK2_PHASE_SHIFT) {
      Serial.println("Si5351 Phase Shift clk0 clk1 mode!");
      set_90deg_phase_offset(CONFIG.si5351_phase_offset_freq, CONFIG.si5351_phase_offset_pll_freq, CONFIG.si5351_phase_offset_even_divider);
      si5351.set_freq(current_clk2_freq * SI5351_FREQ_MULT, SI5351_CLK2);
  }
  si5351.pll_reset(SI5351_PLLA);  
  si5351.update_status();
}



bool _tx_on = false;


void si5351_calibrate() {
  Serial.print("si5351_calibrate, cal_factor: "); Serial.println(CONFIG.si5351_cal_factor);
  si5351.set_correction((si5351_drive)CONFIG.si5351_cal_factor, SI5351_PLL_INPUT_XO);
  delay(100);
  si5351_update();
}


const char PROGMEM_CONTENT_TYPE_TEXT_HTML[]  PROGMEM = "text/html";

ESP8266WebServer server(80);
const char* serverUpdateIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

bool ap_mode = false;

void WiFi_Connect() {
  Serial.print("WiFi_Connect ap_mode=");
  Serial.println(ap_mode ? "true" : "false");
  if(ap_mode) {
    Serial.print(" -> AP MODE, ap_ssid=");
    Serial.print(CONFIG.ap_ssid);
    Serial.print(", ap_pass=");
    Serial.println(CONFIG.ap_pass);
    WiFi.softAP(CONFIG.ap_ssid, CONFIG.ap_pass);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print(" -> AP IP address: ");
    Serial.println(myIP);
  } else {
    Serial.print(" -> STA MODE, joining existing network ssid=");
    Serial.print(CONFIG.wifi_ssid);
    ////Serial.print(", psk=");
    ////Serial.println(CONFIG.wifi_pass);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(CONFIG.wifi_ssid, CONFIG.wifi_pass);
    delay(100);
    bool wifi_ok = false;
    for(int i=0; i<20; i++) {
      if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println(" -> WiFi Failed");
        delay(200);
      } else {
        wifi_ok = true;
      }
    }
    if(!wifi_ok) {
      Serial.println("WIFI failed 10 times. Reboot!");
      Serial.println("RESTARTING in 10s!");
      delay(10000);
      Serial.println("reboot!");
      ESP.restart();
    } else {
      Serial.println(" -> Wifi connected!");
    }
  }
}

void WiFi_Ensure() {
  static uint32_t prev_tick_mod = 0;
  uint32_t new_tick_mod = ( millis() / 1000 ) % 2;
  if(new_tick_mod != prev_tick_mod) {
    prev_tick_mod = new_tick_mod;
    seconds_online += 1;
    //blink();
    if(seconds_online > (60*60)) {
      Serial.println("REBOOT - PLANNED REBOOT, 1h online already!");
      Serial.println("RESTARTING in 1.5s!");
      delay(1500);
      Serial.println("reboot!");
      //reboot();
      ESP.restart();
    }
    if(seconds_online % 30 == 0) {
      Serial.print("PING: UPTIME="); Serial.print(seconds_online); Serial.print(" [s], ");
      Serial.print("Si5351 OK? "); Serial.println(CONFIG.si5351_ok);
      Serial.print("MODE: "); ap_mode ? Serial.print("AP") : Serial.print("WIFI CLIENT");
      Serial.print(", IP: ");
      if(ap_mode) {
        Serial.println(WiFi.softAPIP());
      } else {
        Serial.println(WiFi.localIP());
      }
    }

  }
  if(ap_mode) {
    ESP.wdtFeed();
    return;
  }
  if(WiFi.status() == WL_CONNECTED) {
    ESP.wdtFeed();
    return;
  }
  Serial.println("WIFI not connected. RESTARTING in 1.5s!");
  delay(1500);
  Serial.println("reboot!");
  ESP.restart();
}

void set_default_api_headers() {
  server.sendHeader("Connection", "close");
  //server.sendHeader("Content-type", "application/json");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Credentials", "true");
  server.sendHeader("Access-Control-Allow-Headers", "Origin,Content-Type,X-Amz-Date,Authorization,X-Api-Key,X-Amz-Security-Token,locale");
  server.sendHeader("Access-Control-Allow-Methods", "GET, PUT, POST, DELETE, OPTIONS");
}


void setup(void) {
  //Serial.begin(115200);//, //Serial_8N1, //Serial_TX_ONLY);
  //Serial.println();
  ////Serial.println("rradio_tx setup... BUILD: " FULL_VERSION);
  //Serial.println("Configuring WDT...");
  ESP.wdtEnable(WDT_TIMEOUT_MS);
  delay(3000);

  //i2c_scanner();
  //while(1) init_si5351();


  ////Serial.println("BUILD: " FULL_VERSION);
  //pinMode(LED_STATUS, OUTPUT);
  pinMode(AP_MODE_PIN, INPUT_PULLUP); // complete
  pinMode(RESTORE_FACTORY_DEFAULTS_PIN, INPUT_PULLUP); // complete
  delay(100);
  if(digitalRead(RESTORE_FACTORY_DEFAULTS_PIN)) { // default, as it's PULLED UP!
    //Serial.println("main(): RESTORE_FACTORY_DEFAULTS_PIN is HIGH (pullup), normal operation");
  } else {
      delay(3000);
      if(digitalRead(RESTORE_FACTORY_DEFAULTS_PIN)) {
        //Serial.println("  -> main(): RESTORE_FACTORY_DEFAULTS_PIN is LOW -> restore default config.");
        delay(10000);
        CFG_SaveDefault();
        delay(1000);
        ESP.restart();
      } else {
        //Serial.println("RESTORE_FACTORY_DEFAULTS_PIN was low, but not for 1s!");
      }
  }
  delay(1000);
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  if(digitalRead(AP_MODE_PIN)) { // default, as it's PULLED UP!
    Serial.println("main(): AP_MODE_PIN is HIGH (pullup), normal operation - AP mode");
    ap_mode = true;
  } else {
    Serial.println("main(): AP_MODE_PIN is LOW (pullup), STA mode");
    ap_mode = false;
  }


  //Serial.println("Loading preferences from flash...");
  CFG_Load();

  init_si5351();

  WiFi_Connect();

  MDNS.begin(CONFIG.ap_ssid);
  MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, []() {
    set_default_api_headers();
    server.send_P(200, PROGMEM_CONTENT_TYPE_TEXT_HTML, index_html_data, index_html_data_length);
  });
  server.on("/api/v1/calibrate", HTTP_GET, []() {
    set_default_api_headers();
    si5351_calibrate();
    String ret = String(CONFIG.si5351_cal_factor);
    server.send(200, "text/html", ret);
  });

  server.on("/api/v1/config", HTTP_GET, []() {
    set_default_api_headers();
    server.send(200, "text/html", CFG_toString(CONFIG));
  });
  server.on("/api/v1/mode", HTTP_GET, []() {
    set_default_api_headers();
    String ret = String(CONFIG.mode);
    server.send(200, "text/html", ret);
  });
  server.on("/api/v1/cal", HTTP_GET, []() {
    set_default_api_headers();
    String ret = String(CONFIG.si5351_cal_factor);
    server.send(200, "text/html", ret);
  });
  server.on("/api/v1/freq0", HTTP_GET, []() {
    set_default_api_headers();
    String ret = String(current_clk0_freq);
    server.send(200, "text/html", ret);
  });
  server.on("/api/v1/freq0", HTTP_POST, []() {
    set_default_api_headers();
    String f;
    if (server.hasArg("f")) {
      f = server.arg("f");
    } else {
      server.send(401, "application/json", "{ \"error\": \"ERROR: missing f\" }");
      return;
    }
    float new_f = f.toFloat();
    current_clk0_freq = new_f;
    si5351_update();
    server.send(200, "text/html", f);
  });
  server.on("/api/v1/freq1", HTTP_GET, []() {
    set_default_api_headers();
    String ret = String(current_clk1_freq);
    server.send(200, "text/html", ret);
  });
  server.on("/api/v1/freq1", HTTP_POST, []() {
    set_default_api_headers();
    String f;
    if (server.hasArg("f")) {
      f = server.arg("f");
    } else {
      server.send(401, "application/json", "{ \"error\": \"ERROR: missing f\" }");
      return;
    }
    float new_f = f.toFloat();
    current_clk1_freq = new_f;
    si5351_update();
    server.send(200, "text/html", f);
  });
  server.on("/api/v1/freq2", HTTP_GET, []() {
    set_default_api_headers();
    String ret = String(current_clk2_freq);
    server.send(200, "text/html", ret);
  });
  server.on("/api/v1/freq2", HTTP_POST, []() {
    set_default_api_headers();
    String f;
    if (server.hasArg("f")) {
      f = server.arg("f");
    } else {
      server.send(401, "application/json", "{ \"error\": \"ERROR: missing f\" }");
      return;
    }
    float new_f = f.toFloat();
    current_clk2_freq = new_f;
    si5351_update();
    server.send(200, "text/html", f);
  });
  server.on("/api/v1/reboot", HTTP_POST, []() {
    set_default_api_headers();
    //Serial.println("/api/v1/reboot!");
    ESP.restart();
    server.send(200, "text/html", "DEFCONFIG");
  });
  server.on("/api/v1/defconfig", HTTP_POST, []() {
    set_default_api_headers();
    //Serial.println("/api/v1/defconfig!");
    CFG_SaveDefault();
    server.send(200, "text/html", "DEFCONFIG");
  });

  server.on("/api/v1/set", HTTP_POST, []() {
    //Serial.println("/api/v1/set");
    set_default_api_headers();
    String name;
    String value;
    if (server.hasArg("name")) {
        name = server.arg("name");
    } else {
      server.send(401, "application/json", "{ \"error\": \"ERROR: missing name\" }");
      return;
    }
    if (server.hasArg("value")) {
        value = server.arg("value");
    } else {
      server.send(401, "application/json", "{ \"error\": \"ERROR: missing value\" }");
      return;
    }
    //Serial.print("UPDATING: name=");
    Serial.println(name);
    Serial.print("value=");
    Serial.println(value);
    bool string_size_ok = false;
    if(value.length() < CONFIG_MAX_STRING_LEN) {
      string_size_ok = true;
    }
    Config tmp_config = CONFIG;
    if(name == "ap_ssid" && string_size_ok) {
      strcpy(tmp_config.ap_ssid, value.c_str());
    } else if(name == "wifi_ssid" && string_size_ok) {
      strcpy(tmp_config.wifi_ssid, value.c_str());
    } else if(name == "ap_pass" && string_size_ok) {
      strcpy(tmp_config.ap_pass, value.c_str());
    } else if(name == "wifi_pass" && string_size_ok) {
      strcpy(tmp_config.wifi_pass, value.c_str());
    } else if(name == "si5351_clk0_enabled") {
      tmp_config.si5351_clk0_enabled = value.toInt();
    } else if(name == "si5351_clk0_power") {
      tmp_config.si5351_clk0_power = value.toInt();
    } else if(name == "si5351_clk0_freq") {
      tmp_config.si5351_clk0_freq = value.toFloat();
    } else if(name == "si5351_clk1_enabled") {
      tmp_config.si5351_clk1_enabled = value.toInt();
    } else if(name == "si5351_clk1_power") {
      tmp_config.si5351_clk1_power = value.toInt();
    } else if(name == "si5351_clk1_freq") {
      tmp_config.si5351_clk1_freq = value.toFloat();
    } else if(name == "si5351_clk2_enabled") {
      tmp_config.si5351_clk2_enabled = value.toInt();
    } else if(name == "si5351_clk2_power") {
      tmp_config.si5351_clk2_power = value.toInt();
    } else if(name == "si5351_clk2_freq") {
      tmp_config.si5351_clk2_freq = value.toFloat();
    } else if(name == "si5351_phase_offset_freq") {
      tmp_config.si5351_phase_offset_freq = value.toInt();
    } else if(name == "si5351_phase_offset_pll_freq") {
      tmp_config.si5351_phase_offset_pll_freq = value.toInt();
    } else if(name == "si5351_phase_offset_even_divider") {
      tmp_config.si5351_phase_offset_even_divider = value.toInt();
    } else if(name == "mode") {
      tmp_config.mode = value.toInt();
    } else if(name == "si5351_cal_factor") {
      tmp_config.si5351_cal_factor = value.toInt();
      Serial.println("Recalibrating live!");
      CONFIG.si5351_cal_factor = value.toInt();
      si5351_calibrate();
    } else {
      Serial.println("UNKNOWN FIELD, ABORTING.");
      server.send(401, "application/json", "{ \"error\": \"ERROR: unknown name\" }");
      return;
    }
    //Serial.println(new_ssid);
    //strncpy(tmp_config.wifi_ssid, new_ssid.c_str(), 32);
    Serial.println(CFG_toString(tmp_config));
    CFG_Save(tmp_config);
    CFG_Load();
    Serial.println(CFG_toString(CONFIG));
    server.send(200, "text/plain", name + '=' + value);
  });
  server.on("/upgrade", HTTP_GET, []() {
    set_default_api_headers();
    server.send(200, "text/html", serverUpdateIndex);
  });
    server.on(
      "/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          Serial.setDebugOutput(true);
          WiFiUDP::stopAll();
          Serial.printf("Update: %s\n", upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace)) {  // start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {  // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
          Serial.setDebugOutput(false);
        }
        yield();
      });
  server.begin();

  Serial.printf("Ready! Open http://%s.local in your browser\n", CONFIG.ap_ssid);
}

void loop(void) {
  server.handleClient();
  MDNS.update();
  WiFi_Ensure();
}
