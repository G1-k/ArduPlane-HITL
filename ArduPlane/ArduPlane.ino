/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#define THISFIRMWARE "ArduPlane V2.73-xp1 updated by JLN"
//
// This firmware is fully based on the Arduplane v2.73, this is an improved version with personnal updates done by Jean-Louis Naudin
// this firmware has been tested in flight and in HIL mode on X-Plane v9.70 on ArduPilot Mega (APM) v1.5, v2, v2.5 and ArduFlyer v2.5.2
// More infos at: http://diydrones.com/profile/JeanLouisNaudin 
/*
 *  Lead developer: Andrew Tridgell
 *
 *  Authors:    Doug Weibel, Jose Julio, Jordi Munoz, Jason Short, Randy Mackay, Pat Hickey, John Arne Birkeland, Olivier Adler, Amilcar Lucas, Gregory Fletcher, Paul Riseborough, Brandon Jones, Jon Challinger
 *  Thanks to:  Chris Anderson, Michael Oborne, Paul Mather, Bill Premerlani, James Cohen, JB from rotorFX, Automatik, Fefenin, Peter Meister, Remzibi, Yury Smirnov, Sandro Benigno, Max Levine, Roberto Navoni, Lorenz Meier, Yury MonZon
 *  Please contribute your ideas!
 *
 * This version include some JLN updates (see below), this version uses some parts of the ThermoPilot project
 *
 *
 *  This firmware is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 */

// =======================================================================================================
// THIS IS AN EXPERIMENTAL VERSION UNDER TEST FROM THE LATEST RELEASED VERSION OF THE ARDUPLANE FIRMWARE
// **** WARNING THIS IS AN EXPERIMENTAL VERSION FOR ALPHA VERSION TESTERS AND EXPERIENCED PILOTS ONLY ****
// DISCLAIMER:
// If you decide to test it on a real model this is at your own risk and I assume no responsability...
//
// Arduplane v2.73 xp1 - compatible with APM v1.5, APM v2.4, ArduFlyer v2.5x board and Mission Planner v1.2.84 and v1.2.86 or above, (don't use the MP v1.2.85)
//
// see the full video of the AUTO-LANDING and AUTO-TAKEOFF test on X-Plane v9.70 in HIL mode at: http://vimeo.com/77580524
// ------------------------------------------------------
// Jean-Louis Naudin (JLN) updates: Last update : December 17, 2013
//
// DOLIST:
//
//-------------------------------------------------------------------------------------------------------------------------
// 2013-12-17: Update of scaling_speed parameter for X_Plane simulation in HIL mode 
// 2013-12-08: Update closed loop nav altitude set between the last and the first WP to avoid pull up throttle effect. 
// 2013-11-12: Update the restart FPL when AUTO is set 
// 2013-11-12: Update the CH7 switch reading for the Cleanup FPL, save WP and auto RTL 
// 2013-11-11: Tested in flight on the Calmato 40 Alpha with an ArduFlyer v2.5.2 board
// 2013-11-07: Mavlink (send_gps_raw) update about the hdop and the numsat value used during X-Plane HIL mode simulation
// 2013-10-22: Updated for Xplane v9.70 - tested and update for auto-takeoff and auto-landing
// 2013-06-30: Threshold low altitude with throttle management updated
// 2012-06-14: Tested in flight on MAJA drone
// 2012-04-08: Add MSL_REF for real altitude (msl) in AUTO navigation (TP_MSL_REF = 1 if enabled)
// 2012-04-08: If CH7_SAVE_WP = ON and AUTO then RTL
// 2012-04-04: Implementing Self-Learning Flight Plan - WP are saved with SW on Ch7 - MANUAL mode = Reset, STABILIZE mode = Save Wp (=1)
// 2012-04-02: Added a new params: TP_AGL_REF (ground altitude in meters MSL)
// 2012-02-02: Auto calculation of the wp_radius Vs the ground_speed and the max turn angle in AUTO mode so as to get the best turning point at the Wp (AUTO_WP_RADIUS ENABLED)
// 2012-02-02: add the CLOSED_LOOP_NAV ENABLED switch to allow redoing the flight plan after the end of the exec: set #define CLOSED_LOOP_NAV  ENABLED

////////////////////////////////////////////////////////////////////////////////
// Header includes
////////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include <AP_Common.h>
#include <AP_Progmem.h>
#include <AP_HAL.h>
#include <AP_Menu.h>
#include <AP_Param.h>
#include <AP_GPS.h>         // ArduPilot GPS library
#include <AP_Baro.h>        // ArduPilot barometer library
#include <AP_Compass.h>     // ArduPilot Mega Magnetometer Library
#include <AP_Math.h>        // ArduPilot Mega Vector/Matrix math Library
#include <AP_ADC.h>         // ArduPilot Mega Analog to Digital Converter Library
#include <AP_ADC_AnalogSource.h>
#include <AP_InertialSensor.h> // Inertial Sensor Library
#include <AP_AHRS.h>         // ArduPilot Mega DCM Library
#include <PID.h>            // PID library
#include <RC_Channel.h>     // RC Channel Library
#include <AP_RangeFinder.h>     // Range finder library
#include <Filter.h>                     // Filter library
#include <AP_Buffer.h>      // APM FIFO Buffer
#include <AP_Relay.h>       // APM relay
#include <AP_Camera.h>          // Photo or video camera
#include <AP_Airspeed.h>
#include <memcheck.h>

#include <APM_OBC.h>
#include <APM_Control.h>
#include <GCS_MAVLink.h>    // MAVLink GCS definitions
#include <AP_Mount.h>           // Camera/Antenna mount
#include <AP_Declination.h> // ArduPilot Mega Declination Helper Library
#include <DataFlash.h>
#include <SITL.h>

#include <AP_Navigation.h>
#include <AP_L1_Control.h>

// Pre-AP_HAL compatibility
#include "compat.h"

// Configuration
#include "config.h"

