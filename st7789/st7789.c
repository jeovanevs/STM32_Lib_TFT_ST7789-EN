/*

  ******************************************************************************
  * @file 			( file ):   ST7789.c
  * @brief 		( description ):  	
  ******************************************************************************
  * @attention 	( attention ):	 author: Golinskiy Konstantin	e-mail: golinskiy.konstantin@gmail.com
  ******************************************************************************
  
*/

#include <ST7789.h>


uint16_t ST7789_X_Start = ST7789_XSTART;	
uint16_t ST7789_Y_Start = ST7789_YSTART;

uint16_t ST7789_Width = 0;
uint16_t ST7789_Height = 0;

#if FRAME_BUFFER
// frame buffer array
	uint16_t buff_frame[ST7789_WIDTH*ST7789_HEIGHT] = { 0x0000, };
#endif

static void ST7789_ExecuteCommandList(const uint8_t *addr);
static void ST7789_Unselect(void);
static void ST7789_Select(void);
static void ST7789_SendCmd(uint8_t Cmd);
static void ST7789_SendData(uint8_t Data );
static void ST7789_SendDataMASS(uint8_t* buff, size_t buff_size);
static void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static void ST7789_RamWrite(uint16_t *pBuff, uint32_t Len);
static void ST7789_ColumnSet(uint16_t ColumnStart, uint16_t ColumnEnd);
static void ST7789_RowSet(uint16_t RowStart, uint16_t RowEnd);
static void SwapInt16Values(int16_t *pValue1, int16_t *pValue2);
static void ST7789_DrawLine_Slow(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);


//==== initialization data for the ST7789_240X320 display ==========

// the initialization is the same for all displays because the driver is designed for a maximum size of 240x320
// to adjust it to your size, modify the display size in the rotation function
static const uint8_t init_cmds[] = {
		9,                       			// 9 commands in list:
		ST7789_SWRESET,   DELAY,    		// 1: Software reset, no args, w/delay
		  150,                     			//    150 ms delay
		ST7789_SLPOUT ,  DELAY,    			// 2: Out of sleep mode, no args, w/delay
		  255,                            	//    255 = 500 ms delay
		ST7789_COLMOD , 1+DELAY,    		// 3: Set color mode, 1 arg + delay:
		  (ST7789_ColorMode_65K | ST7789_ColorMode_16bit),           //    16-bit color 0x55
		  10,                             	//    10 ms delay
		ST7789_MADCTL , 1,                 	// 4: Memory access ctrl (directions), 1 arg:
		  ST7789_ROTATION,                  //    Row addr/col addr, bottom to top refresh
		ST7789_CASET  , 4,                 	// 5: Column addr set, 4 args, no delay:
		  ST7789_XSTART>>8,ST7789_XSTART&0xff,  //    XSTART = 0>>8, 0&0xff,
		  (ST7789_WIDTH-1)>>8,(ST7789_WIDTH-1)&0xff,    //    XEND = (320-1)>>8,(320-1)&0xff,
		ST7789_RASET  , 4,                 	// 6: Row addr set, 4 args, no delay:
		  ST7789_YSTART>>8,ST7789_YSTART&0xff,  //    YSTART = 0>>8, 0&0xff,
		  (ST7789_HEIGHT-1)>>8,(ST7789_HEIGHT-1)&0xff,  //    YEND = (320-1)>>8,(320-1)&0xff,
		ST7789_INVERSION ,   DELAY,     		// 7: Inversion ON
		  10,
		ST7789_NORON  ,   DELAY,    		// 8: Normal display on, no args, w/delay
		  10,                              	// 10 ms delay
		ST7789_DISPON ,   DELAY,    		// 9: Main screen turn on, no args, w/delay
		  10 
	};
	//---------------------------------------------------------------------------------------------
	
//===============================================================
	
	
//##############################################################################
	  
	  
//==============================================================================
	  
	  
	  
//==============================================================================
// Display initialization procedure
//==============================================================================
void ST7789_Init(void){
	
	// Power-up delay
	// if the display does not always start reliably, increase this delay
	HAL_Delay(200);	
	
	ST7789_Width = ST7789_WIDTH;
	ST7789_Height = ST7789_HEIGHT;
	
  ST7789_Select();

  ST7789_HardReset(); 
  ST7789_ExecuteCommandList(init_cmds);
	
  ST7789_Unselect();
	
#if FRAME_BUFFER
	ST7789_ClearFrameBuffer();
#endif

}
//==============================================================================


//==============================================================================
// SPI control procedure
//==============================================================================
static void ST7789_Select(void) {
	
    #ifdef CS_PORT
	
			//-- if we want to switch to HAL ------------------	
			#ifdef ST7789_SPI_HAL
				HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
			#endif
			//-----------------------------------------------------
			
			//-- if we want to switch to CMSIS  ---------------
			#ifdef ST7789_SPI_CMSIS
				CS_GPIO_Port->BSRR = ( CS_Pin << 16 );
			#endif
			//-----------------------------------------------------
	#endif
	
}
//==============================================================================


//==============================================================================
// SPI control procedure
//==============================================================================
static void ST7789_Unselect(void) {
	
    #ifdef CS_PORT
	
			//-- if we want to switch to HAL ------------------	
			#ifdef ST7789_SPI_HAL
				HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
			#endif
			//-----------------------------------------------------
			
			//-- if we want to switch to CMSIS  ---------------
			#ifdef ST7789_SPI_CMSIS
					 CS_GPIO_Port->BSRR = CS_Pin;
			#endif
			//-----------------------------------------------------
	
	#endif
	
}
//==============================================================================


//==============================================================================
// Procedure for sending initialization data to the display
//==============================================================================
static void ST7789_ExecuteCommandList(const uint8_t *addr) {
	
    uint8_t numCommands, numArgs;
    uint16_t ms;

    numCommands = *addr++;
    while(numCommands--) {
        uint8_t cmd = *addr++;
        ST7789_SendCmd(cmd);

        numArgs = *addr++;
        // If high bit set, delay follows args
        ms = numArgs & DELAY;
        numArgs &= ~DELAY;
        if(numArgs) {
            ST7789_SendDataMASS((uint8_t*)addr, numArgs);
            addr += numArgs;
        }

        if(ms) {
            ms = *addr++;
            if(ms == 255) ms = 500;
            HAL_Delay(ms);
        }
    }
}
//==============================================================================


//==============================================================================
// Procedure for displaying a color image on the screen
//==============================================================================
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
	
    if((x >= ST7789_Width) || (y >= ST7789_Height)){
		return;
	}
	
    if((x + w - 1) >= ST7789_Width){
		return;
	}
	
    if((y + h - 1) >= ST7789_Height){
		return;
	}
	
#if FRAME_BUFFER	// if frame buffering is enabled
		for( uint16_t i = 0; i < h; i++ ){
			for( uint16_t j = 0; j < w; j++ ){
				buff_frame[( y + i ) * ST7789_Width + x + j] = *data;
				data++;
			}
		}
#else	// if pixel-by-pixel output
    ST7789_SetWindow(x, y, x+w-1, y+h-1);
	
		ST7789_Select();
	
    ST7789_SendDataMASS((uint8_t*)data, sizeof(uint16_t)*w*h);
	
    ST7789_Unselect();
#endif
}
//==============================================================================


//==============================================================================
// Hardware reset procedure for the display (using the RESET pin)
//==============================================================================
void ST7789_HardReset(void){

	HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(20);	
	HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);
	
}
//==============================================================================


