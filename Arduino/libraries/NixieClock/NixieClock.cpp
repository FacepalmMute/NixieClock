/*
    NixieClock.cpp

    Created on: 24.02.2017
    Copyright 2017 Michael Hoffmann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NixieClock.h"

#include <cstdio>
#include <cstring>
#include "Wire.h"
#include "TimeLib.h"
#include "Arduino.h"

//constexpr unsigned char NixieClock::MASK_DOT;
//constexpr unsigned char NixieClock::MASK_COLON;
//constexpr unsigned char NixieClock::HIGH_VOLTAGE_SWITCH;

void NixieClock::begin(const uint8_t twi_address[3], bool has_power_control,
		      bool has_dotcontrol)
{
	for (unsigned i=0; i<sizeof(_twi_address)/sizeof(_twi_address[0]); i++)
		_twi_address[i] = twi_address[i];
	_has_power_control = has_power_control;
	_has_dotcontrol = has_dotcontrol;

	//Power control when supported
	if ( _has_power_control ) {
		pinMode(HIGH_VOLTAGE_SWITCH, OUTPUT);
		power_on();
	}
}

void NixieClock::power_on()
{
	if ( _has_power_control )
		digitalWrite(HIGH_VOLTAGE_SWITCH, HIGH);
}

void NixieClock::power_off()
{
	if ( _has_power_control )
		digitalWrite(HIGH_VOLTAGE_SWITCH, LOW);
}

void NixieClock::update(enum format_t format, time_t time)
{
	char obuf[7] = {0,0,0,0,0,0,0};
	char year_s[5] = {0,0,0,0,0};
	enum {TIME, DATE, YEAR} output_type;

	//Generate string representation of output
	switch (format) {
	case HMS:
		output_type = TIME;
		snprintf(obuf, sizeof(obuf), "%02i%02i%02i", hour(time),
			 minute(time), second(time));
		break;
	case dmy:
		output_type = DATE;
		snprintf(year_s, sizeof(year_s), "%04i", year(time));
		snprintf(obuf, sizeof(obuf), "%02i%02i%s", day(time),
			 month(time), &year_s[2]);
		break;
	case HM:
		output_type = TIME;
		snprintf(obuf, sizeof(obuf), "00%02i%02i", hour(time),
			 minute(time));
		break;
	case dm:
		output_type = DATE;
		snprintf(obuf, sizeof(obuf), "00%02i%02i", day(time),
			 month(time));
		break;
	case Y:
		output_type = YEAR;
		snprintf(obuf, sizeof(obuf), "00%04i", year(time));
		break;
	default:
		return;
	}

	//Prepare binary representation of output
	unsigned char twi_out[3];
	//Index 0 are leftmost digits on display
	twi_out[0] = ((obuf[0]-'0') << 4) | (obuf[1]-'0');
	twi_out[1] = ((obuf[2]-'0') << 4) | (obuf[3]-'0');
	twi_out[2] = ((obuf[4]-'0') << 4) | (obuf[5]-'0');

	if ( _has_dotcontrol ) {
		if ( (output_type == TIME) && ( (time & 1) == 0) )
			twi_out[0] |= MASK_COLON; //May show colon at even seconds
		if ( output_type == DATE )
			twi_out[0] |= MASK_DOT; //May show single dot at date
	}

	//TWI write to buffer ICs
	for (unsigned i=0; i<sizeof(twi_out); i++) {
		Wire.beginTransmission(_twi_address[i]);
		Wire.write(twi_out[i]);
		Wire.endTransmission();
		delayMicroseconds(10);
	}
}
