/*
 * resource_pwm_motor.c
 *
 *  Created on: Dec 24, 2019
 *      Author: byoungnamjang
 */

#include <stdio.h>
#include <stdlib.h>
#include <peripheral_io.h>
#include <system_info.h>
#include <unistd.h>
#include <peripheral_io.h>
#include <app_common.h>
#include <math.h>
#include "hello_tizen.h"
#include "mpu9250.h"
#include "hello.h"

#define MODEL_NAME_KEY "http://tizen.org/system/model_name"
#define MODEL_NAME_RPI3 "rpi3"
#define MODEL_NAME_ARTIK "artik"
#define MODEL_NAME_ANCHOR3 "anchor3"
#define MODEL_NAME_SDTA7D "sdta7d"

static peripheral_spi_h MPU9250_H = NULL;
static unsigned int ref_count = 0;

/* for MPU9250 SPI */
#define	MPU9250_SPEED 1000000	// MAX 1MHz
#define MPU9250_BPW 16

#define retv_if(expr, val) do { \
	if (expr) { \
		LOGE("(%s) -> %s() return", #expr, __FUNCTION__); \
		return (val); \
	} \
} while (0)

/* MPU9250 driver */
static uint8_t magnetometer_calib[3];
static float gyro_div;
static float accel_div;

typedef enum {
    MPU9250_STAT_NONE = 0,
    MPU9250_STAT_IDLE,
    MPU9250_STAT_MAESUREING,
} MPU9250_STAT;

static MPU9250_STAT stat = MPU9250_STAT_NONE;

#define PI (3.1415926535)

union bytes_float {
    uint8_t bytes[4];
    float   value;
};

static union bytes_float maesure_gyro[3];
static union bytes_float maesure_acel[3];
static union bytes_float maesure_magm[3];
static union bytes_float maesure_axangl[2];

/*  API FUNCTIONS  */
int resource_mpu9250_spi_init(void) {
	int ret = 0;
	int bus = -1;
	char *model_name = NULL;
	/* Initial SPI peripheral-I/O */
	if (MPU9250_H) {
		LOGD("SPI device already initialized [ref_count : %u]", ref_count); ref_count++;
		return 0;
	}

	system_info_get_platform_string(MODEL_NAME_KEY, &model_name);
	if (!model_name) {
		LOGE("fail to get model name");
		return -1;
	}

	if (!strcmp(model_name, MODEL_NAME_RPI3)) {
		bus = 0;
	} else if (!strcmp(model_name, MODEL_NAME_ANCHOR3)) {
		bus = 0; // ANCHOR (0)
	} else if (!strcmp(model_name, MODEL_NAME_SDTA7D)) {
		bus = 2; // SDTA7D (2)
	} else {
		LOGE("unknown model name : %s", model_name); free(model_name);
		return -1;
	}

	LOGI("%s model_name: %s, bus: %d", __func__, model_name, bus);
	free(model_name);
	model_name = NULL;

	ret = peripheral_spi_open(bus, 0, &MPU9250_H);
	if (PERIPHERAL_ERROR_NONE != ret) {
		LOGE("spi open failed :%s ", get_error_message(ret));
		return -1;
	}

	ret = peripheral_spi_set_mode(MPU9250_H, PERIPHERAL_SPI_MODE_0);

	if (PERIPHERAL_ERROR_NONE != ret) {
		LOGE("peripheral_spi_set_mode failed :%s ", get_error_message(ret));
		goto error_after_open;
	}

	ret = peripheral_spi_set_bit_order(MPU9250_H, PERIPHERAL_SPI_BIT_ORDER_MSB);
	if (PERIPHERAL_ERROR_NONE != ret) {
		LOGE("peripheral_spi_set_bit_order failed :%s ", get_error_message(ret));
		goto error_after_open;
	}

	ret = peripheral_spi_set_bits_per_word(MPU9250_H, MPU9250_BPW);
	if (PERIPHERAL_ERROR_NONE != ret) {
		LOGE("peripheral_spi_set_bits_per_word failed :%s ", get_error_message(ret));
		goto error_after_open;
	}

	ret = peripheral_spi_set_frequency(MPU9250_H, MPU9250_SPEED);
	if (PERIPHERAL_ERROR_NONE != ret) {
		LOGE("peripheral_spi_set_frequency failed :%s ", get_error_message(ret));
		goto error_after_open;
	}

	LOGI("%s success: %d", __func__, ref_count);
	ref_count++;
	return 0;

error_after_open:
	LOGI("%s error: %d", __func__, ref_count);
	peripheral_spi_close(MPU9250_H);
	MPU9250_H = NULL;
	return -1;
}

