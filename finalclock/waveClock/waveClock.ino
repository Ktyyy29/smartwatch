/* Using LVGL with Arduino requires some extra steps:
 * Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html
 */

#include "Display_SPD2010.h"
#include "RTC_PCF85063.h"
#include "LVGL_Driver.h"
#include "ui.h"

// 🔹 Added Libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define BAT_ADC_PIN   8
#define Measurement_offset 0.990476 
#define MIN_VOLTAGE 3.2
#define MAX_VOLTAGE 4.2

int brightness[5] = {20, 40, 60, 80, 100};
int bright = 1;

String daysWeek[7] = {"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};

int h, s, m, y, mm, d, dow;
String hr, mi, se, da, mo, timeStr, dateStr, batStr, percentStr = "";
float voltage, percent = 0;

int sethour, setminute, setmonth, setday, setyear, setdow;

// 🔹 WiFi & Firebase
const char* WIFI_SSID = "Oten";
const char* WIFI_PASS = "8051b862";
const char* FIREBASE_EMOTION_URL = "https://smartwatch-f6cce-default-rtdb.firebaseio.com/live_monitoring/current.json";

String currentEmotion = "IDLE";


// 🔹 Real-time emotion fetch
unsigned long lastEmotionFetch = 0;
const unsigned long EMOTION_INTERVAL = 500; // 0.5 seconds

// 🔹 Battery Functions
float BAT_Get_Volts() {
    int Volts = analogReadMilliVolts(BAT_ADC_PIN); // millivolts
    float BAT_analogVolts = (float)(Volts * 3.0 / 1000.0) / Measurement_offset;
    return BAT_analogVolts;
}

float getBatteryPercentage(float voltage) {
    if (voltage > MAX_VOLTAGE) voltage = MAX_VOLTAGE;
    if (voltage < MIN_VOLTAGE) voltage = MIN_VOLTAGE;
    return ((voltage - MIN_VOLTAGE) / (MAX_VOLTAGE - MIN_VOLTAGE)) * 100.0;
}

// 🔹 WiFi Connection
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
    }
}

// 🔹 Fetch Emotion from Firebase
String getEmotion() {
    if (WiFi.status() != WL_CONNECTED) return "";

    HTTPClient http;
    http.begin(FIREBASE_EMOTION_URL);
    int httpCode = http.GET();
    String emotion = "";

    if (httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc.containsKey("emotion")) {
            emotion = String(doc["emotion"].as<const char*>());
        }
    }

    http.end();
    return emotion;
}

// 🔹 Update Emotion UI
void updateEmotionUI(String emotion) {
    lv_label_set_text(ui_Label36, emotion.c_str());
    lv_label_set_text(ui_Label39, emotion.c_str());

    if (emotion == "Happy") {
        lv_img_set_src(ui_Image4, &ui_img_happy_png);
        lv_img_set_src(ui_Image5, &ui_img_happy_png);
    } else if (emotion == "Sad") {
        lv_img_set_src(ui_Image4, &ui_img_sad_png);
        lv_img_set_src(ui_Image5, &ui_img_sad_png);
    } else if (emotion == "Angry") {
        lv_img_set_src(ui_Image4, &ui_img_angry_png);
        lv_img_set_src(ui_Image5, &ui_img_angry_png);
    } else if (emotion == "Fear") {
        lv_img_set_src(ui_Image4, &ui_img_fear_png);
        lv_img_set_src(ui_Image5, &ui_img_fear_png);
    } else if (emotion == "Disgust") {
        lv_img_set_src(ui_Image4, &ui_img_disgust_png);
        lv_img_set_src(ui_Image5, &ui_img_disgust_png);
    } else {
        lv_img_set_src(ui_Image4, &ui_img_neutral_png);
        lv_img_set_src(ui_Image5, &ui_img_neutral_png);
    }
}

// 🔹 Set RTC Time
void settTime(int y,int m,int d,int h,int mm,int s,int dow) {
    datetime_t tt;
    tt.year = y + 2000;
    tt.month = m;
    tt.day = d;
    tt.hour = h;
    tt.minute = mm;
    tt.second = s;
    tt.dotw = dow;
    PCF85063_Set_All(tt);
}

// 🔹 Setup
void setup() {
    pinMode(7, OUTPUT);
    digitalWrite(7, 1);

    connectWiFi();
    LCD_Init();
    Backlight_Init();
    Set_Backlight(40);
    I2C_Init();
    TCA9554PWR_Init(0x00);
    PCF85063_Init();

    Lvgl_Init();
    ui_init();

    Serial.begin(115200);
}

// 🔹 Brightness Control
void cngBright(lv_event_t * e) {
    bright++;
    if (bright > 4) bright = 0;
    Set_Backlight(brightness[bright]);

    lv_obj_add_flag(ui_Panel8, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Panel9, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Panel13, LV_OBJ_FLAG_HIDDEN);

    if (bright >= 1) lv_obj_clear_flag(ui_Panel8, LV_OBJ_FLAG_HIDDEN);
    if (bright >= 2) lv_obj_clear_flag(ui_Panel9, LV_OBJ_FLAG_HIDDEN);
    if (bright >= 3) lv_obj_clear_flag(ui_Panel10, LV_OBJ_FLAG_HIDDEN);
    if (bright >= 4) lv_obj_clear_flag(ui_Panel13, LV_OBJ_FLAG_HIDDEN);
}

