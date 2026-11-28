#pragma once
/*
 * Ashish Automation - 7 segment pixel-LED clock WITH BATTERY MONITOR for WLED 0.14.4 (ESP8266 + ESP32)
 *
 * Digits are fixed at build time: build flag CLOCK_4_DIGIT = 4 digits (HH:MM), otherwise 6 digits (HH:MM:SS)
 *
 * LED chain order (one data line):
 *   HH -> colon -> MM -> colon -> SS -> weekdays SUN..SAT -> AM LEDs -> PM LEDs   (HH:MM:SS)
 *   HH -> colon -> MM -> weekdays SUN..SAT -> AM LEDs -> PM LEDs                   (HH:MM)
 *   (AM/PM LEDs only exist in the chain when the AM/PM option is on)
 *   digit = 7 segments (physical order selectable, default F-A-B-C-D-E-G), 1..10 LEDs per segment
 *   colon = 1..6 LEDs per colon
 *
 * The usermod only switches LEDs OFF (or recolours lit ones if you set a colour).
 * Use the "Solid" effect on the main page for a plain clock.
 */

#include "wled.h"


// works on WLED 0.14.4 (VERSION 2405180) and WLED 0.15.0 (VERSION 2412100)
#if VERSION >= 2410000
  #define C7_PM PinManager::
  #define C7_OAPPEND(x) oappend(F(x))
#else
  #define C7_PM pinManager.
  #define C7_OAPPEND(x) oappend(SET_F(x))
#endif

#ifdef ESP8266
  #define C7_DEF_SDA 4    // D2
  #define C7_DEF_SCL 5    // D1
  #define C7_DEF_LDR 17   // A0 (fixed on ESP8266)
#else
  #define C7_DEF_SDA 21
  #define C7_DEF_SCL 22
  #define C7_DEF_LDR 34
#endif

#define C7_RTC_ADDR 0x68  // DS1307 / DS3231

#ifdef CLOCK_4_DIGIT
  #define C7_FIXED_MODE 1   // 4 digits HH:MM
#else
  #define C7_FIXED_MODE 0   // 6 digits HH:MM:SS
#endif

#ifdef ESP8266
  #define C7_DEF_BAT 17         // A0 (fixed on ESP8266)
  #define C7_DEF_CHG 14         // D5
  #define C7_DEF_TRX -1         // ESP8266: no RX wire (send only), use the BUSY pin instead
  #define C7_DEF_TTX 13         // D7  -> DFPlayer RX (through 1k resistor)
  #define C7_DEF_BATMUL 4.2f    // NodeMCU / D1 mini: 100k in series from battery +
#else
  #define C7_DEF_BAT 35         // ADC1 pin (32-39)
  #define C7_DEF_CHG 27
  #define C7_DEF_TRX 18         // Serial2 RX <- DFPlayer TX (not 16: WLED uses GPIO16 for LED data)
  #define C7_DEF_TTX 19         // Serial2 TX -> DFPlayer RX (through 1k resistor)
  #define C7_DEF_BATMUL 2.0f    // 100k + 100k divider
#endif

class UsermodAshishClock7SegBat : public Usermod {
  private:
    // ---------- settings ----------
    bool    enabled      = true;
    uint8_t mode         = C7_FIXED_MODE;      // 0 = HH:MM:SS (6 digits), 1 = HH:MM (4 digits)
    uint8_t hourFormat   = 0;      // 0 = 24H, 1 = 12H
    bool    leadingZero  = true;
    bool    colonBlink   = true;
    uint8_t weekdayMode  = 1;      // 0 off, 1 today only, 2 all on (today highlighted)
    uint8_t weekdayOrder = 0;      // LED order of the days: 0 Sun>Sat, 1 Sat>Sun (flipped), 2 Mon>Sun, 3 Sun>Mon (flipped)
    bool    startupTest  = false;  // lamp test for 3 s after power on

    uint16_t firstLed    = 0;
    uint8_t segPreset    = 1;      // 0 = custom, 1 = F-A-B-G-E-D-C (default), 2 = A-B-C-D-E-F-G, 3 = F-A-B-C-D-E-G
    char    segCustom[9] = "FABGEDC";
    uint8_t ledsPerSeg   = 1;      // 1..10
    uint8_t colonLeds    = 2;      // 1..6 LEDs PER colon
    uint8_t ledsPerDay   = 1;      // 1..5
    bool    ampmOn       = false;  // AM / PM LEDs after the weekday LEDs
    bool    ampmBlink    = false;  // the lit AM/PM LED blinks
    bool    blankExtra   = true;   // turn off LEDs after the clock chain
    uint8_t ampmLeds     = 1;      // 1..6 LEDs for AM, same for PM

    bool    dateShow     = false;
    uint8_t dateOrder    = 0;      // 0 = DD MM, 1 = MM DD
    uint8_t secTime      = 6;
    uint8_t secDayMonth  = 2;
    uint8_t secYear      = 2;

    bool    tempShow     = false;  // needs DS3231
    uint8_t tempUnit     = 0;      // 0 = C, 1 = F
    uint8_t secTemp      = 2;
    int8_t  tempOffset   = 0;      // degrees C added to reading

    char    wdHex[10]    = "";     // today colour (empty = effect colour; mode 2 default red)
    char    otherHex[10] = "";     // other days colour in mode 2 (empty = effect colour)
    char    weekendHex[10] = "";   // Sat/Sun colour in mode 2 (empty = other days colour)
    char    amHex[10]    = "";
    char    pmHex[10]    = "";
    char    colonHex[10] = "";
    char    dateHex[10]  = "";
    char    tempHex[10]  = "";

    bool    alarmOn      = false;
    uint8_t alarmHour    = 7;
    uint8_t alarmMin     = 0;
    uint8_t alarmDays    = 0;      // 0 every day, 1 Mon-Fri, 2 Sat-Sun
    uint8_t ringSec      = 30;
    uint8_t snoozeMin    = 5;      // 0 = no snooze

    bool    chimeOn      = false;
    uint8_t chimeStart   = 8;
    uint8_t chimeEnd     = 22;

    int8_t  buzzerPin    = -1;
    bool    buzzerInvert = false;
    int8_t  buttonPin    = -1;

    bool    nightOn      = false;
    bool    greetOn      = true;   // show ASHISH after power on
    uint8_t greetSec     = 5;
    uint8_t nightStart   = 23;
    uint8_t nightEnd     = 6;
    uint8_t nightAction  = 0;      // 0 display off, 1 dim, 2 keep on
    uint8_t nightDim     = 20;     // percent brightness when dimmed

    bool    autoBri      = false;
    int8_t  ldrPin       = C7_DEF_LDR;
    bool    ldrInvert    = false;
    uint8_t briMin       = 20;
    uint8_t briMax       = 255;
    // per-part brightness levels 0..255 (255 = full), multiplied with WLED's own brightness
    uint8_t colorSource  = 0;      // 0 from effect, 1 main colour, 2 effect but never black
    uint8_t lvAll = 255, lvDigits = 255, lvColon = 255, lvToday = 255, lvOther = 255, lvAmPm = 255;

    bool    tempAutoColor = false; // blue cold, green normal, red hot
    bool    warnShow     = true;   // show bAtt when RTC battery/time problem
    bool    battCheck    = true;   // RTC battery / time check on or off
    bool    rtcEnabled   = true;
    uint8_t rtcChip      = 0;      // 0 = DS1307, 1 = DS3231
    int8_t  rtcSda       = C7_DEF_SDA;
    int8_t  rtcScl       = C7_DEF_SCL;
    uint8_t rtcSyncHours = 6;
    bool    rereadOn     = true;   // re-read RTC when there is no internet time
    uint8_t rereadMin    = 2;      // minutes between re-reads

    uint8_t testMode     = 0;      // 0 off, 1 all on, 2 segment walk, 3 digit count

    // ---------- battery monitor ----------
    bool    batOn        = true;
    int8_t  batPin       = C7_DEF_BAT;     // ESP32: ADC1 pin 32-39. ESP8266: always A0
    float   batMult      = C7_DEF_BATMUL;  // voltage divider ratio
    float   batCal       = 0.0f;           // offset in volts added to the reading
    float   batMinV      = 3.3f;           // 0 %
    float   batMaxV      = 4.2f;           // 100 %
    uint8_t batIntSec    = 10;             // seconds between readings
    bool    batShow      = true;           // show battery on the display
    uint8_t batShowAs    = 0;              // 0 percent, 1 volts
    uint8_t secBat       = 2;              // seconds on the display
    float   batLowV      = 3.5f;           // low warning (battery shown in red, always shown)
    float   batCutV      = 3.2f;           // display OFF at or below this voltage
    bool    batAutoCol   = true;           // green / orange / red
    char    batHex[10]   = "";
    bool    batBriOn     = true;           // brightness follows battery voltage
    float   batFullV     = 3.9f;           // full brightness at or above this voltage
    uint8_t batFloor     = 25;             // % brightness at the cut-off voltage
    bool    curveOn      = true;           // real Li-ion percent curve instead of a straight line
    bool    barOn        = true;           // battery bar on the weekday LEDs during the battery screen
    bool    recOn        = true;           // cut-off recovery: stay off until charger / recover voltage
    float   batRecV      = 3.7f;
    bool    chgOn        = false;          // TP4056 CHRG pin detection
    int8_t  chgPin       = C7_DEF_CHG;
    bool    chgLow       = true;           // CHRG pin is LOW while charging
    bool    beepOn       = false;          // low battery beep (buzzer needed)
    uint8_t beepMin      = 10;             // repeat every N minutes
    bool    sleepOn      = false;          // sleep on battery: display mostly off
    uint8_t sleepAwake   = 5;              // seconds ON
    uint8_t sleepOff     = 25;             // seconds OFF
    bool    jsonOn       = true;           // battery values in /json/state

    // ---------- Bangla talking clock (DFPlayer Mini) ----------
    bool    talkOn       = false;
    int8_t  talkRx       = C7_DEF_TRX;     // ESP RX  <- DFPlayer TX (-1 = not connected, fixed word timing)
    int8_t  talkTx       = C7_DEF_TTX;     // ESP TX  -> DFPlayer RX
    int8_t  talkBusyPin  = -1;             // optional: DFPlayer BUSY pin (LOW while playing) = exact end of every word
    uint8_t talkVol      = 20;             // 0..30
    bool    talkHourly   = true;           // speak the time every full hour
    uint8_t talkFrom     = 7;              // hourly speech from hour
    uint8_t talkTo       = 22;             // hourly speech until hour
    bool    talkBtnTime  = true;           // short button press speaks the time
    bool    talkBtnDate  = false;          // ... and the date
    bool    talkBtnTemp  = false;          // ... and the temperature
    bool    talkBtnBat   = false;          // ... and the battery
    bool    talkAlarm    = true;           // speak when the alarm rings
    bool    talkLowBat   = true;           // speak "battery low"
    bool    talkChgSay   = true;           // speak "charging" when the charger is connected, "battery full" when it ends full
    uint8_t talkLowMin   = 15;             // repeat battery warning every N minutes
    uint16_t talkWordMs  = 1200;           // ms per word when no RX wire / no finish message
    bool    talkWordMin  = true;           // say the word "minute" after the minutes
    bool    talkWordBaje = true;           // say the closing word ("baje") after the time
    bool    talkNums     = true;           // minute number clips 1-59 exist (off for the classic voice: only hour + 15/30/45 clips)
    bool    talkQuarter  = false;          // also speak at :15 :30 :45 (inside From/To hour)
    bool    talkExact    = false;          // use the special clips for exactly 15 / 30 / 45 minutes (tracks 116 / 117 / 118)

    // ---------- runtime ----------
    char     order[8]    = "FABGEDC";
    bool     orderBad    = false;
    bool     setupDone   = false;
    bool     configRead  = false;
    int8_t   heldSda = -1, heldScl = -1, heldLdr = -1, heldBuz = -1, heldBtn = -1, heldBat = -1;
    bool     rtcReady    = false;
    float    batV = -1.0f; int8_t batPct = -1; bool batReady = false, batLow = false, batCrit = false;
    unsigned long lastBat = 0;
    bool charging = false, chgRaw = false, batLatched = false; int8_t heldChg = -1;
    unsigned long chgChange = 0, lastLowBeep = 0, lowBeepStart = 0, wakeUntil = 0;
    bool dfActive = false, talkReady = false, talkBusy = false, trackDone = false, talkAck = false;
    uint8_t talkStage = 0, talkHead = 0, talkLen = 0, talkTimeouts = 0, talkQ[32], rxBuf[10], rxLen = 0;
    int8_t heldTRx = -1, heldTTx = -1, heldTBusy = -1;
    unsigned long talkInitAt = 0, talkNextAt = 0, talkStart = 0, lastLowSay = 0;
    uint32_t talkKey = 0;
    bool     rtcPinError = false;
    bool     rtcLoaded   = false;
    bool     rtcWritten  = false;
    float    tempC       = -100.0f;
    bool     offline     = true;
    int32_t  rtcDiffMs   = 0;      // last measured clock - RTC difference (ms, + = clock was ahead)
    uint16_t rtcBadReads = 0;      // RTC reads thrown away (noisy I2C)
    uint8_t  syncState   = 0;      // 0 idle, 1 waiting for the RTC second to change
    time_t   syncBase    = 0;
    unsigned long syncStart = 0, syncPollAt = 0, lastRtcWriteTry = 0;
    uint16_t rereadCount = 0;
    unsigned long lastReread = 0, lastRereadAt = 0;
    uint8_t  ampmMask    = 0;
    uint32_t mainCol     = 0xFFFFFF;
    unsigned long lastBattCheck = 0;
    bool     rtcWarn = false, rtcOsf = false, rtcInvalid = false;
    unsigned long lastRtcWrite = 0, lastRtcTry = 0, lastTick = 0, lastBriApply = 0, lastTemp = 0;
    uint32_t lastSig     = 0xFFFFFFFF;
    float    ldrAvg      = -1;
    uint8_t  lastBriSet  = 0;