void resource_mpu9250_spi_fini(void) {

	if (MPU9250_H) {
		ref_count--;
	}
	else
		return;

	if (ref_count == 0) {
		peripheral_spi_close(MPU9250_H);
		MPU9250_H = NULL;
	}
}

static int resource_mpu9250_read_byte(uint8_t addr, uint8_t *val)
{
	unsigned char rx[2] = {0,};
	unsigned char tx[2] = {0,};

	retv_if(MPU9250_H==NULL, -1);
	retv_if(val==NULL, -1);

	addr |= 0x80;	/* send read flag */
	tx[1] = addr;	/* build send frame. */

	peripheral_spi_transfer(MPU9250_H, tx, rx, 2);

	*val = rx[0] & 0xFF;

	return 0;
}

static int resource_mpu9250_write_byte(uint8_t addr, uint8_t *val)
{
	unsigned char rx[2] = {0,};
	unsigned char tx[2] = {0,};

	retv_if(MPU9250_H==NULL, -1);
//	retv_if(val==NULL, -1);

	tx[1] = addr;
	tx[0] = val; 	/* build send frame. */

	peripheral_spi_transfer(MPU9250_H, tx, rx, 2);

	return 0;
}
/*
 * Initialize MPU9250 9axis sensor.
 * see: MPU-9250 Product Specification 7.5 SPI interface.
 */
bool resource_mpu9250_dev_init(void)
{
	uint8_t val = 0x00;

	/* MPU9250 reset & initial */

	LOGI("%s read who am I", __func__);
	for (int i = 0; i < 10000; i++) {
		/*
		 * IMPLEMENT HERE : read MPU9250_REG_WHO_AM_I register value
		 * how to :
		 * 	function : SPI read function
		 * 	reg name : MPU9250_REG_WHO_AM_I (defined on header file)
		 * 	buffer L val
		 * if read value is equal to 0x71, then break
		 */
		resource_mpu9250_read_byte(MPU9250_REG_WHO_AM_I, &val);
		if (val == 0x71) {
			break;
	       	}
	        usleep(100);  /* 100ms */
	}
	if (val != 0x71) {
	    	LOGI("%s fail to read device info", __func__);
	        return false;
	}

	uint8_t init_conf[][2] = {
	        {MPU9250_REG_PWR_MGMT_1,    0x80},  /* reset */
	        {MPU9250_REG_PWR_MGMT_1,    0x01},  /* clock source */
	        {MPU9250_REG_PWR_MGMT_2,    0x3f},  /* Disable Accel & Gyro */
	        {MPU9250_REG_INT_PIN_CFG,   0x30},  /* Pin config */
	        {MPU9250_REG_I2C_MST_CTRL,  0x0D},  /* I2C multi-master / 400kHz */
	        {MPU9250_REG_USER_CTRL,     0x20},  /* I2C master mode */
	        {0xff,                      0xff}
	};

	for (int i = 0; init_conf[i][0] != 0xff; i++) {
	        resource_mpu9250_write_byte(init_conf[i][0], init_conf[i][1]);
	        usleep(1000);
	}

	/* AK8963(MPU9250 built in.) */
	/* Reset */
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_REG,  AK8963_REG_CNTL2);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_DO,   0x01);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_CTRL, 0x81);
	usleep(10000);  /* 10ms */

	/* read calibration data */
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | 0x80);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_REG,  AK8963_REG_ASAX);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_CTRL, 0x83);
	usleep(10000);  /* 10ms */
	for (int i = 0; i < sizeof(magnetometer_calib); i++) {
	        resource_mpu9250_read_byte(MPU9250_REG_EXT_SENS_DATA_00 + i, &magnetometer_calib[i]);
	}

	/* 16bit periodical mode. (8Hz) */
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_REG,  AK8963_REG_CNTL1);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_DO,   0x12);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_CTRL, 0x81);
	usleep(10000);  /* 10ms */

	/* Read `who am i' */
	LOGI("%s read who am I", __func__);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | 0x80);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_REG,  AK8963_REG_WIA);
	resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_CTRL, 0x81);
	for (int i = 0; i < 100; i++) {
	    	usleep(10000);
	        resource_mpu9250_read_byte(MPU9250_REG_EXT_SENS_DATA_00, &val);
	        if (val == 0x48) {
	            break;
	        }
	}
	if (val != 0x48) {
	    	LOGI("%s fail to read device info", __func__);
	        return false;
	}

	stat = MPU9250_STAT_IDLE;   /* Update STATE */
	return true;
}



