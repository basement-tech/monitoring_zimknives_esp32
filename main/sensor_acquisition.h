/*
 * sensor_acquisition.h
 * confluence of all api's for access to sensor acquisition and data
 * 
 * sensor structure is exposed for access to data
 * 
 */

#ifndef __SENSOR_ACQUISITION_H__

#include "esp_system.h"  // for types (at least)

/*
 * sensor api's must provide a simple function (acq_fcn) that acquires their
 * data value and places it in the pointer provided as an argument.
 * acq_fcn must return a simple 1 for success, 0 for failure.
 * (NOTE: case the data pointer during use in the function)
 */
typedef int (*acquisition_function_t)(void *data);

/*
 * provide a confluence of sensor api's:
 *
 * acq_fcn provides general way to cause acquisition of data and void *data,
 * a place to store results for subsequent use (case when using)
 * 
 */
typedef struct {
  acquisition_function_t  acq_fcn;    // pointer to function to cause data acquisition
  void *data;     // pointer to actual data value
  uint8_t  data_type; // type of data
  char *label;    // human readable label
  char *topic;    // topic for mqtt publish
  bool slow_acq;  // whether to acquire on the slow loop
  bool publish;   // whether to publish this sensors result
  bool display;   // whether to display for actions that care
  bool valid;     // set true if data acquisition is successful
} sensor_data_t;

/*
 * types of sensor data
 */
#define PARM_UND   -1  /* undefined */
#define PARM_INT    0
#define PARM_FLOAT  1
#define PARM_BOOL   2
#define PARM_STRING 3

extern sensor_data_t sensors[];  // sensor acq and data structure

/*
 * public functions
 */
void sensor_init_slow(void);
void acquire_sensors(void);
void display_sensors(void);

#define __SENSOR_ACQUISITION_H__
#endif