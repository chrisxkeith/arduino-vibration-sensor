// Please credit chris.keith@gmail.com .

#include <U8g2lib.h>

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

U8G2_SSD1327_EA_W128128_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

class OLEDWrapper {
  private:
      const int START_BASELINE = 50;
      int   baseLine = START_BASELINE;
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
      u8g2.firstPage();
      do {
          u8g2_prepare();
          u8g2.setFont(u8g2_font_fur11_tf);
          u8g2.drawUTF8(6, this->baseLine, s.c_str());
      } while( u8g2.nextPage() );
    }

    void drawInt(int val) {
      u8g2.firstPage();
      do {
          u8g2_prepare();
          u8g2.drawUTF8(2, this->baseLine, String(val).c_str());
          u8g2.setFont(u8g2_font_fur11_tf);
          u8g2.drawUTF8(6, this->baseLine + 20, "Fahrenheit");
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
OLEDWrapper oledWrapper;

void Utils::publish(String s) {
  if (DO_SERIAL) {
    char buf[100];
    int totalSeconds = millis() / 1000;
    int secs = totalSeconds % 60;
    int minutes = (totalSeconds / 60) % 60;
    int hours = (totalSeconds / 60) / 60;
    sprintf(buf, "%02u:%02u:%02u", hours, minutes, secs);
    String s1(buf);
    s1.concat(" ");
    s1.concat(s);
    Serial.println(s1);
  }
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
    int seconds_for_sample = 1;

    const int PIEZO_PIN_UNWEIGHTED = A0;
    const int PIEZO_PIN_WEIGHTED = A1;
    const int WAIT_BETWEEN_READS_MS = 25;
    const int NUM_SAMPLES = (seconds_for_sample * 1000) / 25;

    float getVoltage(int pin) {
      float total_piezo_0 = 0.0;
      for (int i = 0; i < NUM_SAMPLES; i++) {
        int piezoADC = analogRead(pin);
        float piezoV = piezoADC / 1023.0 * 5.0;
        total_piezo_0 += piezoV;
        delay(WAIT_BETWEEN_READS_MS);
      }
      return total_piezo_0 / NUM_SAMPLES;
    }
    void  sample_and_publish_(int pin, String theType) {
      float v = getVoltage(pin);
      String val1(v);
      String event("Voltage ");
      event.concat(theType);
      event.concat(" sensor");
      String s(event);
      s.concat(": ");
      s.concat(v);
      Utils::publish(s);
      oledWrapper.drawString(s);
      int theDelay = publishRateHandler.publishRateInSeconds - seconds_for_sample;
      delay(theDelay * 1000);
    }
  public:
    SensorHandler() {
      pinMode(PIEZO_PIN_UNWEIGHTED, INPUT);
      pinMode(PIEZO_PIN_WEIGHTED, INPUT);
    }
    int sample_and_publish() {
      sample_and_publish_(PIEZO_PIN_UNWEIGHTED, "unweighted");
      sample_and_publish_(PIEZO_PIN_WEIGHTED, "weighted");
      return 1;
    }
};
SensorHandler sensorHandler;

const String githubRepo("https://github.com/chrisxkeith/arduino-vibration-sensor");

class App {
  private:
    int lastDisplay = 0;
    int lastShift = 0;

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
      Utils::publish("Finished setup...");
      oledWrapper.drawString("Finished setup...");
    }
    void loop() {
      const int DISPLAY_RATE_IN_MS = 2000;
      int thisMS = millis();
      if (thisMS - lastDisplay > DISPLAY_RATE_IN_MS) {
        sensorHandler.sample_and_publish();
        const int SHIFT_RATE = 1000 * 60 * 2; // Shift display every 2 minutes to avoid OLED burn-in.
        if (thisMS - lastShift > SHIFT_RATE) {
          oledWrapper.shiftDisplay(2);
          lastShift = thisMS;
        }
        lastDisplay = thisMS;
      }
      checkSerial();
    }
};
App app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
