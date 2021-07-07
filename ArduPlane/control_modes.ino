/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-


static void read_control_switch()
{
    static bool switch_debouncer;
    uint8_t switchPosition = readSwitch();

    // If switchPosition = 255 this indicates that the mode control channel input was out of range
    // If we get this value we do not want to change modes.
    if(switchPosition == 255) return;

    if (ch3_failsafe) {
        // when we are in ch3_failsafe mode then RC input is not
        // working, and we need to ignore the mode switch channel
        return;
    }

    // we look for changes in the switch position. If the
    // RST_SWITCH_CH parameter is set, then it is a switch that can be
    // used to force re-reading of the control switch. This is useful
    // when returning to the previous mode after a failsafe or fence
    // breach. This channel is best used on a momentary switch (such
    // as a spring loaded trainer switch).
    if (oldSwitchPosition != switchPosition ||
        (g.reset_switch_chan != 0 &&
         hal.rcin->read(g.reset_switch_chan-1) > RESET_SWITCH_CHAN_PWM)) {

        if (switch_debouncer == false) {
            // this ensures that mode switches only happen if the
            // switch changes for 2 reads. This prevents momentary
            // spikes in the mode control channel from causing a mode
            // switch
            switch_debouncer = true;
            return;
        }

        set_mode((enum FlightMode)(flight_modes[switchPosition].get()));

        oldSwitchPosition = switchPosition;
        prev_WP = current_loc;
    }

    if (g.reset_mission_chan != 0 &&
        hal.rcin->read(g.reset_mission_chan-1) > RESET_SWITCH_CHAN_PWM) {
        // reset to first waypoint in mission
        prev_WP = current_loc;
        change_command(0);
    }

    switch_debouncer = false;

    if (g.inverted_flight_ch != 0) {
        // if the user has configured an inverted flight channel, then
        // fly upside down when that channel goes above INVERTED_FLIGHT_PWM
        inverted_flight = (control_mode != MANUAL && hal.rcin->read(g.inverted_flight_ch-1) > INVERTED_FLIGHT_PWM);
    }
}

static uint8_t readSwitch(void){
    uint16_t pulsewidth = hal.rcin->read(g.flight_mode_channel - 1);
    if (pulsewidth <= 910 || pulsewidth >= 2090) return 255;            // This is an error condition
    if (pulsewidth > 1230 && pulsewidth <= 1360) return 1;
    if (pulsewidth > 1360 && pulsewidth <= 1490) return 2;
    if (pulsewidth > 1490 && pulsewidth <= 1620) return 3;
    if (pulsewidth > 1620 && pulsewidth <= 1749) return 4;              // Software Manual
    if (pulsewidth >= 1750) return 5;                                                           // Hardware Manual
    return 0;
}

static void reset_control_switch()
{
    oldSwitchPosition = 0;
    read_control_switch();
}

#define CH_7_PWM_TRIGGER 1800

// read at 10 hz
// set this to your trainer switch
static void read_trim_switch()   // JLN
{
if (g.ch7_option == CH7_SAVE_WP){         // set to 1
		if (g.rc_7.radio_in > CH_7_PWM_TRIGGER){ // switch is engaged
			trim_flag = true;

		}else{ // switch is disengaged
			if(trim_flag){
				trim_flag = false;

				if(control_mode == MANUAL){          // if SW7 is ON in MANUAL = Erase the Flight Plan
					cleanup_fpl();                   
					return;
				} else if (control_mode == STABILIZE) {    // if SW7 is ON in STABILIZE = record the Wp                                 
        			        store_newwp();
                                } else if (control_mode == AUTO) {    // if SW7 is ON in AUTO = set to RTL  
                                    set_mode(RTL);
                                }
                             }
			}
		} 
}

static void cleanup_fpl()   // JLN ThermoPilot
{					// reset the mission
					CH7_wp_index = 0;
					g.command_total.set_and_save(CH7_wp_index);
                                        g.command_total = 0;
                                        g.command_index =0;
                                        nav_command_index = 0;
                                        if(g.channel_roll.control_in > 3000) // if roll is full right store the current location as home
                                            ground_start_count = 5;                                        
                                        CH7_wp_index = 1;        
}

static void store_newwp() // JLN ThermoPilot
{        			   // set the next_WP (home is stored at 0)
        			   // max out at 100 since I think we need to stay under the EEPROM limit
        			   CH7_wp_index = constrain_int16(CH7_wp_index, 1, 100);
        
    				  current_loc.id = MAV_CMD_NAV_WAYPOINT;  
    
                                  // store the index
                                  g.command_total.set_and_save(CH7_wp_index);
                                  g.command_total = CH7_wp_index;
                                  g.command_index = CH7_wp_index;
                                  nav_command_index = 0;
                                  
        			  // save command
        			  set_cmd_with_index(current_loc, CH7_wp_index);

                                    // increment index
    				   CH7_wp_index++; 
}
