#include <WiFi.h>
#include <M5StickCPlus2.h>

// NTP設定
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPass"
#define NTP_SERVER1 "ntp.nict.jp"
#define NTP_SERVER2 "1.pool.ntp.org"
#define NTP_SERVER3 "2.pool.ntp.org"

bool rtcInitialized = false;

void setup(void) {
    StickCP2.begin();
    StickCP2.Display.setRotation(1);
    StickCP2.Display.setTextColor(BLUE);
    Serial.begin(115200);

    if (!StickCP2.Rtc.isEnabled()) {
        Serial.println("RTC not found.");
        StickCP2.Display.println("RTC not found.");
        for (;;) {
            delay(500);
        }
    }

    Serial.println("RTC found.");

    auto dt = StickCP2.Rtc.getDateTime();
    Serial.printf("RTC Time Check: %04d/%02d/%02d %02d:%02d:%02d\n",
                  dt.date.year, dt.date.month, dt.date.date,
                  dt.time.hours, dt.time.minutes, dt.time.seconds);

    if (dt.date.year < 2023 || dt.date.year > 2025 || dt.date.month == 0 || dt.date.month > 12 || dt.date.date == 0 || dt.date.date > 31) {
        rtcInitialized = false;
        Serial.println("RTC time is invalid. Awaiting user input for correction...");
    } else {
        rtcInitialized = true;
        Serial.println("RTC time is valid. You can enter 'RESET' to resync with NTP or modify the time.");
    }
}

void loop(void) {
    static constexpr const char* const wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    // シリアルモニタからの入力処理
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.equalsIgnoreCase("RESET")) {
            Serial.println("Forcing NTP synchronization...");
            syncNTP();
        } else if (input.indexOf('/') > 0) { // 日付設定 (yy/mm/dd)
            int year = input.substring(0, 2).toInt() + 2000; // 年 (20xx形式に変換)
            int month = input.substring(3, 5).toInt();       // 月
            int day = input.substring(6, 8).toInt();         // 日

            // デバッグ出力
            Serial.printf("Parsed Date - Year: %d, Month: %d, Day: %d\n", year, month, day);

            if (year >= 2023 && year <= 2025 && month > 0 && month <= 12 && day > 0 && day <= 31) {
                auto dt = StickCP2.Rtc.getDateTime();
                StickCP2.Rtc.setDateTime({ { year, month, day }, { dt.time.hours, dt.time.minutes, dt.time.seconds } });
                Serial.printf("Date set to: %04d/%02d/%02d\n", year, month, day);
            } else {
                Serial.println("Invalid date input. Please use 'yy/mm/dd' format with valid values.");
            }
        } else if (input.indexOf(':') > 0) { // 時刻設定 (hh:mm:ss)
            int hour = input.substring(0, 2).toInt();        // 時
            int minute = input.substring(3, 5).toInt();     // 分
            int second = input.substring(6, 8).toInt();     // 秒

            // デバッグ出力
            Serial.printf("Parsed Time - Hour: %d, Minute: %d, Second: %d\n", hour, minute, second);

            if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
                auto dt = StickCP2.Rtc.getDateTime();
                StickCP2.Rtc.setDateTime({ { dt.date.year, dt.date.month, dt.date.date }, { hour, minute, second } });
                Serial.printf("Time set to: %02d:%02d:%02d\n", hour, minute, second);
            } else {
                Serial.println("Invalid time input. Please use 'hh:mm:ss' format with valid values.");
            }
        } else {
            Serial.println("Invalid input. Use 'RESET', 'yy/mm/dd', or 'hh:mm:ss'.");
        }
    }

    // RTCの現在時刻を取得 (UTC)
    auto dt = StickCP2.Rtc.getDateTime();

    // JSTに変換
    int localHours = dt.time.hours + 9;
    int localMinutes = dt.time.minutes;
    int localSeconds = dt.time.seconds;

    if (localHours >= 24) {
        localHours -= 24;
        dt.date.date++;
        if (dt.date.date > 31) {
            dt.date.date = 1;
            dt.date.month++;
            if (dt.date.month > 12) {
                dt.date.month = 1;
                dt.date.year++;
            }
        }
    }

    // 静的変数を使用して前回表示した時刻を記録
    static int prevHours = -1;
    static int prevMinutes = -1;
    static int prevSeconds = -1;
    static int prevDay = -1;

    // 時刻が変更された場合のみ更新
    if (localHours != prevHours) {
        StickCP2.Display.setTextSize(6);
        StickCP2.Display.setCursor(10, 60);
        StickCP2.Display.fillRect(10, 60, 120, 60, BLACK); // 時間の部分をクリア
        StickCP2.Display.printf("%02d", localHours);
        prevHours = localHours; // 最新の時間を記録
    }

    if (localMinutes != prevMinutes) {
        StickCP2.Display.setTextSize(6);
        StickCP2.Display.setCursor(80, 60);
        StickCP2.Display.fillRect(80, 60, 120, 60, BLACK); // 分の部分をクリア
        StickCP2.Display.printf(":%02d", localMinutes);
        prevMinutes = localMinutes; // 最新の分を記録
    }

    if (localSeconds != prevSeconds) {
        StickCP2.Display.setTextSize(3);
        StickCP2.Display.setCursor(195, 85);
        StickCP2.Display.fillRect(195, 85, 40, 30, BLACK); // 秒の部分をクリア
        StickCP2.Display.printf("%02d", localSeconds);
        prevSeconds = localSeconds; // 最新の秒を記録
    }

    // 日付が変更された場合のみ更新
    if (dt.date.date != prevDay) {
        StickCP2.Display.setTextSize(2);
        StickCP2.Display.setCursor(60, 20);
        StickCP2.Display.fillRect(60, 20, 180, 30, BLACK); // 日付全体をクリア
        StickCP2.Display.printf("%04d/%02d/%02d", dt.date.year, dt.date.month, dt.date.date);
        prevDay = dt.date.date; // 最新の日付を記録
    }

    delay(1000); // 1秒ごとに更新
}

void syncNTP() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        Serial.print('.');
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\r\nWiFi Connected. Syncing NTP...");
        configTzTime("UTC", NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
        
        unsigned long syncTimeout = millis();
        while (time(nullptr) <= 0 && millis() - syncTimeout < 5000) {
            Serial.print('.');
            delay(500);
        }

        if (time(nullptr) > 0) {
            time_t t = time(nullptr);
            StickCP2.Rtc.setDateTime(gmtime(&t));
            Serial.println("NTP Sync Completed. RTC updated.");
        } else {
            Serial.println("NTP Sync Failed.");
        }
    } else {
        Serial.println("\r\nWiFi Connection Failed.");
    }
}

