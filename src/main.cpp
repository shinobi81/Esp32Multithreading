#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define BUTTON_PIN 33    // GPIO pin for button
#define QUEUE_SIZE 10    // Size of the button press queue

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int buttonLastState = HIGH; // previous state of the button
int buttonCurrentState;     // current state of the button

QueueHandle_t buttonPressQueue = NULL; // Queue to hold button press events

void readButtonPresses(void * pvParameters)
{
  int pressCount = 0;
  for (;;)
  {
    buttonCurrentState = digitalRead(BUTTON_PIN); // Read the current state of the button

    if (buttonCurrentState != buttonLastState) // Check for button state change
    {
      if (buttonCurrentState == HIGH) // If the button is pressed
      {
        pressCount++;
        xQueueSend(buttonPressQueue, &pressCount, portMAX_DELAY); // Send the press count to the queue
      }
      vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce delay
    }
    buttonLastState = buttonCurrentState;
  }
}

void displayPressCount(void * pvParameters)
{
  for (;;)
  {
    int pressCount;
    if (xQueueReceive(buttonPressQueue, &pressCount, portMAX_DELAY))
    {
      display.clearDisplay();
      display.setCursor(2, 2);
      display.print(F("Button Pressed: "));
      display.println(pressCount);
      display.display();
      // add 2 second delay to allow user to see the update
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
  }
}

void setup()
{
  Serial.begin(115200);

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

  // Create the button press queue
  buttonPressQueue = xQueueCreate(QUEUE_SIZE, sizeof(int));
  if (buttonPressQueue == NULL)
  {
    Serial.println(F("Failed to create button press queue"));
    for (;;)
      ;
  }

  xTaskCreatePinnedToCore(
      readButtonPresses,   // Task function
      "ReadButtonPresses", // Name of the task
      2048,                // Stack size
      NULL,                // Parameter to pass
      1,                   // Task priority
      NULL,                // Task handle
      1);                  // Run on core 1

  xTaskCreatePinnedToCore(
      displayPressCount,   // Task function
      "DisplayPressCount", // Name of the task
      2048,                // Stack size
      NULL,                // Parameter to pass
      1,                   // Task priority
      NULL,                // Task handle
      1);                  // Run on core 1
}

void loop()
{
}