// Local modules
#include "defines.h"
#include "Parameters.h"
#include "GCS.h"

#include <AP_HAL_AVR.h>
#include <AP_HAL_AVR_SITL.h>
#include <AP_HAL_PX4.h>
#include <AP_HAL_Empty.h>

AP_HAL::BetterStream* cliSerial;

const AP_HAL::HAL& hal = AP_HAL_BOARD_DRIVER;


////////////////////////////////////////////////////////////////////////////////
// Outback Challenge Failsafe Support
////////////////////////////////////////////////////////////////////////////////
#if OBC_FAILSAFE == ENABLED
APM_OBC obc;
#endif

////////////////////////////////////////////////////////////////////////////////
// the rate we run the main loop at
////////////////////////////////////////////////////////////////////////////////
static const AP_InertialSensor::Sample_rate ins_sample_rate = AP_InertialSensor::RATE_50HZ;

////////////////////////////////////////////////////////////////////////////////
// Parameters
////////////////////////////////////////////////////////////////////////////////
//
// Global parameters are all contained within the 'g' class.
//
static Parameters g;

////////////////////////////////////////////////////////////////////////////////
// prototypes
static void update_events(void);
void gcs_send_text_fmt(const prog_char_t *fmt, ...);
static void print_flight_mode(AP_HAL::BetterStream *port, uint8_t mode);


////////////////////////////////////////////////////////////////////////////////
// DataFlash
////////////////////////////////////////////////////////////////////////////////
#if LOGGING_ENABLED == ENABLED
#if CONFIG_HAL_BOARD == HAL_BOARD_APM1
DataFlash_APM1 DataFlash;
#elif CONFIG_HAL_BOARD == HAL_BOARD_APM2
DataFlash_APM2 DataFlash;
#elif CONFIG_HAL_BOARD == HAL_BOARD_AVR_SITL
DataFlash_SITL DataFlash;
#elif CONFIG_HAL_BOARD == HAL_BOARD_PX4
static DataFlash_File DataFlash("/fs/microsd/APM/logs");
#else
// no dataflash driver
DataFlash_Empty DataFlash;
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
// Sensors
////////////////////////////////////////////////////////////////////////////////
//
// There are three basic options related to flight sensor selection.
//
// - Normal flight mode.  Real sensors are used.
// - HIL Attitude mode.  Most sensors are disabled, as the HIL
//   protocol supplies attitude information directly.
// - HIL Sensors mode.  Synthetic sensors are configured that
//   supply data from the simulation.
//

// All GPS access should be through this pointer.
static GPS         *g_gps;

// flight modes convenience array
static AP_Int8          *flight_modes = &g.flight_mode1;

#if CONFIG_ADC == ENABLED
static AP_ADC_ADS7844 adc;
#endif

#if CONFIG_BARO == AP_BARO_BMP085
static AP_Baro_BMP085 barometer;
#elif CONFIG_BARO == AP_BARO_PX4
static AP_Baro_PX4 barometer;
#elif CONFIG_BARO == AP_BARO_HIL
static AP_Baro_HIL barometer;
#elif CONFIG_BARO == AP_BARO_MS5611
 #if CONFIG_MS5611_SERIAL == AP_BARO_MS5611_SPI
 static AP_Baro_MS5611 barometer(&AP_Baro_MS5611::spi);
 #elif CONFIG_MS5611_SERIAL == AP_BARO_MS5611_I2C
 static AP_Baro_MS5611 barometer(&AP_Baro_MS5611::i2c);
 #else
 #error Unrecognized CONFIG_MS5611_SERIAL setting.
 #endif
#else
 #error Unrecognized CONFIG_BARO setting
#endif

#if CONFIG_COMPASS == AP_COMPASS_PX4
static AP_Compass_PX4 compass;
#elif CONFIG_COMPASS == AP_COMPASS_HMC5843
static AP_Compass_HMC5843 compass;
#elif CONFIG_COMPASS == AP_COMPASS_HIL
static AP_Compass_HIL compass;
#else
 #error Unrecognized CONFIG_COMPASS setting
#endif

// GPS selection
#if   GPS_PROTOCOL == GPS_PROTOCOL_AUTO
AP_GPS_Auto     g_gps_driver(&g_gps);

#elif GPS_PROTOCOL == GPS_PROTOCOL_NMEA
AP_GPS_NMEA     g_gps_driver;

#elif GPS_PROTOCOL == GPS_PROTOCOL_SIRF
AP_GPS_SIRF     g_gps_driver;

#elif GPS_PROTOCOL == GPS_PROTOCOL_UBLOX
AP_GPS_UBLOX    g_gps_driver;

#elif GPS_PROTOCOL == GPS_PROTOCOL_MTK
AP_GPS_MTK      g_gps_driver;

#elif GPS_PROTOCOL == GPS_PROTOCOL_MTK19
AP_GPS_MTK19    g_gps_driver;

#elif GPS_PROTOCOL == GPS_PROTOCOL_NONE
AP_GPS_None     g_gps_driver;

#elif GPS_PROTOCOL == GPS_PROTOCOL_HIL
AP_GPS_HIL      g_gps_driver;

#else
  #error Unrecognised GPS_PROTOCOL setting.
#endif // GPS PROTOCOL

#if CONFIG_INS_TYPE == CONFIG_INS_MPU6000
AP_InertialSensor_MPU6000 ins;
#elif CONFIG_INS_TYPE == CONFIG_INS_PX4
AP_InertialSensor_PX4 ins;
#elif CONFIG_INS_TYPE == CONFIG_INS_STUB
AP_InertialSensor_Stub ins;
#elif CONFIG_INS_TYPE == CONFIG_INS_OILPAN
AP_InertialSensor_Oilpan ins( &adc );
#else
  #error Unrecognised CONFIG_INS_TYPE setting.
#endif // CONFIG_INS_TYPE

AP_AHRS_DCM ahrs(&ins, g_gps);