/*
 * Start maesure.
 */
bool resource_mpu9250_start_maesure(MPU9250_BIT_GYRO_FS_SEL gyro_fs, MPU9250_BIT_ACCEL_FS_SEL accel_fs, MPU9250_BIT_DLPF_CFG dlpf_cfg, MPU9250_BIT_A_DLPFCFG a_dlpfcfg)
{
    if (stat != MPU9250_STAT_IDLE) {
        return false;
    }

    uint8_t init_conf[][2] = {
        {MPU9250_REG_PWR_MGMT_2,    0x00},                      /* Enable Accel & Gyro */
        {MPU9250_REG_CONFIG,        (uint8_t)dlpf_cfg},         /* Gyro LPF */
        {MPU9250_REG_GYRO_CONFIG,   (uint8_t)gyro_fs},          /* Gyro configuration */
        {MPU9250_REG_ACCEL_CONFIG,  (uint8_t)accel_fs},         /* Accel configuration */
        {MPU9250_REG_ACCEL_CONFIG2, 0x08 | (uint8_t)a_dlpfcfg}, /* Accel LPF */
        {0xff,                      0xff}
    };
    for (int i = 0; init_conf[i][0] != 0xff; i++) {
        resource_mpu9250_write_byte(init_conf[i][0], init_conf[i][1]);
        usleep(1000);
    }

    switch (gyro_fs) {
    case MPU9250_BIT_GYRO_FS_SEL_250DPS:
        gyro_div = 131.0;
        break;
    case MPU9250_BIT_GYRO_FS_SEL_500DPS:
        gyro_div = 65.5;
        break;
    case MPU9250_BIT_GYRO_FS_SEL_1000DPS:
        gyro_div = 32.8;
        break;
    case MPU9250_BIT_GYRO_FS_SEL_2000DPS:
        gyro_div = 16.4;
        break;
    default:
        gyro_div = 131.0;
        break;
    }

    switch (accel_fs) {
    case MPU9250_BIT_ACCEL_FS_SEL_2G:
        accel_div = 16384;
        break;
    case MPU9250_BIT_ACCEL_FS_SEL_4G:
        accel_div = 8192;
        break;
    case MPU9250_BIT_ACCEL_FS_SEL_8G:
        accel_div = 4096;
        break;
    case MPU9250_BIT_ACCEL_FS_SEL_16G:
        accel_div = 2048;
        break;
    default:
        accel_div = 16384;
        break;
    }

    stat = MPU9250_STAT_MAESUREING; /* Update STATE. */
    return true;
}

/*
 * Stop maesure.
 */
bool resource_mpu9250_stop_maesure(void)
{
    if (stat != MPU9250_STAT_MAESUREING) {
        return false;
    }

    resource_mpu9250_write_byte(MPU9250_REG_PWR_MGMT_2, 0x3f);   /* Disable Accel & Gyro */
    usleep(1000);

    stat = MPU9250_STAT_IDLE;   /* Update STATE. */
    return true;
}

/*
 * Read Gyro.
 */
