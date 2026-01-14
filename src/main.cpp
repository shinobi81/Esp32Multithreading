#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <atomic>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDR 0x3C // I2C address for the OLED display
#define BUTTON_PIN 33    // GPIO pin for button
#define DHT_QUEUE_SIZE 2 // Size of the DHT data queue
#define DHTPIN 14        // GPIO pin for DHT22 sensor
#define DHTTYPE DHT22    // DHT 22 (AM2302), AM2321

// Struct to hold DHT sensor data
struct DHTData
{
  float temperature;
  float humidity;
};

// Create display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
const unsigned long displayTaskInterval = 2000;
TaskHandle_t displayTaskHandle = NULL;

// Flags and variables for button handling
bool turnOff = true;
std::atomic<bool> buttonPressed{false};
const unsigned long DEBOUNCE_DELAY = 50;
volatile unsigned long lastPressedTime = 0;

// DHT Sensor setup
DHT dht(DHTPIN, DHTTYPE);
const long dhtReadingInterval = 5000;
TaskHandle_t dhtTaskHandle = NULL;
QueueHandle_t dhtDataQueue = NULL;

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
    vTaskResume(displayTaskHandle);
    vTaskResume(dhtTaskHandle);
  }
  turnOff = !turnOff;
}

void displayTemperatureAndHumidity(void *pvParameters)
{
  for (;;)
  {
    DHTData dhtData;
    if (xQueueReceive(dhtDataQueue, &dhtData, portMAX_DELAY) == pdTRUE) // Wait indefinitely for data
    {
      display.clearDisplay();

      // display temperature
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("Temperature: ");
      display.setTextSize(2);
      display.setCursor(0, 10);
      display.print(dhtData.temperature);
      display.print(" ");
      display.setTextSize(1);
      display.cp437(true);
      display.write(167);
      display.setTextSize(2);
      display.print("C");

      // display humidity
      display.setTextSize(1);
      display.setCursor(0, 35);
      display.print("Humidity: ");
      display.setTextSize(2);
      display.setCursor(0, 45);
      display.print(dhtData.humidity);
      display.print(" %");

      display.display();
    }
    vTaskDelay(displayTaskInterval / portTICK_PERIOD_MS);
  }
}

void readDHTSensor(void *pvParameters)
{
  for (;;)
  {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (!isnan(humidity) && !isnan(temperature))
    {
      DHTData dhtData = {temperature, humidity};
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

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(500);
  display.clearDisplay();
  display.setTextSize(1);              // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);             // Start at top-left corner
  display.println(F("Starting system..."));
  display.display();
  delay(1000);

  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Configure button pin with internal pull-up
  attachInterrupt(BUTTON_PIN, handleButtonPress, FALLING);

  // Create the DHT data queue
  dhtDataQueue = xQueueCreate(DHT_QUEUE_SIZE, sizeof(DHTData));
  if (dhtDataQueue == NULL)
  {
    Serial.println(F("Failed to create DHT data queue"));
    for (;;)
      ;
  }

  xTaskCreatePinnedToCore(
      displayTemperatureAndHumidity,   // Task function
      "DisplayTemperatureAndHumidity", // Name of the task
      4096,                            // Stack size
      NULL,                            // Parameter to pass
      1,                               // Task priority
      &displayTaskHandle,              // Task handle
      1);                              // Run on core 1

  xTaskCreatePinnedToCore(
      readDHTSensor,   // Task function
      "ReadDHTSensor", // Name of the task
      4096,            // Stack size
      NULL,            // Parameter to pass
      2,               // Task priority
      &dhtTaskHandle,  // Task handle
      1);              // Run on core 1
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