static AP_L1_Control L1_controller(&ahrs);

#if CONFIG_HAL_BOARD == HAL_BOARD_AVR_SITL
SITL sitl;
#endif

// Training mode
static bool training_manual_roll;  // user has manual roll control
static bool training_manual_pitch; // user has manual pitch control

////////////////////////////////////////////////////////////////////////////////
// GCS selection
////////////////////////////////////////////////////////////////////////////////
static GCS_MAVLINK gcs0;
static GCS_MAVLINK gcs3;

// selected navigation controller
static AP_Navigation *nav_controller = &L1_controller;

////////////////////////////////////////////////////////////////////////////////
// Analog Inputs
////////////////////////////////////////////////////////////////////////////////

static AP_HAL::AnalogSource *pitot_analog_source;

// a pin for reading the receiver RSSI voltage. 
static AP_HAL::AnalogSource *rssi_analog_source;

static AP_HAL::AnalogSource *vcc_pin;

static AP_HAL::AnalogSource * batt_volt_pin;
static AP_HAL::AnalogSource * batt_curr_pin;

////////////////////////////////////////////////////////////////////////////////
// Relay
////////////////////////////////////////////////////////////////////////////////
static AP_Relay relay;

// Camera
#if CAMERA == ENABLED
static AP_Camera camera(&relay);
#endif

////////////////////////////////////////////////////////////////////////////////
// Global variables
////////////////////////////////////////////////////////////////////////////////

// APM2 only
#if USB_MUX_PIN > 0
static bool usb_connected;
#endif

/* Radio values
 *               Channel assignments
 *                       1   Ailerons
 *                       2   Elevator
 *                       3   Throttle
 *                       4   Rudder
 *                       5   Aux5
 *                       6   Aux6
 *                       7   Aux7
 *                       8   Aux8/Mode
 *               Each Aux channel can be configured to have any of the available auxiliary functions assigned to it.
 *               See libraries/RC_Channel/RC_Channel_aux.h for more information
 */

////////////////////////////////////////////////////////////////////////////////
// Radio
////////////////////////////////////////////////////////////////////////////////
// This is the state of the flight control system
// There are multiple states defined such as MANUAL, FBW-A, AUTO
enum FlightMode control_mode  = INITIALISING;
// Used to maintain the state of the previous control switch position
// This is set to -1 when we need to re-read the switch
uint8_t oldSwitchPosition;
// This is used to enable the inverted flight feature
bool inverted_flight     = false;

static struct {
    // These are trim values used for elevon control
    // For elevons radio_in[CH_ROLL] and radio_in[CH_PITCH] are
    // equivalent aileron and elevator, not left and right elevon
    uint16_t trim1;
    uint16_t trim2;
    // These are used in the calculation of elevon1_trim and elevon2_trim
    uint16_t ch1_temp;
    uint16_t ch2_temp;
} elevon = {
	trim1 : 1500,
    trim2 : 1500,
    ch1_temp : 1500,
    ch2_temp : 1500
};

// A flag if GCS joystick control is in use
static bool rc_override_active = false;

////////////////////////////////////////////////////////////////////////////////
// Failsafe
////////////////////////////////////////////////////////////////////////////////
// A tracking variable for type of failsafe active
// Used for failsafe based on loss of RC signal or GCS signal
static int16_t failsafe;
// Used to track if the value on channel 3 (throtttle) has fallen below the failsafe threshold
// RC receiver should be set up to output a low throttle value when signal is lost
static bool ch3_failsafe;
// A timer used to help recovery from unusual attitudes.  If we enter an unusual attitude
// while in autonomous flight this variable is used  to hold roll at 0 for a recovery period
static uint8_t crash_timer;

// the time when the last HEARTBEAT message arrived from a GCS
static uint32_t last_heartbeat_ms;

// A timer used to track how long we have been in a "short failsafe" condition due to loss of RC signal
static uint32_t ch3_failsafe_timer = 0;

////////////////////////////////////////////////////////////////////////////////
// LED output
////////////////////////////////////////////////////////////////////////////////
// state of the GPS light (on/off)
static bool GPS_light;

////////////////////////////////////////////////////////////////////////////////
// GPS variables
////////////////////////////////////////////////////////////////////////////////
// This is used to scale GPS values for EEPROM storage
// 10^7 times Decimal GPS means 1 == 1cm
// This approximation makes calculations integer and it's easy to read
static const float t7                        = 10000000.0;
// We use atan2 and other trig techniques to calaculate angles
// A counter used to count down valid gps fixes to allow the gps estimate to settle
// before recording our home position (and executing a ground start if we booted with an air start)
static uint8_t ground_start_count      = 5;
// Used to compute a speed estimate from the first valid gps fixes to decide if we are
// on the ground or in the air.  Used to decide if a ground start is appropriate if we
// booted with an air start.
static int16_t ground_start_avg;

// true if we have a position estimate from AHRS
static bool have_position;

////////////////////////////////////////////////////////////////////////////////
// Location & Navigation
////////////////////////////////////////////////////////////////////////////////

// Direction held during phases of takeoff and landing
// deg * 100 dir of plane,  A value of -1 indicates the course has not been set/is not in use
// this is a 0..36000 value, or -1 for disabled
static int32_t hold_course_cd                 = -1;              // deg * 100 dir of plane

// There may be two active commands in Auto mode.
// This indicates the active navigation command by index number
static uint8_t nav_command_index;
// This indicates the active non-navigation command by index number
static uint8_t non_nav_command_index;
// This is the command type (eg navigate to waypoint) of the active navigation command
static uint8_t nav_command_ID          = NO_COMMAND;
static uint8_t non_nav_command_ID      = NO_COMMAND;

////////////////////////////////////////////////////////////////////////////////
// Airspeed
////////////////////////////////////////////////////////////////////////////////
// The calculated airspeed to use in FBW-B.  Also used in higher modes for insuring min ground speed is met.
// Also used for flap deployment criteria.  Centimeters per second.
static int32_t target_airspeed_cm;