bool resource_mpu9250_read_gyro(MPU9250_gyro_val *gyro_val)
{
    uint8_t vals[6];

    if (stat != MPU9250_STAT_MAESUREING) {
        return false;
    }

    if (gyro_val == NULL) {
        return false;
    }

    for (int i = 0; i < sizeof(vals); i++) {
        resource_mpu9250_read_byte(MPU9250_REG_GYRO_XOUT_HL + i, &vals[i]);
    }

    gyro_val->raw_x = ((uint16_t)vals[0] << 8) | vals[1];
    gyro_val->raw_y = ((uint16_t)vals[2] << 8) | vals[3];
    gyro_val->raw_z = ((uint16_t)vals[4] << 8) | vals[5];

    gyro_val->x = (float)(int16_t)gyro_val->raw_x / gyro_div;
    gyro_val->y = (float)(int16_t)gyro_val->raw_y / gyro_div;
    gyro_val->z = (float)(int16_t)gyro_val->raw_z / gyro_div;

    return true;
}

/*
 * Read Accel.
 */
bool resource_mpu9250_read_accel(MPU9250_accel_val *accel_val)
{
    uint8_t vals[6];

    if (stat != MPU9250_STAT_MAESUREING) {
        return false;
    }

    if (accel_val == NULL) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        resource_mpu9250_read_byte(MPU9250_REG_ACCEL_XOUT_HL + i, &vals[i]);
    }

    accel_val->raw_x = ((uint16_t)vals[0] << 8) | vals[1];
    accel_val->raw_y = ((uint16_t)vals[2] << 8) | vals[3];
    accel_val->raw_z = ((uint16_t)vals[4] << 8) | vals[5];

    accel_val->x = (float)(int16_t)accel_val->raw_x / accel_div;
    accel_val->y = (float)(int16_t)accel_val->raw_y / accel_div;
    accel_val->z = (float)(int16_t)accel_val->raw_z / accel_div;

    return true;
}

/*
 * Read chip temperature.
 */
bool resource_mpu9250_read_temperature(MPU9250_temperature_val *temperature_val)
{
    uint8_t val[2];

    if (stat != MPU9250_STAT_MAESUREING) {
        return false;
    }

    if (temperature_val == NULL) {
        return false;
    }

    resource_mpu9250_read_byte(MPU9250_REG_TEMP_HL, &val[0]);
    resource_mpu9250_read_byte(MPU9250_REG_TEMP_HL + 1, &val[1]);

    temperature_val->raw = ((uint16_t)val[0] << 8) | val[1];

    return true;
}

/*
 * Read Magnetometer.
 */
bool resource_mpu9250_read_magnetometer(MPU9250_magnetometer_val *magnetometer_val)
{
    uint8_t vals[8];

    if (stat != MPU9250_STAT_MAESUREING) {
        return false;
    }

    if (magnetometer_val == NULL) {
        return false;
    }
    /* set read flag & slave address. */
    resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_ADDR, AK8963_I2C_ADDR | 0x80);
    /* set register address. */
    resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_REG, AK8963_REG_ST1);
    /* transfer */
    resource_mpu9250_write_byte(MPU9250_REG_I2C_SLV0_CTRL, 0x88);

    usleep(1000);
    for (int i = 0; i < sizeof(vals); i++) {
        resource_mpu9250_read_byte(MPU9250_REG_EXT_SENS_DATA_00 + i, &vals[i]);
    }

    if ((vals[7] & 0x08) != 0) {
        //detect overflow
        return false;
    }
    /* RAW data */
    magnetometer_val->raw_x = ((uint16_t)vals[2] << 8) | vals[1];
    magnetometer_val->raw_y = ((uint16_t)vals[4] << 8) | vals[3];
    magnetometer_val->raw_z = ((uint16_t)vals[6] << 8) | vals[5];
    /* Real data */
    magnetometer_val->x = (int16_t)magnetometer_val->raw_x * ((((float)(int8_t)magnetometer_calib[0] - 128) / 256) + 1);
    magnetometer_val->y = (int16_t)magnetometer_val->raw_y * ((((float)(int8_t)magnetometer_calib[1] - 128) / 256) + 1);
    magnetometer_val->z = (int16_t)magnetometer_val->raw_z * ((((float)(int8_t)magnetometer_calib[2] - 128) / 256) + 1);

    return true;
}


/*
 * Read Gyro
 *  rx, ry, rz: raw value.
 *  x, y, z   : computed value.(UNIT: dig/sec)
 */
