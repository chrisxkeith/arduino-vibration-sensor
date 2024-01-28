// Please credit chris.keith@gmail.com .

#define USE_OLED false
#if USE_OLED
#include <U8g2lib.h>
#endif
#include <SD.h>

class Utils {
  public:
    const static bool DO_SERIAL = true;
    static void publish(String s);
    static String toString(bool b) {
      if (b) {
        return "true";
      }
      return "false";
    }
    static int setInt(String command, int& i, int lower, int upper) {
      int tempMin = command.toInt();
      if (tempMin >= lower && tempMin <= upper) {
          i = tempMin;
          return 1;
      }
      return -1;
    }
};

class JSonizer {
  public:
    static void addFirstSetting(String& json, String key, String val);
    static void addSetting(String& json, String key, String val);
    static String toString(bool b);
};

void JSonizer::addFirstSetting(String& json, String key, String val) {
    json.concat("\"");
    json.concat(key);
    json.concat("\":\"");
    json.concat(val);
    json.concat("\"");
}

void JSonizer::addSetting(String& json, String key, String val) {
    json.concat(",");
    addFirstSetting(json, key, val);
}

String JSonizer::toString(bool b) {
    if (b) {
        return "true";
    }
    return "false";
}

class SDWriter {
  private:
    File myFile;
    bool didTest = false;
  public:
    SDWriter() {
      Serial.println("Initializing SD card...");
      pinMode(10, OUTPUT);
      if (!SD.begin(10)) {
        Serial.println("SD card initialization failed!");
      } else {
        Serial.println("SD card initialization done.");
      }
    }
    void open(String fn) {
      myFile = SD.open(fn.c_str(), FILE_WRITE);
      if (!myFile) {
        String s("error opening: ");
        s.concat(fn);
        Serial.println(s);
      }
    }
    void close() {
      if (myFile) {
        myFile.close();
      }
    }
    void write(String msg) {
      if (myFile) {
        myFile.println(msg.c_str());
      }
    }
    void read(String fn) {
      // can't get read() to work, but don't need it.
      if (myFile) {
        while (myFile.available()) {
          Serial.println(myFile.read());
        }
      }
    }
};

#if USE_OLED
class ScreenBuffer {
  private:
    const int         MAX_LENGTH = 12;
    int               next_line = 0;
  public:
    const static int  MAX_LINES = 4;
    String            lines_of_text[MAX_LINES];

    void addText(String msg) {
      while (msg.length() > 0) {
        if (next_line == MAX_LINES - 1) {
          for (int i = 1; i < MAX_LINES; i++) {
            lines_of_text[i - 1] = lines_of_text[i];
          }
        } else {
          next_line++;
        }
        lines_of_text[next_line] = msg.substring(0, MAX_LENGTH);
        msg.remove(0, MAX_LENGTH);
      }
    }
};

U8G2_SSD1327_EA_W128128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

class OLEDWrapper {
  private:
      const int     START_BASELINE = 50;
      int           baseLine = START_BASELINE;
      ScreenBuffer  screenBuffer;
      const int     SMALL_FONT_HEIGHT = 20;
  public:
    void u8g2_prepare(void) {
      u8g2.setFont(u8g2_font_fur49_tn);
      u8g2.setFontRefHeightExtendedText();
      u8g2.setDrawColor(1);
      u8g2.setFontDirection(0);
    }
    
    void drawEdge() {
      u8g2.drawLine(0, 0, 0, 95);
      u8g2.drawLine(0, 95, 127, 95);  
      u8g2.drawLine(127, 95, 127, 0);  
      u8g2.drawLine(127, 0, 0, 0);  
    }

    void drawString(String s) {
      screenBuffer.addText(s);
      u8g2.firstPage();
      do {
          u8g2_prepare();
          u8g2.setFont(u8g2_font_fur11_tf);
          int baseLine = 0;
          for (int i = 0; i < screenBuffer.MAX_LINES; i++) {
            baseLine += SMALL_FONT_HEIGHT;
            u8g2.drawUTF8(6, baseLine, screenBuffer.lines_of_text[i].c_str());

          }
      } while( u8g2.nextPage() );
    }

    void drawInt(int val) {
      u8g2.firstPage();
      do {
          u8g2_prepare();
          u8g2.drawUTF8(2, this->baseLine, String(val).c_str());
          u8g2.setFont(u8g2_font_fur11_tf);
          u8g2.drawUTF8(6, this->baseLine + SMALL_FONT_HEIGHT, "Fahrenheit");
      } while( u8g2.nextPage() );
    }

    void clear() {
      u8g2.firstPage();
      do {
      } while( u8g2.nextPage() );      
    }

    void shiftDisplay(int shiftAmount) {
      this->baseLine += shiftAmount;
      if (this->baseLine > 70) {
        this->baseLine = START_BASELINE;
      }
    }

