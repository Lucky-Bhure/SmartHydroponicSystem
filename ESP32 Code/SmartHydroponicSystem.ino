// libraries -
// firebase & wifi connection
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info and the RTDB payload printing info and other helper functions.
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Define Firebase Data object
FirebaseData fbdo, fbdo1;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Insert your network credentials
#define WIFI_SSID "Wi-Fi Name"
#define WIFI_PASSWORD "Wi-Fi Password"

// Insert Firebase project API Key
#define API_KEY "Firebase API Key"

// Insert Firebase RTDB URL*/
#define DATABASE_URL "Firebase RTDB URL"

// for dht
#include "DHT.h"
#define DHTPIN 4 // D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// for water sensor
#define FloatSensor 23        // D23
int buttonState = 1;

// for water temperature
#include <OneWire.h>
#include <DallasTemperature.h>
#define DS18B20PIN 5 // D5
OneWire oneWire(DS18B20PIN);
DallasTemperature sensor(&oneWire);

// for TDS sensor
#define TdsSensorPin 35        // D35
#define VREF 3.3               // analog reference voltage(Volt) of the ADC
#define SCOUNT 30              // sum of sample point

int analogBuffer[SCOUNT];      // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;         // current temperature for compensation

unsigned long lastPrintTime = 0;

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen)
{
    int bTab[iFilterLen];
    for (int i = 0; i < iFilterLen; i++)
        bTab[i] = bArray[i];
    int i, j, bTemp;
    for (j = 0; j < iFilterLen - 1; j++)
    {
        for (i = 0; i < iFilterLen - j - 1; i++)
        {
            if (bTab[i] > bTab[i + 1])
            {
                bTemp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = bTemp;
            }
        }
    }
    if (iFilterLen % 2 == 0)
    {
        bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
    }
    else
    {
        bTemp = bTab[iFilterLen / 2];
    }
    return bTemp;
}


// for ph sensor
#define potPin 34 // D34
float ph = 0;
float Value = 0;

float dhtsensor()
{
    float t = dht.readTemperature();
    if (isnan(t))
    {
        Serial.println("Failed to read from DHT sensor!");
    }
    else
    {
        Serial.print("Room Temperature: ");
        Serial.print(t);
        Serial.println("°C");

        if (t > 24) // 24
        {
//            digitalWrite(33, LOW);         // Turn on the FAN
        }
        if (t < 18) // 18
        {
//            digitalWrite(33, HIGH);        // Turn off the FAN
        }
    }
    return t;
}
int8_t floatswitch()
{
    buttonState = digitalRead(FloatSensor);
    if (buttonState == HIGH)
    {
        digitalWrite(27, HIGH);
        tdssensor();
        int value =1;
        return value;
    }
    else
    {
        digitalWrite(27, LOW);
        int value = 0;
        return value;
    }
}
float watertemp()
{
    sensor.requestTemperatures();
    float tempinC = sensor.getTempCByIndex(0);
    Serial.print("Water Temperature :");
    Serial.print(tempinC);
    Serial.println("ºC");
    temperature = tempinC;
    return temperature;
}
float tdssensor()
{
    static unsigned long analogSampleTimepoint = millis();
    if (millis() - analogSampleTimepoint > 10U)
    {
        analogSampleTimepoint = millis();
        analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
        analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;       // Circular buffer
    }

    unsigned long currentTime = millis();

    if (currentTime - lastPrintTime > 1000U)
    {
        int medianValue = getMedianNum(analogBuffer, SCOUNT);
        averageVoltage = medianValue * (float)VREF / 4096.0;
        float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
        float compensationVoltage = averageVoltage / compensationCoefficient;
        tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
        Serial.print("TDS Value:");
        Serial.print(tdsValue);
        Serial.println(" ppm");

        lastPrintTime = currentTime;
    }
    if ((tdsValue > 1073)) // 1073

    {
//        digitalWrite(25, LOW);
        Serial.println("Solenoid valve ON");
    }
    return tdsValue;
}
float phsensor()
{
    Value = analogRead(potPin);
    Serial.print("PH value:");
    float voltage = Value * (3.3 / 4095.0);
    ph = (2.6 * voltage);
    Serial.println(ph);
    return ph;
}

