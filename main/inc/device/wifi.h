/*
 * wifi.h
 *
 *  Created on: 2018-02-11 06:56
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef DEVICE_WIFI_H_
#define DEVICE_WIFI_H_

extern char wifi_hostname[40];
extern char wifi_mac_string[18];
extern char wifi_mac_address[6];

extern void wifi_init(void);

#endif /* DEVICE_WIFI_H_ */
