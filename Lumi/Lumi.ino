/****************/
/***** Lumi *****/
/****************/

/***** Includes *****/
#include "AdafruitIO_WiFi.h"
#include <ESP32Servo.h>
#include <Preferences.h>
#include "globals.h"
#include "bluetooth.h"
#include "leds.h"


/***** Definitions *****/
//uncomment to print to serial for debugging
#define DEBUGGING  

// uncomment to load secrets.h credentials into non-volatile memory
#include "secrets.h"
// #define MANUAL_CREDS


/***** Adafruit IO Derived *****/
class AIO_WiFi: public AdafruitIO_WiFi {
  
  using AdafruitIO_WiFi::AdafruitIO_WiFi;
  
  public:
    // setters for WiFi SSID and password
    void setSSID(const char* ssid) {
      _ssid = ssid;
    }
    void setPass(const char* pass) {
      _pass = pass;
    }

    // getters for WiFi SSID and password
    const char* getSSID() {
      return _ssid;
    }
    const char* getPass() {
      return _pass;
    }
};

// IO instance
AIO_WiFi *io;

// IO light feed
AdafruitIO_Feed *lightFeed;


/***** State Machines *****/
// main device states
typedef enum {
  INITIALIZING, REPAIR_WIFI, REPAIR_AIO, WAITING, SWITCHING 
}DeviceState;

// main light states
typedef enum {
  LIGHT_OFF, LIGHT_ON, LIGHT_UNKNOWN
}LightState;

// indicator light states
typedef enum {
  IL_OFF, IL_ON, IL_FLASHING, IL_LOADING
}IndicatorState;

// bluetooth states
typedef enum {
  BT_HIDDEN, BT_WAITING, BT_CONNECTED
}BTState;

// bluetooth repair steps
typedef enum {
  BT_IDLE, BT_SSID, BT_PASS, BT_USER, BT_KEY
}BTStep;

// battery states
typedef enum {
  BATTERY, AC
}PowerState;

static DeviceState deviceState;
static LightState lightState;
static IndicatorState indicatorState;
static BTState bluetoothState;
static BTStep bluetoothStep;
static PowerState powerState;


/***** Service Routines *****/
// debug button ISR: 
//    toggles the bluetooth server adv
void IRAM_ATTR buttonISR() {
  if(bluetoothState == BT_HIDDEN) {
    advertiseBT();
    bluetoothState = BT_WAITING;
  } else {
    hideBT();
    bluetoothState = BT_HIDDEN;
  }
}

// indicator timer ISR
void ARDUINO_ISR_ATTR indicatorTimerISR(){
  indicatorTimeout = true;
}

// servo timer ISR:
//    does not change deviceState if called during initialization process
void ARDUINO_ISR_ATTR servoTimerISR(){
  servoTimeout = true;
}

// touch ISR:
//    flips the direction of touch listener
void touchISR(){
  touchActive = !touchActive;
  touchInterruptSetThresholdDirection(!touchActive);
}

/***** Bluetooth Callbacks *****/
// handles bluetooth connected and disconnected events
class BTServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      bluetoothState = BT_CONNECTED;
      bluetoothStep = BT_IDLE;
      notify("Bluetooth connected.");
    }

    void onDisconnect(BLEServer* pServer) {
      bluetoothState = BT_HIDDEN;
      notify("Bluetooth disconnected.");

      // re-advertise if not done repairing
      if (deviceState == REPAIR_WIFI) { repairWifi(); }
      if (deviceState == REPAIR_AIO) { repairAIO(); }
    }
};

// handles user input on BLE UART
class BTCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        notify("Received value:");
        notify(rxValue);
        
        if (deviceState == REPAIR_WIFI){
          if (bluetoothStep == BT_IDLE) {
            bluetoothStep = BT_SSID;
            notify("Enter WiFi SSID:");

          } else if (bluetoothStep == BT_SSID) {
            SSID = rxValue;
            io->setSSID(SSID.c_str());
            bluetoothStep = BT_PASS;
            notify("WiFi SSID entered. Enter WiFi password (leave blank to use existing password):");

          } else if (bluetoothStep == BT_PASS) {
            PASS = rxValue;
            io->setPass(PASS.c_str());
            notify("WiFi password entered.");
            connectIO();
          }
        }
        
      } else if (deviceState == REPAIR_WIFI && bluetoothStep == BT_PASS) {
        connectIO();
      }
    }
};