void setup()
{
    Serial.begin(9600);

      //to connect wifi & firebase
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)

    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

        /* Assign the api key (required) */
        config.api_key = API_KEY;

        /* Assign the RTDB URL (required) */
        config.database_url = DATABASE_URL;

        /* Sign up */
        if (Firebase.signUp(&config, &auth, "Account E-mail", "Account Password"))

    {
        Serial.println("ok");
        signupOK = true;
    }
    else
    {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }

  /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback;       // see addons/TokenHelper.h

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    dht.begin();                        // for dht to begin
    pinMode(FloatSensor, INPUT_PULLUP); // for float sensor
    sensor.begin();                     // for water sensor
    pinMode(TdsSensorPin, INPUT);       // for TDS sensor
    pinMode(potPin, INPUT);             //for ph sensor
    
    // Relay Pins:
    pinMode(27, OUTPUT);    // IN1 for Water Tank Valve
    pinMode(26, OUTPUT);    // IN2 for Submersible Pump
    pinMode(25, OUTPUT);    // IN3 for Solenoid Valve
    pinMode(33, OUTPUT);    // IN4 for Fan
    
}

void loop()
{
    float env_temperature = dhtsensor();    // DHT11 Data
    int float_level = floatswitch();        // Float Senser Data
    float water_temp = watertemp();         // DS18B20 Data
    float water_quality = tdssensor();      // TDS Data
    float ph_Value = phsensor();            // pH Sensor Data
    //delay(3000);

    if (Firebase.ready() && water_quality != 0 && (millis() - sendDataPrevMillis > 1 || sendDataPrevMillis == 0))
    {
        // since we want the data to be updated every second
        sendDataPrevMillis = millis();

        // Environment Temperature From DHT_11 to Firebase
        if (Firebase.RTDB.setFloat(&fbdo, "DHT_11/Temperature", env_temperature))
        {
            Serial.print("Temperature : ");
            Serial.println(env_temperature);
        }
        else
        {
            Serial.println(" | Failed to Read from the DHT11 Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // Water Temperature From DS18B20 to Firebase
        if (Firebase.RTDB.setFloat(&fbdo, "Water/Temperature", water_temp))
        {
            Serial.print(" | Water Temperature : ");
            Serial.print(water_temp);
        }
        else
        {
            Serial.println(" | Failed to Read from the DS18B20 Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // Water Temperature From Float Sensor to Firebase
        if (Firebase.RTDB.setFloat(&fbdo, "Float/Level", float_level))

        {
            Serial.print(" | Float Level : ");
            Serial.print(float_level);
        }
        else
        {
            Serial.println(" | Failed to Read from the Float Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // Water Temperature From pH Sensor to Firebase
        if (Firebase.RTDB.setFloat(&fbdo, "pH_Sensor/pH_Value", ph_Value))
        {
            Serial.print(" | pH Value : ");
            Serial.print(ph_Value);
        }
        else
        {
            Serial.println(" | Failed to Read from the pH Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // Water Temperature From TDS Sensor to Firebase
        if (Firebase.RTDB.setFloat(&fbdo, "TDS/Water_Quality", water_quality))
        {
            Serial.print(" | TDS : ");
            Serial.print(water_quality);
        }
        else
        {
            Serial.println(" | Failed to Read from the TDS Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // ON and OFF Fan with Firebase
        bool fan;
        if (Firebase.RTDB.getBool(&fbdo, "/fanStatus"))
        {
            fan = fbdo.boolData();
            if (fan == false)
            {
                digitalWrite(33, HIGH);
                Serial.print("| Fan is OFF |");
            }
            else
            {
                digitalWrite(33, LOW);
                Serial.print(" | Fan is ON |");
            }
        }
        else
        {
            Serial.println(" | Failed to Read from the pH Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // ON and OFF Submersible Pump with Firebase
        bool pump;
        if (Firebase.RTDB.getBool(&fbdo, "/pumpStatus"))
        {
            pump = fbdo.boolData();
            if (pump == false)
            {
                digitalWrite(26, HIGH);
                Serial.print(" | Sumbersible Pump is OFF |");
            }
            else
            {
                digitalWrite(26, LOW);
                Serial.print(" | Sumbersible Pump is ON |");
            }
        }
        else
        {
            Serial.println(" | Failed to Read from the pH Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

        // ON and OFF Solenoid Valve with Firebase
        bool solenoid;
        if (Firebase.RTDB.getBool(&fbdo, "/solenoidStatus"))
        {
            solenoid = fbdo.boolData();
            if (solenoid == false)
            {
                digitalWrite(25, HIGH);
                Serial.print(" | Solenoid Valve is OFF |");
            }
            else
            {
                digitalWrite(25, LOW);
                Serial.print(" | Solenoid Valve is ON |");
            }
        }
        else
        {
            Serial.println(" | Failed to Read from the pH Sensor |");
            Serial.println("REASON: " + fbdo.errorReason());
        }

      }
}