// The difference between current and desired airspeed.  Used in the pitch controller.  Centimeters per second.
static float airspeed_error_cm;

// The calculated total energy error (kinetic (altitude) plus potential (airspeed)).
// Used by the throttle controller
static int32_t energy_error;

// kinetic portion of energy error (m^2/s^2)
static int32_t airspeed_energy_error;

// An amount that the airspeed should be increased in auto modes based on the user positioning the
// throttle stick in the top half of the range.  Centimeters per second.
static int16_t airspeed_nudge_cm;

// Similar to airspeed_nudge, but used when no airspeed sensor.
// 0-(throttle_max - throttle_cruise) : throttle nudge in Auto mode using top 1/2 of throttle stick travel
static int16_t throttle_nudge = 0;

// receiver RSSI
static uint8_t receiver_rssi;


////////////////////////////////////////////////////////////////////////////////
// Ground speed
////////////////////////////////////////////////////////////////////////////////
// The amount current ground speed is below min ground speed.  Centimeters per second
static int32_t groundspeed_undershoot = 0;

// Difference between current altitude and desired altitude.  Centimeters
static int32_t altitude_error_cm;

////////////////////////////////////////////////////////////////////////////////
// Battery Sensors
////////////////////////////////////////////////////////////////////////////////
// Battery pack 1 voltage.  Initialized above the low voltage threshold to pre-load the filter and prevent low voltage events at startup.
static float battery_voltage1        = LOW_VOLTAGE * 1.05;
// Battery pack 1 instantaneous currrent draw.  Amperes
static float current_amps1;
// Totalized current (Amp-hours) from battery 1
static float current_total1;

// To Do - Add support for second battery pack
//static float  battery_voltage2    = LOW_VOLTAGE * 1.05;		// Battery 2 Voltage, initialized above threshold for filter
//static float	current_amps2;									// Current (Amperes) draw from battery 2
//static float	current_total2;									// Totalized current (Amp-hours) from battery 2

////////////////////////////////////////////////////////////////////////////////
// Airspeed Sensors
////////////////////////////////////////////////////////////////////////////////
AP_Airspeed airspeed;

////////////////////////////////////////////////////////////////////////////////
// Altitude Sensor variables

////////////////////////////////////////////////////////////////////////////////
// flight mode specific
////////////////////////////////////////////////////////////////////////////////
// Flag for using gps ground course instead of INS yaw.  Set false when takeoff command in process.
static bool takeoff_complete    = true;
// Flag to indicate if we have landed.
//Set land_complete if we are within 2 seconds distance or within 3 meters altitude of touchdown
static bool land_complete;
// Altitude threshold to complete a takeoff command in autonomous modes.  Centimeters
static int32_t takeoff_altitude;

// Minimum pitch to hold during takeoff command execution.  Hundredths of a degree
static int16_t takeoff_pitch_cd;

// this controls throttle suppression in auto modes
static bool throttle_suppressed;

////////////////////////////////////////////////////////////////////////////////
// Loiter management
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Navigation control variables
////////////////////////////////////////////////////////////////////////////////
// The instantaneous desired bank angle.  Hundredths of a degree
static int32_t nav_roll_cd;

// The instantaneous desired pitch angle.  Hundredths of a degree
static int32_t nav_pitch_cd;

////////////////////////////////////////////////////////////////////////////////
// Waypoint distances
////////////////////////////////////////////////////////////////////////////////
// Distance between plane and next waypoint.  Meters
// is not static because AP_Camera uses it
uint32_t wp_distance;

// Distance between previous and next waypoint.  Meters
static uint32_t wp_totalDistance;

/*
  meta data to support counting the number of circles in a loiter
 */
static struct {
    // previous target bearing, used to update sum_cd
    int32_t old_target_bearing_cd;

    // Total desired rotation in a loiter.  Used for Loiter Turns commands. 
    int32_t total_cd;

    // total angle completed in the loiter so far
    int32_t sum_cd;

	// Direction for loiter. 1 for clockwise, -1 for counter-clockwise
    int8_t direction;

	// start time of the loiter.  Milliseconds.
    uint32_t start_time_ms;

	// The amount of time we should stay in a loiter for the Loiter Time command.  Milliseconds.
    uint32_t time_max_ms;
} loiter;


// event control state
enum event_type { 
    EVENT_TYPE_RELAY=0,
    EVENT_TYPE_SERVO=1
};

static struct {
    enum event_type type;

	// when the event was started in ms
    uint32_t start_time_ms;

	// how long to delay the next firing of event in millis
    uint16_t delay_ms;

	// how many times to cycle : -1 (or -2) = forever, 2 = do one cycle, 4 = do two cycles
    int16_t repeat;

    // RC channel for servos
    uint8_t rc_channel;

	// PWM for servos
	uint16_t servo_value;

	// the value used to cycle events (alternate value to event_value)
    uint16_t undo_value;
} event_state;


////////////////////////////////////////////////////////////////////////////////
// Conditional command
////////////////////////////////////////////////////////////////////////////////
// A value used in condition commands (eg delay, change alt, etc.)
// For example in a change altitude command, it is the altitude to change to.
static int32_t condition_value;
// A starting value used to check the status of a conditional command.
// For example in a delay command the condition_start records that start time for the delay
static uint32_t condition_start;
// A value used in condition commands.  For example the rate at which to change altitude.
static int16_t condition_rate;

////////////////////////////////////////////////////////////////////////////////
// 3D Location vectors
// Location structure defined in AP_Common
////////////////////////////////////////////////////////////////////////////////
// The home location used for RTL.  The location is set when we first get stable GPS lock
static struct   Location home;
// Flag for if we have g_gps lock and have set the home location
static bool home_is_set;
// The location of the previous waypoint.  Used for track following and altitude ramp calculations
static struct   Location prev_WP;
// The plane's current location
static struct   Location current_loc;
// The location of the current/active waypoint.  Used for altitude ramp, track following and loiter calculations.
static struct   Location next_WP;
// The location of the active waypoint in Guided mode.
static struct   Location guided_WP;
// The location structure information from the Nav command being processed
static struct   Location next_nav_command;
// The location structure information from the Non-Nav command being processed
static struct   Location next_nonnav_command;
static long    wp_radius; // JLN Update