//==============================================================================
// Procedure for sending a command to the display
//==============================================================================
__inline static void ST7789_SendCmd(uint8_t Cmd){	
		
	//-- if we want to switch to HAL ------------------	
	#ifdef ST7789_SPI_HAL
	
		 // pin DC LOW
		 HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET);
					 
		 HAL_SPI_Transmit(&ST7789_SPI_HAL, &Cmd, 1, HAL_MAX_DELAY);
		 while(HAL_SPI_GetState(&ST7789_SPI_HAL) != HAL_SPI_STATE_READY){};
				
		 // pin DC HIGH
		 HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_SET);
		 
	#endif
	//-----------------------------------------------------
	
	//-- if we want to switch to CMSIS  ---------------------------------------------
	#ifdef ST7789_SPI_CMSIS
		
		// pin DC LOW
		DC_GPIO_Port->BSRR = ( DC_Pin << 16 );
	
		//======  FOR F-SERIES ===========================================================
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 &= ~SPI_CR1_SPE;
			// Enable SPI
			if((ST7789_SPI_CMSIS->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE){
				// If disabled, I enable it
				SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_SPE;
			}
			
			// Wait until the transmit buffer is free
			// TXE(Transmit buffer empty) – is set when the transmit buffer (SPI_DR register) is empty and cleared when data is loaded
			while( (ST7789_SPI_CMSIS->SR & SPI_SR_TXE) == RESET ){};	
			
			// fill the transmit buffer with 1 byte of data --------------
			*((__IO uint8_t *)&ST7789_SPI_CMSIS->DR) = Cmd;
			
			// TXE(Transmit buffer empty) – is set when the transmit buffer (SPI_DR register) is empty and cleared when data is loaded
			while( (ST7789_SPI_CMSIS->SR & (SPI_SR_TXE | SPI_SR_BSY)) != SPI_SR_TXE ){};
				
			// Wait until the SPI is free from the previous transfer
			//while((ST7789_SPI_CMSIS->SR&SPI_SR_BSY)){};	

			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);
			
		//================================================================================
		
/*		//======  FOR H-SERIES ===========================================================

			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 &= ~SPI_CR1_SPE;
			// Enable SPI
			// Enable SPI
			if((ST7789_SPI_CMSIS->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE){
				// If disabled, I enable it
				SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_SPE;
			}
			
			SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_CSTART);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_CSTART;
			
			// wait until the SPI is free ------------
			//while (!(ST7789_SPI_CMSIS->SR & SPI_SR_TXP)){};		
		
			// send 1 byte of data --------------
			*((__IO uint8_t *)&ST7789_SPI_CMSIS->TXDR )  = Cmd;
				
			// Wait for transmission to finish ---------------
			while (!( ST7789_SPI_CMSIS -> SR & SPI_SR_TXC )){};
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);
			
*/		//================================================================================
		
		// pin DC HIGH
		DC_GPIO_Port->BSRR = DC_Pin;
	
	#endif
	//-----------------------------------------------------------------------------------

}
//==============================================================================


//==============================================================================
// Procedure for sending a display parameter (1 byte)
//==============================================================================
__inline static void ST7789_SendData(uint8_t Data ){
	
	//-- if we want to switch to HAL ------------------
	#ifdef ST7789_SPI_HAL
	
		HAL_SPI_Transmit(&ST7789_SPI_HAL, &Data, 1, HAL_MAX_DELAY);
		while(HAL_SPI_GetState(&ST7789_SPI_HAL) != HAL_SPI_STATE_READY){};
		
	#endif
	//-----------------------------------------------------
	
	
	//-- if we want to switch to CMSIS  ---------------------------------------------
	#ifdef ST7789_SPI_CMSIS
		
		//======  FOR F-SERIES ===========================================================
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 &= ~SPI_CR1_SPE;
			// Enable SPI
			if((ST7789_SPI_CMSIS->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE){
				// If disabled, I enable it
				SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_SPE;
			}

			// Wait until the transmit buffer is free
			// TXE(Transmit buffer empty) – is set when the transmit buffer (SPI_DR register) is empty and cleared when data is loaded
			while( (ST7789_SPI_CMSIS->SR & SPI_SR_TXE) == RESET ){};
		
			// send 1 byte of data --------------
			*((__IO uint8_t *)&ST7789_SPI_CMSIS->DR) = Data;

			// TXE(Transmit buffer empty) – is set when the transmit buffer (SPI_DR register) is empty and cleared when data is loaded
			while( (ST7789_SPI_CMSIS->SR & (SPI_SR_TXE | SPI_SR_BSY)) != SPI_SR_TXE ){};

			// Wait until the transmit buffer is free
			//while((ST7789_SPI_CMSIS->SR&SPI_SR_BSY)){};	
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);
			
		//================================================================================
		
/*		//======  FOR H-SERIES ===========================================================

			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 &= ~SPI_CR1_SPE;
			// Enable SPI
			if((ST7789_SPI_CMSIS->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE){
				// If disabled, I enable it
				SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_SPE;
			}

			SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_CSTART);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_CSTART;
			
			// wait until the SPI is free ------------
			//while (!(ST7789_SPI_CMSIS->SR & SPI_SR_TXP)){};		
		
			// send 1 byte of data --------------
			*((__IO uint8_t *)&ST7789_SPI_CMSIS->TXDR )  = Data;
				
			// Wait for transmission to finish ---------------
			while (!( ST7789_SPI_CMSIS -> SR & SPI_SR_TXC )){};
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);
			
*/		//================================================================================
		
	#endif
	//-----------------------------------------------------------------------------------

}
//==============================================================================


//==============================================================================
// Procedure for sending display parameters in bulk (MASS)
//==============================================================================
__inline static void ST7789_SendDataMASS(uint8_t* buff, size_t buff_size){
	
	//-- if we want to switch to HAL ------------------
	#ifdef ST7789_SPI_HAL
		
		if( buff_size <= 0xFFFF ){
			HAL_SPI_Transmit(&ST7789_SPI_HAL, buff, buff_size, HAL_MAX_DELAY);
		}
		else{
			while( buff_size > 0xFFFF ){
				HAL_SPI_Transmit(&ST7789_SPI_HAL, buff, 0xFFFF, HAL_MAX_DELAY);
				buff_size-=0xFFFF;
				buff+=0xFFFF;
			}
			HAL_SPI_Transmit(&ST7789_SPI_HAL, buff, buff_size, HAL_MAX_DELAY);
		}
		
		while(HAL_SPI_GetState(&ST7789_SPI_HAL) != HAL_SPI_STATE_READY){};

	#endif
	//-----------------------------------------------------
	
	
	//-- if we want to switch to CMSIS  ---------------------------------------------
	#ifdef ST7789_SPI_CMSIS	

		//======  FOR F-SERIES ===========================================================
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 &= ~SPI_CR1_SPE;
			// Enable SPI
			if((ST7789_SPI_CMSIS->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE){
				// If disabled, I enable it
				SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_SPE;
			}
			
			while( buff_size ){
				
			// Wait until the transmit buffer is free
			// TXE(Transmit buffer empty) – is set when the transmit buffer (SPI_DR register) is empty and cleared when data is loaded
			while( (ST7789_SPI_CMSIS->SR & SPI_SR_TXE) == RESET ){};
					
				// send 1 byte of data --------------
				*((__IO uint8_t *)&ST7789_SPI_CMSIS->DR) = *buff++;

				buff_size--;
			}
			
			// TXE(Transmit buffer empty) – is set when the transmit buffer (SPI_DR register) is empty and cleared when data is loaded
			while( (ST7789_SPI_CMSIS->SR & (SPI_SR_TXE | SPI_SR_BSY)) != SPI_SR_TXE ){};
				
			// Wait until the transmit buffer is free
			// while((ST7789_SPI_CMSIS->SR&SPI_SR_BSY)){};
				
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);
			
		//================================================================================
		
/*		//======  FOR H-SERIES ===========================================================

			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 &= ~SPI_CR1_SPE;
			// Enable SPI
			if((ST7789_SPI_CMSIS->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE){
				// If disabled, I enable it
				SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_SPE;
			}

			SET_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_CSTART);	// ST7789_SPI_CMSIS->CR1 |= SPI_CR1_CSTART;
			
			// wait until the SPI is free ------------
			//while (!(ST7789_SPI_CMSIS->SR & SPI_SR_TXP)){};		
			
			while( buff_size ){
		
				// send 1 byte of data --------------
				*((__IO uint8_t *)&ST7789_SPI_CMSIS->TXDR )  = *buff++;
				
				// Wait for transmission to finish ---------------
				while (!( ST7789_SPI_CMSIS -> SR & SPI_SR_TXC )){};

				buff_size--;

			}
			
			// Disable SPI	
			//CLEAR_BIT(ST7789_SPI_CMSIS->CR1, SPI_CR1_SPE);
			
*/		//================================================================================
		
	#endif
	//-----------------------------------------------------------------------------------

}
//==============================================================================


