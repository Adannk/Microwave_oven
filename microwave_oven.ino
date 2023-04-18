//Include libraries
//#include <Arduino_FreeRTOS.h>
#include <FreeRTOS_SAMD21.h>

//Define pins used
#define DOOR_BUTTON 2
#define HEATER_LED 3
#define MICROWAVE_LED 4
#define BUTTON_1 8
#define BUTTON_2 9
#define BUTTON_3 10

// Global variables
volatile bool doorOpen = false;
volatile bool microwaveStarted = false;
uint8_t programChooser = 0;
uint8_t button1State = 0;
uint8_t button2State = 0;
uint8_t button3State = 0;
TaskHandle_t mainProgram_Handle, microWave_Handle;

//Struct thats used in creating the different heating programs
typedef struct 
{
  uint16_t programLengthSeconds;
  uint16_t heaterEffect;
} MicrowaveParams;

//The 3 diffrent programs
MicrowaveParams defrostMeat = {300, 800};
MicrowaveParams defrostVegetables  = {60, 400};
MicrowaveParams generalHeating = {30, 800};

void setup() 
{
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(BUTTON_3, INPUT);
  pinMode(HEATER_LED, OUTPUT);
  pinMode(MICROWAVE_LED, OUTPUT);
  pinMode(DOOR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DOOR_BUTTON), doorButton_ISR, FALLING);
  
  Serial.begin(9600);
  while (!Serial) {}

  xTaskCreate(controlLED, "Controls Lamp", 128, NULL, 1, NULL);
  xTaskCreate(mainProgram, "Main Task that controls input", 128, NULL, 1, &mainProgram_Handle);
}

//Function that controls heater and plate
void microWave(void *pvParameters)
{
  MicrowaveParams * localStruct = (MicrowaveParams *) pvParameters;
  uint16_t resetTime = localStruct->programLengthSeconds;
  uint16_t microWavePlate = 0;

  //Variables for clock timer
  uint16_t lenghtH = (localStruct->programLengthSeconds / 3600); 
  uint16_t lenghtM = (localStruct->programLengthSeconds - (3600 * lenghtH)) /60;
  uint16_t lenghtS = (localStruct->programLengthSeconds - (3600 * lenghtH) - (lenghtM * 60));

  //Effect used in wattage
  Serial.print("Effect set to: ");
  Serial.println(localStruct->heaterEffect);
  
  //Length of the heating program
  Serial.print("Length of program: ");
  Serial.print(lenghtH); 
  Serial.print(":");  
  Serial.print(lenghtM); 
  Serial.print(":");  
  Serial.println(lenghtS);
 
  Serial.println("Heater Started");    

  while (localStruct->programLengthSeconds >= 0) 
  {
    digitalWrite(HEATER_LED, 1);

    //Check if door is open
    if (doorOpen == true) 
    {
      Serial.println("Door Opend Heating Paused");
      digitalWrite(HEATER_LED, 0);
      vTaskSuspend(NULL);
    }

    //Calculate the time remaining    
    uint16_t lenghtH = (localStruct->programLengthSeconds / 3600); 
    uint16_t lenghtM = (localStruct->programLengthSeconds - (3600 * lenghtH)) /60;
    uint16_t lenghtS = (localStruct->programLengthSeconds - (3600 * lenghtH) - (lenghtM * 60));
    localStruct->programLengthSeconds -= 1;

    //Time remaining prints
        
    Serial.print(lenghtH); 
    Serial.print(":");  
    Serial.print(lenghtM); 
    Serial.print(":");
    if(lenghtS < 10)
    {
      Serial.print(0);
      Serial.println(lenghtS);
    }
    else
    {
      Serial.println(lenghtS);
    }
    
    
    //Plate calculation and print
    Serial.print("Microwave plate at: ");
    Serial.print(microWavePlate);
    Serial.println("d");
    microWavePlate += 30;
    if (microWavePlate == 360)
    {
      microWavePlate = 0;
    }

    //When timer hits 0 end task
    if(localStruct->programLengthSeconds == 0)
    {
      Serial.println("Heater stopped");
      digitalWrite(HEATER_LED, 0);
      microwaveStarted = false;
      localStruct->programLengthSeconds = resetTime;
      vTaskResume(mainProgram_Handle);
      vTaskDelete(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

//Function that controls lamp in microwave
void controlLED(void *pvParameters)
{
  while (true) 
  {
    if((doorOpen == true) || (microwaveStarted == true))
    {
      digitalWrite(MICROWAVE_LED, 1);
    }
    else 
    {
      digitalWrite(MICROWAVE_LED, 0);
    } 
    vTaskDelay(pdMS_TO_TICKS(10));     
  }
}

//Main function task that controls input
void mainProgram(void *pvParameters)
{
  uint8_t input;

  while (true) 
  {
    menuMessage();
    //Wait for user input from terminal or buttons
    while (Serial.available() == 0) 
    {
      button1State = digitalRead(BUTTON_1);
      button2State = digitalRead(BUTTON_2);
      button3State = digitalRead(BUTTON_3);
      if((button1State == 1) || (button2State == 1) || (button3State == 1))
      {
        break;
      }
    }
    input = Serial.parseInt();

    //Program 1    
    if((input == 1) || (button1State == 1))
    {
      if(doorOpen == true)
      {
        Serial.println("Close the microwave door before starting");
      }
      else 
      {
        microwaveStarted = true;
        Serial.println("Defrost Meat Selected");
        xTaskCreate(microWave, "Microwave function", 128, &defrostMeat, 1, &microWave_Handle);
        vTaskSuspend(NULL);
      }      
    }

    //Program 2
    else if((input == 2) || (button2State == 1))
    {
      if(doorOpen == true)
      {
        Serial.println("Close the microwave door before starting");
      }
      else 
      {
        microwaveStarted = true;
        Serial.println("Defrost Vegetables Selected");
        xTaskCreate(microWave, "Microwave function", 128, &defrostVegetables, 1, &microWave_Handle);
        vTaskSuspend(NULL);    
      }
    }

    //Program 3
    else if((input == 3) || (button3State == 1))
    {
      if(doorOpen == true)
      {
        Serial.println("Close the microwave door before starting");
      }
      else 
      {
        microwaveStarted = true;
        Serial.println("General Heating Selected");
        xTaskCreate(microWave, "Microwave function", 128, &generalHeating, 1, &microWave_Handle);
        vTaskSuspend(NULL); 
      }
    }

    //Invalid option
    else 
    {
      Serial.println("Please choose a valid selection");
    }

    //Clears the input
    input = Serial.read();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

//Menu function
void menuMessage()
{
  Serial.println("Welcome to the microwave!");
  Serial.println("Enter one of the following programs:");
  Serial.println("[1] Defrost meat 5 minutes");
  Serial.println("[2] Vegetables 1 minute");
  Serial.println("[3] General Heating 30 seconds");
}

//ISR that controls the door
void doorButton_ISR()
{
  microwaveStarted = !microwaveStarted;
  doorOpen = !doorOpen;
  xTaskResumeFromISR(microWave_Handle);
}

void loop() {}