/*

  ******************************************************************************
  * @file 			( file ):   analog_scale.h
  * @brief 		( description ):  	
  ******************************************************************************
  * @attention 	( attention ):  	author: Golinskiy Konstantin	e-mail: golinskiy.konstantin@gmail.com
  ******************************************************************************
  
  to use it in the project, include #include "analog_scale.h"
  
  ///////////  draw the dial as a circle   //////////////////////////
	
	 analogMeter(); // draw the dial interface (once at startup)
	 
	 HAL_Delay (1500);
	 
	 // call plotNeedle();
	 // first parameter is the actual value from 0 to 100
	 // second parameter is the delay (you can set it to 0 and handle the delay separately)
	 for( int i = 0; i<100; i++){
		 plotNeedle(i, 0);
		 HAL_Delay (10);
	 }
	 for( int i = 100; i>=0; i--){
		 plotNeedle(i, 10);
		 //HAL_Delay (10);
	 }
	 
	//////////////////////////////////////////////////////////////////////////
	
	
		////  draw elongated indicators from 1 to 6 //////////////////////////////////////////////////////////////////////////////
		
	 uint8_t d = 40;	// offset of each new dial by 40 pixels on the x axis
	 plotLinear("A1", d*0, 10 );	// create and draw the dial once at startup with the name "A1" and shift it by d*0 on x and 10 on y
	 plotLinear("A2", d*1, 10 );	// create and draw the dial once at startup with the name "A2" and shift it by d*1 on x and 10 on y
	 plotLinear("A3", d*2, 10 );	// create and draw the dial once at startup with the name "A3" and shift it by d*2 on x and 10 on y
	 plotLinear("A4", d*3, 10 );	// create and draw the dial once at startup with the name "A4" and shift it by d*3 on x and 10 on y
	 plotLinear("A5", d*4, 10 );	// create and draw the dial once at startup with the name "A5" and shift it by d*4 on x and 10 on y
	 plotLinear("A6", d*5, 10 );	// create and draw the dial once at startup with the name "A6" and shift it by d*5 on x and 10 on y
	 // also, when shifting by x or y (for the first value), set the offset and the number of dials in the function void plotPointer(void)
			 //int x = 0;	// set the offset of the starting indicator relative to x
			 //int y = 10; // set the offset of the starting indicator relative to y
			 //int count = 6; // set the number of indicators from 1 to 6
			 // then fill the value[] array with data and display it on the screen via plotPointer();
			 
	 for( int i = 0; i<101; i++){
		 value[0] = i;
		 value[1] = 100-i;
		 value[2] = i;
		 value[3] = 100-i;
		 value[4] = i;
		 value[5] = 100-i;
		 plotPointer();
		 HAL_Delay (10);
	 }
	  
	HAL_Delay (1500);
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	 
	
	
*/

#ifndef _ANALOG_SCALE_H
#define _ANALOG_SCALE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ----------------------------------------------------------*/

// A #include "main.h" is required
// to avoid separately including MCU-related and standard library files
#include "main.h"

extern int value[6];

// #########################################################################
// Update the needle and value on the dial (call this function with a delay of at least 10 ms)
// the first parameter is our value (from -10 to 110 gauge units)
// to keep it within the dial scale range, pass from 0 to 100
// the second parameter is the time until the next update; recommended to set to 0 and call the function at least every 10 ms
// #########################################################################
void plotNeedle(int8_t value, uint8_t ms_delay);

// #########################################################################
// draw the dial itself (call this function once at startup)
// #########################################################################
void analogMeter(void);

// #########################################################################
// draw a vertical scale (value range from 0 to 100)
// first parameter is the scale name (for example, "A1")
// second parameter is the x coordinate (scale width is 40, so the next scale is drawn at x + 40)
// third parameter is the y coordinate
// #########################################################################
void plotLinear(char *label, int x, int y);

// #########################################################################
// this function reads the data array and displays it on the screen
// #########################################################################
void plotPointer(void);

//------------------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif

#endif	/*	_ANALOG_SCALE_H */

/************************ (C) COPYRIGHT GKP *****END OF FILE****/