//==============================================================================
// Procedure to enter sleep mode
//==============================================================================
void ST7789_SleepModeEnter( void ){
	
	ST7789_Select(); 
	
	ST7789_SendCmd(ST7789_SLPIN);
	
	ST7789_Unselect();
	
	HAL_Delay(250);
}
//==============================================================================


//==============================================================================
// Procedure to exit sleep mode
//==============================================================================
void ST7789_SleepModeExit( void ){
	
	ST7789_Select(); 
	
	ST7789_SendCmd(ST7789_SLPOUT);
	
	ST7789_Unselect();
	
	HAL_Delay(250);
}
//==============================================================================


//==============================================================================
// Procedure to enable/disable partial-screen inversion mode
//==============================================================================
void ST7789_InversionMode(uint8_t Mode){
	
  ST7789_Select(); 
	
  if (Mode){
    ST7789_SendCmd(ST7789_INVON);
  }
  else{
    ST7789_SendCmd(ST7789_INVOFF);
  }
  
  ST7789_Unselect();
}
//==============================================================================


//==============================================================================
// Fills the screen with the given color
//==============================================================================
void ST7789_FillScreen(uint16_t color){
	
  ST7789_FillRect(0, 0,  ST7789_Width, ST7789_Height, color);
}
//==============================================================================


//==============================================================================
// Screen clear procedure - fills the screen with black
//==============================================================================
void ST7789_Clear(void){
	
  ST7789_FillRect(0, 0,  ST7789_Width, ST7789_Height, 0);
}
//==============================================================================


//==============================================================================
// Fills a rectangle with the specified color
//==============================================================================
void ST7789_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
	
  if ((x >= ST7789_Width) || (y >= ST7789_Height)){
	  return;
  }
  
  if ((x + w) > ST7789_Width){	  
	  w = ST7789_Width - x;
  }
  
  if ((y + h) > ST7789_Height){
	  h = ST7789_Height - y;
  }
  
#if FRAME_BUFFER	// if the frame buffer is enabled
	if( x >=0 && y >=0 ){
		for( uint16_t i = 0; i < h; i++ ){
			for( uint16_t j = 0; j < w; j++ ){
				buff_frame[( y + i ) * ST7789_Width + x + j] = ((color & 0xFF)<<8) | (color >> 8 );
			}
		}
	}
#else	// if per-pixel output is enabled
  ST7789_SetWindow(x, y, x + w - 1, y + h - 1);
		
  ST7789_RamWrite(&color, (h * w));
#endif	
}
//==============================================================================


//==============================================================================
// Sets the screen boundaries for filling
//==============================================================================
static void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
	
	ST7789_Select();
	
	ST7789_ColumnSet(x0, x1);
	ST7789_RowSet(y0, y1);
	
	// write to RAM
	ST7789_SendCmd(ST7789_RAMWR);
	
	ST7789_Unselect();
	
}
//==============================================================================


//==============================================================================
// Writes data to the display
//==============================================================================
static void ST7789_RamWrite(uint16_t *pBuff, uint32_t Len){
	
  ST7789_Select();
	
  uint8_t buff[2];
  buff[0] = *pBuff >> 8;
  buff[1] = *pBuff & 0xFF;
	
  while (Len--){
	  ST7789_SendDataMASS( buff, 2);
  } 
	
  ST7789_Unselect();
}
//==============================================================================


//==============================================================================
// Procedure to set the start and end column addresses
//==============================================================================
static void ST7789_ColumnSet(uint16_t ColumnStart, uint16_t ColumnEnd){
	
  if (ColumnStart > ColumnEnd){
    return;
  }
  
  if (ColumnEnd > ST7789_Width){
    return;
  }
  
  ColumnStart += ST7789_X_Start;
  ColumnEnd += ST7789_X_Start;
  
  ST7789_SendCmd(ST7789_CASET);
  ST7789_SendData(ColumnStart >> 8);  
  ST7789_SendData(ColumnStart & 0xFF);  
  ST7789_SendData(ColumnEnd >> 8);  
  ST7789_SendData(ColumnEnd & 0xFF);  
  
}
//==============================================================================


//==============================================================================
// Procedure to set the start and end row addresses
//==============================================================================
static void ST7789_RowSet(uint16_t RowStart, uint16_t RowEnd){
	
  if (RowStart > RowEnd){
    return;
  }
  
  if (RowEnd > ST7789_Height){
    return;
  }
  
  RowStart += ST7789_Y_Start;
  RowEnd += ST7789_Y_Start;
 
  ST7789_SendCmd(ST7789_RASET);
  ST7789_SendData(RowStart >> 8);  
  ST7789_SendData(RowStart & 0xFF);  
  ST7789_SendData(RowEnd >> 8);  
  ST7789_SendData(RowEnd & 0xFF);  

}
//==============================================================================


//==============================================================================
// Backlight control procedure (PWM)
//==============================================================================
void ST7789_SetBL(uint8_t Value){
	
//  if (Value > 100)
//    Value = 100;

//	tmr2_PWM_set(ST77xx_PWM_TMR2_Chan, Value);

}
//==============================================================================


//==============================================================================
// Turn the display power on/off
//==============================================================================
void ST7789_DisplayPower(uint8_t On){
	
  ST7789_Select(); 
	
  if (On){
    ST7789_SendCmd(ST7789_DISPON);
  }
  else{
    ST7789_SendCmd(ST7789_DISPOFF);
  }
  
  ST7789_Unselect();
}
//==============================================================================


//==============================================================================
// Procedure for drawing a rectangle (outline)
//==============================================================================
void ST7789_DrawRectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
	
  ST7789_DrawLine(x1, y1, x1, y2, color);
  ST7789_DrawLine(x2, y1, x2, y2, color);
  ST7789_DrawLine(x1, y1, x2, y1, color);
  ST7789_DrawLine(x1, y2, x2, y2, color);
	
}
//==============================================================================


//==============================================================================
// Helper for --- Rectangle drawing procedure (filled)
//==============================================================================
static void SwapInt16Values(int16_t *pValue1, int16_t *pValue2){
	
  int16_t TempValue = *pValue1;
  *pValue1 = *pValue2;
  *pValue2 = TempValue;
}
//==============================================================================


//==============================================================================
// Procedure for drawing a rectangle (filled)
//==============================================================================
void ST7789_DrawRectangleFilled(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t fillcolor) {
	
  if (x1 > x2){
    SwapInt16Values(&x1, &x2);
  }
  
  if (y1 > y2){
    SwapInt16Values(&y1, &y2);
  }
  
  ST7789_FillRect(x1, y1, x2 - x1, y2 - y1, fillcolor);
}
//==============================================================================


//==============================================================================
// Helper for --- line drawing procedure
//==============================================================================
static void ST7789_DrawLine_Slow(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
	
  const int16_t deltaX = abs(x2 - x1);
  const int16_t deltaY = abs(y2 - y1);
  const int16_t signX = x1 < x2 ? 1 : -1;
  const int16_t signY = y1 < y2 ? 1 : -1;

  int16_t error = deltaX - deltaY;

  ST7789_DrawPixel(x2, y2, color);

  while (x1 != x2 || y1 != y2) {
	  
    ST7789_DrawPixel(x1, y1, color);
    const int16_t error2 = error * 2;
 
    if (error2 > -deltaY) {
		
      error -= deltaY;
      x1 += signX;
    }
    if (error2 < deltaX){
		
      error += deltaX;
      y1 += signY;
    }
  }
}
//==============================================================================


