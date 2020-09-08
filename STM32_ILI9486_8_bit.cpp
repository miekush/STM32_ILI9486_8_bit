// Graphics library by ladyada/adafruit with init code from Rossum
// MIT license
// ported to STM32duino ST Core by MKE

#include "STM32_ILI9486_8_bit.h"

static const uint8_t ILI9486_regValues_ada[] PROGMEM = {
	//from mcufriend lib
	0xC0, 2, 0x0d, 0x0d,        //Power Control 1 [0E 0E]
    0xC1, 2, 0x43, 0x00,        //Power Control 2 [43 00]
    0xC2, 1, 0x00,      //Power Control 3 [33]
    0xC5, 4, 0x00, 0x48, 0x00, 0x48,    //VCOM  Control 1 [00 40 00 40]
    0xB4, 1, 0x00,      //Inversion Control [00]
    0xB6, 3, 0x02, 0x02, 0x3B,  // Display Function Control [02 02 3B]
	0xE0, 15, 0x0F, 0x21, 0x1C, 0x0B, 0x0E, 0x08, 0x49, 0x98, 0x38, 0x09, 0x11, 0x03, 0x14, 0x10, 0x00, //Positive Gamma Correction
	0xE1, 15, 0x0F, 0x2F, 0x2B, 0x0C, 0x0E, 0x06, 0x47, 0x76, 0x37, 0x07, 0x11, 0x04, 0x23, 0x1E, 0x00, //Negative Gamma Correction
};

/*****************************************************************************/
static void WriteCmdParamN(uint16_t cmd, int8_t N, const uint8_t * block)
{
    writeCommand(cmd);
	while (N-- > 0) {
		uint8_t u8 = *block++;
		CD_DATA;
		write8(u8);
	}
    CS_IDLE;
}

/*****************************************************************************/
static void init_table(const uint8_t *table, int16_t size)
{
	while (size > 0) {
		uint8_t cmd = *table++;
		uint8_t len = *table++;
		if (cmd == TFTLCD_DELAY8) {
			delay(len);
			len = 0;
		} else {
			WriteCmdParamN(cmd, len, table);
			table += len;
		}
		size -= len + 2;
	}
}

const uint8_t reset_off[] PROGMEM = {
	0x01, 0,            //Soft Reset
	TFTLCD_DELAY8, 150,  // .kbv will power up with ONLY reset, sleep out, display on
	0x28, 0,            //Display Off
	0x3A, 1, 0x55,      //Pixel read=565, write=565.
};

const uint8_t wake_on[] PROGMEM = {
	0x11, 0,            //Sleep Out
	TFTLCD_DELAY8, 150,
	0x29, 0,            //Display On
	// //additional settings
	ILI9486_INVERTOFF, 0,			// invert off
	0x36, 1, 0x48,      //Memory Access
	0xB0, 1, 0x40,      //RGB Signal [40] RCM=2
};

/*****************************************************************************/
// Constructor
/*****************************************************************************/
STM32_ILI9486_8_bit :: STM32_ILI9486_8_bit(void)
: Adafruit_GFX(TFTWIDTH, TFTHEIGHT)
{
}

/*****************************************************************************/
void STM32_ILI9486_8_bit::begin(void)
{
	reset();
	init_table(reset_off, sizeof(reset_off));
	init_table(ILI9486_regValues_ada, sizeof(ILI9486_regValues_ada));
	init_table(wake_on, sizeof(wake_on));
}