    // alarm / chime / button runtime
    bool     alarmArmed  = true;   // quick toggle with long button press (not saved)
    bool     ringing     = false;
    unsigned long ringEnd = 0, chimeBeepStart = 0, beepUntil = 0, peekStart = 0;
    uint32_t snoozeAt = 0, alarmKey = 0, chimeKey = 0;
    bool     btnDown = false;
    unsigned long bootTestUntil = 0, greetFrom = 0, greetUntil = 0;
    bool     secHiddenNow = false;
    unsigned long btnSince = 0, btnChange = 0;

    // bit0=A ... bit6=G ; 0-9 digits, 10 blank, 11 dash, 12 degree, 13 C, 14 F, 15 b, 16 A, 17 t, 18 H
    static constexpr uint8_t PAT[22] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x00,0x40,0x63,0x39,0x71,0x7C,0x77,0x78,0x76,0x3E,0x38,0x3D};

    static const char _name[];

    // ---------- helpers ----------
    uint8_t digitCount() const { return mode == 0 ? 6 : 4; }
    uint8_t colonCount() const { return mode == 0 ? 2 : 1; }
    // chain: HH colon MM colon SS weekdays  -> a colon sits after every pair of digits
    uint16_t digitBase(uint8_t d) const { return firstLed + (uint16_t)d * 7 * ledsPerSeg + (uint16_t)(d / 2) * colonLeds; }
    uint16_t colonBaseAt(uint8_t k) const { return firstLed + (uint16_t)(k + 1) * 2 * 7 * ledsPerSeg + (uint16_t)k * colonLeds; }
    uint16_t dayBase()   const { return firstLed + (uint16_t)digitCount() * 7 * ledsPerSeg + (uint16_t)colonCount() * colonLeds; }
    uint16_t ampmBase()  const { return dayBase() + 7 * ledsPerDay; }
    uint16_t ledsNeeded() const { return (uint16_t)digitCount() * 7 * ledsPerSeg + (uint16_t)colonCount() * colonLeds + 7 * ledsPerDay + (ampmOn ? 2 * ampmLeds : 0); }

    static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

    static bool parseHex(const char* s, uint32_t& out) {
      if (!s) return false;
      if (*s == '#') s++;
      if (strlen(s) != 6) return false;
      for (int i = 0; i < 6; i++) if (!isxdigit((unsigned char)s[i])) return false;
      out = (uint32_t)strtoul(s, nullptr, 16);
      return true;
    }

    static void copyStr(char* dst, size_t n, const char* src) {
      strncpy(dst, src ? src : "", n - 1); dst[n - 1] = 0;
    }

    void buildOrder() {
      orderBad = false;
      switch (segPreset) {
        case 1: strcpy(order, "FABGEDC"); return;
        case 2: strcpy(order, "ABCDEFG"); return;
        case 3: strcpy(order, "FABCDEG"); return;
        default: break;
      }
      char tmp[8]; uint8_t seen = 0; uint8_t n = 0;
      for (const char* p = segCustom; *p && n < 7; p++) {
        char c = toupper((unsigned char)*p);
        if (c == '-' || c == ' ' || c == ',') continue;
        if (c < 'A' || c > 'G' || (seen & (1 << (c - 'A')))) { orderBad = true; break; }
        seen |= 1 << (c - 'A'); tmp[n++] = c;
      }
      if (orderBad || n != 7) { orderBad = true; strcpy(order, "FABGEDC"); return; }
      tmp[7] = 0; strcpy(order, tmp);
    }

    // ---------- RTC (DS1307 / DS3231, time stored as UTC like WLED core) ----------
    bool rtcProbe() {
      Wire.beginTransmission(C7_RTC_ADDR);
      return Wire.endTransmission() == 0;
    }

    // One raw read of the RTC.  0 = ok, 1 = time invalid (halted / never set / bad digits), 2 = bus error
    uint8_t rtcReadOnce(time_t &out) {
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0);
      if (Wire.endTransmission() != 0) return 2;
      if (Wire.requestFrom((uint8_t)C7_RTC_ADDR, (uint8_t)7) != 7) return 2;
      uint8_t b[7];
      for (int i = 0; i < 7; i++) b[i] = Wire.read();
      if (b[0] & 0x80) return 1;                              // DS1307 clock halted
      for (int i = 0; i < 7; i++) {                           // every BCD digit must be 0-9 (a noisy bus gives garbage)
        if (i == 3) continue;                                 // weekday register
        if ((b[i] & 0x0F) > 9 || ((b[i] >> 4) & 0x0F) > 9) return 1;
      }
      tmElements_t tm;
      tm.Second = bcd2dec(b[0] & 0x7F);
      tm.Minute = bcd2dec(b[1] & 0x7F);
      uint8_t h;
      if (b[2] & 0x40) { h = bcd2dec(b[2] & 0x1F); if (h == 12) h = 0; if (b[2] & 0x20) h += 12; }
      else h = bcd2dec(b[2] & 0x3F);
      tm.Hour  = h;
      tm.Day   = bcd2dec(b[4] & 0x3F);
      tm.Month = bcd2dec(b[5] & 0x1F);
      tm.Year  = bcd2dec(b[6]) + 30;
      if (tm.Second > 59 || tm.Minute > 59 || tm.Hour > 23 || tm.Day < 1 || tm.Day > 31 || tm.Month < 1 || tm.Month > 12) return 1;
      if (tm.Year < 50) return 1;                             // before 2020 = never set / battery was empty
      out = makeTime(tm);
      return 0;
    }

    // Stable read: two reads in a row must agree (within 1 s) before the value is trusted.
    // A single flipped bit on the I2C wires (e.g. 08 read as 18) can no longer move the clock by 10 seconds.
    bool rtcRead(time_t &out) {
      for (uint8_t attempt = 0; attempt < 3; attempt++) {
        time_t a = 0, b = 0;
        uint8_t ra = rtcReadOnce(a);
        delay(2);
        uint8_t rb = rtcReadOnce(b);
        if (ra == 0 && rb == 0) {
          int32_t d = (int32_t)((uint32_t)b - (uint32_t)a);
          if (d >= 0 && d <= 1) { out = b; rtcInvalid = false; return true; }
        } else if (ra == 1 && rb == 1) { rtcInvalid = true; return false; }   // really invalid (twice)
        if (rtcBadReads < 65535) rtcBadReads++;               // the two reads disagreed: noise, try again
      }
      return false;
    }

    bool rtcWrite(time_t t) {
      tmElements_t tm; breakTime(t, tm);
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0);
      Wire.write(dec2bcd(tm.Second));
      Wire.write(dec2bcd(tm.Minute));
      Wire.write(dec2bcd(tm.Hour));
      Wire.write(dec2bcd(tm.Wday));
      Wire.write(dec2bcd(tm.Day));
      Wire.write(dec2bcd(tm.Month));
      Wire.write(dec2bcd(tm.Year - 30));
      bool ok = Wire.endTransmission() == 0;
      if (ok) { rtcInvalid = false; if (rtcChip == 1) rtcClearOsf(); updateRtcWarn(); }
      return ok;
    }

    // DS3231 status register 0x0F bit 7 = oscillator stopped (battery was empty / power lost)
    void rtcReadOsf() {
      uint8_t st1 = 0, st2 = 0;
      if (i2cReadRegs(C7_RTC_ADDR, 0x0F, &st1, 1) && i2cReadRegs(C7_RTC_ADDR, 0x0F, &st2, 1)) rtcOsf = ((st1 & st2) & 0x80) != 0;
    }
    void rtcClearOsf() { if (i2cWriteReg(C7_RTC_ADDR, 0x0F, 0x00)) rtcOsf = false; }
    void updateRtcWarn() { rtcWarn = battCheck && rtcEnabled && rtcReady && (rtcInvalid || (rtcChip == 1 && rtcOsf)); }
    void rtcCheckBattery() {
      if (!battCheck) { rtcWarn = false; return; }
      time_t tmp;
      rtcRead(tmp);                                            // updates rtcInvalid
      if (rtcChip == 1) rtcReadOsf();
      updateRtcWarn();
    }

    bool rtcReadTemp() {                                       // DS3231 only
      Wire.beginTransmission(C7_RTC_ADDR);
      Wire.write((uint8_t)0x11);
      if (Wire.endTransmission() != 0) return false;
      if (Wire.requestFrom((uint8_t)C7_RTC_ADDR, (uint8_t)2) != 2) return false;
      int8_t msb = (int8_t)Wire.read();
      uint8_t lsb = Wire.read();
      float c = msb + (lsb >> 6) * 0.25f;
      if (c < -40.0f || c > 85.0f) return false;
      tempC = c;
      return true;
    }

    void rtcLoadToWled() {
      time_t t;
      if (!rtcRead(t)) return;
      if (toki.getTimeSource() < TOKI_TS_RTC) {
        toki.setTime(t, TOKI_NO_MS_ACCURACY, TOKI_TS_RTC);
        updateLocalTime();
      }
      rtcLoaded = true;
    }

    // ---------- pins / hardware ----------
    void releasePins() {
      if (heldSda >= 0) { C7_PM deallocatePin(heldSda, PinOwner::UM_Unspecified); heldSda = -1; }
      if (heldScl >= 0) { C7_PM deallocatePin(heldScl, PinOwner::UM_Unspecified); heldScl = -1; }
      if (heldLdr >= 0) { C7_PM deallocatePin(heldLdr, PinOwner::UM_Unspecified); heldLdr = -1; }
      if (heldBuz >= 0) { digitalWrite(heldBuz, buzzerInvert ? HIGH : LOW); C7_PM deallocatePin(heldBuz, PinOwner::UM_Unspecified); heldBuz = -1; }
      if (heldBtn >= 0) { C7_PM deallocatePin(heldBtn, PinOwner::UM_Unspecified); heldBtn = -1; }
      if (heldBat >= 0) { C7_PM deallocatePin(heldBat, PinOwner::UM_Unspecified); heldBat = -1; }
      if (heldChg >= 0) { C7_PM deallocatePin(heldChg, PinOwner::UM_Unspecified); heldChg = -1; }
      talkEnd();
    }

    // ---------- I2C helpers (used by the DS3231 status register) ----------
    bool i2cReadRegs(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
      Wire.beginTransmission(addr);
      Wire.write(reg);
      if (Wire.endTransmission() != 0) return false;
      if ((uint8_t)Wire.requestFrom(addr, n) != n) return false;
      for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
      return true;
    }

    bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t v) {
      Wire.beginTransmission(addr);
      Wire.write(reg);
      Wire.write(v);
      return Wire.endTransmission() == 0;
    }

    void initHw() {
      releasePins();
      rtcReady = false; rtcPinError = false; rtcLoaded = false; ldrAvg = -1; tempC = -100.0f;
      if (rtcEnabled && rtcSda >= 0 && rtcScl >= 0 && rtcSda != rtcScl) {
        bool sameAsGlobal = (rtcSda == i2c_sda && rtcScl == i2c_scl);
        bool ok = true;
        if (!sameAsGlobal) {
          managed_pin_type p[2] = { {(int8_t)rtcSda, true}, {(int8_t)rtcScl, true} };
          if (C7_PM allocateMultiplePins(p, 2, PinOwner::UM_Unspecified)) { heldSda = rtcSda; heldScl = rtcScl; }
          else { ok = false; rtcPinError = true; }
        }
        if (ok) {
          if (!sameAsGlobal) Wire.begin(rtcSda, rtcScl);
          if (rtcEnabled) {
            rtcReady = rtcProbe();
            if (rtcReady) { rtcLoadToWled(); if (rtcChip == 1) rtcReadTemp(); rtcCheckBattery(); }
          }
        }
      }
      lastReread = millis() - (unsigned long)rereadMin * 60000UL + 3000UL;      // first RTC alignment 3 s after start (if offline)
      batV = -1.0f; batPct = -1; batLow = false; batCrit = false; batReady = false;
      lastBat = millis() - (unsigned long)batIntSec * 1000UL + 3000UL;          // first reading 3 s after start
      #ifdef ESP8266
      batReady = batOn;
      #else
      if (batOn && batPin >= 0 && C7_PM allocatePin(batPin, false, PinOwner::UM_Unspecified)) { heldBat = batPin; batReady = true; pinMode(heldBat, INPUT); }
      #endif
      charging = false; chgRaw = false; chgChange = millis(); batLatched = false; lastLowBeep = 0; lowBeepStart = 0;
      if (chgOn && chgPin >= 0 && C7_PM allocatePin(chgPin, false, PinOwner::UM_Unspecified)) { heldChg = chgPin; pinMode(heldChg, INPUT_PULLUP); }
      #ifndef ESP8266
      if (autoBri && ldrPin >= 0) {
        if (C7_PM allocatePin(ldrPin, false, PinOwner::UM_Unspecified)) heldLdr = ldrPin;
      }
      #endif
      if (buzzerPin >= 0 && C7_PM allocatePin(buzzerPin, true, PinOwner::UM_Unspecified)) {
        heldBuz = buzzerPin; pinMode(heldBuz, OUTPUT); digitalWrite(heldBuz, buzzerInvert ? HIGH : LOW);
      }
      if (buttonPin >= 0 && C7_PM allocatePin(buttonPin, false, PinOwner::UM_Unspecified)) {
        heldBtn = buttonPin; pinMode(heldBtn, INPUT_PULLUP);
      }
      dfBegin();
    }

    // ---------- battery ----------
    float readBatRaw() {                                       // volts at the battery (after divider ratio + calibration)
      #ifdef ESP8266
        float v = analogRead(A0) / 1023.0f;
      #else
        if (heldBat < 0) return -1.0f;
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 8; i++) {
          #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2)
            sum += analogReadMilliVolts(heldBat);
          #else
            sum += (uint32_t)(analogRead(heldBat) * 3300UL / 4095UL);
          #endif
        }
        float v = (sum / 8.0f) / 1000.0f;
      #endif
      return v * batMult + batCal;
    }

    uint8_t percentFromV(float v) const {
      float x = (v - batMinV) / (batMaxV - batMinV);
      if (x <= 0.0f) return 0;
      if (x >= 1.0f) return 100;
      if (!curveOn) return (uint8_t)lroundf(x * 100.0f);
      static const uint16_t CV[12] = {330,361,369,373,377,380,384,387,395,402,411,420};   // 1-cell Li-ion curve (volts x100)
      static const uint8_t  CP[12] = {0,5,10,20,30,40,50,60,70,80,90,100};              // percent
      float v1 = 3.30f + x * 0.90f;                                                        // scale any pack to the 1-cell curve
      for (uint8_t i = 1; i < 12; i++) {
        float hi = CV[i] / 100.0f;
        if (v1 <= hi) { float lo = CV[i - 1] / 100.0f; return (uint8_t)lroundf(CP[i - 1] + (CP[i] - CP[i - 1]) * (v1 - lo) / (hi - lo)); }
      }
      return 100;
    }

    void batEval() {                                           // low / cut-off flags
      if (batV < 0) { batLow = false; batCrit = false; batLatched = false; return; }
      if (batV <= batLowV) batLow = true; else if (batV > batLowV + 0.05f) batLow = false;
      if (charging) { batLatched = false; batCrit = false; }                       // on the charger: never switch off
      else if (recOn) {                                                            // stays off until recover voltage (charger connected)
        if (batV <= batCutV) batLatched = true; else if (batV >= batRecV) batLatched = false;
        batCrit = batLatched;
      } else { if (batV <= batCutV) batCrit = true; else if (batV > batCutV + 0.10f) batCrit = false; }
    }

    void batUpdate() {
      float r = readBatRaw();
      if (r < 1.0f) { batV = -1.0f; batPct = -1; batLow = false; batCrit = false; batLatched = false; return; }   // nothing connected: do nothing
      batV = (batV < 0) ? r : batV + 0.2f * (r - batV);                                       // smoothing
      batPct = (int8_t)percentFromV(batV);
      batEval();
    }

    void chgPoll(unsigned long now) {                          // TP4056 CHRG pin, must be stable for 2 s
      if (!chgOn || heldChg < 0) { charging = false; return; }
      bool raw = (digitalRead(heldChg) == (chgLow ? LOW : HIGH));
      if (raw != chgRaw) { chgRaw = raw; chgChange = now; }
      if (raw != charging && now - chgChange > 2000) {
        charging = raw; batEval();
        if (talkOn && talkChgSay) {                              // spoken charger events (tracks 184 / 185)
          if (charging) talkPush(184);
          else if (batV >= batMaxV - 0.1f) talkPush(185);
        }
      }
    }

    bool asleepNow() const {                                   // sleep on battery: display mostly off
      if (!sleepOn || charging || batV < 0 || testMode) return false;
      if ((long)(wakeUntil - millis()) > 0) return false;
      uint32_t per = (uint32_t)sleepAwake + sleepOff;
      return ((uint32_t)localTime % per) >= sleepAwake;
    }

    uint8_t batDimAmt() const {                                // 255 = full brightness
      if (!batBriOn || batV < 0 || batFullV <= batCutV || batV >= batFullV) return 255;
      float f = (batV - batCutV) / (batFullV - batCutV);
      if (f < 0.0f) f = 0.0f;
      uint8_t fl = (uint8_t)(((uint16_t)batFloor * 255) / 100);
      return fl + (uint8_t)((255 - fl) * f);
    }

    // ---------- LDR auto brightness (ESP32 only) ----------
    #ifndef ESP8266
    int readLdr() {   // returns 0..1023
      if (heldLdr < 0) return -1;
      int raw = analogRead(heldLdr) >> 2;
      if (ldrInvert) raw = 1023 - raw;
      return raw;
    }

    void handleLdr(unsigned long now) {
      if (!autoBri) return;
      int raw = readLdr();
      if (raw < 0) return;
      if (ldrAvg < 0) ldrAvg = raw; else ldrAvg += ((float)raw - ldrAvg) / 8.0f;
      uint8_t lo = briMin < briMax ? briMin : briMax;
      uint8_t hi = briMin < briMax ? briMax : briMin;
      int target = lo + (int)((hi - lo) * (ldrAvg / 1023.0f));
      lastBriSet = (uint8_t)target;
      if (now - lastBriApply < 1000 || bri == 0) return;
      lastBriApply = now;
      if (abs(target - (int)bri) >= 4) { bri = (uint8_t)target; stateUpdated(CALL_MODE_DIRECT_CHANGE); }
    }
    #else
    void handleLdr(unsigned long) {}              // ESP8266: A0 is used by the battery, no LDR
    #endif

    // ---------- Bangla talking clock (DFPlayer Mini, SD card: /MP3/0001.mp3 ... 0190.mp3) ----------
    // track map: 1-99 numbers, 100 zero, 101-112 hours (1 o'clock..12 o'clock), 113 "now the time is", 114 "minutes", 115 "o'clock/it is",
    // 120-125 morning/noon/afternoon/evening/night/dawn, 130-136 Sunday..Saturday, 140-151 January..December, 160 "today", 162 "year", 163 "two thousand",
    // 170 "temperature", 171 "degree celsius", 172 "degree fahrenheit", 173 "minus", 180 "battery", 181 "percent", 182 "one hundred",
    // 116/117/118 exact 15/30/45 minute clips (classic voice), 183 "battery low, please charge", 184 "charging", 185 "battery full", 190 alarm message
    void talkEnd() {
      talkReady = false; talkBusy = false; talkLen = 0; talkHead = 0; talkStage = 0;
      #ifndef ESP8266
      if (dfActive) Serial2.end();
      #endif
      dfActive = false;
      if (heldTRx >= 0) { C7_PM deallocatePin(heldTRx, PinOwner::UM_Unspecified); heldTRx = -1; }
      if (heldTTx >= 0) { C7_PM deallocatePin(heldTTx, PinOwner::UM_Unspecified); heldTTx = -1; }
      if (heldTBusy >= 0) { C7_PM deallocatePin(heldTBusy, PinOwner::UM_Unspecified); heldTBusy = -1; }
    }

    void dfBegin() {
      talkAck = false; talkStage = 0; talkReady = false; rxLen = 0;
      if (!talkOn || talkTx < 0) return;
      if (!C7_PM allocatePin(talkTx, true, PinOwner::UM_Unspecified)) return;
      heldTTx = talkTx;
      if (talkBusyPin >= 0 && talkBusyPin != talkTx && C7_PM allocatePin(talkBusyPin, false, PinOwner::UM_Unspecified)) { heldTBusy = talkBusyPin; pinMode(heldTBusy, INPUT_PULLUP); }
      #ifdef ESP8266
      if (talkTx > 15) { C7_PM deallocatePin(heldTTx, PinOwner::UM_Unspecified); heldTTx = -1; return; }   // ESP8266 send-only serial works on GPIO 0-15
      pinMode(talkTx, OUTPUT); digitalWrite(talkTx, HIGH);
      #else
      if (talkRx >= 0 && C7_PM allocatePin(talkRx, false, PinOwner::UM_Unspecified)) heldTRx = talkRx;
      Serial2.begin(9600, SERIAL_8N1, heldTRx >= 0 ? heldTRx : 18, talkTx);
      #endif
      dfActive = true;
      talkInitAt = millis() + 2000UL; talkStage = 1;                          // DFPlayer needs about 2 s to read the SD card
    }

    #ifdef ESP8266
    void bbByte(uint8_t b) {                                   // 1 start bit, 8 data bits, 1 stop bit at 9600 baud, about 1 ms with interrupts off
      const uint32_t bitCycles = F_CPU / 9600UL;
      uint16_t w = ((uint16_t)b << 1) | 0x200;
      uint32_t mask = 1UL << talkTx;
      noInterrupts();
      uint32_t t = ESP.getCycleCount();
      for (uint8_t i = 0; i < 10; i++) {
        if ((w >> i) & 1) GPOS = mask; else GPOC = mask;
        t += bitCycles;
        while ((int32_t)(ESP.getCycleCount() - t) < 0) {}
      }
      interrupts();
    }
    #endif

    void dfWrite(const uint8_t* b, uint8_t n) {
      #ifdef ESP8266
      for (uint8_t i = 0; i < n; i++) { bbByte(b[i]); yield(); }      // ESP8266: send-only 9600 baud, bit-banged (SoftwareSerial does not compile with WLED 0.14.4)
      #else
      Serial2.write(b, n);
      #endif
    }

    void dfCmd(uint8_t cmd, uint8_t p1, uint8_t p2) {
      uint8_t f[10] = {0x7E, 0xFF, 0x06, cmd, 0x01, p1, p2, 0, 0, 0xEF};
      uint16_t sum = 0;
      for (uint8_t i = 1; i < 7; i++) sum += f[i];
      sum = (uint16_t)(0 - sum);
      f[7] = sum >> 8; f[8] = sum & 0xFF;
      dfWrite(f, 10);
    }

    void dfPoll(unsigned long now) {                           // reads DFPlayer answers: any frame = module alive, 0x3D = track finished
      #ifdef ESP8266
      return;                                                  // ESP8266: send only (use the BUSY pin to know when a word ends)
      #else
      for (;;) {
        if (Serial2.available() <= 0) break;
        int c = Serial2.read();
        if (rxLen == 0 && c != 0x7E) continue;
        rxBuf[rxLen++] = (uint8_t)c;
        if (rxLen >= 10) {
          if (rxBuf[9] == 0xEF) {
            talkAck = true;
            if (rxBuf[3] == 0x3D && now - talkStart > 150) trackDone = true;          // track finished
            if (rxBuf[3] == 0x40 && talkBusy) trackDone = true;                        // error (file not found): go on with the next word
          }
          rxLen = 0;
        }
      }
      #endif
    }

    void talkLoop(unsigned long now) {
      if (!dfActive) return;
      dfPoll(now);
      if (talkStage) {                                         // start-up: select SD card, set volume
        if ((long)(now - talkInitAt) < 0) return;
        if (talkStage == 1) { dfCmd(0x09, 0, 2); talkInitAt = now + 400; talkStage = 2; }
        else if (talkStage == 2) { dfCmd(0x06, 0, talkVol); talkInitAt = now + 300; talkStage = 3; }
        else { talkStage = 0; talkReady = true; }
        return;
      }
      if (talkBusy) {
        bool useFinish = (heldTRx >= 0 || heldTBusy >= 0) && talkTimeouts < 2;   // no finish signal twice: switch to fixed timing
        if (heldTBusy >= 0 && now - talkStart > 400 && digitalRead(heldTBusy) == HIGH) trackDone = true;   // BUSY pin is LOW while a word plays
        unsigned long lim = useFinish ? 5000UL : talkWordMs;
        if (trackDone) { talkTimeouts = 0; talkBusy = false; talkNextAt = now + 60; }
        else if (now - talkStart > lim) { if ((heldTRx >= 0 || heldTBusy >= 0) && talkTimeouts < 255) talkTimeouts++; talkBusy = false; talkNextAt = now + 60; }
      }
      if (!talkBusy && talkLen && (long)(now - talkNextAt) >= 0) {
        uint8_t tr = talkQ[talkHead]; talkHead = (talkHead + 1) % 32; talkLen--;
        dfCmd(0x12, 0, tr);                                    // play /MP3/000N.mp3
        talkBusy = true; trackDone = false; talkStart = now;
      }
    }

    void talkPush(uint8_t tr) {
      if (!dfActive || talkLen >= 32) return;
      talkQ[(talkHead + talkLen) % 32] = tr; talkLen++;
    }

    void stopTalk() { talkLen = 0; talkBusy = false; if (dfActive) dfCmd(0x16, 0, 0); }

    void sayTime() {                                           // "now the time is <period> <hour>-o'clock <minute> minutes"
      time_t t = localTime;
      if (t < 1600000000L) return;
      uint8_t h = hour(t), m = minute(t);
      talkPush(113);
      talkPush(h >= 4 && h < 6 ? 125 : (h >= 6 && h < 12 ? 120 : (h >= 12 && h < 16 ? 121 : (h >= 16 && h < 18 ? 122 : (h >= 18 && h < 20 ? 123 : 124)))));
      uint8_t h12 = h % 12; if (h12 == 0) h12 = 12;
      talkPush(100 + h12);
      if (talkExact && (m == 15 || m == 30 || m == 45)) talkPush(m == 15 ? 116 : (m == 30 ? 117 : 118));   // classic voice: exact minute clips
      else if (m && talkNums) { talkPush(m); if (talkWordMin) talkPush(114); }
      if (talkWordBaje) talkPush(115);
    }

    void sayDate() {                                           // "today <weekday> <day> <month> two-thousand <year> year"
      time_t t = localTime;
      if (t < 1600000000L) return;
      talkPush(160);
      talkPush(130 + (weekday(t) - 1));
      talkPush(day(t));
      talkPush(140 + (month(t) - 1));
      int y = year(t);
      if (y >= 2000 && y < 2100) { talkPush(163); if (y % 100) talkPush(y % 100); }
      talkPush(162);
    }

    void sayTemp() {
      if (!tempUsable()) return;
      int tv = tempShown(); bool neg = tv < 0; int a = abs(tv); if (a > 99) a = 99;
      talkPush(170);
      if (neg) talkPush(173);
      talkPush(a == 0 ? 100 : a);
      talkPush(tempUnit ? 172 : 171);
    }

    void sayBattery() {
      if (batV < 0) return;
      talkPush(180);
      int pc = batPct < 0 ? 0 : batPct;
      talkPush(pc >= 100 ? 182 : (pc == 0 ? 100 : pc));
      talkPush(181);
      if (charging) talkPush(184);
    }

    void sayButton() {
      if (talkBtnTime) sayTime();
      if (talkBtnDate) sayDate();
      if (talkBtnTemp) sayTemp();
      if (talkBtnBat) sayBattery();
    }

    void sayCmd(uint8_t c) {                                   // from JSON {"clk7":{"say":1}}: 1 time, 2 date, 3 temperature, 4 battery, 5 stop
      if (c == 1) sayTime(); else if (c == 2) sayDate(); else if (c == 3) sayTemp(); else if (c == 4) sayBattery(); else if (c == 5) stopTalk();
    }

    // ---------- alarm / chime / button ----------
    bool alarmDayOk(uint8_t wd) const {          // wd: 1 = Sunday
      if (alarmDays == 1) return wd >= 2 && wd <= 6;
      if (alarmDays == 2) return wd == 1 || wd == 7;
      return true;
    }

    void startRing() {
      ringing = true; ringEnd = millis() + (unsigned long)ringSec * 1000UL;
      if (talkOn && talkAlarm) { talkPush(190); sayTime(); }
    }

    void checkAlarmAndChime() {
      time_t t = localTime;
      if (t < 1600000000L) return;               // time not set yet
      uint8_t h = hour(t), m = minute(t), s = second(t), wd = weekday(t);
      if (alarmOn && alarmArmed && !ringing && s < 3 && h == alarmHour && m == alarmMin && alarmDayOk(wd)) {
        uint32_t key = (uint32_t)(t / 60);
        if (key != alarmKey) { alarmKey = key; startRing(); }
      }
      if (snoozeAt && !ringing && (uint32_t)t >= snoozeAt) { snoozeAt = 0; startRing(); }
      if (chimeOn && m == 0 && s < 3) {
        bool inWin = chimeStart <= chimeEnd ? (h >= chimeStart && h <= chimeEnd) : (h >= chimeStart || h <= chimeEnd);
        uint32_t key = (uint32_t)(t / 3600);
        if (inWin && key != chimeKey) { chimeKey = key; chimeBeepStart = millis(); }
      }
      if (talkOn && talkHourly && (m == 0 || (talkQuarter && m % 15 == 0)) && s < 3) {   // speak the time every full hour (and every quarter)
        bool inW = talkFrom <= talkTo ? (h >= talkFrom && h <= talkTo) : (h >= talkFrom || h <= talkTo);
        uint32_t key = (uint32_t)(t / 900);
        if (inW && key != talkKey) { talkKey = key; sayTime(); }
      }
    }

    // ---------- button ----------
    void onPress(bool longPress) {
      unsigned long now = millis();
      wakeUntil = now + 10000UL;                   // any press wakes the display (sleep on battery)
      if (ringing) {
        ringing = false;
        if (!longPress && snoozeMin > 0) snoozeAt = (uint32_t)localTime + (uint32_t)snoozeMin * 60UL;
        else snoozeAt = 0;
        beepUntil = now + 80;
      } else if (longPress) {
        alarmArmed = !alarmArmed; snoozeAt = 0;
        beepUntil = now + (alarmArmed ? 250 : 80);
      } else {
        peekStart = now;                           // show date for a few seconds
        if (talkOn && (talkBtnTime || talkBtnDate || talkBtnTemp || talkBtnBat)) { if (talkBusy || talkLen) stopTalk(); else sayButton(); }
      }
    }

    void pollButton(unsigned long now) {
      if (heldBtn < 0) return;
      bool down = digitalRead(heldBtn) == LOW;
      if (down && !btnDown && now - btnChange > 40) { btnDown = true; btnChange = now; btnSince = now; }
      else if (!down && btnDown && now - btnChange > 40) {
        btnDown = false; btnChange = now;
        onPress(now - btnSince >= 1000);           // short press: date / snooze, long press: alarm on/off
      }
    }

    void updateBuzzer(unsigned long now) {
      if (heldBuz < 0) return;
      bool want = false;
      if (ringing) want = ((now / 250) % 2) == 0;
      else if (chimeBeepStart) {
        unsigned long t = now - chimeBeepStart;
        if (t >= 600) chimeBeepStart = 0; else want = (t < 200) || (t >= 400);
      } else if (lowBeepStart) {                   // low battery: 3 short beeps
        unsigned long t = now - lowBeepStart;
        if (t >= 900) lowBeepStart = 0; else want = (t % 300) < 150;
      }
      if ((long)(beepUntil - now) > 0) want = true;
      digitalWrite(heldBuz, (want != buzzerInvert) ? HIGH : LOW);
    }

    // ---------- weekday LED order ----------
    // day: 0 = Sunday ... 6 = Saturday.  Returns the LED position (0..6) of that day in the chain.
    uint8_t weekdayPos(uint8_t day) const {
      switch (weekdayOrder) {
        case 1:  return 6 - day;                  // Saturday first ... Sunday last (flipped)
        case 2:  return (day + 6) % 7;            // Monday first ... Sunday last
        case 3:  return 6 - ((day + 6) % 7);      // Sunday first ... Monday last (flipped)
        default: return day;                      // Sunday first ... Saturday last
      }
    }
    uint8_t weekdayAtPos(uint8_t pos) const {     // which day sits at LED position pos
      for (uint8_t d = 0; d < 7; d++) if (weekdayPos(d) == pos) return d;
      return 0;
    }

    // ---------- brightness levels ----------
    uint8_t comb(uint8_t group) const { return (uint8_t)(((uint16_t)group * lvAll + 127) / 255); }   // group x all
    // lit LED: own colour (hex) or the colour source, then scale by level (255 = unchanged)
    void putLit(uint16_t i, bool hasCol, uint32_t col, uint8_t level) {
      if (!hasCol && level == 255 && colorSource == 0) return;
      uint32_t c;
      if (hasCol) c = col;
      else {
        c = strip.getPixelColor(i);
        if (colorSource == 1) c = mainCol;                                           // always the main page colour
        else if (colorSource == 2) {                                                 // effect colour, but never black
          uint16_t sum = ((c >> 24) & 255) + ((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255);
          if (sum < 24) c = mainCol;
        }
      }
      strip.setPixelColor(i, level == 255 ? c : color_fade(c, level));
    }
    template<typename V> bool readLevel(const V& v, uint8_t& dst) {
      uint16_t t = 255;
      bool ok = getJsonValue(v, t, (uint16_t)255);
      dst = t > 255 ? 255 : (uint8_t)t;
      return ok;
    }

    // ---------- display logic ----------
    bool greetActive() const {
      unsigned long now = millis();
      return greetUntil && (long)(now - greetFrom) >= 0 && (long)(greetUntil - now) > 0;
    }

    bool inNight() const {
      if (!nightOn) return false;
      uint8_t h = hour(localTime);
      if (nightStart == nightEnd) return false;
      return nightStart < nightEnd ? (h >= nightStart && h < nightEnd) : (h >= nightStart || h < nightEnd);
    }

    bool internetOk() const { return WLED_CONNECTED && ntpEnabled && ntpLastSyncTime != NTP_NEVER && (millis() - ntpLastSyncTime) < 45000000UL; }

    bool tempUsable() const { return tempShow && tempC > -90.0f && rtcReady && rtcChip == 1; }

    uint8_t displayState() {   // 0 time, 1 day+month, 2 year, 3 temperature, 5 battery warning
      if (peekStart) {
        unsigned long e = millis() - peekStart;
        if (e < 3000) return 1;
        if (e < 5000) return 2;
        peekStart = 0;
      }
      bool useDate = dateShow, useTemp = tempUsable(), useWarn = rtcWarn && warnShow;
      bool useBat = batOn && batReady && batV >= 0 && (batShow || batLow);
      if (!useDate && !useTemp && !useWarn && !useBat) return 0;
      uint16_t cyc = secTime + (useDate ? secDayMonth + secYear : 0) + (useTemp ? secTemp : 0) + (useBat ? secBat : 0) + (useWarn ? 2 : 0);
      uint16_t p = (uint32_t)localTime % cyc;
      if (p < secTime) return 0;
      p -= secTime;
      if (useDate) {
        if (p < secDayMonth) return 1;
        p -= secDayMonth;
        if (p < secYear) return 2;
        p -= secYear;
      }
      if (useTemp) {
        if (p < secTemp) return 3;
        p -= secTemp;
      }
      if (useBat) {
        if (p < secBat) return 7;
        p -= secBat;
      }
      return 5;
    }

    int tempShown() const {
      float c = tempC + tempOffset;
      float v = tempUnit ? c * 9.0f / 5.0f + 32.0f : c;
      return (int)lroundf(v);
    }

    // Builds segment bit masks per digit, colon state, weekday mask; returns display state
    uint8_t buildFrame(uint8_t* patt, bool& colonOn, uint8_t& wdMask) {
      uint8_t n = digitCount();
      for (uint8_t i = 0; i < 6; i++) patt[i] = 0;
      colonOn = false; wdMask = 0; secHiddenNow = false;
      time_t t = localTime;
      uint8_t wd = weekday(t);                                // 1 = Sunday
      if (wd >= 1 && wd <= 7) wdMask = 1 << (wd - 1);
      ampmMask = (hour(t) >= 12) ? 2 : 1;                     // bit0 = AM, bit1 = PM
      if (ampmBlink && toki.getTime().ms >= 500) ampmMask = 0;  // blink: off in the second half of each second

      uint8_t tm = testMode;
      if (!tm && bootTestUntil) { if ((long)(bootTestUntil - millis()) > 0) tm = 1; else bootTestUntil = 0; }
      if (tm) {
        uint8_t step = (millis() / 800) % 7;
        ampmMask = (tm == 2) ? ((step & 1) ? 2 : 1) : 3;
        if (tm == 1) { for (uint8_t i = 0; i < n; i++) patt[i] = 0x7F; colonOn = true; wdMask = 0x7F; }
        else if (tm == 2) { for (uint8_t i = 0; i < n; i++) patt[i] = 1 << step; wdMask = 1 << weekdayAtPos(step); }   // weekday LEDs walk along the chain
        else { uint8_t d = (millis() / 1000) % 10; for (uint8_t i = 0; i < n; i++) patt[i] = PAT[d]; colonOn = true; }
        return 0;
      }

      uint8_t v[6] = {10,10,10,10,10,10};
      if (!tm) {                                              // boot name ASHISH
        bool g = greetActive();
        if (!g && greetUntil && (long)(greetUntil - millis()) <= 0) greetUntil = 0;
        if (g) {
          static const uint8_t NAME[6] = {16, 5, 18, 1, 5, 18};      // A S H I S H
          if (mode == 0) { for (uint8_t i = 0; i < 6; i++) v[i] = NAME[i]; }       // 6 digits: stays on
          else {                                                       // 4 digits: [A S H I] 1.25 s, then [_ S H _] 1.25 s, repeat
            unsigned long frame = (millis() - greetFrom) / 1250UL;
            if ((frame & 1) == 0) { for (uint8_t i = 0; i < 4; i++) v[i] = NAME[i]; }   // A S H I
            else { v[1] = NAME[1]; v[2] = NAME[2]; }                                     // S H on the middle two digits
          }
          for (uint8_t i = 0; i < n; i++) patt[i] = PAT[v[i]];
          wdMask = 0; ampmMask = 0; colonOn = false;
          return 6;
        }
      }
      uint8_t st = displayState();
      if (st == 0) {
        int h = hourFormat ? hourFormat12(t) : hour(t);
        int m = minute(t), s = second(t);
        v[0] = (h < 10 && !leadingZero) ? 10 : h / 10; v[1] = h % 10;
        v[2] = m / 10; v[3] = m % 10;
        if (mode == 0) { v[4] = s / 10; v[5] = s % 10; }
        colonOn = !colonBlink || (toki.getTime().ms < 500);
      } else if (st == 1) {
        int a = dateOrder == 0 ? day(t) : month(t);
        int b = dateOrder == 0 ? month(t) : day(t);
        if (mode == 0) { v[0] = a / 10; v[1] = a % 10; v[2] = 11; v[3] = 11; v[4] = b / 10; v[5] = b % 10; }
        else           { v[0] = a / 10; v[1] = a % 10; v[2] = b / 10; v[3] = b % 10; }
      } else if (st == 2) {
        int y = year(t);
        if (mode == 0) { v[0] = 10; v[1] = y / 1000; v[2] = (y / 100) % 10; v[3] = (y / 10) % 10; v[4] = y % 10; v[5] = 10; }
        else           { v[0] = y / 1000; v[1] = (y / 100) % 10; v[2] = (y / 10) % 10; v[3] = y % 10; }
      } else if (st == 7) {                                   // battery: percent "b  85" or volts "4:12 U"
        if (charging && ((uint32_t)localTime & 1)) {                  // charging: CHG and percent alternate every second
          if (mode == 0) { v[0] = 10; v[1] = 13; v[2] = 18; v[3] = 21; v[4] = 10; v[5] = 10; }
          else           { v[0] = 13; v[1] = 18; v[2] = 21; v[3] = 10; }
        } else if (batShowAs == 1) {
          int cv = (int)lroundf(batV * 100.0f); if (cv > 9999) cv = 9999; if (cv < 0) cv = 0;
          uint8_t d0 = cv >= 1000 ? cv / 1000 : 10, d1 = (cv / 100) % 10, d2 = (cv / 10) % 10, d3 = cv % 10;
          if (mode == 0) { v[0] = d0; v[1] = d1; v[2] = d2; v[3] = d3; v[4] = 19; v[5] = 10; secHiddenNow = true; }   // 4:12 U (volts, colon = decimal point)
          else           { v[0] = d0; v[1] = d1; v[2] = d2; v[3] = d3; }
          colonOn = true;
        } else {
          int pc = batPct < 0 ? 0 : batPct;
          uint8_t hu = pc >= 100 ? 1 : 10, te = pc >= 10 ? (pc / 10) % 10 : 10, ones = pc % 10;
          if (mode == 0) { v[0] = 15; v[1] = 10; v[2] = hu; v[3] = te; v[4] = ones; v[5] = 10; }
          else           { v[0] = 15; v[1] = hu; v[2] = te; v[3] = ones; }
        }
      } else if (st == 5) {                                   // RTC battery / time problem: bAtt
        if (mode == 0) { v[0] = 10; v[1] = 15; v[2] = 16; v[3] = 17; v[4] = 17; v[5] = 10; }
        else           { v[0] = 15; v[1] = 16; v[2] = 17; v[3] = 17; }
      } else {
        int tv = tempShown(); bool neg = tv < 0; int a = abs(tv); if (a > 99) a = 99;
        uint8_t tens = a / 10, ones = a % 10, unitCh = tempUnit ? 14 : 13;
        uint8_t t1 = (a < 10) ? (neg ? 11 : 10) : tens;
        if (mode == 0) { v[0] = 10; v[1] = t1; v[2] = ones; v[3] = 12; v[4] = unitCh; v[5] = 10; }
        else           { v[0] = t1; v[1] = ones; v[2] = 12; v[3] = unitCh; }
      }
      for (uint8_t i = 0; i < n; i++) patt[i] = PAT[v[i]];
      return st;
    }

    uint32_t frameSignature() {
      uint8_t patt[6]; bool colon; uint8_t wdm;
      buildFrame(patt, colon, wdm);
      uint32_t sig = 17;
      for (uint8_t i = 0; i < 6; i++) sig = sig * 131 + patt[i];
      sig = sig * 3 + (colon ? 1 : 0);
      sig = sig * 131 + wdm;
      sig = sig * 5 + ampmMask;
      sig = sig * 3 + (inNight() ? 1 + nightAction : 0);
      sig = sig * 3 + (batCrit ? 1 : 0);
      sig = sig * 3 + (asleepNow() ? 1 : 0);
      sig = sig * 3 + (charging ? 1 : 0);
      sig = sig * 131 + (batDimAmt() >> 2);
      sig = sig * 3 + (ringing ? 1 + ((millis() / 500) % 2) : 0);
      return sig;
    }

    static void fmtDays(uint8_t d, char* out, size_t n) {
      const char* s = d == 1 ? "Mon-Fri" : (d == 2 ? "Sat-Sun" : "every day");
      strncpy(out, s, n - 1); out[n - 1] = 0;
    }

  public:
    void setup() {
      setupDone = true;
      buildOrder();
      if (startupTest) bootTestUntil = millis() + 3000;
      if (greetOn) { greetFrom = millis() + (startupTest ? 3000UL : 0UL); greetUntil = greetFrom + (unsigned long)greetSec * 1000UL; }
      if (configRead) initHw();
    }

    // Waits (non-blocking) for the RTC second to change, then aligns the clock to that exact moment
    void pollEdge(unsigned long now) {
      if (now - syncPollAt < 20) return;
      syncPollAt = now;
      if (now - syncStart > 1600) { syncState = 0; return; }   // RTC second never changed: give up
      time_t t1, t2;
      if (rtcReadOnce(t1) != 0) return;
      if (t1 != syncBase + 1) return;                          // not the next second yet (or noise)
      if (rtcReadOnce(t2) != 0 || t2 != t1) return;            // confirm with a second read
      // the RTC second changed within the last ~20 ms
      int32_t sysMs = (int32_t)toki.getTime().ms;
      int32_t offMs = (int32_t)((uint32_t)toki.second() - (uint32_t)t1) * 1000 + sysMs - 10;   // + = clock ahead of the RTC
      rtcDiffMs = offMs; lastRereadAt = now; if (rereadCount < 65535) rereadCount++;
      if (offMs >= 150 || offMs <= -150) { toki.setTime(t1, 10, TOKI_TS_RTC); updateLocalTime(); }
      syncState = 0;
    }

    void loop() {
      unsigned long now = millis();
      pollButton(now);
      updateBuzzer(now);
      talkLoop(now);
      if (syncState) pollEdge(now);
      if (now - lastTick < 100) return;
      lastTick = now;

      // temperature sensor (every 10 s)
      if (now - lastTemp > 10000) {
        lastTemp = now;
        if (rtcEnabled && rtcReady && rtcChip == 1) rtcReadTemp();
      }

      // battery voltage
      if (batOn && batReady && now - lastBat >= (unsigned long)batIntSec * 1000UL) { lastBat = now; batUpdate(); }
      chgPoll(now);
      if (beepOn && batLow && !batCrit && !charging && !inNight()) {            // low battery beep
        if (lastLowBeep == 0 || now - lastLowBeep >= (unsigned long)beepMin * 60000UL) { lastLowBeep = now ? now : 1; lowBeepStart = now ? now : 1; }
      } else if (!batLow) lastLowBeep = 0;
      if (talkOn && talkLowBat && batLow && !batCrit && !charging && !inNight()) {   // spoken low battery warning
        if (lastLowSay == 0 || now - lastLowSay >= (unsigned long)talkLowMin * 60000UL) { lastLowSay = now ? now : 1; talkPush(183); }
      } else if (!batLow) lastLowSay = 0;

      // RTC maintenance
      if (rtcEnabled && rtcReady) {
        if (battCheck && now - lastBattCheck > 60000UL) { lastBattCheck = now; rtcCheckBattery(); }
        if (!rtcLoaded && now - lastRtcTry > 10000) { lastRtcTry = now; rtcLoadToWled(); }

        // internet time good? (WiFi up, NTP on, synced in the last ~12 h)
        bool online = internetOk();
        if (online) { if (offline) { offline = false; rtcWritten = false; } }   // internet is back: update RTC soon
        else offline = true;

        // no internet time: every N minutes read the RTC, then wait for its next second edge and
        // align the clock to it (accuracy about 0.02 s). Only moved when 0.15 s or more off.
        if (rereadOn && !online && syncState == 0 && now - lastReread >= (unsigned long)rereadMin * 60000UL) {
          lastReread = now;
          time_t rt;
          if (rtcRead(rt)) { syncBase = rt; syncState = 1; syncStart = now; syncPollAt = 0; }
        }

        uint8_t src = toki.getTimeSource();
        if (src > TOKI_TS_RTC && (lastRtcWriteTry == 0 || now - lastRtcWriteTry > 30000UL)) {   // time came from NTP / browser
          bool due = !rtcWritten || (rtcSyncHours > 0 && now - lastRtcWrite > (unsigned long)rtcSyncHours * 3600000UL);
          if (due && toki.isTick()) {
            lastRtcWriteTry = now ? now : 1;
            bool ok = false;
            if (rtcWrite(toki.second())) {                    // write, then read back to be sure it is right
              time_t chk;
              delay(3);
              if (rtcRead(chk)) { int32_t d = (int32_t)((uint32_t)chk - (uint32_t)toki.second()); ok = (d >= -2 && d <= 2); }
            }
            if (ok) { rtcWritten = true; lastRtcWrite = now; } else rtcWritten = false;           // retry in 30 s
          }
        }
      }

      checkAlarmAndChime();
      if (ringing && (long)(now - ringEnd) >= 0) ringing = false;

      if (!enabled) return;
      handleLdr(now);

      uint32_t sig = frameSignature();
      if (sig != lastSig) { lastSig = sig; strip.trigger(); }
    }

    void handleOverlayDraw() {
      if (!enabled) return;
      uint16_t total = strip.getLengthTotal();
      uint8_t n = digitCount();
      uint16_t endLed = firstLed + ledsNeeded();
      if (blankExtra) { for (uint16_t i = endLed; i < total; i++) strip.setPixelColor(i, 0); }   // LEDs after the clock chain stay off
      if (endLed > total) endLed = total;

      bool night = inNight() && !testMode && !greetActive();
      bool ringBlank = ringing && ((millis() / 500) % 2) == 1;
      bool asleep = asleepNow() && !greetActive();
      if (ringBlank || batCrit || asleep || (night && nightAction == 0)) {   // batCrit: battery empty, asleep: sleep on battery
        for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, 0);
        return;
      }

      uint8_t patt[6]; bool colon; uint8_t wdm;
      uint8_t st = buildFrame(patt, colon, wdm);

      uint32_t dcol = 0;
      bool hasDigitCol = false;
      if (st == 1 || st == 2) hasDigitCol = parseHex(dateHex, dcol);
      else if (st == 3) {
        hasDigitCol = parseHex(tempHex, dcol);
        if (!hasDigitCol && tempAutoColor) {                  // blue under 18 C, red over 30 C, else green
          float tc = tempC + tempOffset;
          dcol = tc < 18.0f ? 0x0066FF : (tc > 30.0f ? 0xFF0000 : 0x00FF00);
          hasDigitCol = true;
        }
      }
      else if (st == 5) { dcol = 0xFF0000; hasDigitCol = true; }
      else if (st == 7) {                                     // battery colour: own hex, or green / orange / red
        hasDigitCol = parseHex(batHex, dcol);
        if (!hasDigitCol && batAutoCol) { dcol = batLow ? 0xFF0000 : (batPct < 60 ? 0xFF8000 : 0x00FF00); hasDigitCol = true; }
      }

      if (colorSource) { mainCol = strip.getMainSegment().colors[0]; if (mainCol == 0) mainCol = 0xFFFFFF; }   // main page colour (white if black)
      uint8_t sDig = comb(lvDigits), sCol = comb(lvColon), sToday = comb(lvToday), sOther = comb(lvOther), sAp = comb(lvAmPm);

      // digits
      for (uint8_t d = 0; d < n; d++) {
        for (uint8_t s = 0; s < 7; s++) {
          bool lit = (patt[d] >> (order[s] - 'A')) & 1;
          uint16_t base = digitBase(d) + (uint16_t)s * ledsPerSeg;
          for (uint8_t k = 0; k < ledsPerSeg; k++) {
            if (base + k >= total) break;
            if (!lit) strip.setPixelColor(base + k, 0);
            else putLit(base + k, hasDigitCol, dcol, sDig);
          }
        }
      }

      // colons (one after HH, one after MM)
      uint32_t col;
      bool hasColonCol = parseHex(colonHex, col);
      for (uint8_t c = 0; c < colonCount(); c++) {
        for (uint8_t k = 0; k < colonLeds; k++) {
          uint16_t i = colonBaseAt(c) + k;
          if (i >= total) break;
          if (!colon || (c == 1 && secHiddenNow)) strip.setPixelColor(i, 0);
          else putLit(i, hasColonCol, col, sCol);
        }
      }

      // weekdays: 0 = off, 1 = only today, 2 = all on (today in its own colour)
      bool barNow = barOn && st == 7 && batPct >= 0;                 // battery bar on the weekday LEDs
      uint8_t barCnt = barNow ? (uint8_t)((batPct * 7 + 50) / 100) : 0;
      if (barNow && barCnt < 1) barCnt = 1;
      uint32_t barCol = batLow ? 0xFF0000 : (batPct < 60 ? 0xFF8000 : 0x00FF00);
      parseHex(batHex, barCol);
      uint32_t todayCol = 0xFF0000, otherCol = 0;
      bool hasToday = parseHex(wdHex, todayCol);
      bool hasOther = parseHex(otherHex, otherCol);
      uint32_t weekendCol = 0;
      bool hasWeekend = parseHex(weekendHex, weekendCol);
      for (uint8_t d = 0; d < 7; d++) {
        bool isToday = (wdm >> d) & 1;
        for (uint8_t k = 0; k < ledsPerDay; k++) {
          uint16_t i = dayBase() + weekdayPos(d) * ledsPerDay + k;
          if (i >= total) break;
          if (barNow) { if (weekdayPos(d) < barCnt) putLit(i, true, barCol, sToday); else strip.setPixelColor(i, 0); }
          else if (weekdayMode == 0 || st == 6) strip.setPixelColor(i, 0);
          else if (weekdayMode == 1) {
            if (!isToday) strip.setPixelColor(i, 0);
            else putLit(i, hasToday, todayCol, sToday);
          } else {
            if (isToday) putLit(i, true, todayCol, sToday);                                  // red unless you set Weekday Hex
            else if ((d == 0 || d == 6) && hasWeekend) putLit(i, true, weekendCol, sOther);  // Sun / Sat
            else putLit(i, hasOther, otherCol, sOther);                                      // else main page colour
          }
        }
      }

      // AM / PM LEDs (after the weekday LEDs): AM group first, then PM group
      if (ampmOn) {
        uint32_t amCol = 0, pmCol = 0;
        bool hasAm = parseHex(amHex, amCol), hasPm = parseHex(pmHex, pmCol);
        for (uint8_t g = 0; g < 2; g++) {
          bool lit = (ampmMask >> g) & 1;
          for (uint8_t k = 0; k < ampmLeds; k++) {
            uint16_t i = ampmBase() + g * ampmLeds + k;
            if (i >= total) break;
            if (!lit) strip.setPixelColor(i, 0);
            else putLit(i, g == 0 ? hasAm : hasPm, g == 0 ? amCol : pmCol, sAp);
          }
        }
      }

      // battery voltage based brightness: lower voltage = dimmer display
      {
        uint8_t ba = batDimAmt();
        if (ba < 255) for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, color_fade(strip.getPixelColor(i), ba));
      }

      // night dim: lower the brightness of everything that is lit
      if (night && nightAction == 1) {
        uint8_t amt = (uint8_t)(((uint16_t)constrain(nightDim, 1, 100) * 255) / 100);
        for (uint16_t i = firstLed; i < endLed; i++) strip.setPixelColor(i, color_fade(strip.getPixelColor(i), amt));
      }
    }

    // ---------- JSON / Info page ----------
    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));

      static const char* const WD[7]  = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
      static const char* const MON[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
      time_t t = localTime;
      bool timeSet = t > 1600000000L;
      char b[48];

      // 1) Time
      if (hourFormat) snprintf(b, sizeof(b), "%d:%02d:%02d %s", hourFormat12(t), minute(t), second(t), hour(t) >= 12 ? "PM" : "AM");
      else            snprintf(b, sizeof(b), "%02d:%02d:%02d", hour(t), minute(t), second(t));
      JsonArray a1 = user.createNestedArray(F("Clock time"));
      a1.add(String(b));
      if (!timeSet) a1.add(F(" (not set)"));

      // 2) Date
      uint8_t mo = month(t); if (mo < 1 || mo > 12) mo = 1;
      snprintf(b, sizeof(b), "%02d %s %d", day(t), MON[mo - 1], year(t));
      JsonArray a2 = user.createNestedArray(F("Clock date"));
      a2.add(String(b));

      // 3) Weekday
      uint8_t wd = weekday(t); if (wd < 1 || wd > 7) wd = 1;
      JsonArray a3 = user.createNestedArray(F("Clock weekday"));
      a3.add(WD[wd - 1]);
      snprintf(b, sizeof(b), " (LED %d of 7)", (int)weekdayPos(wd - 1) + 1);
      a3.add(String(b));

      // 4) LDR / brightness (ESP32 only)
      #ifndef ESP8266
      JsonArray a4 = user.createNestedArray(F("Clock LDR"));
      if (!autoBri) a4.add(F("auto brightness OFF"));
      else if (ldrAvg < 0) a4.add(F("no reading (check pin)"));
      else { snprintf(b, sizeof(b), "light %d%%, bri %d", (int)(ldrAvg * 100.0f / 1023.0f), lastBriSet); a4.add(String(b)); }

      #endif

      // 5) RTC
      JsonArray a5 = user.createNestedArray(F("Clock RTC"));
      if (!rtcEnabled) a5.add(F("disabled"));
      else if (rtcPinError) a5.add(F("pin conflict"));
      else if (rtcReady) { a5.add(rtcLoaded || rtcWritten ? F("OK") : F("found, time not set yet")); a5.add(rtcChip ? F(" (DS3231)") : F(" (DS1307)")); }
      else a5.add(F("NOT found - check SDA/SCL/power"));

      // 5b) RTC re-read (when there is no internet time)
      JsonArray a5b = user.createNestedArray(F("Clock RTC re-read"));
      if (!rtcEnabled || !rtcReady) a5b.add(F("n/a"));
      else if (!rereadOn) a5b.add(F("off"));
      else if (internetOk()) a5b.add(F("not needed (internet time OK)"));
      else if (rereadCount == 0) { snprintf(b, sizeof(b), "every %d min, waiting", rereadMin); a5b.add(String(b)); }
      else { char bb[72]; snprintf(bb, sizeof(bb), "every %d min, last diff %ld ms, bad reads %d", rereadMin, (long)rtcDiffMs, (int)rtcBadReads); a5b.add(String(bb)); }

      // 5c) RTC battery / time warning
      JsonArray a5c = user.createNestedArray(F("Clock RTC battery"));
      if (!battCheck) a5c.add(F("check is OFF"));
      else if (!rtcEnabled || !rtcReady) a5c.add(F("n/a"));
      else if (rtcWarn) a5c.add(F("WARNING: time lost - replace CR2032 / set time"));
      else a5c.add(F("OK (time valid)"));

      // 6) Temperature (DS3231 inside the RTC module)
      JsonArray a6 = user.createNestedArray(F("Clock temperature"));
      if (rtcChip == 1 && tempC > -90.0f) { snprintf(b, sizeof(b), "%d %s", tempShown(), tempUnit ? "F" : "C"); a6.add(String(b)); a6.add(F(" (DS3231)")); }
      else a6.add(F("n/a (needs DS3231)"));

      // 6b) Battery
      JsonArray a6b = user.createNestedArray(F("Clock battery"));
      if (!batOn) a6b.add(F("off"));
      else if (!batReady) a6b.add(F("pin error"));
      else if (batV < 0) a6b.add(F("no battery / waiting"));
      else {
        int cv = (int)lroundf(batV * 100.0f);
        snprintf(b, sizeof(b), "%d.%02d V, %d %%", cv / 100, cv % 100, (int)batPct);
        a6b.add(String(b));
        if (charging) a6b.add(F(" CHARGING")); else if (batCrit) a6b.add(F(" CUT-OFF")); else if (batLow) a6b.add(F(" LOW"));
      }

      // 6c) Talking clock
      JsonArray a6t = user.createNestedArray(F("Clock talking"));
      if (!talkOn) a6t.add(F("off"));
      else if (!dfActive) a6t.add(F("pin error"));
      else if (!talkReady) a6t.add(F("starting..."));
      else if (heldTBusy >= 0) a6t.add(F("ready (BUSY pin)"));
      else if (heldTRx < 0) a6t.add(F("ready (send only, fixed word timing)"));
      else a6t.add(talkAck ? F("ready, DFPlayer answers") : F("ready, NO answer from DFPlayer (check wiring / SD card)"));

      // 7) Alarm
      JsonArray a7 = user.createNestedArray(F("Clock alarm"));
      if (!alarmOn) a7.add(F("off"));
      else {
        char d[12]; fmtDays(alarmDays, d, sizeof(d));
        snprintf(b, sizeof(b), "%02d:%02d %s%s", alarmHour, alarmMin, d, alarmArmed ? "" : " (paused)");
        a7.add(String(b));
        if (ringing) a7.add(F(" RINGING"));
      }

      // 8) LED count
      JsonArray a8 = user.createNestedArray(F("Clock LEDs needed"));
      a8.add(ledsNeeded() + firstLed);
      a8.add(strip.getLengthTotal() >= ledsNeeded() + firstLed ? F(" (strip OK)") : F(" - raise LED count in LED Preferences!"));
      if (orderBad) { JsonArray a9 = user.createNestedArray(F("Clock seg order")); a9.add(F("invalid - using FABGEDC")); }
    }

    void addToJsonState(JsonObject& root) {
      JsonObject o = root.createNestedObject(F("clk7"));
      o[F("test")] = testMode;
      o[F("date")] = dateShow;
      o[F("on")]   = enabled;
      if (jsonOn && batOn) {                         // battery values for apps / Home Assistant: /json/state
        o[F("batv")] = (float)lroundf(batV * 100.0f) / 100.0f;   // volts, -1 = no battery
        o[F("batp")] = batPct;                                    // percent, -1 = no battery
        o[F("chg")]  = charging;
        o[F("low")]  = batLow;
        o[F("cut")]  = batCrit;
      }
    }

    void readFromJsonState(JsonObject& root) {
      JsonObject o = root[F("clk7")];
      if (o.isNull()) return;
      testMode = o[F("test")] | testMode;
      dateShow = o[F("date")] | dateShow;
      enabled  = o[F("on")]   | enabled;
      uint8_t say = o[F("say")] | 0;                  // {"clk7":{"say":1}} speaks the time (1 time, 2 date, 3 temp, 4 battery, 5 stop)
      if (say) sayCmd(say);
    }

    // ---------- settings page ----------
    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[F("enabled")] = enabled;

      JsonObject w = top.createNestedObject(F("wiring"));
      w[F("first-led")] = firstLed;
      w[F("seg-preset")] = segPreset;
      w[F("seg-order")] = segCustom;
      w[F("leds-per-segment")] = ledsPerSeg;
      w[F("leds-per-colon")] = colonLeds;
      w[F("leds-per-weekday")] = ledsPerDay;
      w[F("blank-extra")] = blankExtra;

      JsonObject ap = top.createNestedObject(F("am-pm-leds"));
      ap[F("enabled")] = ampmOn;
      ap[F("leds-each")] = ampmLeds;
      ap[F("am-hex")] = amHex;
      ap[F("pm-hex")] = pmHex;
      ap[F("blink")] = ampmBlink;

      JsonObject d = top.createNestedObject(F("display"));
      d[F("hour-format")] = hourFormat;
      d[F("leading-zero")] = leadingZero;
      d[F("colon-blink")] = colonBlink;
      d[F("weekday-mode")] = weekdayMode;
      d[F("weekday-order")] = weekdayOrder;
      d[F("startup-test")] = startupTest;

      JsonObject dt = top.createNestedObject(F("date"));
      dt[F("show")] = dateShow;
      dt[F("order")] = dateOrder;
      dt[F("time-sec")] = secTime;
      dt[F("day-month-sec")] = secDayMonth;
      dt[F("year-sec")] = secYear;

      JsonObject tp = top.createNestedObject(F("temperature"));
      tp[F("show")] = tempShow;
      tp[F("auto-color")] = tempAutoColor;
      tp[F("unit")] = tempUnit;
      tp[F("temp-sec")] = secTemp;
      tp[F("offset")] = tempOffset;

      JsonObject c = top.createNestedObject(F("colors"));
      c[F("color-source")] = colorSource;
      c[F("weekday-hex")] = wdHex;
      c[F("other-days-hex")] = otherHex;
      c[F("weekend-hex")] = weekendHex;
      c[F("colon-hex")] = colonHex;
      c[F("date-hex")] = dateHex;
      c[F("temp-hex")] = tempHex;

      JsonObject al = top.createNestedObject(F("alarm"));
      al[F("enabled")] = alarmOn;
      al[F("hour")] = alarmHour;
      al[F("minute")] = alarmMin;
      al[F("days")] = alarmDays;
      al[F("ring-sec")] = ringSec;
      al[F("snooze-min")] = snoozeMin;

      JsonObject ch = top.createNestedObject(F("hourly-chime"));
      ch[F("enabled")] = chimeOn;
      ch[F("start-hour")] = chimeStart;
      ch[F("end-hour")] = chimeEnd;

      JsonObject bz = top.createNestedObject(F("buzzer-button"));
      bz[F("buzzer-pin")] = buzzerPin;
      bz[F("buzzer-invert")] = buzzerInvert;
      bz[F("button-pin")] = buttonPin;

      JsonObject bn = top.createNestedObject(F("boot-name"));
      bn[F("enabled")] = greetOn;
      bn[F("seconds")] = greetSec;

      JsonObject lv = top.createNestedObject(F("levels"));
      lv[F("all")] = lvAll;
      lv[F("digits")] = lvDigits;
      lv[F("colon")] = lvColon;
      lv[F("today")] = lvToday;
      lv[F("other-days")] = lvOther;
      lv[F("am-pm")] = lvAmPm;

      JsonObject n = top.createNestedObject(F("night"));
      n[F("enabled")] = nightOn;
      n[F("start-hour")] = nightStart;
      n[F("end-hour")] = nightEnd;
      n[F("action")] = nightAction;
      n[F("dim-percent")] = nightDim;

      #ifndef ESP8266
      JsonObject b = top.createNestedObject(F("brightness"));
      b[F("auto")] = autoBri;
      b[F("ldr-pin")] = ldrPin;
      b[F("ldr-invert")] = ldrInvert;
      b[F("min")] = briMin;
      b[F("max")] = briMax;
      #endif

      JsonObject r = top.createNestedObject(F("rtc"));
      r[F("enabled")] = rtcEnabled;
      r[F("chip")] = rtcChip;
      r[F("sda-pin")] = rtcSda;
      r[F("scl-pin")] = rtcScl;
      r[F("sync-hours")] = rtcSyncHours;
      r[F("reread-on")] = rereadOn;
      r[F("reread-min")] = rereadMin;
      r[F("battery-check")] = battCheck;
      r[F("warn-display")] = warnShow;

      JsonObject t = top.createNestedObject(F("test"));
      t[F("mode")] = testMode;

      JsonObject bt = top.createNestedObject(F("battery"));
      bt[F("enabled")] = batOn;
      bt[F("adc-pin")] = batPin;
      bt[F("multiplier")] = batMult;
      bt[F("calibration-v")] = batCal;
      bt[F("min-volt")] = batMinV;
      bt[F("max-volt")] = batMaxV;
      bt[F("read-sec")] = batIntSec;
      bt[F("show-display")] = batShow;
      bt[F("show-as")] = batShowAs;
      bt[F("show-sec")] = secBat;
      bt[F("low-volt")] = batLowV;
      bt[F("cutoff-volt")] = batCutV;
      bt[F("auto-color")] = batAutoCol;
      bt[F("digit-hex")] = batHex;
      bt[F("bri-by-volt")] = batBriOn;
      bt[F("full-bri-volt")] = batFullV;
      bt[F("min-bri-pct")] = batFloor;
      bt[F("curve")] = curveOn;
      bt[F("bar")] = barOn;

      JsonObject rc = top.createNestedObject(F("cutoff-recovery"));
      rc[F("enabled")] = recOn;
      rc[F("recover-volt")] = batRecV;

      JsonObject cg = top.createNestedObject(F("charging"));
      cg[F("enabled")] = chgOn;
      cg[F("pin")] = chgPin;
      cg[F("active-low")] = chgLow;

      JsonObject lb = top.createNestedObject(F("low-beep"));
      lb[F("enabled")] = beepOn;
      lb[F("repeat-min")] = beepMin;

      JsonObject sl = top.createNestedObject(F("sleep"));
      sl[F("enabled")] = sleepOn;
      sl[F("awake-sec")] = sleepAwake;
      sl[F("sleep-sec")] = sleepOff;

      JsonObject js = top.createNestedObject(F("json"));
      js[F("enabled")] = jsonOn;

      JsonObject tk = top.createNestedObject(F("talking"));
      tk[F("enabled")] = talkOn;
      tk[F("rx-pin")] = talkRx;
      tk[F("tx-pin")] = talkTx;
      tk[F("busy-pin")] = talkBusyPin;
      tk[F("volume")] = talkVol;
      tk[F("hourly")] = talkHourly;
      tk[F("from-hour")] = talkFrom;
      tk[F("to-hour")] = talkTo;
      tk[F("button-time")] = talkBtnTime;
      tk[F("button-date")] = talkBtnDate;
      tk[F("button-temp")] = talkBtnTemp;
      tk[F("button-battery")] = talkBtnBat;
      tk[F("alarm-say")] = talkAlarm;
      tk[F("lowbat-say")] = talkLowBat;
      tk[F("charge-say")] = talkChgSay;
      tk[F("lowbat-min")] = talkLowMin;
      tk[F("word-ms")] = talkWordMs;
      tk[F("word-minute")] = talkWordMin;
      tk[F("word-baje")] = talkWordBaje;
      tk[F("exact-minute")] = talkExact;
      tk[F("minute-numbers")] = talkNums;
      tk[F("quarter-hours")] = talkQuarter;
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root[FPSTR(_name)];
      bool ok = !top.isNull();
      if (!ok) { configRead = true; if (setupDone) { buildOrder(); initHw(); } return false; }

      ok &= getJsonValue(top[F("enabled")], enabled, true);

      JsonObject w = top[F("wiring")];
      ok &= getJsonValue(w[F("first-led")], firstLed, (uint16_t)0);
      ok &= getJsonValue(w[F("seg-preset")], segPreset, (uint8_t)1);
      copyStr(segCustom, sizeof(segCustom), w[F("seg-order")] | "FABGEDC");
      ok &= getJsonValue(w[F("leds-per-segment")], ledsPerSeg, (uint8_t)1);
      ok &= getJsonValue(w[F("leds-per-colon")], colonLeds, (uint8_t)2);
      ok &= getJsonValue(w[F("leds-per-weekday")], ledsPerDay, (uint8_t)1);
      ok &= getJsonValue(w[F("blank-extra")], blankExtra, true);

      JsonObject ap = top[F("am-pm-leds")];
      ok &= getJsonValue(ap[F("enabled")], ampmOn, false);
      ok &= getJsonValue(ap[F("leds-each")], ampmLeds, (uint8_t)1);
      copyStr(amHex, sizeof(amHex), ap[F("am-hex")] | "");
      copyStr(pmHex, sizeof(pmHex), ap[F("pm-hex")] | "");
      ok &= getJsonValue(ap[F("blink")], ampmBlink, false);
      if (ap.isNull()) ok = false;

      JsonObject d = top[F("display")];
      ok &= getJsonValue(d[F("hour-format")], hourFormat, (uint8_t)0);
      ok &= getJsonValue(d[F("leading-zero")], leadingZero, true);
      ok &= getJsonValue(d[F("colon-blink")], colonBlink, true);
      ok &= getJsonValue(d[F("weekday-mode")], weekdayMode, (uint8_t)1);
      ok &= getJsonValue(d[F("weekday-order")], weekdayOrder, (uint8_t)0);
      ok &= getJsonValue(d[F("startup-test")], startupTest, false);

      JsonObject dt = top[F("date")];
      ok &= getJsonValue(dt[F("show")], dateShow, false);
      ok &= getJsonValue(dt[F("order")], dateOrder, (uint8_t)0);
      ok &= getJsonValue(dt[F("time-sec")], secTime, (uint8_t)6);
      ok &= getJsonValue(dt[F("day-month-sec")], secDayMonth, (uint8_t)2);
      ok &= getJsonValue(dt[F("year-sec")], secYear, (uint8_t)2);

      JsonObject tp = top[F("temperature")];
      ok &= getJsonValue(tp[F("show")], tempShow, false);
      ok &= getJsonValue(tp[F("auto-color")], tempAutoColor, false);
      ok &= getJsonValue(tp[F("unit")], tempUnit, (uint8_t)0);
      ok &= getJsonValue(tp[F("temp-sec")], secTemp, (uint8_t)2);
      ok &= getJsonValue(tp[F("offset")], tempOffset, (int8_t)0);

      JsonObject c = top[F("colors")];
      ok &= getJsonValue(c[F("color-source")], colorSource, (uint8_t)0);
      copyStr(wdHex, sizeof(wdHex), c[F("weekday-hex")] | "");
      copyStr(otherHex, sizeof(otherHex), c[F("other-days-hex")] | "");
      copyStr(weekendHex, sizeof(weekendHex), c[F("weekend-hex")] | "");
      copyStr(colonHex, sizeof(colonHex), c[F("colon-hex")] | "");
      copyStr(dateHex, sizeof(dateHex), c[F("date-hex")] | "");
      copyStr(tempHex, sizeof(tempHex), c[F("temp-hex")] | "");
      if (c.isNull()) ok = false;

      JsonObject al = top[F("alarm")];
      ok &= getJsonValue(al[F("enabled")], alarmOn, false);
      ok &= getJsonValue(al[F("hour")], alarmHour, (uint8_t)7);
      ok &= getJsonValue(al[F("minute")], alarmMin, (uint8_t)0);
      ok &= getJsonValue(al[F("days")], alarmDays, (uint8_t)0);
      ok &= getJsonValue(al[F("ring-sec")], ringSec, (uint8_t)30);
      ok &= getJsonValue(al[F("snooze-min")], snoozeMin, (uint8_t)5);

      JsonObject ch = top[F("hourly-chime")];
      ok &= getJsonValue(ch[F("enabled")], chimeOn, false);
      ok &= getJsonValue(ch[F("start-hour")], chimeStart, (uint8_t)8);
      ok &= getJsonValue(ch[F("end-hour")], chimeEnd, (uint8_t)22);

      JsonObject bz = top[F("buzzer-button")];
      ok &= getJsonValue(bz[F("buzzer-pin")], buzzerPin, (int8_t)-1);
      ok &= getJsonValue(bz[F("buzzer-invert")], buzzerInvert, false);
      ok &= getJsonValue(bz[F("button-pin")], buttonPin, (int8_t)-1);

      JsonObject bn = top[F("boot-name")];
      ok &= getJsonValue(bn[F("enabled")], greetOn, true);
      ok &= getJsonValue(bn[F("seconds")], greetSec, (uint8_t)5);

      JsonObject lv = top[F("levels")];
      ok &= readLevel(lv[F("all")], lvAll);
      ok &= readLevel(lv[F("digits")], lvDigits);
      ok &= readLevel(lv[F("colon")], lvColon);
      ok &= readLevel(lv[F("today")], lvToday);
      ok &= readLevel(lv[F("other-days")], lvOther);
      ok &= readLevel(lv[F("am-pm")], lvAmPm);

      JsonObject n = top[F("night")];
      ok &= getJsonValue(n[F("enabled")], nightOn, false);
      ok &= getJsonValue(n[F("start-hour")], nightStart, (uint8_t)23);
      ok &= getJsonValue(n[F("end-hour")], nightEnd, (uint8_t)6);
      ok &= getJsonValue(n[F("action")], nightAction, (uint8_t)0);
      ok &= getJsonValue(n[F("dim-percent")], nightDim, (uint8_t)20);

      #ifndef ESP8266
      JsonObject b = top[F("brightness")];
      ok &= getJsonValue(b[F("auto")], autoBri, false);
      ok &= getJsonValue(b[F("ldr-pin")], ldrPin, (int8_t)C7_DEF_LDR);
      ok &= getJsonValue(b[F("ldr-invert")], ldrInvert, false);
      ok &= getJsonValue(b[F("min")], briMin, (uint8_t)20);
      ok &= getJsonValue(b[F("max")], briMax, (uint8_t)255);
      #endif

      JsonObject r = top[F("rtc")];
      ok &= getJsonValue(r[F("enabled")], rtcEnabled, true);
      ok &= getJsonValue(r[F("chip")], rtcChip, (uint8_t)0);
      ok &= getJsonValue(r[F("sda-pin")], rtcSda, (int8_t)C7_DEF_SDA);
      ok &= getJsonValue(r[F("scl-pin")], rtcScl, (int8_t)C7_DEF_SCL);
      ok &= getJsonValue(r[F("sync-hours")], rtcSyncHours, (uint8_t)6);
      ok &= getJsonValue(r[F("reread-on")], rereadOn, true);
      ok &= getJsonValue(r[F("reread-min")], rereadMin, (uint8_t)2);
      ok &= getJsonValue(r[F("battery-check")], battCheck, true);
      ok &= getJsonValue(r[F("warn-display")], warnShow, true);

      JsonObject t = top[F("test")];
      ok &= getJsonValue(t[F("mode")], testMode, (uint8_t)0);

      JsonObject bt = top[F("battery")];
      ok &= getJsonValue(bt[F("enabled")], batOn, true);
      ok &= getJsonValue(bt[F("adc-pin")], batPin, (int8_t)C7_DEF_BAT);
      ok &= getJsonValue(bt[F("multiplier")], batMult, (float)C7_DEF_BATMUL);
      ok &= getJsonValue(bt[F("calibration-v")], batCal, 0.0f);
      ok &= getJsonValue(bt[F("min-volt")], batMinV, 3.3f);
      ok &= getJsonValue(bt[F("max-volt")], batMaxV, 4.2f);
      ok &= getJsonValue(bt[F("read-sec")], batIntSec, (uint8_t)10);
      ok &= getJsonValue(bt[F("show-display")], batShow, true);
      ok &= getJsonValue(bt[F("show-as")], batShowAs, (uint8_t)0);
      ok &= getJsonValue(bt[F("show-sec")], secBat, (uint8_t)2);
      ok &= getJsonValue(bt[F("low-volt")], batLowV, 3.5f);
      ok &= getJsonValue(bt[F("cutoff-volt")], batCutV, 3.2f);
      ok &= getJsonValue(bt[F("auto-color")], batAutoCol, true);
      copyStr(batHex, sizeof(batHex), bt[F("digit-hex")] | "");
      ok &= getJsonValue(bt[F("bri-by-volt")], batBriOn, true);
      ok &= getJsonValue(bt[F("full-bri-volt")], batFullV, 3.9f);
      ok &= getJsonValue(bt[F("min-bri-pct")], batFloor, (uint8_t)25);
      ok &= getJsonValue(bt[F("curve")], curveOn, true);
      ok &= getJsonValue(bt[F("bar")], barOn, true);

      JsonObject rc = top[F("cutoff-recovery")];
      ok &= getJsonValue(rc[F("enabled")], recOn, true);
      ok &= getJsonValue(rc[F("recover-volt")], batRecV, 3.7f);
      if (rc.isNull()) ok = false;

      JsonObject cg = top[F("charging")];
      ok &= getJsonValue(cg[F("enabled")], chgOn, false);
      ok &= getJsonValue(cg[F("pin")], chgPin, (int8_t)C7_DEF_CHG);
      ok &= getJsonValue(cg[F("active-low")], chgLow, true);
      if (cg.isNull()) ok = false;

      JsonObject lb = top[F("low-beep")];
      ok &= getJsonValue(lb[F("enabled")], beepOn, false);
      ok &= getJsonValue(lb[F("repeat-min")], beepMin, (uint8_t)10);
      if (lb.isNull()) ok = false;

      JsonObject sl = top[F("sleep")];
      ok &= getJsonValue(sl[F("enabled")], sleepOn, false);
      ok &= getJsonValue(sl[F("awake-sec")], sleepAwake, (uint8_t)5);
      ok &= getJsonValue(sl[F("sleep-sec")], sleepOff, (uint8_t)25);
      if (sl.isNull()) ok = false;

      JsonObject js = top[F("json")];
      ok &= getJsonValue(js[F("enabled")], jsonOn, true);
      if (js.isNull()) ok = false;

      JsonObject tk = top[F("talking")];
      ok &= getJsonValue(tk[F("enabled")], talkOn, false);
      ok &= getJsonValue(tk[F("rx-pin")], talkRx, (int8_t)C7_DEF_TRX);
      ok &= getJsonValue(tk[F("tx-pin")], talkTx, (int8_t)C7_DEF_TTX);
      ok &= getJsonValue(tk[F("busy-pin")], talkBusyPin, (int8_t)-1);
      ok &= getJsonValue(tk[F("volume")], talkVol, (uint8_t)20);
      ok &= getJsonValue(tk[F("hourly")], talkHourly, true);
      ok &= getJsonValue(tk[F("from-hour")], talkFrom, (uint8_t)7);
      ok &= getJsonValue(tk[F("to-hour")], talkTo, (uint8_t)22);
      ok &= getJsonValue(tk[F("button-time")], talkBtnTime, true);
      ok &= getJsonValue(tk[F("button-date")], talkBtnDate, false);
      ok &= getJsonValue(tk[F("button-temp")], talkBtnTemp, false);
      ok &= getJsonValue(tk[F("button-battery")], talkBtnBat, false);
      ok &= getJsonValue(tk[F("alarm-say")], talkAlarm, true);
      ok &= getJsonValue(tk[F("lowbat-say")], talkLowBat, true);
      ok &= getJsonValue(tk[F("charge-say")], talkChgSay, true);
      ok &= getJsonValue(tk[F("lowbat-min")], talkLowMin, (uint8_t)15);
      ok &= getJsonValue(tk[F("word-ms")], talkWordMs, (uint16_t)1200);
      ok &= getJsonValue(tk[F("word-minute")], talkWordMin, true);
      ok &= getJsonValue(tk[F("word-baje")], talkWordBaje, true);
      ok &= getJsonValue(tk[F("exact-minute")], talkExact, false);
      ok &= getJsonValue(tk[F("minute-numbers")], talkNums, true);
      ok &= getJsonValue(tk[F("quarter-hours")], talkQuarter, false);
      if (tk.isNull()) ok = false;
      if (bt.isNull()) ok = false;

      // sanity limits
      mode = C7_FIXED_MODE;                                   // fixed by the firmware version (6 or 4 digits)
      hourFormat = hourFormat ? 1 : 0;
      ledsPerSeg = constrain(ledsPerSeg, 1, 10);
      colonLeds  = constrain(colonLeds, 1, 6);
      weekdayMode = weekdayMode > 2 ? 1 : weekdayMode;
      weekdayOrder = weekdayOrder > 3 ? 0 : weekdayOrder;
      colorSource = colorSource > 2 ? 0 : colorSource;
      ampmLeds = constrain(ampmLeds, 1, 6); rereadMin = constrain(rereadMin, 1, 60);
      nightAction = nightAction > 2 ? 0 : nightAction; greetSec = constrain(greetSec, 1, 15); nightDim = constrain(nightDim, 1, 100);
      ledsPerDay = constrain(ledsPerDay, 1, 5);
      segPreset  = segPreset > 3 ? 1 : segPreset;
      dateOrder  = dateOrder ? 1 : 0;
      secTime = constrain(secTime, 1, 60); secDayMonth = constrain(secDayMonth, 1, 30); secYear = constrain(secYear, 1, 30);
      secTemp = constrain(secTemp, 1, 30);
      tempUnit = tempUnit ? 1 : 0;
      tempOffset = constrain(tempOffset, -9, 9);
      alarmHour %= 24; alarmMin %= 60; alarmDays = alarmDays > 2 ? 0 : alarmDays;
      ringSec = constrain(ringSec, 5, 255); snoozeMin = constrain(snoozeMin, 0, 60);
      chimeStart %= 24; chimeEnd %= 24;
      nightStart %= 24; nightEnd %= 24;
      rtcChip = rtcChip ? 1 : 0;
      testMode = testMode > 3 ? 0 : testMode;
      batIntSec = constrain(batIntSec, 3, 255); secBat = constrain(secBat, 1, 30); batShowAs = batShowAs ? 1 : 0;
      batFloor = constrain(batFloor, 5, 100);
      if (batMaxV < batMinV + 0.5f) batMaxV = batMinV + 0.5f;
      if (batCutV >= batLowV) batCutV = batLowV - 0.1f;
      if (batRecV < batCutV + 0.2f) batRecV = batCutV + 0.2f;
      beepMin = constrain(beepMin, 1, 120); sleepAwake = constrain(sleepAwake, 1, 60); sleepOff = constrain(sleepOff, 5, 250);
      talkVol = constrain(talkVol, 0, 30); talkFrom %= 24; talkTo %= 24; talkLowMin = constrain(talkLowMin, 1, 120); talkWordMs = constrain(talkWordMs, 300, 4000);
      if (talkTx == talkRx) talkRx = -1;
      #ifdef ESP8266
      talkRx = -1;                                            // ESP8266: send only
      #endif

      configRead = true;
      lastSig = 0xFFFFFFFF;
      alarmKey = 0; snoozeAt = 0; ringing = false; alarmArmed = true; lastReread = millis();
      if (setupDone) { buildOrder(); initHw(); }
      return ok;
    }

    void appendConfigData() {
      // WLED 0.14.4 on ESP8266 has only 2048 bytes for the whole settings script: list boxes first, hints last.
      C7_OAPPEND("var U='Ashish Battery Clock:',dd,c7d=function(g,f,o){try{dd=addDropdown(U+g,f);for(var i=0;i<o.length;i++)addOption(dd,o[i],i)}catch(e){}},c7i=function(g,f,t){try{addInfo(U+g+':'+f,1,'<br><small>'+t+'</small>')}catch(e){}};");
      C7_OAPPEND("c7d('display','weekday-mode',['Off','Today only','All on, today differs']);");
      C7_OAPPEND("c7d('display','weekday-order',['Sun to Sat','Sat to Sun','Mon to Sun','Sun to Mon']);");
      C7_OAPPEND("c7d('battery','show-as',['Percent','Volts']);");
      C7_OAPPEND("c7d('colors','color-source',['From effect','Main colour','Effect, no black']);");
      C7_OAPPEND("c7d('display','hour-format',['24 hour','12 hour']);");
      C7_OAPPEND("c7d('wiring','seg-preset',['Custom','F-A-B-G-E-D-C (default)','A-B-C-D-E-F-G','F-A-B-C-D-E-G']);");
      C7_OAPPEND("c7d('temperature','unit',['C','F']);");
      C7_OAPPEND("c7d('date','order',['Day-Month','Month-Day']);");
      C7_OAPPEND("c7d('alarm','days',['Every day','Mon-Fri','Sat-Sun']);");
      C7_OAPPEND("c7d('night','action',['Off','Dim','Keep on']);");
      C7_OAPPEND("c7d('rtc','chip',['DS1307','DS3231']);");
      C7_OAPPEND("c7d('test','mode',['Off','All ON','Seg walk','Digit count']);");
      C7_OAPPEND("c7i('battery','adc-pin','ESP32: 32-39');");
      C7_OAPPEND("c7i('battery','multiplier','Divider ratio');");
      C7_OAPPEND("c7i('battery','calibration-v','Volts +/-');");
      C7_OAPPEND("c7i('battery','min-volt','= 0 %');");
      C7_OAPPEND("c7i('battery','max-volt','= 100 %');");
      C7_OAPPEND("c7i('battery','low-volt','Low warning');");
      C7_OAPPEND("c7i('battery','cutoff-volt','Display OFF');");
      C7_OAPPEND("c7i('battery','full-bri-volt','Full bright above');");
      C7_OAPPEND("c7i('charging','pin','TP4056 CHRG. D5=14');");
      C7_OAPPEND("c7i('cutoff-recovery','recover-volt','Back ON above');");
      #ifndef ESP8266
      C7_OAPPEND("c7i('battery','min-bri-pct','At cut-off');");
      C7_OAPPEND("c7i('talking','rx-pin','DFPlayer TX');");
      C7_OAPPEND("c7i('talking','tx-pin','DFPlayer RX, 1k');");
      C7_OAPPEND("c7i('wiring','seg-order','If preset = Custom');");
      C7_OAPPEND("c7i('wiring','leds-per-colon','LEDs in ONE colon');");
      C7_OAPPEND("c7i('buzzer-button','buzzer-pin','-1 = none');");
      C7_OAPPEND("c7i('buzzer-button','button-pin','-1 = none');");
      C7_OAPPEND("c7i('night','dim-percent','If Action = Dim');");
      C7_OAPPEND("c7i('colors','weekday-hex','RRGGBB');");
      #endif
    }

    uint16_t getId() { return USERMOD_ID_UNSPECIFIED; }
};

const char UsermodAshishClock7SegBat::_name[] PROGMEM = "Ashish Battery Clock";
constexpr uint8_t UsermodAshishClock7SegBat::PAT[22];