////////////////////////////////////////////////////////////////////////////////
// Altitude / Climb rate control
////////////////////////////////////////////////////////////////////////////////
// The current desired altitude.  Altitude is linearly ramped between waypoints.  Centimeters
static int32_t target_altitude_cm;
// Altitude difference between previous and current waypoint.  Centimeters
static int32_t offset_altitude_cm;
static int32_t          gps_base_alt;

////////////////////////////////////////////////////////////////////////////////
// CH7 control    - JLN Update
////////////////////////////////////////////////////////////////////////////////

// Used to track the CH7 toggle state.
// When CH7 goes LOW PWM from HIGH PWM, this value will have been set true
// This allows advanced functionality to know when to execute
static bool trim_flag;
// This register tracks the current Mission Command index when writing
// a mission using CH7 in flight
static int8_t CH7_wp_index = 0;

////////////////////////////////////////////////////////////////////////////////
// INS variables
////////////////////////////////////////////////////////////////////////////////
// The main loop execution time.  Seconds
//This is the time between calls to the DCM algorithm and is the Integration time for the gyros.
static float G_Dt                                               = 0.02;

////////////////////////////////////////////////////////////////////////////////
// Performance monitoring
////////////////////////////////////////////////////////////////////////////////
// Timer used to accrue data and trigger recording of the performanc monitoring log message
static int32_t perf_mon_timer;
// The maximum main loop execution time recorded in the current performance monitoring interval
static int16_t G_Dt_max = 0;
// The number of gps fixes recorded in the current performance monitoring interval
static uint8_t gps_fix_count = 0;
// A variable used by developers to track performanc metrics.
// Currently used to record the number of GCS heartbeat messages received
static int16_t pmTest1 = 0;


////////////////////////////////////////////////////////////////////////////////
// System Timers
////////////////////////////////////////////////////////////////////////////////
// Time in miliseconds of start of main control loop.  Milliseconds
static uint32_t fast_loopTimer_ms;

// Time Stamp when fast loop was complete.  Milliseconds
static uint32_t fast_loopTimeStamp_ms;

// Number of milliseconds used in last main loop cycle
static uint8_t delta_ms_fast_loop;

// Counter of main loop executions.  Used for performance monitoring and failsafe processing
static uint16_t mainLoop_count;

// Time in miliseconds of start of medium control loop.  Milliseconds
static uint32_t medium_loopTimer_ms;

// Counters for branching from main control loop to slower loops
static uint8_t medium_loopCounter;
// Number of milliseconds used in last medium loop cycle
static uint8_t delta_ms_medium_loop;

// Counters for branching from medium control loop to slower loops
static uint8_t slow_loopCounter;
// Counter to trigger execution of very low rate processes
static uint8_t superslow_loopCounter;
// Counter to trigger execution of 1 Hz processes
static uint8_t counter_one_herz;

// % MCU cycles used
static float load;


// Camera/Antenna mount tracking and stabilisation stuff
// --------------------------------------
#if MOUNT == ENABLED
// current_loc uses the baro/gps soloution for altitude rather than gps only.
// mabe one could use current_loc for lat/lon too and eliminate g_gps alltogether?
AP_Mount camera_mount(&current_loc, g_gps, &ahrs, 0);
#endif

#if MOUNT2 == ENABLED
// current_loc uses the baro/gps soloution for altitude rather than gps only.
// mabe one could use current_loc for lat/lon too and eliminate g_gps alltogether?
AP_Mount camera_mount2(&current_loc, g_gps, &ahrs, 1);
#endif

#if CAMERA == ENABLED
//pinMode(camtrig, OUTPUT);			// these are free pins PE3(5), PH3(15), PH6(18), PB4(23), PB5(24), PL1(36), PL3(38), PA6(72), PA7(71), PK0(89), PK1(88), PK2(87), PK3(86), PK4(83), PK5(84), PK6(83), PK7(82)
#endif

////////////////////////////////////////////////////////////////////////////////
// Top-level logic
////////////////////////////////////////////////////////////////////////////////

// setup the var_info table
AP_Param param_loader(var_info, WP_START_BYTE);

void setup() {
    // this needs to be the first call, as it fills memory with
    // sentinel values
    memcheck_init();

    cliSerial = hal.console;

    // load the default values of variables listed in var_info[]
    AP_Param::setup_sketch_defaults();

    rssi_analog_source = hal.analogin->channel(ANALOG_INPUT_NONE);

#if CONFIG_PITOT_SOURCE == PITOT_SOURCE_ADC
    pitot_analog_source = new AP_ADC_AnalogSource( &adc,
                                         CONFIG_PITOT_SOURCE_ADC_CHANNEL, 1.0f);
#elif CONFIG_PITOT_SOURCE == PITOT_SOURCE_ANALOG_PIN
    pitot_analog_source = hal.analogin->channel(CONFIG_PITOT_SOURCE_ANALOG_PIN);
    hal.gpio->write(hal.gpio->analogPinToDigitalPin(CONFIG_PITOT_SOURCE_ANALOG_PIN), 0);
#endif
    vcc_pin = hal.analogin->channel(ANALOG_INPUT_BOARD_VCC);

    batt_volt_pin = hal.analogin->channel(g.battery_volt_pin);
    batt_curr_pin = hal.analogin->channel(g.battery_curr_pin);
    
    airspeed.init(pitot_analog_source);
    init_ardupilot();
}