/*****************************************************************************/
void STM32_ILI9486_8_bit::reset(void)
{
	setCntrlDir();
	CS_IDLE; // Set all control bits to HIGH (idle)
	CD_DATA; // Signals are ACTIVE LOW
	WR_IDLE;
	RD_IDLE;
	setWriteDir();
	RST_TOGGLE;
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::setAddrWindow(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
	writeRegister32(ILI9486_COLADDRSET, ((uint32_t)(x1<<16) | x2));  // HX8357D uses same registers!
	writeRegister32(ILI9486_PAGEADDRSET, ((uint32_t)(y1<<16) | y2)); // HX8357D uses same registers!
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::flood(uint16_t color, uint32_t len)
{
	uint16_t blocks;
	uint8_t  i, hi = color >> 8, lo = color;
	CS_ACTIVE_CD_COMMAND;
	writeCommand(ILI9486_MEMORYWRITE);
	CD_DATA;
	write8(hi);
	write8(lo);
	len--;

  blocks = (uint16_t)(len / 64); // 64 pixels/block
  if(hi == lo) {
    // High and low bytes are identical.  Leave prior data
    // on the port(s) and just toggle the write strobe.
    while(blocks--) {
      i = 16; // 64 pixels/block / 4 pixels/pass
      do {
        WR_STROBE; WR_STROBE; WR_STROBE; WR_STROBE; // 2 bytes/pixel
        WR_STROBE; WR_STROBE; WR_STROBE; WR_STROBE; // x 4 pixels
      } while(--i);
    }
    // Fill any remaining pixels (1 to 64)
	i = len & 63;
    while (i--) {
		WR_STROBE; WR_STROBE;
	}
  } else {
    while(blocks--) {
      i = 16; // 64 pixels/block / 4 pixels/pass
      do {
        write8(hi); write8(lo); write8(hi); write8(lo);
        write8(hi); write8(lo); write8(hi); write8(lo);
      } while(--i);
    }
	i = len & 63;
    while (i--) { // write here the remaining data
      write8(hi); write8(lo);
    }
  }
  CS_IDLE;
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::drawFastHLine(int16_t x, int16_t y, int16_t length, uint16_t color)
{
  int16_t x2;

  // Initial off-screen clipping
  if((length <= 0     ) ||
     (y      <  0     ) || ( y                  >= _height) ||
     (x      >= _width) || ((x2 = (x+length-1)) <  0      )) return;

  if(x < 0) {        // Clip left
    length += x;
    x       = 0;
  }
  if(x2 >= _width) { // Clip right
    x2      = _width - 1;
    length  = x2 - x + 1;
  }

  setAddrWindow(x, y, x2, y);
  flood(color, length);
  writeRegisterPair(0x04, 0x05, TFTWIDTH  - 1);
  writeRegisterPair(0x08, 0x09, TFTHEIGHT - 1);
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::drawFastVLine(int16_t x, int16_t y, int16_t length, uint16_t color)
{
  int16_t y2;

  // Initial off-screen clipping
  if((length <= 0      ) ||
     (x      <  0      ) || ( x                  >= _width) ||
     (y      >= _height) || ((y2 = (y+length-1)) <  0     )) return;
  if(y < 0) {         // Clip top
    length += y;
    y       = 0;
  }
  if(y2 >= _height) { // Clip bottom
    y2      = _height - 1;
    length  = y2 - y + 1;
  }

  setAddrWindow(x, y, x, y2);
  flood(color, length);
  writeRegisterPair(0x04, 0x05, TFTWIDTH  - 1);
  writeRegisterPair(0x08, 0x09, TFTHEIGHT - 1);
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::fillRect(int16_t x1, int16_t y1, int16_t w, int16_t h, uint16_t fillcolor)
{

  int16_t  x2, y2;

  // Initial off-screen clipping
  if( (w            <= 0     ) ||  (h             <= 0      ) ||
      (x1           >= _width) ||  (y1            >= _height) ||
     ((x2 = x1+w-1) <  0     ) || ((y2  = y1+h-1) <  0      )) return;
  if(x1 < 0) { // Clip left
    w += x1;
    x1 = 0;
  }
  if(y1 < 0) { // Clip top
    h += y1;
    y1 = 0;
  }
  if(x2 >= _width) { // Clip right
    x2 = _width - 1;
    w  = x2 - x1 + 1;
  }
  if(y2 >= _height) { // Clip bottom
    y2 = _height - 1;
    h  = y2 - y1 + 1;
  }

  setAddrWindow(x1, y1, x2, y2);
  flood(fillcolor, (uint32_t)w * (uint32_t)h);
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::fillScreen(uint16_t color)
{
	setAddrWindow(0, 0, _width - 1, _height - 1);
	flood(color, (long)TFTWIDTH * (long)TFTHEIGHT);
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::drawPixel(int16_t x, int16_t y, uint16_t color)
{
  // Clip
  if((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return;
  setAddrWindow(x, y, x+1, y+1);
  writeRegister16(0x2C, color);
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t * bitmap)
{
	if ( x>=0 && (x+w)<_width && y>=0 && (y+h)<=_height ) {
		// all pixel visible, do it in the fast way
		setAddrWindow(x,y,x+w-1,y+h-1);
		pushColors((uint16_t*)bitmap, w*h, true);
	} else {
		// some pixels outside visible area, do it in the classical way to disable off-screen points
		int16_t i, j;
		uint16_t * colorP = (uint16_t*)bitmap;
		for(j=0; j<h; j++) {
			for(i=0; i<w; i++ ) {
				drawPixel(x+i, y+j, *colorP++);
			}
		}
	}
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::pushColors(uint16_t *data, int16_t len, boolean first)
{
  uint16_t color;
  uint8_t  hi, lo;
  CS_ACTIVE;
  if(first == true) { // Issue GRAM write command only on first call
    CD_COMMAND;
    write8(0x2C);
  }
  CD_DATA;
  while(len--) {
    color = *data++;
    hi    = color >> 8; // Don't simplify or merge these
    lo    = color;      // lines, there's macro shenanigans
    write8(hi);         // going on.
    write8(lo);
  }
  CS_IDLE;
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::invertDisplay(boolean i)
{
	writeCommand( i ? ILI9486_INVERTON : ILI9486_INVERTOFF);
	CS_IDLE;
}

/*****************************************************************************/

void STM32_ILI9486_8_bit::setRotation(uint8_t x)
{
  // Call parent rotation func first -- sets up rotation flags, etc.
  Adafruit_GFX::setRotation(x);
  // Then perform hardware-specific rotation operations...
   uint16_t t;
   switch (rotation) {
   case 1:
     t = ILI9486_MADCTL_MX | ILI9486_MADCTL_MY | ILI9486_MADCTL_MV | ILI9486_MADCTL_BGR;
     break;
   case 2:
     t = ILI9486_MADCTL_MX | ILI9486_MADCTL_BGR;
     break;
   case 3:
     t = ILI9486_MADCTL_MV | ILI9486_MADCTL_BGR;
     break;
   case 0:
   default:
    t = ILI9486_MADCTL_MY | ILI9486_MADCTL_BGR;
    break;
  }
   writeRegister8(ILI9486_MADCTL, t ); // MADCTL
   // For 9341, init default full-screen address window:
   //setAddrWindow(0, 0, _width - 1, _height - 1); // CS_IDLE happens here
}

/*****************************************************************************/

uint8_t read8_(void)
{
  RD_ACTIVE;
  delayMicroseconds(10);
  uint8_t temp = (TFT_DATA_PORT->IDR & 0x00FF);
  delayMicroseconds(10);
  RD_IDLE;
  delayMicroseconds(10);
  return temp;
}

/*****************************************************************************/

inline void writeCommand(uint16_t c)
{
	CS_ACTIVE_CD_COMMAND;
	write8(c>>8);
	write8(c);
}

/*****************************************************************************/

uint16_t STM32_ILI9486_8_bit::readPixel(int16_t x, int16_t y)
{
	if((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return 0;
	return 0;
}

/*****************************************************************************/

uint16_t STM32_ILI9486_8_bit::readID(void)
{
  uint16_t id = readReg32(0xD3);
  return id;
}

/*****************************************************************************/

uint32_t readReg32(uint8_t r)
{
  uint32_t id;
  uint8_t x;

  // try reading register #4
  writeCommand(r);
  setReadDir();  // Set up LCD data port(s) for READ operations
  CD_DATA;
  delayMicroseconds(50);
  read8(x);
  id = x;          // Do not merge or otherwise simplify
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id  |= x;        // shenanigans that are going on.
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id  |= x;        // shenanigans that are going on.
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id  |= x;        // shenanigans that are going on.
  CS_IDLE;
  setWriteDir();  // Restore LCD data port(s) to WRITE configuration
  return id;
}
/*****************************************************************************/

uint16_t readReg(uint8_t r)
{
  uint16_t id;
  uint8_t x;

  writeCommand(r);
  setReadDir();  // Set up LCD data port(s) for READ operations
  CD_DATA;
  delayMicroseconds(10);
  read8(x);
  id = x;          // Do not merge or otherwise simplify
  id <<= 8;              // these lines.  It's an unfortunate
  read8(x);
  id |= x;        // shenanigans that are going on.
  CS_IDLE;
  setWriteDir();  // Restore LCD data port(s) to WRITE configuration

  return id;
}

/*****************************************************************************/

void writeRegister8(uint16_t a, uint8_t d)
{
  writeCommand(a);
  CD_DATA;
  write8(d);
  CS_IDLE;
}

/*****************************************************************************/

void writeRegister16(uint16_t a, uint16_t d)
{
  writeCommand(a);
  CD_DATA;
  write8(d>>8);
  write8(d);
  CS_IDLE;
}

/*****************************************************************************/

void writeRegisterPair(uint16_t aH, uint16_t aL, uint16_t d)
{
  writeRegister8(aH, d>>8);
  writeRegister8(aL, d);
}

/*****************************************************************************/

void writeRegister24(uint16_t r, uint32_t d)
{
  writeCommand(r); // includes CS_ACTIVE
  CD_DATA;
  write8(d >> 16);
  write8(d >> 8);
  write8(d);
  CS_IDLE;
}

/*****************************************************************************/

void writeRegister32(uint16_t r, uint32_t d)
{
  writeCommand(r);
  CD_DATA;
  write8(d >> 24);
  write8(d >> 16);
  write8(d >> 8);
  write8(d);
  CS_IDLE;
}

//STM32_ILI9486_8_bit tft;
