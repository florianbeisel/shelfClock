/* 
    ShelfClock

    Based on https://github.com/DIY-Machines/DigitalClockSmartShelving
*/

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#endif

#include <DS3231_Simple.h>
DS3231_Simple Clock;
DateTime MyDateAndTime;

#include <Tasker.h>
Tasker timer;

#include "digits.ino"

// Which pin on the Arduino is connected to the NeoPixels?
#define LEDCLOCK_PIN 6
#define LEDDOWNLIGHT_PIN 5

// How many NeoPixels are attached to the Arduino?
#define LEDCLOCK_COUNT 216
#define LEDDOWNLIGHT_COUNT 12

const uint8_t brightnessButtonPin = 3;

//
enum class LightState : uint8_t
{
    automatic = 2,
    manual = 1,
    off = 0
};

enum class BrightnessLevel : uint8_t
{
    high = 255,
    middle = 178,
    low = 100
};

//(red * 65536) + (green * 256) + blue ->for 32-bit merged colour value so 16777215 equals white
int clockMinuteColour = 51200;   //1677
int clockHourColour = 140000000; //7712

LightState clockLightState = LightState::automatic;

volatile bool ISR_BrightnessButtonPressed = false;

int lightSensorValue;
uint8_t clockFaceBrightness = 0;

