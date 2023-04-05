/*
 * config.h
 *
 * Created: 5-4-2013 10:36:17
 *  Author: Emile
 */ 


#ifndef INCFILE1_H_
#define INCFILE1_H_

#define USE_STATIC_OPTIONS               (USB_DEVICE_OPT_FULLSPEED | USB_OPT_RC32MCLKSRC | USB_OPT_BUSEVENT_PRIHIGH)
#define USE_FLASH_DESCRIPTORS
#define FIXED_CONTROL_ENDPOINT_SIZE      8
#define FIXED_NUM_CONFIGURATIONS         1
#define MAX_ENDPOINT_INDEX               4



#endif /* INCFILE1_H_ */