    void setup_OLED() {
      pinMode(10, OUTPUT);
      pinMode(9, OUTPUT);
      digitalWrite(10, 0);
      digitalWrite(9, 0);
      u8g2.begin();
      u8g2.setBusClock(400000);
    }
};
#else
class OLEDWrapper {
  public:
    void u8g2_prepare(void) {
    }    
    void drawEdge() {
    }
    void drawString(String s) {
    }
    void drawInt(int val) {
    }
    void clear() {
    }
    void shiftDisplay(int shiftAmount) {
    }
    void setup_OLED() {
    }
};
#endif

OLEDWrapper oledWrapper;

void Utils::publish(String s) {
  if (DO_SERIAL) {
    Serial.println(s);
  }
  oledWrapper.drawString(s);
}

class PublishRateHandler {
  public:
    int publishRateInSeconds = 2;

    int setPublishRate(String cmd) {
      return Utils::setInt(cmd, publishRateInSeconds, 1, 60);
    }
};
PublishRateHandler publishRateHandler;

class SensorHandler {
  private:
    const int MS_FOR_SAMPLE = 1000;
    float max_weighted = __FLT_MIN__;
    float max_unweighted = __FLT_MIN__;
    int total_reads = 0;

    const int PIEZO_PIN_UNWEIGHTED = A0;
    const int PIEZO_PIN_WEIGHTED = A1;

    float getVoltage(int pin) {
        int piezoADC = analogRead(pin);
        return piezoADC / 1023.0 * 5.0;
    }
    void getVoltages() {
      max_weighted = __FLT_MIN__;
      max_unweighted = __FLT_MIN__;
      total_reads = 0;
      int then = millis();
      while (millis() - then < MS_FOR_SAMPLE) {
        float piezoV = getVoltage(PIEZO_PIN_WEIGHTED);
        if (piezoV > max_weighted) {
          max_weighted = piezoV;
        }
        piezoV = getVoltage(PIEZO_PIN_UNWEIGHTED);
        if (piezoV > max_unweighted) {
          max_unweighted = piezoV;
        }
        total_reads++;
      }
    }
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_UNWEIGHTED, INPUT);
      pinMode(PIEZO_PIN_WEIGHTED, INPUT);
    }
    bool sample_and_publish() {
      getVoltages();
      if (total_reads == 0) {
        return false;
      }
      String csv(max_weighted);
      csv.concat(",");
      csv.concat(max_unweighted);
      csv.concat(",");
      csv.concat(total_reads);
      Utils::publish(csv);
      int theDelay = publishRateHandler.publishRateInSeconds - (MS_FOR_SAMPLE / 1000);
      delay(theDelay * 1000);
      return true;
    }
    void getAverageReadTime() {
      int t = 0;
      const int NUM_READS = 100;
      for (int i = 0; i < NUM_READS; i++) {
        int now = millis();
        getVoltage(A0);
        t += (millis() - now);
      }
      String s("100 reads: ");
      s.concat(t);
      s.concat(" ms");
      Utils::publish(s);
    }
};
SensorHandler sensorHandler;

const String githubRepo("https://github.com/chrisxkeith/arduino-vibration-sensor");

class App {
  private:
    int lastDisplay = 0;
    int lastShift = 0;
    SDWriter*  sdWriter;
    unsigned int  nPublishes = 0;

    void status() {
      Utils::publish(githubRepo);
    }

    void checkSerial() {
      if (Utils::DO_SERIAL) {
        int now = millis();
        while (Serial.available() == 0) {
          if (millis() - now > 500) {
            return;
          }
        }
        String teststr = Serial.readString();  //read until timeout
        teststr.trim();                        // remove any \r \n whitespace at the end of the String
        if (teststr == "?") {
          status();
        } else {
          String msg("Unknown command: ");
          msg.concat(teststr);
          Serial.println(msg);
        }
      }
    }
  public:
    void setup() {
      if (Utils::DO_SERIAL) {
        Serial.begin(57600);
        delay(1000);
      }
      Utils::publish("Started setup...");
      status();

      oledWrapper.setup_OLED();
      delay(1000);
      sdWriter = new SDWriter();
      Utils::publish("Finished setup...");
    }
    void loop() {
      const int DISPLAY_RATE_IN_MS = 2000;
      int thisMS = millis();
      if (thisMS - lastDisplay > DISPLAY_RATE_IN_MS) {
        if (!sensorHandler.sample_and_publish()) {
          String msg("Reads failed after ");
          msg.concat(nPublishes);
          msg.concat(" publishs.");
          Utils::publish(msg);
        } else {
          nPublishes++;
        }
        const int SHIFT_RATE = 1000 * 60 * 2; // Shift display every 2 minutes to avoid OLED burn-in.
        if (thisMS - lastShift > SHIFT_RATE) {
          oledWrapper.shiftDisplay(2);
          lastShift = thisMS;
        }
        lastDisplay = thisMS;
      }
      // checkSerial();
    }
};
App app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