bool mpu9250_gyro_read(uint16_t *rx, uint16_t *ry, uint16_t *rz, float *x, float *y, float *z)
{
    MPU9250_gyro_val gyro;
    gyro.raw_x = 0;
    gyro.raw_y = 0;
    gyro.raw_z = 0;

    if (resource_mpu9250_read_gyro(&gyro)) {
        if (rx != NULL) {
            *rx = gyro.raw_x;
        }
        if (ry != NULL) {
            *ry = gyro.raw_y;
        }
        if (rz != NULL) {
            *rz = gyro.raw_z;
        }

        if (x != NULL) {
            *x = gyro.x;
        }
        if (y != NULL) {
            *y = gyro.y;
        }
        if (z != NULL) {
            *z = gyro.z;
        }

        return true;
    }
    return false;
}

/*
 * Read Accel
 *  rx, ry, rz: raw value.
 *  x, y, z   : comuted value.(UNIT: 1G)
 */
bool mpu9250_accel_read(uint16_t *rx, uint16_t *ry, uint16_t *rz, float *x, float *y, float *z)
{
    MPU9250_accel_val acc;
    acc.raw_x = 0;
    acc.raw_y = 0;
    acc.raw_z = 0;

    if (resource_mpu9250_read_accel(&acc)) {
        if (rx != NULL) {
            *rx = acc.raw_x;
        }
        if (ry != NULL) {
            *ry = acc.raw_y;
        }
        if (rz != NULL) {
            *rz = acc.raw_z;
        }

        if (x != NULL) {
            *x = acc.x;
        }
        if (y != NULL) {
            *y = acc.y;
        }
        if (z != NULL) {
            *z = acc.z;
        }

        return true;
    }
    return false;
}

/*
 * Read Chip Temperature
 *  rt: Raw value.
 *   t: Computed value(digC)
 */
bool mpu9250_temperature_read(uint16_t *rt, float *t)
{
    float ft;
    MPU9250_temperature_val temp;
    temp.raw = 0;

    if (resource_mpu9250_read_temperature(&temp)) {
        if (rt != NULL) {
            *rt = temp.raw;
        }

        if (t != NULL) {
            ft = (float)(int16_t)temp.raw;
            *t = ((ft - 21) / 333.87) + 21;
        }
        return true;
    }
    return false;
}

/*
 * Read Magnetometer
 *  rx, ry, rz: Raw value.
 *  x, y, z   : Computed value(uT)
 */
bool mpu9250_magnetometer_read(uint16_t *rx, uint16_t *ry, uint16_t *rz, float *x, float *y, float *z)
{
    MPU9250_magnetometer_val mag;
    mag.raw_x = 0;
    mag.raw_y = 0;
    mag.raw_z = 0;

    if (resource_mpu9250_read_magnetometer(&mag)) {
        if (rx != NULL) {
            *rx = mag.raw_x;
        }
        if (ry != NULL) {
            *ry = mag.raw_y;
        }
        if (rz != NULL) {
            *rz = mag.raw_z;
        }

        if (x != NULL) {
            *x = mag.x;
        }
        if (y != NULL) {
            *y = mag.y;
        }
        if (z != NULL) {
            *z = mag.z;
        }

        return true;
    }
    return false;
}

/*
 * Computed AXIS angle from Accel
 */
bool mpu9250_acc_axis_angle(float *pitch_rad, float *roll_rad)
{
    MPU9250_accel_val acc;
    if (resource_mpu9250_read_accel(&acc)) {
        mpu9250_compute_axis_angle(acc.x, acc.y, acc.z, pitch_rad, roll_rad);
        return true;
    }
    return false;
}

/*
 * Compute axis angle from Accel
 */
void mpu9250_compute_axis_angle(float acc_x, float acc_y, float acc_z, float *pitch_rad, float *roll_rad)
{
    float x_rad, y_rad, z_rad;

    x_rad = atan(acc_x / sqrt(pow(acc_y, 2) + pow(acc_z, 2)));
    y_rad = atan(acc_y / sqrt(pow(acc_x, 2) + pow(acc_z, 2)));
    z_rad = atan(sqrt(pow(acc_x, 2) + pow(acc_y, 2)) / acc_z);

    if (z_rad >= 0.0) {
        /* first quadrant & fourth quadrant */
        /* pitch */
        if (pitch_rad != NULL) {
            if (x_rad >= 0.0) {
                *pitch_rad = x_rad;             /* first quadrant */
            } else {
                *pitch_rad = (2 * PI) + x_rad;  /* fourth quadrant */
            }
        }
        /* roll */
        if (roll_rad != NULL) {
            if (y_rad >= 0.0) {
                *roll_rad = y_rad;              /* first quadrant */
            } else {
                *roll_rad = (2 * PI) + y_rad;   /* fourth quadrant */
            }
        }
    } else {
        /* second quadrant & third quadrant */
        /* pitch */
        if (pitch_rad != NULL) {
            *pitch_rad = PI - x_rad;
        }
        /* roll */
        if (roll_rad != NULL) {
            *roll_rad = PI - y_rad;
        }
    }
}