void loop()
{
    // We want this to execute at 50Hz, but synchronised with the gyro/accel
    uint16_t num_samples = ins.num_samples_available();
    if (num_samples >= 1) {
        delta_ms_fast_loop      = millis() - fast_loopTimer_ms;
        load                = (float)(fast_loopTimeStamp_ms - fast_loopTimer_ms)/delta_ms_fast_loop;
        G_Dt                = (float)delta_ms_fast_loop / 1000.f;
        fast_loopTimer_ms   = millis();

        mainLoop_count++;

        // Execute the fast loop
        // ---------------------
        fast_loop();

        // Execute the medium loop
        // -----------------------
        medium_loop();

        counter_one_herz++;
        if(counter_one_herz == 50) {
            one_second_loop();
            counter_one_herz = 0;
        }

        if (millis() - perf_mon_timer > 20000) {
            if (mainLoop_count != 0) {
                if (g.log_bitmask & MASK_LOG_PM)
                    Log_Write_Performance();
                    resetPerfData();
            }
        }

        fast_loopTimeStamp_ms = millis();
    } else if (millis() - fast_loopTimeStamp_ms < 19) {
        // less than 19ms has passed. We have at least one millisecond
        // of free time. The most useful thing to do with that time is
        // to accumulate some sensor readings, specifically the
        // compass, which is often very noisy but is not interrupt
        // driven, so it can't accumulate readings by itself
        if (g.compass_enabled) {
            compass.accumulate();
        }
    }
}

// Main loop 50Hz
static void fast_loop()
{
    // This is the fast loop - we want it to execute at 50Hz if possible
    // -----------------------------------------------------------------
    if (delta_ms_fast_loop > G_Dt_max)
        G_Dt_max = delta_ms_fast_loop;

    // Read radio
    // ----------
    read_radio();

    // try to send any deferred messages if the serial port now has
    // some space available
    gcs_send_message(MSG_RETRY_DEFERRED);

    // check for loss of control signal failsafe condition
    // ------------------------------------
    check_short_failsafe();

#if HIL_MODE != HIL_MODE_DISABLED
    // update hil before AHRS update
    gcs_update();
#endif

    ahrs.update();

    if (g.log_bitmask & MASK_LOG_ATTITUDE_FAST)
        Log_Write_Attitude();

    if (g.log_bitmask & MASK_LOG_IMU)
        Log_Write_IMU();

    // inertial navigation
    // ------------------
#if INERTIAL_NAVIGATION == ENABLED
    // TODO: implement inertial nav function
    inertialNavigation();
#endif

    // custom code/exceptions for flight modes
    // calculates roll, pitch and yaw demands from navigation demands
    // --------------------------------------------------------------
    update_current_flight_mode();

    // apply desired roll, pitch and yaw to the plane
    // ----------------------------------------------
    if (control_mode > MANUAL)
        stabilize();

    // write out the servo PWM values
    // ------------------------------
    set_servos();

    gcs_update();
    gcs_data_stream_send();
}

static void medium_loop()
{
#if MOUNT == ENABLED
    camera_mount.update_mount_position();
#endif

#if MOUNT2 == ENABLED
    camera_mount2.update_mount_position();
#endif

#if CAMERA == ENABLED
    camera.trigger_pic_cleanup();
#endif

    // This is the start of the medium (10 Hz) loop pieces
    // -----------------------------------------
    switch(medium_loopCounter) {

    // This case deals with the GPS
    //-------------------------------
    case 0:
        medium_loopCounter++;
        update_GPS();
        calc_gndspeed_undershoot();

        if (g.compass_enabled && compass.read()) {
            ahrs.set_compass(&compass);
            compass.null_offsets();
            if (g.log_bitmask & MASK_LOG_COMPASS) {
                Log_Write_Compass();
            }
        } else {
            ahrs.set_compass(NULL);
        }

        break;

    // This case performs some navigation computations
    //------------------------------------------------
    case 1:
        medium_loopCounter++;

        // Read 6-position switch on radio
        // -------------------------------
        read_control_switch();

        // calculate the plane's desired bearing
        // -------------------------------------
        navigate();

        break;

    // command processing
    //------------------------------
    case 2:
        medium_loopCounter++;

        // Read Airspeed
        // -------------
        if (airspeed.enabled()) {
            read_airspeed();
        }

        read_receiver_rssi();

        // Read altitude from sensors
        // ------------------
        update_alt();

        // altitude smoothing
        // ------------------
        if (control_mode != FLY_BY_WIRE_B)
            calc_altitude_error();

        // perform next command
        // --------------------
        update_commands();
        break;

    // This case deals with sending high rate telemetry
    //-------------------------------------------------
    case 3:
        medium_loopCounter++;

        if ((g.log_bitmask & MASK_LOG_ATTITUDE_MED) && !(g.log_bitmask & MASK_LOG_ATTITUDE_FAST))
            Log_Write_Attitude();

        if (g.log_bitmask & MASK_LOG_CTUN)
            Log_Write_Control_Tuning();

        if (g.log_bitmask & MASK_LOG_NTUN)
            Log_Write_Nav_Tuning();
        break;

    // This case controls the slow loop
    //---------------------------------
    case 4:
        medium_loopCounter = 0;
        delta_ms_medium_loop    = millis() - medium_loopTimer_ms;
        medium_loopTimer_ms     = millis();

        if (g.battery_monitoring != 0) {
            read_battery();
        }

        read_trim_switch(); // JLN Save current position as WP in the FPL and clear FPL

        slow_loop();

#if OBC_FAILSAFE == ENABLED
        // perform OBC failsafe checks
        obc.check(OBC_MODE(control_mode),
                  last_heartbeat_ms,
                  g_gps ? g_gps->last_fix_time : 0);
#endif

        break;
    }
}