// 🔹 Prepare Time (for set screen)
void prepareTime(lv_event_t * e) {
    sethour = h;
    setminute = m;
    setyear = 25;
    setmonth = mm;
    setday = d;
    setdow = dow;
}

// 🔹 Choose Time Buttons
void choseTime(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    if (btn == ui_hrUp) sethour = (sethour + 1) % 24;
    if (btn == ui_hrDown) sethour = (sethour - 1 + 24) % 24;
    if (btn == ui_minUp) setminute = (setminute + 1) % 60;
    if (btn == ui_minDown) setminute = (setminute - 1 + 60) % 60;
    if (btn == ui_monthUP) setmonth = (setmonth % 12) + 1;
    if (btn == ui_monthDowm) setmonth = (setmonth - 2 + 12) % 12 + 1;
    if (btn == ui_dayUp) setday = (setday % 31) + 1;
    if (btn == ui_dayDown) setday = (setday - 2 + 31) % 31 + 1;
    if (btn == ui_yearUp) setyear = (setyear + 1) % 100;
    if (btn == ui_yearDown) setyear = (setyear - 1 + 100) % 100;
    if (btn == ui_dowUp) setdow = (setdow + 1) % 7;
    if (btn == ui_dowDown) setdow = (setdow - 1 + 7) % 7;

    if (btn == ui_saveTime || btn == ui_saveDate) {
        settTime(setyear, setmonth, setday, sethour, setminute, 0, setdow);
        lv_scr_load(ui_Screen1);
    }
}

// 🔹 Prepare Emotion Toggle
void prepareEmotion(lv_event_t * e) {
    emotionRunning = !emotionRunning;

    if (emotionRunning) {
        lv_obj_set_style_bg_color(ui_Button7, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_Label37, "STOP");
        lv_label_set_text(ui_Label36, "Fetching...");
        lv_label_set_text(ui_Label39, "Fetching...");

        currentEmotion = getEmotion();
        updateEmotionUI(currentEmotion);
    } else {
        lv_obj_set_style_bg_color(ui_Button7, lv_color_hex(0x3B9750), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_Label37, "START");
        lv_label_set_text(ui_Label36, "IDLE");
        lv_label_set_text(ui_Label39, "IDLE");

        lv_img_set_src(ui_Image4, &ui_img_idle_png);
        lv_img_set_src(ui_Image5, &ui_img_idle_png);
    }
}

// 🔹 Main Loop
void loop() {
    if (lv_scr_act() == ui_Screen1) {
        PCF85063_Loop();
        s = datetime.second;
        m = datetime.minute;
        h = datetime.hour;
        d = datetime.day;
        y = datetime.year;
        mm = datetime.month;
        dow = datetime.dotw;

        se = (s < 10 ? "0" : "") + String(s);
        mi = (m < 10 ? "0" : "") + String(m);
        hr = (h < 10 ? "0" : "") + String(h);
        da = (d < 10 ? "0" : "") + String(d);
        mo = (mm < 10 ? "0" : "") + String(mm);
        timeStr = hr + ":" + mi;
        dateStr = mo + "-" + da;

        voltage = BAT_Get_Volts();
        percent = getBatteryPercentage(voltage);
        batStr = String(voltage) + " V";
        percentStr = String((int)percent) + "%";

        lv_label_set_text(ui_timeLBL, timeStr.c_str());
        lv_label_set_text(ui_secLBL, se.c_str());
        lv_label_set_text(ui_dateLBL, dateStr.c_str());
        lv_label_set_text(ui_dayLBL, daysWeek[dow].c_str());
        lv_label_set_text(ui_batPercent, percentStr.c_str());
        lv_bar_set_value(ui_Bar1, (int)percent, LV_ANIM_OFF);

        // 🔹 Fetch emotion every 3s if running
        static String lastEmotion = "";
        if (emotionRunning && millis() - lastEmotionFetch > EMOTION_INTERVAL) {
            lastEmotionFetch = millis();
            String emotion = getEmotion();
            if (emotion != "" && emotion != lastEmotion) {
                lastEmotion = emotion;
                currentEmotion = emotion;
                updateEmotionUI(currentEmotion);
                Serial.println("Emotion: " + currentEmotion);
            }
        }
    }

    // Update time set screen
    if (lv_scr_act() == ui_setTime) {
        lv_label_set_text(ui_setHrLBL, String(sethour).c_str());
        lv_label_set_text(ui_setMinLBL, String(setminute).c_str());
    }

    // Update date set screen
    if (lv_scr_act() == ui_setDate) {
        lv_label_set_text(ui_setMonthLBL, String(setmonth).c_str());
        lv_label_set_text(ui_setDayLBL, String(setday).c_str());
        lv_label_set_text(ui_setYearLBL, String(setyear).c_str());
        lv_label_set_text(ui_setDowLBL, String(setdow).c_str());
    }

    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
}
