/* Global variables for Lumi */

/***** Definitions *****/
// timing
#define WIFI_RUN_WAIT               300
#define WIFI_CONN_WAIT              400
#define SERVO_WAIT                  1000
#define INDICATOR_FADE_WAIT         100

// thresholds
#define INDICATOR_THRESHOLD_LOW     50
#define INDICATOR_THRESHOLD_HIGH    200
#define INDICATOR_FADE              40

// pins
#define SERVO_PIN     13
#define DEBUG_BUTTON  0 

// servo angles
#define ON_SERVO_ANGLE        45
#define OFF_SERVO_ANGLE       180
#define NEUTRAL_SERVO_ANGLE   100


/***** Variables *****/
// non-volatile storage
Preferences prefs;

// IO variables
String SSID;
String PASS;
String KEY;
String USER;

// indicator fade
static int8_t indicatorFade = INDICATOR_FADE;

// timers and time stamps
hw_timer_t * indicatorTimer = NULL;
hw_timer_t * servoTimer = NULL;
bool indicatorTimeout = false;
bool servoTimeout = false;

// servo control
Servo servo;
bool touchActive = false;
bool lastTouchActive = false;


/***** Function Declarations *****/
template <typename T>
void notify(const T debugText);
void updateIndicator();
void handleFeed( AdafruitIO_Data *data );
void handleTouch();
void setLight(bool state);
void resetServo();
void repairWifi();
void repairAIO();
void loadCredentials();
void reloadCredentials();
void saveCredentials();
void connectIO();