static void slow_loop()
{
    // This is the slow (3 1/3 Hz) loop pieces
    //----------------------------------------
    switch (slow_loopCounter) {
    case 0:
        slow_loopCounter++;
        check_long_failsafe();
        superslow_loopCounter++;
        if(superslow_loopCounter >=200) {                                               
            //	200 = Execute every minute
            if(g.compass_enabled) {
                compass.save_offsets();
            }

            superslow_loopCounter = 0;
        }
        break;

    case 1:
        slow_loopCounter++;

#if CONFIG_HAL_BOARD == HAL_BOARD_PX4
        update_aux_servo_function(&g.rc_5, &g.rc_6, &g.rc_7, &g.rc_8, &g.rc_9, &g.rc_10, &g.rc_11, &g.rc_12);
#elif CONFIG_HAL_BOARD == HAL_BOARD_APM2
        update_aux_servo_function(&g.rc_5, &g.rc_6, &g.rc_7, &g.rc_8, &g.rc_9, &g.rc_10, &g.rc_11);
#else
        update_aux_servo_function(&g.rc_5, &g.rc_6, &g.rc_7, &g.rc_8);
#endif
        enable_aux_servos();

#if MOUNT == ENABLED
        camera_mount.update_mount_type();
#endif
#if MOUNT2 == ENABLED
        camera_mount2.update_mount_type();
#endif
        break;

    case 2:
        slow_loopCounter = 0;
        update_events();

        mavlink_system.sysid = g.sysid_this_mav;                // This is just an ugly hack to keep mavlink_system.sysid sync'd with our parameter

        check_usb_mux();

        break;
    }
}

static void one_second_loop()
{
    if (g.log_bitmask & MASK_LOG_CURRENT)
        Log_Write_Current();

    // send a heartbeat
    gcs_send_message(MSG_HEARTBEAT);
}

static void update_GPS(void)
{
    static uint32_t last_gps_reading;
    g_gps->update();
    update_GPS_light();

    if (g_gps->last_message_time_ms() != last_gps_reading) {
        last_gps_reading = g_gps->last_message_time_ms();
        if (g.log_bitmask & MASK_LOG_GPS) {
            Log_Write_GPS();
        }
    }

    // get position from AHRS
    have_position = ahrs.get_position(&current_loc);

    if (g_gps->new_data && g_gps->status() >= GPS::GPS_OK_FIX_3D) {
        g_gps->new_data = false;

        // for performance
        // ---------------
        gps_fix_count++;

        if(ground_start_count > 1) {
            ground_start_count--;
            ground_start_avg += g_gps->ground_speed;

        } else if (ground_start_count == 1) {
            // We countdown N number of good GPS fixes
            // so that the altitude is more accurate
            // -------------------------------------
            if (current_loc.lat == 0) {
                ground_start_count = 5;

            } else {
                if(ENABLE_AIR_START == 1 && (ground_start_avg / 5) < SPEEDFILT) {
                    startup_ground();

                    if (g.log_bitmask & MASK_LOG_CMD)
                        Log_Write_Startup(TYPE_GROUNDSTART_MSG);

                    init_home();
                } else if (ENABLE_AIR_START == 0) {
                    init_home();
                }

                if (g.compass_enabled) {
                    // Set compass declination automatically
                    compass.set_initial_location(g_gps->latitude, g_gps->longitude);
                }
                ground_start_count = 0;
            }
        }

        // see if we've breached the geo-fence
        geofence_check(false);
    }
}