/***** Setup *****/
// Device initalization routine
void setup() {
  // set state to initializing
  deviceState = INITIALIZING;

  // initialize serial output if in DEBUG mode
  #ifdef DEBUGGING 
    //Initialize serial and wait for port to open:
    Serial.begin(115200);

    // wait for serial monitor to open
    while(! Serial);
  #endif

  // initialize indicator LED
  initLED();
  indicatorState = IL_OFF;

  // initialize indicator timer
  indicatorTimer = timerBegin(1000000);
  timerAttachInterrupt(indicatorTimer, &indicatorTimerISR);

  notify("Initializing...");

  // initialize bluetooth
  initBLE(new BTServerCallbacks(), new BTCallbacks());
  advertiseBT();
  bluetoothState = BT_WAITING;

  // initialize bluetooth button
  pinMode(DEBUG_BUTTON, INPUT_PULLUP);
	attachInterrupt(DEBUG_BUTTON, buttonISR, FALLING);

  // initialize servo
  servo.attach(SERVO_PIN);
  resetServo();

  // initialize servo timer
  servoTimer = timerBegin(1000000);
  timerAttachInterrupt(servoTimer, &servoTimerISR);
  // timerAlarm(servoTimer, SERVO_WAIT * 1000, false, 0);

  // initialize touch interrupt 
  touchAttachInterrupt(T6, touchISR, 60);
    
  // initialize non volatile storage
  prefs.begin("credentials", false);

  // store new wifi credentials only when debugging
  #ifdef MANUAL_CREDS:
    prefs.putString("ssid", WIFI_SSID);
    prefs.putString("pass", WIFI_PASS);
    prefs.putString("key", IO_KEY);
    prefs.putString("user", IO_USERNAME); 
  #endif

  // otherwise load credentials in memory 
  loadCredentials();

  // initialize Adafruit IO object
  io = new AIO_WiFi(USER.c_str(), KEY.c_str(), SSID.c_str(), PASS.c_str());

  // create IO message feed and handlers
  lightFeed = io->feed("lumi_test");
  lightFeed->onMessage(handleFeed);
  
  // connect to Adafruit IO
  connectIO();

  lightState = LIGHT_UNKNOWN;

  // hide bluetooth if not subscribed and IO was successful
  if (bluetoothState != BT_CONNECTED && deviceState == WAITING) {
    hideBT();
    bluetoothState = BT_HIDDEN;
  };
  
  notify("Initialization complete!");
}

/***** Loop *****/
void loop() {
  // keeps the client connected to io.adafruit.com, 
  // and processes any incoming data.
  if (deviceState == WAITING){
    if(io->run(WIFI_RUN_WAIT,false) < AIO_CONNECTED) connectIO();
  }
  
  // check if there was a touch input detected and handle it
  if (touchActive != lastTouchActive){
    handleTouch();
  }

  // check if servo timer timed out and handle it
  if (servoTimeout){
    deviceState = WAITING;
    resetServo();
    servoTimeout = false;
  }

  // check if indicator light timed out
  if (indicatorTimeout){
    updateIndicator();
    indicatorTimeout = false;
  }
}


/***** Functions *****/
// handle Indicator LEDs
void updateIndicator() {
    if (indicatorState == IL_FLASHING) {
      int val = getIndicatorBrightness();
      if (val < INDICATOR_THRESHOLD_LOW) {indicatorFade = INDICATOR_FADE;}
      if (val > INDICATOR_THRESHOLD_HIGH) {indicatorFade = -INDICATOR_FADE;}      
      setIndicatorBrightness(val+indicatorFade); 
    
    } else if (indicatorState == IL_LOADING) {
      advanceIndicator();
    }

    if (indicatorState == IL_FLASHING || indicatorState == IL_LOADING) {
      timerRestart(indicatorTimer);
      timerAlarm(indicatorTimer, INDICATOR_FADE_WAIT * 1000, false, 0);
    } else {
      resetIndicator();
    }
}

