#ifndef __I2C_MLX90614_H
#define	__I2C_MLX90614_H

#include "c_types.h"
#include "ets_sys.h"
#include "osapi.h"

#define MLX90614_ADDRESS	0xB4 

#define MLX90614_RAWIR1	0x04
#define MLX90614_RAWIR2	0x05
#define MLX90614_TA		0x06
#define MLX90614_TOBJ1	0x07
#define MLX90614_TOBJ2	0x08

#define MLX90614_TOMAX	0x20
#define MLX90614_TOMIN	0x21
#define MLX90614_PWMCTRL	0x22
#define MLX90614_TARANGE	0x23
#define MLX90614_EMISS	0x24
#define MLX90614_CONFIG	0x25

#define MLX90614_ID1	0x3C
#define MLX90614_ID2	0x3D
#define MLX90614_ID3	0x3E
#define MLX90614_ID4	0x3F 

#ifdef CONFIG_USEFLOAT
float LAST_MLX90614_AMBIENT_TEMPERATURE;
float LAST_MLX90614_OBJECT_TEMPERATURE;
#else
sint16 LAST_MLX90614_AMBIENT_TEMPERATURE;
sint16 LAST_MLX90614_OBJECT_TEMPERATURE;
#endif
static bool IS_ALREADY_INITED = false;

bool MLX90614_Init(void);
bool MLX90614_Read(void);

#endif