/*
 * test Gyro sensor with SPI interface
 */
int  spi_gyro_test_main(void)
{
	int len = 0;
	char msg[62];
	int i;

	LOGI("%s starting...\n", __func__);

	resource_mpu9250_spi_init();
	/*
	 * IMPLEMENT HERE : initialize SPI interface
	 * How to
	 * 	call SPI initialize function
	 */
	resource_mpu9250_dev_init();


	/*
	 * IMPLEMENT HERE : MPU9250 device initialize
	 * How to
	 * 	call MPU9250 initialize function
	 */

	if (resource_mpu9250_start_maesure(MPU9250_BIT_GYRO_FS_SEL_2000DPS, MPU9250_BIT_ACCEL_FS_SEL_16G, MPU9250_BIT_DLPF_CFG_20HZ, MPU9250_BIT_A_DLPFCFG_20HZ) == false) {
		LOGI("%s MPU9250 start measure fail ...", __func__);
		goto error;
	}

	for (i=0; i<1000; i++)
	{
		/* IMPLEMENT HERE : read gyro sensor value and print
		 * How to
		 * 	function : mpu9250_gyro_read
		 * 	rx = NULL, ry=NULL, rz=NULL
		 * 	x,y,z = measure_gyro[x].value, x=0,1,2
		 */
		mpu9250_gyro_read(NULL, NULL, NULL, &maesure_gyro[0].value, &maesure_gyro[1].value, &maesure_gyro[2].value);
		len = sprintf(msg, "{gx:%0.1f,gy:%0.1f,gz:%0.1f}", maesure_gyro[0].value, maesure_gyro[1].value, maesure_gyro[2].value );
		LOGI("MPU9250 Gyro \t= %s", msg);

		/* IMPLEMENT HERE : read accel sensor value and print
		 * How to
		 * 	function : mpu9250_accel_read
		 * 	rx = NULL, ry=NULL, rz=NULL
		 * 	x,y,z = measure_acel[x].value, x=0,1,2
		 */
		mpu9250_accel_read(NULL, NULL, NULL, &maesure_acel[0].value, &maesure_acel[1].value, &maesure_acel[2].value);
		len = sprintf(msg, "{ax:%0.1f,ay:%0.1f,az:%0.1f}", maesure_acel[0].value, maesure_acel[1].value, maesure_acel[2].value );
		LOGI("MPU9250 Accel \t= %s", msg);

		mpu9250_magnetometer_read(NULL, NULL, NULL, &maesure_magm[0].value, &maesure_magm[1].value, &maesure_magm[2].value);
		len = sprintf(msg, "{mx:%0.1f,my:%0.1f,mz:%0.1f}", maesure_magm[0].value, maesure_magm[1].value, maesure_magm[2].value );
		LOGI("MPU9250 Magneto \t= %s", msg);

		mpu9250_compute_axis_angle(maesure_acel[0].value, maesure_acel[1].value, maesure_acel[2].value,&maesure_axangl[0].value, &maesure_axangl[1].value);
		len = sprintf(msg, "{pitch:%0.4f,roll:%0.4f}", maesure_axangl[0].value, maesure_axangl[1].value );
		LOGI("MPU9250 Axis Angle \t= %s", msg);
		LOGI("\n");
		sleep(2);
	}


	resource_mpu9250_stop_maesure();
	resource_mpu9250_spi_fini();
	LOGI("%s exiting...\n", __func__);
	return 0;

error:
	LOGI("%s error exiting...\n", __func__);
	resource_mpu9250_stop_maesure();
	resource_mpu9250_spi_fini();
	return -1;
}