// handle IO event changes
void handleFeed ( AdafruitIO_Data *data ) {  
  // convert the data to integer and pass it
  setLight(data->toInt());  
}

// handle touch event
void handleTouch() {
  notify("Touched");
  if(touchActive){
    if(lightState == LIGHT_ON || lightState == LIGHT_UNKNOWN) {
      setLight(true);
    } else {
      setLight(false);
    }    
  } else{
    //do nothing on release
  }
  lastTouchActive = touchActive;
}

// switch room lights state with servo
void setLight(bool state) {  
  deviceState = SWITCHING;
  notify("Switching lights...");
  if(state) {
    servo.write(ON_SERVO_ANGLE);
    lightState == LIGHT_ON;
  } else {
    servo.write(OFF_SERVO_ANGLE);
    lightState == LIGHT_OFF;
  }
  // start timer to reset servo
  timerRestart(servoTimer);
  timerAlarm(servoTimer, SERVO_WAIT * 1000, false, 0);
}

// reset servo to neutral position
void resetServo() {
  servo.write(NEUTRAL_SERVO_ANGLE);
  notify("Servo on neutral");
}

// set to repair WiFi connection
void repairWifi(){
  deviceState = REPAIR_WIFI;
  notify("Repairing WiFi...");
  if(bluetoothState == BT_HIDDEN) {
    advertiseBT();
    bluetoothState = BT_WAITING;
  }
}

// set to repair IO connection
void repairAIO(){
  deviceState = REPAIR_AIO;
  notify("Repairing AIO...");
  if(bluetoothState == BT_HIDDEN) {
    advertiseBT();
    bluetoothState = BT_WAITING;
  }
}

// load credentials in non-volatile memory
void loadCredentials(){
  SSID = prefs.getString("ssid", "");
  PASS = prefs.getString("pass", "");
  KEY = prefs.getString("key", "");
  USER = prefs.getString("user", ""); 
}


// save current variables into non-volatile memory
void saveCredentials(){
  if (deviceState == REPAIR_WIFI) {
    prefs.putString("ssid", SSID);
    prefs.putString("pass", PASS);
  } else if (deviceState == REPAIR_AIO) {
    prefs.putString("key", KEY);
    prefs.putString("user", USER);  
  }
}

// connect to WiFi and io.adafruit.com
void connectIO(){
  // attempt to connect, with a timeout
  notify("Connecting to Adafruit IO...");
  io->run(WIFI_CONN_WAIT,false);

  // check connection attempt, open BLE server if unsuccessful
  if (io->status() <= AIO_NET_CONNECT_FAILED) {
    repairWifi();
  } else if (io->status() < AIO_CONNECTED) {
    repairAIO();
  } else {
    if(deviceState == REPAIR_WIFI || deviceState == REPAIR_AIO){ saveCredentials();}
    deviceState = WAITING;
    notify("Successfully connected!");
  }
}

// notify via Indicator, Serial and/or Bluetooth
template <typename T>
void notify(const T debugText) {    
  // determine the color based on state
  Colors col; 
  switch (deviceState) {
    case INITIALIZING: 
      col = YELLOW;
      indicatorState = IL_ON;
      break;
    case REPAIR_WIFI:
    case REPAIR_AIO:
      col = RED;
      indicatorState = IL_FLASHING; 
      break;
    case SWITCHING:
      col = LAVENDER;
      indicatorState = IL_LOADING;
      break;
    case WAITING:
      col = OFF;
      indicatorState = IL_OFF;
      break;
    default:
      col = OFF;
      indicatorState = IL_OFF;
      break;
  }
  setIndicator(col);
  
  // start indicator timer if FLASHING or LOADING
  if(indicatorState == IL_FLASHING || indicatorState == IL_LOADING) {
      timerAlarm(indicatorTimer, INDICATOR_FADE_WAIT * 1000, false, 0);
  }

  // print debug text to serial if in debug mode
  #ifdef DEBUGGING 
    Serial.println(debugText); 
  #endif

  // send debug text to bluetooth TX channel if connected to a device
  if (bluetoothState == BT_CONNECTED) {
    publishBT(debugText);
  } 
}