//==============================================================================
// Line drawing procedure
//==============================================================================
void ST7789_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {

  if (x1 == x2){

    if (y1 > y2){
      ST7789_FillRect(x1, y2, 1, y1 - y2 + 1, color);
	}
    else{
      ST7789_FillRect(x1, y1, 1, y2 - y1 + 1, color);
	}
	
    return;
  }
  
  if (y1 == y2){
    
    if (x1 > x2){
      ST7789_FillRect(x2, y1, x1 - x2 + 1, 1, color);
	}
    else{
      ST7789_FillRect(x1, y1, x2 - x1 + 1, 1, color);
	}
	
    return;
  }
  
  ST7789_DrawLine_Slow(x1, y1, x2, y2, color);
}
//==============================================================================


//==============================================================================
// Procedure for drawing a line at a specified angle and length
//==============================================================================
void ST7789_DrawLineWithAngle(int16_t x, int16_t y, uint16_t length, double angle_degrees, uint16_t color) {
    // Convert angle to radians
    double angle_radians = (360.0 - angle_degrees) * PI / 180.0;

    // Calculate end coordinates
    int16_t x2 = x + length * cos(angle_radians) + 0.5;
    int16_t y2 = y + length * sin(angle_radians) + 0.5;

    // Use the existing line drawing function
    ST7789_DrawLine(x, y, x2, y2, color);
}
//==============================================================================

//==============================================================================
// Procedure for drawing a triangle (outline)
//==============================================================================
void ST7789_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color){
	/* Draw lines */
	ST7789_DrawLine(x1, y1, x2, y2, color);
	ST7789_DrawLine(x2, y2, x3, y3, color);
	ST7789_DrawLine(x3, y3, x1, y1, color);
}
//==============================================================================


//==============================================================================
// Procedure for drawing a triangle (filled)
//==============================================================================
void ST7789_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color){
	
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, 
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, 
	curpixel = 0;
	
	deltax = abs(x2 - x1);
	deltay = abs(y2 - y1);
	x = x1;
	y = y1;

	if (x2 >= x1) {
		xinc1 = 1;
		xinc2 = 1;
	} 
	else {
		xinc1 = -1;
		xinc2 = -1;
	}

	if (y2 >= y1) {
		yinc1 = 1;
		yinc2 = 1;
	} 
	else {
		yinc1 = -1;
		yinc2 = -1;
	}

	if (deltax >= deltay){
		xinc1 = 0;
		yinc2 = 0;
		den = deltax;
		num = deltax / 2;
		numadd = deltay;
		numpixels = deltax;
	} 
	else {
		xinc2 = 0;
		yinc1 = 0;
		den = deltay;
		num = deltay / 2;
		numadd = deltax;
		numpixels = deltay;
	}

	for (curpixel = 0; curpixel <= numpixels; curpixel++) {
		ST7789_DrawLine(x, y, x3, y3, color);

		num += numadd;
		if (num >= den) {
			num -= den;
			x += xinc1;
			y += yinc1;
		}
		x += xinc2;
		y += yinc2;
	}
}
//==============================================================================


//==============================================================================
// Procedure for painting one display pixel
//==============================================================================
void ST7789_DrawPixel(int16_t x, int16_t y, uint16_t color){
	
  if ((x < 0) ||(x >= ST7789_Width) || (y < 0) || (y >= ST7789_Height)){
    return;
  }
	
#if FRAME_BUFFER	// if the frame buffer is enabled
	buff_frame[y * ST7789_Width + x] = ((color & 0xFF)<<8) | (color >> 8 );
#else	// if per-pixel output is enabled
  ST7789_SetWindow(x, y, x, y);
  ST7789_RamWrite(&color, 1);
#endif
}
//==============================================================================


//==============================================================================
// Procedure for drawing a circle (filled)
//==============================================================================
void ST7789_DrawCircleFilled(int16_t x0, int16_t y0, int16_t radius, uint16_t fillcolor) {
	
  int x = 0;
  int y = radius;
  int delta = 1 - 2 * radius;
  int error = 0;

  while (y >= 0){
	  
    ST7789_DrawLine(x0 + x, y0 - y, x0 + x, y0 + y, fillcolor);
    ST7789_DrawLine(x0 - x, y0 - y, x0 - x, y0 + y, fillcolor);
    error = 2 * (delta + y) - 1;

    if (delta < 0 && error <= 0) {
		
      ++x;
      delta += 2 * x + 1;
      continue;
    }
	
    error = 2 * (delta - x) - 1;
		
    if (delta > 0 && error > 0) {
		
      --y;
      delta += 1 - 2 * y;
      continue;
    }
	
    ++x;
    delta += 2 * (x - y);
    --y;
  }
}
//==============================================================================


//==============================================================================
// Procedure for drawing a circle (outline)
//==============================================================================
void ST7789_DrawCircle(int16_t x0, int16_t y0, int16_t radius, uint16_t color) {
	
  int x = 0;
  int y = radius;
  int delta = 1 - 2 * radius;
  int error = 0;

  while (y >= 0){
	  
    ST7789_DrawPixel(x0 + x, y0 + y, color);
    ST7789_DrawPixel(x0 + x, y0 - y, color);
    ST7789_DrawPixel(x0 - x, y0 + y, color);
    ST7789_DrawPixel(x0 - x, y0 - y, color);
    error = 2 * (delta + y) - 1;

    if (delta < 0 && error <= 0) {
		
      ++x;
      delta += 2 * x + 1;
      continue;
    }
	
    error = 2 * (delta - x) - 1;
		
    if (delta > 0 && error > 0) {
		
      --y;
      delta += 1 - 2 * y;
      continue;
    }
	
    ++x;
    delta += 2 * (x - y);
    --y;
  }
}
//==============================================================================


//==============================================================================
// draw an ellipse
//==============================================================================
void ST7789_DrawEllipse(int16_t x0, int16_t y0, int16_t radiusX, int16_t radiusY, uint16_t color) {
    int x, y;
    for (float angle = 0; angle <= 360; angle += 0.1) {
        x = x0 + radiusX * cos(angle * PI / 180);
        y = y0 + radiusY * sin(angle * PI / 180);
        ST7789_DrawPixel(x, y, color);
    }
}
//==============================================================================


//==============================================================================
// draw an ellipse at a specified rotation angle
//==============================================================================
void ST7789_DrawEllipseWithAngle(int16_t x0, int16_t y0, int16_t radiusX, int16_t radiusY, float angle_degrees, uint16_t color) {
    float cosAngle = cos((360.0 - angle_degrees) * PI / 180);
    float sinAngle = sin((360.0 - angle_degrees) * PI / 180);

    for (int16_t t = 0; t <= 360; t++) {
        float radians = t * PI / 180.0;
        int16_t x = radiusX * cos(radians);
        int16_t y = radiusY * sin(radians);

        int16_t xTransformed = x0 + cosAngle * x - sinAngle * y;
        int16_t yTransformed = y0 + sinAngle * x + cosAngle * y;

        ST7789_DrawPixel(xTransformed, yTransformed, color);
    }
}
//==============================================================================


//==============================================================================
// draw a filled ellipse
//==============================================================================
void ST7789_DrawEllipseFilled(int16_t x0, int16_t y0, int16_t radiusX, int16_t radiusY, uint16_t color) {
	int x, y;

	for (y = -radiusY; y <= radiusY; y++) {
			for (x = -radiusX; x <= radiusX; x++) {
					if ((x * x * radiusY * radiusY + y * y * radiusX * radiusX) <= (radiusX * radiusX * radiusY * radiusY)) {
							ST7789_DrawPixel(x0 + x, y0 + y, color);
					}
			}
	}
}
//==============================================================================


//==============================================================================
// draw a filled ellipse at a specified rotation angle
//==============================================================================
void ST7789_DrawEllipseFilledWithAngle(int16_t x0, int16_t y0, int16_t radiusX, int16_t radiusY, float angle_degrees, uint16_t color) {
   float cosAngle = cos((360.0 - angle_degrees) * PI / 180.0);
    float sinAngle = sin((360.0 - angle_degrees) * PI / 180.0);

    for (int16_t y = -radiusY; y <= radiusY; y++) {
        for (int16_t x = -radiusX; x <= radiusX; x++) {
          float xTransformed = cosAngle * x - sinAngle * y;
          float yTransformed = sinAngle * x + cosAngle * y;

					if ((x * x * radiusY * radiusY + y * y * radiusX * radiusX) <= (radiusX * radiusX * radiusY * radiusY)){
             ST7789_DrawPixel(x0 + xTransformed, y0  + yTransformed, color);
          }
        }
    }
}
//==============================================================================