static void update_current_flight_mode(void)
{
    if(control_mode == AUTO) {
        crash_checker();

        switch(nav_command_ID) {
        case MAV_CMD_NAV_TAKEOFF:
            if (hold_course_cd == -1) {
                // we don't yet have a heading to hold - just level
                // the wings until we get up enough speed to get a GPS heading
                nav_roll_cd = 0;
            } else {
                calc_nav_roll();
                // during takeoff use the level flight roll limit to
                // prevent large course corrections
				nav_roll_cd = constrain_int32(nav_roll_cd, -g.level_roll_limit*100UL, g.level_roll_limit*100UL);
            }

            if (alt_control_airspeed()) {
                calc_nav_pitch();
                if (nav_pitch_cd < takeoff_pitch_cd)
                    nav_pitch_cd = takeoff_pitch_cd;
            } else {
                nav_pitch_cd = (g_gps->ground_speed / (float)g.airspeed_cruise_cm) * takeoff_pitch_cd;
                nav_pitch_cd = constrain_int32(nav_pitch_cd, 500, takeoff_pitch_cd);
            }

            float aspeed;
            if (ahrs.airspeed_estimate(&aspeed)) {
                // don't use a pitch/roll integrators during takeoff if we are
                // below minimum speed
                if (aspeed < g.flybywire_airspeed_min) {
                    g.pidServoPitch.reset_I();
                    g.pidServoRoll.reset_I();
                    g.pitchController.reset_I();
                    g.rollController.reset_I();
                }
            }

            // max throttle for takeoff
            g.channel_throttle.servo_out = g.throttle_max;

            break;

        case MAV_CMD_NAV_LAND:
            calc_nav_roll();

            if (land_complete) {
                // during final approach constrain roll to the range
                // allowed for level flight
                nav_roll_cd = constrain_int32(nav_roll_cd, -g.level_roll_limit*100UL, g.level_roll_limit*100UL);

                // hold pitch constant in final approach
                nav_pitch_cd = g.land_pitch_cd;
            } else {
                calc_nav_pitch();
                if (!alt_control_airspeed()) {
                    // when not under airspeed control, don't allow
                    // down pitch in landing
                    nav_pitch_cd = constrain_int32(nav_pitch_cd, 0, nav_pitch_cd);
                }
            }
            calc_throttle();

            if (land_complete) {
                // we are in the final stage of a landing - force
                // zero throttle
                g.channel_throttle.servo_out = 0;
            }
            break;

        default:
            // we are doing normal AUTO flight, the special cases
            // are for takeoff and landing
            hold_course_cd = -1;
            land_complete = false;
            calc_nav_roll();
            calc_nav_pitch();
            calc_throttle();
            break;
        }
    }else{
        // hold_course is only used in takeoff and landing
        hold_course_cd = -1;

        switch(control_mode) {
        case RTL:
        case LOITER:
        case GUIDED:
            crash_checker();
            calc_nav_roll();
            calc_nav_pitch();
            calc_throttle();
            break;

        case TRAINING: {
            training_manual_roll = false;
            training_manual_pitch = false;

            // if the roll is past the set roll limit, then
            // we set target roll to the limit
            if (ahrs.roll_sensor >= g.roll_limit_cd) {
                nav_roll_cd = g.roll_limit_cd;
            } else if (ahrs.roll_sensor <= -g.roll_limit_cd) {
                nav_roll_cd = -g.roll_limit_cd;                
            } else {
                training_manual_roll = true;
                nav_roll_cd = 0;
            }

            // if the pitch is past the set pitch limits, then
            // we set target pitch to the limit
            if (ahrs.pitch_sensor >= g.pitch_limit_max_cd) {
                nav_pitch_cd = g.pitch_limit_max_cd;
            } else if (ahrs.pitch_sensor <= g.pitch_limit_min_cd) {
                nav_pitch_cd = g.pitch_limit_min_cd;
            } else {
                training_manual_pitch = true;
                nav_pitch_cd = 0;
            }
            if (inverted_flight) {
                nav_pitch_cd = -nav_pitch_cd;
            }
            break;
        }

        case FLY_BY_WIRE_A: {
            // set nav_roll and nav_pitch using sticks
            nav_roll_cd  = g.channel_roll.norm_input() * g.roll_limit_cd;
            nav_roll_cd = constrain_int32(nav_roll_cd, -g.roll_limit_cd, g.roll_limit_cd);
            float pitch_input = g.channel_pitch.norm_input();
            if (pitch_input > 0) {
                nav_pitch_cd = pitch_input * g.pitch_limit_max_cd;
            } else {
                nav_pitch_cd = -(pitch_input * g.pitch_limit_min_cd);
            }
            nav_pitch_cd = constrain_int32(nav_pitch_cd, g.pitch_limit_min_cd.get(), g.pitch_limit_max_cd.get());
            if (inverted_flight) {
                nav_pitch_cd = -nav_pitch_cd;
            }
            break;
        }

        case FLY_BY_WIRE_B: {
            static float last_elevator_input;
            // Substitute stick inputs for Navigation control output
            // We use g.pitch_limit_min because its magnitude is
            // normally greater than g.pitch_limit_max

            // Thanks to Yury MonZon for the altitude limit code!

            nav_roll_cd = g.channel_roll.norm_input() * g.roll_limit_cd;

            float elevator_input;
            elevator_input = g.channel_pitch.norm_input();

            if (g.flybywire_elev_reverse) {
                elevator_input = -elevator_input;
            }

            target_altitude_cm += g.flybywire_climb_rate * elevator_input * delta_ms_fast_loop * 0.1f;

            if (elevator_input == 0.0f && last_elevator_input != 0.0f) {
                // the user has just released the elevator, lock in
                // the current altitude
                target_altitude_cm = current_loc.alt;
            }

            // check for FBWB altitude limit
            if (g.FBWB_min_altitude_cm != 0 && target_altitude_cm < home.alt + g.FBWB_min_altitude_cm) {
                target_altitude_cm = home.alt + g.FBWB_min_altitude_cm;
            }
            altitude_error_cm = target_altitude_cm - adjusted_altitude_cm();

            last_elevator_input = elevator_input;

            calc_throttle();
            calc_nav_pitch();
        }
            break;

        case STABILIZE:
            nav_roll_cd        = 0;
            nav_pitch_cd       = 0;
            // throttle is passthrough
            break;

        case CIRCLE:
            // we have no GPS installed and have lost radio contact
            // or we just want to fly around in a gentle circle w/o GPS,
            // holding altitude at the altitude we set when we
            // switched into the mode
            nav_roll_cd  = g.roll_limit_cd / 3;
            calc_nav_pitch();
            calc_throttle();
            break;

        case MANUAL:
            // servo_out is for Sim control only
            // ---------------------------------
            g.channel_roll.servo_out = g.channel_roll.pwm_to_angle();
            g.channel_pitch.servo_out = g.channel_pitch.pwm_to_angle();
            g.channel_rudder.servo_out = g.channel_rudder.pwm_to_angle();
            break;
            //roll: -13788.000,  pitch: -13698.000,   thr: 0.000, rud: -13742.000

        case INITIALISING:
        case AUTO:
            // handled elsewhere
            break;
        }
    }
}

static void update_navigation()
{
    // wp_distance is in ACTUAL meters, not the *100 meters we get from the GPS
    // ------------------------------------------------------------------------

    // distance and bearing calcs only
    switch(control_mode) {
    case AUTO:
        verify_commands();
        break;
            
    case LOITER:
    case RTL:
    case GUIDED:
        update_loiter();
        break;

    case MANUAL:
    case STABILIZE:
    case TRAINING:
    case INITIALISING:
    case FLY_BY_WIRE_A:
    case FLY_BY_WIRE_B:
    case CIRCLE:
        // nothing to do
        break;
    }
}


static void update_alt()
{
    // this function is in place to potentially add a sonar sensor in the future
    //altitude_sensor = BARO;

    if (barometer.healthy) {
        current_loc.alt = (1 - g.altitude_mix) * g_gps->altitude;                       // alt_MSL centimeters (meters * 100)
        current_loc.alt += g.altitude_mix * (read_barometer() + home.alt);
    } else if (g_gps->status() >= GPS::GPS_OK_FIX_3D) {
        current_loc.alt = g_gps->altitude;     // alt_MSL centimeters (meters * 100)
    }

    geofence_check(true);

    // Calculate new climb rate
    //if(medium_loopCounter == 0 && slow_loopCounter == 0)
    //	add_altitude_data(millis() / 100, g_gps->altitude / 10);
}

AP_HAL_MAIN();
