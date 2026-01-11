#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define BUTTON_PIN 33    // GPIO pin for button
#define DHT_QUEUE_SIZE 2 // Size of the DHT data queue
#define DHTPIN 14        // GPIO pin for DHT22 sensor
#define DHTTYPE DHT22    // DHT 22 (AM2302), AM2321

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const unsigned long displayTaskInterval = 2000; // Interval between display updates in milliseconds
TaskHandle_t displayTaskHandle = NULL;

volatile bool turnOff = false;
volatile bool buttonPressed = false; // Flag to indicate button was pressed
const unsigned long DEBOUNCE_DELAY = 50; // in milliseconds
volatile unsigned long lastPressedTime = 0;

DHT dht(DHTPIN, DHTTYPE);
float temperature = 0.0;
float humidity = 0.0;
const long dhtReadingInterval = 5000; // Interval between DHT readings in milliseconds
TaskHandle_t dhtTaskHandle = NULL;
QueueHandle_t dhtDataQueue = NULL; // Queue to hold DHT sensor data

// Minimal ISR - just set a flag
void ARDUINO_ISR_ATTR handleButtonPress()
{
  unsigned long currentTime = millis();
  if (currentTime - lastPressedTime > DEBOUNCE_DELAY)
  {
    buttonPressed = true;
    lastPressedTime = currentTime;
  }
}

// Handle button press work outside ISR
void handleButtonAction()
{
  if (turnOff)
  {
    Serial.println(F("Suspending tasks and turning off display."));
    vTaskSuspend(dhtTaskHandle);
    vTaskSuspend(displayTaskHandle);
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  else
  {
    Serial.println(F("Resuming tasks and turning on display."));
    display.ssd1306_command(SSD1306_DISPLAYON);
    vTaskResume(dhtTaskHandle);
    vTaskResume(displayTaskHandle);
  }
  turnOff = !turnOff;
}

void displayTemperatureAndHumidity(void *pvParameters)
{
  for (;;)
  {
    float dhtData[2];
    if (xQueueReceive(dhtDataQueue, &dhtData, portMAX_DELAY) == pdTRUE) // Wait indefinitely for data
    {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print(F("Temp: "));
      display.print(dhtData[0]);
      display.println(F(" C"));
      display.print(F("Humidity: "));
      display.print(dhtData[1]);
      display.println(F(" %"));
      display.display();
    }
    vTaskDelay(displayTaskInterval / portTICK_PERIOD_MS);
  }
}

void readDHTSensor(void *pvParameters)
{
  for (;;)
  {
    float newHumidity = dht.readHumidity();
    float newTemperature = dht.readTemperature();

    if (!isnan(newHumidity) && !isnan(newTemperature))
    {
      humidity = newHumidity;
      temperature = newTemperature;
      float dhtData[2] = {temperature, humidity};
      xQueueSend(dhtDataQueue, &dhtData, portMAX_DELAY); // Send data to the queue and wait indefinitely if the queue is full
    }
    else
    {
      Serial.println(F("Failed to read from DHT sensor!"));
    }

    vTaskDelay(dhtReadingInterval / portTICK_PERIOD_MS);
  }
}

void setup()
{
  Serial.begin(115200);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  display.setTextSize(1);              // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(2, 2);             // Start at top-left corner
  display.println(F("Hello, world!"));
  display.drawRect(0, 0, 80, 12, SSD1306_WHITE);
  display.display();

  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure button pin with internal pull-up
  attachInterrupt(BUTTON_PIN, handleButtonPress, FALLING);

  // Create the DHT data queue
  dhtDataQueue = xQueueCreate(DHT_QUEUE_SIZE, sizeof(float) * 2);
  if (dhtDataQueue == NULL)
  {
    Serial.println(F("Failed to create DHT data queue"));
    for (;;)
      ;
  }

  xTaskCreatePinnedToCore(
      displayTemperatureAndHumidity,   // Task function
      "DisplayTemperatureAndHumidity", // Name of the task
      2048,                            // Stack size
      NULL,                            // Parameter to pass
      1,                               // Task priority
      &displayTaskHandle,              // Task handle
      1);                              // Run on core 1

  xTaskCreatePinnedToCore(
      readDHTSensor,          // Task function
      "ReadDHTSensor",       // Name of the task
      2048,                   // Stack size
      NULL,                   // Parameter to pass
      1,                      // Task priority
      &dhtTaskHandle,         // Task handle
      1);                     // Run on core 1
}

void loop()
{
  // Check if button was pressed and handle it in main loop
  if (buttonPressed)
  {
    buttonPressed = false;
    handleButtonAction();
  }
  delay(10); // Small delay to prevent loop from spinning too fast
}