//==============================================================================
// Procedure for drawing a character (one letter or symbol)
//==============================================================================
void ST7789_DrawChar(uint16_t x, uint16_t y, uint16_t TextColor, uint16_t BgColor, uint8_t TransparentBg, FontDef_t* Font, uint8_t multiplier, unsigned char ch){
	
	uint32_t i, b, j;
	
	uint32_t X = x, Y = y;
	
	uint8_t xx, yy;
	
	if( multiplier < 1 ){
		multiplier = 1;
	}

	/* Check available space in LCD */
	if (ST7789_Width >= ( x + Font->FontWidth) || ST7789_Height >= ( y + Font->FontHeight)){

	
			/* Go through font */
			for (i = 0; i < Font->FontHeight; i++) {		
				
				if( ch < 127 ){			
					b = Font->data[(ch - 32) * Font->FontHeight + i];
				}
				
				else if( (uint8_t) ch > 191 ){
					// +96 because Latin letters and symbols occupy 96 positions in the fonts
					// and if the font contains Latin letters and special characters first, 
					// followed by Cyrillic only, then you need to add 95; if the font 
					// contains only Cyrillic, then +96 is not needed
					b = Font->data[((ch - 192) + 96) * Font->FontHeight + i];
				}
				
				else if( (uint8_t) ch == 168 ){	// 168 symbol in ASCII - Yo
					// 160 element (symbol Yo) 
					b = Font->data[( 160 ) * Font->FontHeight + i];
				}
				
				else if( (uint8_t) ch == 184 ){	// 184 symbol in ASCII - yo
					// 161 element (symbol yo) 
					b = Font->data[( 161 ) * Font->FontHeight + i];
				}
				//-------------------------------------------------------------------
				
				//----  Ukrainian layout ----------------------------------------------------
				else if( (uint8_t) ch == 170 ){	// 168 symbol in ASCII - Ye
					// 162 element (symbol Ye)
					b = Font->data[( 162 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 175 ){	// 184 symbol in ASCII - Yi
					// 163 element (symbol Yi)
					b = Font->data[( 163 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 178 ){	// 168 symbol in ASCII - I
					// 164 element (symbol I)
					b = Font->data[( 164 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 179 ){	// 184 symbol in ASCII - i
					// 165 element (symbol i)
					b = Font->data[( 165 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 186 ){	// 184 symbol in ASCII - ye
					// 166 element (symbol ye)
					b = Font->data[( 166 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 191 ){	// 168 symbol in ASCII - yi
					// 167 element (symbol yi)
					b = Font->data[( 167 ) * Font->FontHeight + i];
				}
				//-----------------------------------------------------------------------------
			
				for (j = 0; j < Font->FontWidth; j++) {
					
					if ((b << j) & 0x8000) {
						
						for (yy = 0; yy < multiplier; yy++){
							for (xx = 0; xx < multiplier; xx++){
									ST7789_DrawPixel(X+xx, Y+yy, TextColor);
							}
						}
						
					} 
					else if( TransparentBg ){
						
						for (yy = 0; yy < multiplier; yy++){
							for (xx = 0; xx < multiplier; xx++){
									ST7789_DrawPixel(X+xx, Y+yy, BgColor);
							}
						}
						
					}
					X = X + multiplier;
				}
				X = x;
				Y = Y + multiplier;
			}
	}
}
//==============================================================================


//==============================================================================
// Procedure for drawing a string
//==============================================================================
void ST7789_print(uint16_t x, uint16_t y, uint16_t TextColor, uint16_t BgColor, uint8_t TransparentBg, FontDef_t* Font, uint8_t multiplier, char *str){	
	
	if( multiplier < 1 ){
		multiplier = 1;
	}
	
	unsigned char buff_char;
	
	uint16_t len = strlen(str);
	
	while (len--) {
		
		//---------------------------------------------------------------------
		// check for Cyrillic UTF-8; if Latin letters are used, skip this block
		// extended Win-1251 ASCII Cyrillic characters (code 128-255)
		// check the first byte out of two (since UTF-8 is two bytes)
		// if it is greater than or equal to 0xC0 (the first byte in Cyrillic will be 0xD0 or 0xD1 in the alphabet)
		if ( (uint8_t)*str >= 0xC0 ){	// code 0xC0 corresponds to the Cyrillic letter 'A' in Win-1251 ASCII
			
			// check which byte is first: 0xD0 or 0xD1---------------------------------------------
			switch ((uint8_t)*str) {
				case 0xD0: {
					// advance the pointer because we need the second byte
					str++;
					// check the second byte to get the actual character
					if ((uint8_t)*str >= 0x90 && (uint8_t)*str <= 0xBF){ buff_char = (*str) + 0x30; }	// byte of characters A...Ya...p shift by +48
					else if ((uint8_t)*str == 0x81) { buff_char = 0xA8; break; }		// byte of the Yo symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x84) { buff_char = 0xAA; break; }		// byte of the Ye symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x86) { buff_char = 0xB2; break; }		// byte of the I symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x87) { buff_char = 0xAF; break; }		// byte of the Yi symbol (add more symbols here if needed, and in DrawChar())
					break;
				}
				case 0xD1: {
					// advance the pointer because we need the second byte
					str++;
					// check the second byte to get the actual character
					if ((uint8_t)*str >= 0x80 && (uint8_t)*str <= 0x8F){ buff_char = (*str) + 0x70; }	// byte of characters p...ya shift by +112
					else if ((uint8_t)*str == 0x91) { buff_char = 0xB8; break; }		// byte of the yo symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x94) { buff_char = 0xBA; break; }		// byte of the ye symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x96) { buff_char = 0xB3; break; }		// byte of the i symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x97) { buff_char = 0xBF; break; }		// byte of the yi symbol (add more symbols here if needed, and in DrawChar())
					break;
				}
			}
			//------------------------------------------------------------------------------------------------
			// decrement the counter again because we consumed 2 bytes for Cyrillic
			len--;
			
			ST7789_DrawChar(x, y, TextColor, BgColor, TransparentBg, Font, multiplier, buff_char);
		}
		//---------------------------------------------------------------------
		else{
			ST7789_DrawChar(x, y, TextColor, BgColor, TransparentBg, Font, multiplier, *str);
		}
		
		x = x + (Font->FontWidth * multiplier);
		/* Increase string pointer */
		str++;
	}
}
//==============================================================================


//==============================================================================
// Procedure for drawing a character at a specified angle (one letter or symbol)
//==============================================================================
void ST7789_DrawCharWithAngle(uint16_t x, uint16_t y, uint16_t TextColor, uint16_t BgColor, uint8_t TransparentBg, FontDef_t* Font, uint8_t multiplier, double angle_degrees, unsigned char ch){
	
	uint32_t i, b, j;
	
	uint32_t X = x, Y = y;
	
	uint8_t xx, yy;
	
	// Convert the angle to radians
	double radians = (360.0 - angle_degrees) * PI / 180.0;

	// Calculate the rotation matrix
	double cosTheta = cos(radians);
	double sinTheta = sin(radians);

	// Variables for transformed coordinates
	double newX, newY;
	
	if( multiplier < 1 ){
		multiplier = 1;
	}

	/* Check available space in LCD */
	if (ST7789_Width >= ( x + Font->FontWidth) || ST7789_Height >= ( y + Font->FontHeight)){

			/* Go through font */
			for (i = 0; i < Font->FontHeight; i++) {		
				
				if( ch < 127 ){			
					b = Font->data[(ch - 32) * Font->FontHeight + i];
				}
				
				else if( (uint8_t) ch > 191 ){
					// +96 because Latin letters and symbols occupy 96 positions in the fonts
					// and if the font contains Latin letters and special characters first, 
					// followed by Cyrillic only, then you need to add 95; if the font 
					// contains only Cyrillic, then +96 is not needed
					b = Font->data[((ch - 192) + 96) * Font->FontHeight + i];
				}
				
				else if( (uint8_t) ch == 168 ){	// 168 symbol in ASCII - Yo
					// 160 element (symbol Yo) 
					b = Font->data[( 160 ) * Font->FontHeight + i];
				}
				
				else if( (uint8_t) ch == 184 ){	// 184 symbol in ASCII - yo
					// 161 element (symbol yo) 
					b = Font->data[( 161 ) * Font->FontHeight + i];
				}
				//-------------------------------------------------------------------
				
				//----  Ukrainian layout ----------------------------------------------------
				else if( (uint8_t) ch == 170 ){	// 168 symbol in ASCII - Ye
					// 162 element (symbol Ye)
					b = Font->data[( 162 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 175 ){	// 184 symbol in ASCII - Yi
					// 163 element (symbol Yi)
					b = Font->data[( 163 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 178 ){	// 168 symbol in ASCII - I
					// 164 element (symbol I)
					b = Font->data[( 164 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 179 ){	// 184 symbol in ASCII - i
					// 165 element (symbol i)
					b = Font->data[( 165 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 186 ){	// 184 symbol in ASCII - ye
					// 166 element (symbol ye)
					b = Font->data[( 166 ) * Font->FontHeight + i];
				}
				else if( (uint8_t) ch == 191 ){	// 168 symbol in ASCII - yi
					// 167 element (symbol yi)
					b = Font->data[( 167 ) * Font->FontHeight + i];
				}
				//-----------------------------------------------------------------------------
			
				for (j = 0; j < Font->FontWidth; j++) {
					if ((b << j) & 0x8000) {
							// Apply the rotation to the coordinates
							newX = cosTheta * (X - x) - sinTheta * (Y - y) + x;
							newY = sinTheta * (X - x) + cosTheta * (Y - y) + y;

							for (yy = 0; yy < multiplier; yy++) {
									for (xx = 0; xx < multiplier; xx++) {
											ST7789_DrawPixel(newX + xx, newY + yy, TextColor);
									}
							}
					} else if (TransparentBg) {
							// Likewise for the background
							newX = cosTheta * (X - x) - sinTheta * (Y - y) + x + 0.5;
							newY = sinTheta * (X - x) + cosTheta * (Y - y) + y + 0.5;

							for (yy = 0; yy < multiplier; yy++) {
									for (xx = 0; xx < multiplier; xx++) {
											ST7789_DrawPixel(newX + xx, newY + yy, BgColor);
									}
							}
					}
					X = X + multiplier;
				}
				X = x;
				Y = Y + multiplier;
			}
	}
}
//==============================================================================


//==============================================================================
// Procedure for drawing a string at a specified angle
//==============================================================================
void ST7789_printWithAngle(uint16_t x, uint16_t y, uint16_t TextColor, uint16_t BgColor, uint8_t TransparentBg, FontDef_t* Font, uint8_t multiplier, double angle_degrees, char *str){	
	
	if( multiplier < 1 ){
		multiplier = 1;
	}
	
	unsigned char buff_char;
	
	uint16_t len = strlen(str);
	
	while (len--) {
		
		//---------------------------------------------------------------------
		// check for Cyrillic UTF-8; if Latin letters are used, skip this block
		// extended Win-1251 ASCII Cyrillic characters (code 128-255)
		// check the first byte out of two (since UTF-8 is two bytes)
		// if it is greater than or equal to 0xC0 (the first byte in Cyrillic will be 0xD0 or 0xD1 in the alphabet)
		if ( (uint8_t)*str >= 0xC0 ){	// code 0xC0 corresponds to the Cyrillic letter 'A' in Win-1251 ASCII
			
			// check which byte is first: 0xD0 or 0xD1---------------------------------------------
			switch ((uint8_t)*str) {
				case 0xD0: {
					// advance the pointer because we need the second byte
					str++;
					// check the second byte to get the actual character
					if ((uint8_t)*str >= 0x90 && (uint8_t)*str <= 0xBF){ buff_char = (*str) + 0x30; }	// byte of characters A...Ya...p shift by +48
					else if ((uint8_t)*str == 0x81) { buff_char = 0xA8; break; }		// byte of the Yo symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x84) { buff_char = 0xAA; break; }		// byte of the Ye symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x86) { buff_char = 0xB2; break; }		// byte of the I symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x87) { buff_char = 0xAF; break; }		// byte of the Yi symbol (add more symbols here if needed, and in DrawChar())
					break;
				}
				case 0xD1: {
					// advance the pointer because we need the second byte
					str++;
					// check the second byte to get the actual character
					if ((uint8_t)*str >= 0x80 && (uint8_t)*str <= 0x8F){ buff_char = (*str) + 0x70; }	// byte of characters p...ya shift by +112
					else if ((uint8_t)*str == 0x91) { buff_char = 0xB8; break; }		// byte of the yo symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x94) { buff_char = 0xBA; break; }		// byte of the ye symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x96) { buff_char = 0xB3; break; }		// byte of the i symbol (add more symbols here if needed, and in DrawChar())
					else if ((uint8_t)*str == 0x97) { buff_char = 0xBF; break; }		// byte of the yi symbol (add more symbols here if needed, and in DrawChar())
					break;
				}
			}
			//------------------------------------------------------------------------------------------------
			// decrement the counter again because we consumed 2 bytes for Cyrillic
			len--;
			
			ST7789_DrawCharWithAngle(x, y, TextColor, BgColor, TransparentBg, Font, multiplier, angle_degrees, buff_char);
		}
		//---------------------------------------------------------------------
		else{
			ST7789_DrawCharWithAngle(x, y, TextColor, BgColor, TransparentBg, Font, multiplier, angle_degrees, *str);
		}
		// Move the initial coordinates for each character taking the angle into account
    x += (Font->FontWidth * multiplier * cos((360.0 - angle_degrees) * PI / 180.0) + 0.5);
    y += (Font->FontWidth * multiplier * sin((360.0 - angle_degrees) * PI / 180.0) + 0.5);

		/* Increase string pointer */
		str++;
	}
}
//==============================================================================


//==============================================================================
// Display rotation (orientation) procedure
//==============================================================================
// default mode is 1 (there are 1, 2, 3, 4 total)
void ST7789_rotation( uint8_t rotation ){
	
	ST7789_Select();
	
	ST7789_SendCmd(ST7789_MADCTL);

	// the driver is designed for a 320x240 display (maximum size)
	// to fit any other size, subtract the pixel difference

	  switch (rotation) {
		
		case 1:
			//== 2.25" 76 x 284 ST7789 =================================================
			#ifdef ST7789_IS_76X284
				ST7789_SendData(ST7789_MADCTL_RGB);
				ST7789_Width = 76;
				ST7789_Height = 284;
				ST7789_X_Start = 82;
				ST7789_Y_Start = 18;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		
			//== 1.13" 135 x 240 ST7789 =================================================
			#ifdef ST7789_IS_135X240
				ST7789_SendData(ST7789_MADCTL_RGB);
				ST7789_Width = 135;
				ST7789_Height = 240;
				ST7789_X_Start = 52;
				ST7789_Y_Start = 40;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		
			//== 1.3" 240 x 240 ST7789 =================================================
			#ifdef ST7789_IS_240X240
				ST7789_SendData(ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 240;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 1.47" 172 x 320 ST7789 =================================================
			#ifdef ST7789_IS_172X320
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 320;
				ST7789_Height = 172;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 34;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 1.69" 240 x 280 ST7789 =================================================
			#ifdef ST7789_IS_240X280
				ST7789_SendData(ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 280;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 20;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 2" 240 x 320 ST7789 =================================================
			#ifdef ST7789_IS_240X320
				ST7789_SendData(ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 320;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		 break;
		
		case 2:
			//== 2.25" 76 x 284 ST7789 =================================================
			#ifdef ST7789_IS_76X284
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 284;
				ST7789_Height = 76;
				ST7789_X_Start = 18;
				ST7789_Y_Start = 82;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		
			//== 1.13" 135 x 240 ST7789 =================================================
			#ifdef ST7789_IS_135X240
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 135;
				ST7789_X_Start = 40;
				ST7789_Y_Start = 53;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		
			//== 1.3" 240 x 240 ST7789 =================================================
			#ifdef ST7789_IS_240X240
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 240;		
				ST7789_X_Start = 0;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 1.47" 172 x 320 ST7789 =================================================
			#ifdef ST7789_IS_172X320
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
				ST7789_Width = 172;
				ST7789_Height = 320;
				ST7789_X_Start = 34;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 1.69" 240 x 280 ST7789 =================================================
			#ifdef ST7789_IS_240X280
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 280;
				ST7789_Height = 240;
				ST7789_X_Start = 20;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 2" 240 x 320 ST7789 =================================================
			#ifdef ST7789_IS_240X320
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 320;
				ST7789_Height = 240;		
				ST7789_X_Start = 0;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		 break;
		
	   case 3:
			 //== 2.25" 76 x 284 ST7789 =================================================
			#ifdef ST7789_IS_76X284
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
				ST7789_Width = 76;
				ST7789_Height = 284;
				ST7789_X_Start = 82;
				ST7789_Y_Start = 18;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		 
		   //== 1.13" 135 x 240 ST7789 =================================================
			#ifdef ST7789_IS_135X240
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
				ST7789_Width = 135;
				ST7789_Height = 240;
				ST7789_X_Start = 53;
				ST7789_Y_Start = 40;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
	   
			//== 1.3" 240 x 240 ST7789 =================================================
			#ifdef ST7789_IS_240X240
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 240;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 80;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
	   
			//== 1.47" 172 x 320 ST7789 =================================================
			#ifdef ST7789_IS_172X320
				ST7789_SendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 320;
				ST7789_Height = 172;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 34;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 1.69" 240 x 280 ST7789 =================================================
			#ifdef ST7789_IS_240X280
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 280;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 20;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 2" 240 x 320 ST7789 =================================================
			#ifdef ST7789_IS_240X320
				ST7789_SendData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 320;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
		 break;
	   
	   case 4:
			 //== 2.25" 76 x 284 ST7789 =================================================
			#ifdef ST7789_IS_76X284
				ST7789_SendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 284;
				ST7789_Height = 76;
				ST7789_X_Start = 18;
				ST7789_Y_Start = 82;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		 
		   //== 1.13" 135 x 240 ST7789 =================================================
			#ifdef ST7789_IS_135X240
				ST7789_SendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 135;
				ST7789_X_Start = 40;
				ST7789_Y_Start = 52;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
	   
			//== 1.3" 240 x 240 ST7789 =================================================
			#ifdef ST7789_IS_240X240
				ST7789_SendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 240;
				ST7789_Height = 240;
				ST7789_X_Start = 80;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
	   
		  //== 1.47" 172 x 320 ST7789 =================================================
			#ifdef ST7789_IS_172X320
				ST7789_SendData(ST7789_MADCTL_RGB);
				ST7789_Width = 172;
				ST7789_Height = 320;
				ST7789_X_Start = 34;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 1.69" 240 x 280 ST7789 =================================================
			#ifdef ST7789_IS_240X280
				ST7789_SendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 280;
				ST7789_Height = 240;
				ST7789_X_Start = 20;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
			
			//== 2" 240 x 320 ST7789 =================================================
			#ifdef ST7789_IS_240X320
				ST7789_SendData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
				ST7789_Width = 320;
				ST7789_Height = 240;
				ST7789_X_Start = 0;
				ST7789_Y_Start = 0;
				ST7789_FillScreen(0);
			#endif
			//==========================================================================
		 break;
	   
	   default:
		 break;
	  }
	  
	  ST7789_Unselect();
}
//==============================================================================


//==============================================================================
// Procedure for drawing a monochrome icon
//==============================================================================
void ST7789_DrawBitmap(int16_t x, int16_t y, const unsigned char* bitmap, int16_t w, int16_t h, uint16_t color){

    int16_t byteWidth = (w + 7) / 8; 	// Bitmap scanline pad = whole byte
    uint8_t byte = 0;

    for(int16_t j=0; j<h; j++, y++){
		
        for(int16_t i=0; i<w; i++){
			
            if(i & 7){
               byte <<= 1;
            }
            else{
               byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }
			
            if(byte & 0x80){
							ST7789_DrawPixel(x+i, y, color);
						}
        }
    }
}
//==============================================================================


//==============================================================================
// Procedure for drawing a monochrome icon at a specified angle
//==============================================================================
void ST7789_DrawBitmapWithAngle(int16_t x, int16_t y, const unsigned char* bitmap, int16_t w, int16_t h, uint16_t color, double angle_degrees) {
    // Convert angle to radians
    double angle_radians = (360.0 - angle_degrees) * PI / 180.0;

    // Calculate the rotation matrix
    double cosTheta = cos(angle_radians);
    double sinTheta = sin(angle_radians);

    // Width and height of the rotated image
    int16_t rotatedW = round(fabs(w * cosTheta) + fabs(h * sinTheta));
    int16_t rotatedH = round(fabs(h * cosTheta) + fabs(w * sinTheta));

    // Calculate the center coordinates of the rotated image
    int16_t centerX = x + w / 2;
    int16_t centerY = y + h / 2;

    // Move through each pixel of the image and draw it rotated
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            // Calculate the offset from the center
            int16_t offsetX = i - w / 2;
            int16_t offsetY = j - h / 2;

            // Apply the rotation matrix
            int16_t rotatedX = round(centerX + offsetX * cosTheta - offsetY * sinTheta);
            int16_t rotatedY = round(centerY + offsetX * sinTheta + offsetY * cosTheta);

            // Check whether the pixel is within the screen bounds
            if (rotatedX >= 0 && rotatedX < ST7789_Width && rotatedY >= 0 && rotatedY < ST7789_Height) {
                // Get the pixel color from the source image
                uint8_t byteWidth = (w + 7) / 8;
                uint8_t byte = (*(const unsigned char*)(&bitmap[j * byteWidth + i / 8]));
                if (byte & (0x80 >> (i & 7))) {
                    // Draw the pixel on the screen
                    ST7789_DrawPixel(rotatedX, rotatedY, color);
                }
            }
        }
    }
}
//==============================================================================


//==============================================================================
// Procedure for drawing a rounded rectangle (filled)
//==============================================================================
void ST7789_DrawFillRoundRect(int16_t x, int16_t y, uint16_t width, uint16_t height, int16_t cornerRadius, uint16_t color) {
	
	int16_t max_radius = ((width < height) ? width : height) / 2; // 1/2 minor axis
  if (cornerRadius > max_radius){
    cornerRadius = max_radius;
	}
	
  ST7789_DrawRectangleFilled(x + cornerRadius, y, x + cornerRadius + width - 2 * cornerRadius, y + height, color);
  // draw four corners
  ST7789_DrawFillCircleHelper(x + width - cornerRadius - 1, y + cornerRadius, cornerRadius, 1, height - 2 * cornerRadius - 1, color);
  ST7789_DrawFillCircleHelper(x + cornerRadius, y + cornerRadius, cornerRadius, 2, height - 2 * cornerRadius - 1, color);
}
//==============================================================================

//==============================================================================
// Procedure for drawing a half-circle (right or left) (filled)
//==============================================================================
void ST7789_DrawFillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t color) {

  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;
  int16_t px = x;
  int16_t py = y;

  delta++; // Avoid some +1's in the loop

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    if (x < (y + 1)) {
      if (corners & 1){
        ST7789_DrawLine(x0 + x, y0 - y, x0 + x, y0 - y - 1 + 2 * y + delta, color);
			}
      if (corners & 2){
        ST7789_DrawLine(x0 - x, y0 - y, x0 - x, y0 - y - 1 + 2 * y + delta, color);
			}
    }
    if (y != py) {
      if (corners & 1){
        ST7789_DrawLine(x0 + py, y0 - px, x0 + py, y0 - px - 1 + 2 * px + delta, color);
			}
      if (corners & 2){
        ST7789_DrawLine(x0 - py, y0 - px, x0 - py, y0 - px - 1 + 2 * px + delta, color);
			}
			py = y;
    }
    px = x;
  }
}
//==============================================================================																		

//==============================================================================
// Procedure for drawing a quarter-circle (rounding, arc) (1-pixel width)
//==============================================================================
void ST7789_DrawCircleHelper(int16_t x0, int16_t y0, int16_t radius, int8_t quadrantMask, uint16_t color)
{
    int16_t f = 1 - radius ;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;

    while (x <= y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
				
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (quadrantMask & 0x4) {
            ST7789_DrawPixel(x0 + x, y0 + y, color);
            ST7789_DrawPixel(x0 + y, y0 + x, color);;
        }
        if (quadrantMask & 0x2) {
			ST7789_DrawPixel(x0 + x, y0 - y, color);
            ST7789_DrawPixel(x0 + y, y0 - x, color);
        }
        if (quadrantMask & 0x8) {
			ST7789_DrawPixel(x0 - y, y0 + x, color);
            ST7789_DrawPixel(x0 - x, y0 + y, color);
        }
        if (quadrantMask & 0x1) {
            ST7789_DrawPixel(x0 - y, y0 - x, color);
            ST7789_DrawPixel(x0 - x, y0 - y, color);
        }
    }
}
//==============================================================================		

//==============================================================================
// Procedure for drawing a rounded rectangle (outline)
//==============================================================================
void ST7789_DrawRoundRect(int16_t x, int16_t y, uint16_t width, uint16_t height, int16_t cornerRadius, uint16_t color) {
	
	int16_t max_radius = ((width < height) ? width : height) / 2; // 1/2 minor axis
  if (cornerRadius > max_radius){
    cornerRadius = max_radius;
	}
	
  ST7789_DrawLine(x + cornerRadius, y, x + cornerRadius + width -1 - 2 * cornerRadius, y, color);         // Top
  ST7789_DrawLine(x + cornerRadius, y + height - 1, x + cornerRadius + width - 1 - 2 * cornerRadius, y + height - 1, color); // Bottom
  ST7789_DrawLine(x, y + cornerRadius, x, y + cornerRadius + height - 1 - 2 * cornerRadius, color);         // Left
  ST7789_DrawLine(x + width - 1, y + cornerRadius, x + width - 1, y + cornerRadius + height - 1 - 2 * cornerRadius, color); // Right
	
  // draw four corners
	ST7789_DrawCircleHelper(x + cornerRadius, y + cornerRadius, cornerRadius, 1, color);
  ST7789_DrawCircleHelper(x + width - cornerRadius - 1, y + cornerRadius, cornerRadius, 2, color);
	ST7789_DrawCircleHelper(x + width - cornerRadius - 1, y + height - cornerRadius - 1, cornerRadius, 4, color);
  ST7789_DrawCircleHelper(x + cornerRadius, y + height - cornerRadius - 1, cornerRadius, 8, color);
}
//==============================================================================

//==============================================================================
// Procedure for drawing a thick line (last parameter is thickness)
//==============================================================================
void ST7789_DrawLineThick(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, uint8_t thick) {
	const int16_t deltaX = abs(x2 - x1);
	const int16_t deltaY = abs(y2 - y1);
	const int16_t signX = x1 < x2 ? 1 : -1;
	const int16_t signY = y1 < y2 ? 1 : -1;

	int16_t error = deltaX - deltaY;

	if (thick > 1){
		ST7789_DrawCircleFilled(x2, y2, thick >> 1, color);
	}
	else{
		ST7789_DrawPixel(x2, y2, color);
	}

	while (x1 != x2 || y1 != y2) {
		if (thick > 1){
			ST7789_DrawCircleFilled(x1, y1, thick >> 1, color);
		}
		else{
			ST7789_DrawPixel(x1, y1, color);
		}

		const int16_t error2 = error * 2;
		if (error2 > -deltaY) {
			error -= deltaY;
			x1 += signX;
		}
		if (error2 < deltaX) {
			error += deltaX;
			y1 += signY;
		}
	}
}
//==============================================================================		


//==============================================================================
// thick line of the required length and specified rotation angle (0-360) (last parameter is thickness)
//==============================================================================
void ST7789_DrawLineThickWithAngle(int16_t x, int16_t y, int16_t length, double angle_degrees, uint16_t color, uint8_t thick) {
    double angleRad = (360.0 - angle_degrees) * PI / 180.0;
    int16_t x2 = x + (int16_t)(cos(angleRad) * length) + 0.5;
    int16_t y2 = y + (int16_t)(sin(angleRad) * length) + 0.5;

    ST7789_DrawLineThick(x, y, x2, y2, color, thick);
}
//==============================================================================


//==============================================================================
// Procedure for drawing a thick arc (part of a circle)
//==============================================================================
void ST7789_DrawArc(int16_t x0, int16_t y0, int16_t radius, int16_t startAngle, int16_t endAngle, uint16_t color, uint8_t thick) {
	
    int16_t xLast = -1, yLast = -1;

    if (startAngle > endAngle) {
        // Draw the first part of the arc from startAngle to 360 degrees
        for (int16_t angle = startAngle; angle <= 360; angle += 2) {
            float angleRad = (float)(360 - angle) * PI / 180;
            int x = cos(angleRad) * radius + x0;
            int y = sin(angleRad) * radius + y0;

            if (xLast != -1 && yLast != -1) {
                if (thick > 1) {
                    ST7789_DrawLineThick(xLast, yLast, x, y, color, thick);
                } else {
                    ST7789_DrawLine(xLast, yLast, x, y, color);
                }
            }

            xLast = x;
            yLast = y;
        }

        // Draw the second part of the arc from 0 to endAngle
        for (int16_t angle = 0; angle <= endAngle; angle += 2) {
            float angleRad = (float)(360 - angle) * PI / 180;
            int x = cos(angleRad) * radius + x0;
            int y = sin(angleRad) * radius + y0;

            if (xLast != -1 && yLast != -1) {
                if (thick > 1) {
                    ST7789_DrawLineThick(xLast, yLast, x, y, color, thick);
                } else {
                    ST7789_DrawLine(xLast, yLast, x, y, color);
                }
            }

            xLast = x;
            yLast = y;
        }
    } else {
        // Draw the arc from startAngle to endAngle
        for (int16_t angle = startAngle; angle <= endAngle; angle += 2) {
            float angleRad = (float)(360 - angle) * PI / 180;
            int x = cos(angleRad) * radius + x0;
            int y = sin(angleRad) * radius + y0;

            if (xLast != -1 && yLast != -1) {
                if (thick > 1) {
                    ST7789_DrawLineThick(xLast, yLast, x, y, color, thick);
                } else {
                    ST7789_DrawLine(xLast, yLast, x, y, color);
                }
            }

            xLast = x;
            yLast = y;
        }
    }
}
//==============================================================================


#if FRAME_BUFFER
	//==============================================================================
	// Procedure for outputting the frame buffer to the display
	//==============================================================================
	void ST7789_Update(void){
		
			ST7789_SetWindow(0, 0, ST7789_Width-1, ST7789_Height-1);
		
			ST7789_Select();
		
			ST7789_SendDataMASS((uint8_t*)buff_frame, sizeof(uint16_t)*ST7789_Width*ST7789_Height);
		
			ST7789_Unselect();
	}
	//==============================================================================
	
	//==============================================================================
	// Procedure for clearing only the frame buffer (the screen itself is not cleared)
	//==============================================================================
	void ST7789_ClearFrameBuffer(void){
		memset((uint8_t*)buff_frame, 0x00, ST7789_Width*ST7789_Height*sizeof(uint16_t) );
	}
	//==============================================================================
#endif




//#########################################################################################################################
//#########################################################################################################################




/************************ (C) COPYRIGHT GKP *****END OF FILE****/