// Declare our NeoPixel objects:
Adafruit_NeoPixel stripClock(LEDCLOCK_COUNT, LEDCLOCK_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel stripDownlighter(LEDDOWNLIGHT_COUNT, LEDDOWNLIGHT_PIN, NEO_GRB + NEO_KHZ800);

//Smoothing of the readings from the light sensor so it is not too twitchy
const int numReadings = 12;

int readings[numReadings]; // the readings from the analog input
int readIndex = 0;         // the index of the current reading
long total = 0;            // the running total
long average = 0;          // the average

void setup()
{

    Serial.begin(9600);

    Clock.begin();

    stripClock.begin();
    stripClock.show();
    stripClock.setBrightness(100);

    stripDownlighter.begin();
    stripDownlighter.show();
    stripDownlighter.setBrightness(50);

    //smoothing
    // initialize all the readings to 0:
    for (int thisReading = 0; thisReading < numReadings; thisReading++)
    {
        readings[thisReading] = 0;
    }

    // Update the Clockface every Second
    timer.setInterval(updateTime, 1000);

    // Every 50ms update the Brightness Sensor and adjust averaged brightness
    timer.setInterval(updateBrightness, 50, readIndex);

    pinMode(brightnessButtonPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(brightnessButtonPin), ISR_brightness_button, CHANGE);
}

void loop()
{
    timer.loop();

    if (ISR_BrightnessButtonPressed == true)
    {
        changeManualBrightness();
        // Clear the ISR Button Flag
        ISR_BrightnessButtonPressed = false;
    }
}

void ISR_brightness_button()
{
    ISR_BrightnessButtonPressed = true;
}

void changeManualBrightness()
{
    if (clockLightState == LightState::automatic)
    {
        clockLightState == LightState::manual;
        clockFaceBrightness = (uint8_t)BrightnessLevel::high;
    }
    else if (clockLightState == LightState::manual)
    {
        switch (clockFaceBrightness)
        {
        case (uint8_t)BrightnessLevel::high:
            clockFaceBrightness = (uint8_t)BrightnessLevel::middle;
            break;

        case (uint8_t)BrightnessLevel::middle:
            clockFaceBrightness = (uint8_t)BrightnessLevel::low;
            break;

        case (uint8_t)BrightnessLevel::low:
            clockLightState = LightState::off;
            break;

        default:
            clockFaceBrightness = (uint8_t)BrightnessLevel::high;
            break;
        }
    }
    else
    {
        clockLightState = LightState::automatic;
        clockFaceBrightness = map(lightSensorValue, 50, 1000, 200, 1);
    }

    stripClock.setBrightness(clockFaceBrightness); // Set brightness value of the LEDs
    stripClock.show();
}

void updateBrightness(int currentIndex)
{
    //Record a reading from the light sensor and add it to the array
    readings[currentIndex] = analogRead(A0); //get an average light level from previouse set of samples
    Serial.print("Light sensor value added to array = ");
    Serial.println(readings[currentIndex]);
    readIndex = currentIndex + 1; // advance to the next position in the array:

    // if we're at the end of the array move the index back around...
    if (readIndex >= numReadings)
    {
        // ...wrap around to the beginning:
        readIndex = 0;
    }

    //now work out the sum of all the values in the array
    int sumBrightness = 0;
    for (int i = 0; i < numReadings; i++)
    {
        sumBrightness += readings[i];
    }
    Serial.print("Sum of the brightness array = ");
    Serial.println(sumBrightness);

    // and calculate the average:
    lightSensorValue = sumBrightness / numReadings;
    Serial.print("Average light sensor value = ");
    Serial.println(lightSensorValue);

    // Check if automatic brightness is enabled, only then update the brightness
    if (clockLightState == LightState::automatic)
    {
        // update clockFaceBrightness with new mapping value
        clockFaceBrightness = map(lightSensorValue, 50, 1000, 200, 1);

        stripClock.setBrightness(clockFaceBrightness); // Set brightness value of the LEDs
        stripClock.show();

        Serial.print("Mapped brightness value = ");
        Serial.println(clockFaceBrightness);
    }
}

void enableDownlight()
{
    //(red * 65536) + (green * 256) + blue ->for 32-bit merged colour value so 16777215 equals white
    stripDownlighter.fill(16777215, 0, LEDDOWNLIGHT_COUNT);
    stripDownlighter.show();
}

void disableDownlight()
{
    stripDownlighter.clear();
    stripDownlighter.show();
}

void updateTime()
{
    // Ask the clock for the data.
    MyDateAndTime = Clock.read();

    // And use it
    Serial.println("");
    Serial.print("Time is: ");
    Serial.print(MyDateAndTime.Hour);
    Serial.print(":");
    Serial.print(MyDateAndTime.Minute);
    Serial.print(":");
    Serial.println(MyDateAndTime.Second);
    Serial.print("Date is: 20");
    Serial.print(MyDateAndTime.Year);
    Serial.print(":");
    Serial.print(MyDateAndTime.Month);
    Serial.print(":");
    Serial.println(MyDateAndTime.Day);
    stripClock.clear(); //clear the clock face

    int firstMinuteDigit = MyDateAndTime.Minute % 10; //work out the value of the first digit and then display it
    displayNumber(firstMinuteDigit, 0, clockMinuteColour);

    int secondMinuteDigit = floor(MyDateAndTime.Minute / 10); //work out the value for the second digit and then display it
    displayNumber(secondMinuteDigit, 63, clockMinuteColour);

    int firstHourDigit = MyDateAndTime.Hour; //work out the value for the third digit and then display it
    if (firstHourDigit > 12)
    {
        firstHourDigit = firstHourDigit - 12;
    }

    // Comment out the following three lines if you want midnight to be shown as 12:00 instead of 0:00
    //  if (firstHourDigit == 0){
    //    firstHourDigit = 12;
    //  }

    firstHourDigit = firstHourDigit % 10;
    displayNumber(firstHourDigit, 126, clockHourColour);

    int secondHourDigit = MyDateAndTime.Hour; //work out the value for the fourth digit and then display it

    // Comment out the following three lines if you want midnight to be shwon as 12:00 instead of 0:00
    //  if (secondHourDigit == 0){
    //    secondHourDigit = 12;
    //  }

    if (secondHourDigit > 12)
    {
        secondHourDigit = secondHourDigit - 12;
    }
    if (secondHourDigit > 9)
    {
        stripClock.fill(clockHourColour, 189, 18);
    }
}

void displayNumber(int digitToDisplay, int offsetBy, int colourToUse)
{
    switch (digitToDisplay)
    {
    case 0:
        digitZero(offsetBy, colourToUse);
        break;
    case 1:
        digitOne(offsetBy, colourToUse);
        break;
    case 2:
        digitTwo(offsetBy, colourToUse);
        break;
    case 3:
        digitThree(offsetBy, colourToUse);
        break;
    case 4:
        digitFour(offsetBy, colourToUse);
        break;
    case 5:
        digitFive(offsetBy, colourToUse);
        break;
    case 6:
        digitSix(offsetBy, colourToUse);
        break;
    case 7:
        digitSeven(offsetBy, colourToUse);
        break;
    case 8:
        digitEight(offsetBy, colourToUse);
        break;
    case 9:
        digitNine(offsetBy, colourToUse);
        break;
    default:
        break;
    }
}
