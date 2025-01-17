#include "ch.h"
#include "hal.h"
#include <math.h>
#include <usbcfg.h>
#include <chprintf.h>

#include <main.h>
#include <sensors/proximity.h>
#include <sensors/VL53L0X/VL53L0X.h>

#include <detect_proximity.h>

static thread_t *tp;

static proximity_msg_t prox_values;
static int wall_angle = 0;
static bool wall_detected = 0;

bool return_wall_detected(void);
void approximate_wall_angle(void);


static THD_WORKING_AREA(waDetectProximity, 256);
static THD_FUNCTION(DetectProximity, arg) {

    chRegSetThreadName(__FUNCTION__);
    (void)arg;


    while(!chThdShouldTerminateX()){

    	messagebus_topic_t *prox_topic = messagebus_find_topic_blocking(&bus, "/proximity");
    	messagebus_topic_wait(prox_topic, &prox_values, sizeof(prox_values));

    	approximate_wall_angle();

    	chThdSleepMilliseconds(DETECT_PROXIMITY_WAIT_TIME);
    }
}



void detect_proximity_start(void){		//starts the detect proximity thread
	VL53L0X_start();
	tp = chThdCreateStatic(waDetectProximity, sizeof(waDetectProximity), NORMALPRIO+1, DetectProximity, NULL);

}

void detect_proximity_stop(void){		//stops the detect proximity thread
	chThdTerminate(tp);
	VL53L0X_stop();
}

void approximate_wall_angle(void){		//determines the approximate angle at which the wall is
	bool sensor_see_wall[8];
	uint8_t closest_sensor = 0;
	uint16_t closest_sensor_delta = 0;

	for (uint8_t i = 0; i <= 7; ++i){
		sensor_see_wall[i] = (prox_values.delta[i] > PROXIMITY_COLISION_THRESHOLD); 	//wall distance detection (big threshold = close wall)
	}

	wall_detected = sensor_see_wall[0] || sensor_see_wall[1] || sensor_see_wall[2] ||	//set if a wall is detected by any of the sensors
			sensor_see_wall[3] || sensor_see_wall[4] || sensor_see_wall[5] ||
			sensor_see_wall[6] || sensor_see_wall[7];


	for (uint8_t i = 0; i <= 7; ++i){					//determines which is the closest sensor to the wall
		if(sensor_see_wall[i]){
			if(prox_values.delta[i] > closest_sensor_delta){
				closest_sensor_delta = prox_values.delta[i];
				closest_sensor = i;
			}
		}
	}

	if((prox_values.delta[0] < prox_values.delta[7] + PROXIMITY_FRONT_DELTA)			//case in which both front sensors see the same intensity -> wall in front
			&& (prox_values.delta[0] > prox_values.delta[7] - PROXIMITY_FRONT_DELTA)
			&& (prox_values.delta[0] > PROXIMITY_FRONT_THRESHOLD)){
		wall_angle = 0; //angle in the front (so 0 deg)
		return;
	}
	switch (closest_sensor) {
		case 0: wall_angle = PROXIMITY_ANGLE_1;	break;
		case 1:	wall_angle = PROXIMITY_ANGLE_2;	break;
		case 2:	wall_angle = PROXIMITY_ANGLE_3;	break;
		case 3:	wall_angle = PROXIMITY_ANGLE_4;	break;
		case 4:	wall_angle = PROXIMITY_ANGLE_5;	break;
		case 5:	wall_angle = PROXIMITY_ANGLE_6;	break;
		case 6:	wall_angle = PROXIMITY_ANGLE_7;	break;
		case 7:	wall_angle = PROXIMITY_ANGLE_8;	break;
	}

	return;
}


int return_wall_angle(void){			//returns the angle at which is the wall
	return wall_angle;
}

bool return_wall_detected(void){		//returns true if the robot detects a wall
	return wall_detected;
}

int prox_value_delta(uint8_t sensor){	//returns the intensity measured by a certain sensor
	return prox_values.delta[sensor];
}

float proximity_dist_black(unsigned int value){			//non linear relation between sensor values and distance,
	if(value >= PROX_THRESHOLD){						//this function is calibrated for when there is a black line in front of the robot
		return PROX_SLOP_BIG * value + PROX_OFFSET_BIG;	//approximation for close distances
	}
	return PROX_SLOP_SMALL * value + PROX_OFFSET_SMALL;	//approximation for far distances
}

float return_dist_prox(void){													//Returns the distance of the closest front sensor from the wall
	float dist_proximity1 = proximity_dist_black(prox_values.delta[0]);
	float dist_proximity8 = proximity_dist_black(prox_values.delta[7]);

	if(dist_proximity1 < dist_proximity8) return dist_proximity1;
	return dist_proximity8;
}

float distance_value(void){									//The TOF sensor is not linear and not precise for close distances (below 35mm)
															//this function returns the distance from a wall using TOF if d>35mm and proximity if d<35mm
	float dist_tof = TOF_CORRECTION(VL53L0X_get_dist_mm());

	if(dist_tof < TOF_LIMIT_DIST){
		return return_dist_prox();
	}
	return dist_tof;

}

bool compare_front_prox(void){					//returns true if the sensors at 45� from the front are closer to a wall than the sensors at 10�
	if((prox_values.delta[6] > PROX_THRESHOLD && prox_values.delta[7] > PROX_THRESHOLD)
			|| (prox_values.delta[0] > PROX_THRESHOLD && prox_values.delta[1] > PROX_THRESHOLD)){
		if((prox_values.delta[6] >= prox_values.delta[7]) || (prox_values.delta[1] >= prox_values.delta[0])) return true;
	}
	return false;
}




