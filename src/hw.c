/***************************************************************************
 *
 * Multitouch protocol X driver
 * Copyright (C) 2008 Henrik Rydberg <rydberg@euromail.se>
 * Copyright (C) 2009,2010 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 **************************************************************************/

#include <string.h>
#include <linux/input.h>

#include "hw.h"

void hw_init(struct mtev_hw_state *hw)
{
	memset(hw, 0, sizeof(struct mtev_hw_state));
}

bool hw_read(struct mtev_hw_state *hw, const struct input_event* ev)
{
	// xf86Msg(X_INFO, "event: %d %d %d\n", ev->type, ev->code, ev->value);

	switch (ev->type) {
	case EV_SYN:
		switch (ev->code) {
		case SYN_REPORT:
			hw->num_contacts = hw->num_read;
			hw->num_read = 0;
			hw->num_abs_read = 0;
			return 1;
		case SYN_MT_REPORT:
			if (hw->num_read < HW_MAX_CONTACTS &&
				hw->num_abs_read > 0 &&
				hw->contact[hw->num_read].touch_major > 0) {
				hw->num_read++;
			}
			break;
		}
		break;
	case EV_ABS:
		if (hw->num_read == HW_MAX_CONTACTS)
			break;
		switch (ev->code) {
		case ABS_MT_POSITION_X:
			hw->contact[hw->num_read].position_x = ev->value;
			break;
		case ABS_MT_POSITION_Y:
			hw->contact[hw->num_read].position_y = ev->value;
			break;
		case ABS_MT_TOUCH_MAJOR:
			hw->contact[hw->num_read].touch_major = ev->value;
			break;
		case ABS_MT_TOUCH_MINOR:
			hw->contact[hw->num_read].touch_minor = ev->value;
			break;
		case ABS_MT_WIDTH_MAJOR:
			hw->contact[hw->num_read].width_major = ev->value;
			break;
		case ABS_MT_WIDTH_MINOR:
			hw->contact[hw->num_read].width_minor = ev->value;
			break;
		case ABS_MT_ORIENTATION:
			hw->contact[hw->num_read].orientation = ev->value;
			break;
		case ABS_MT_PRESSURE:
			hw->contact[hw->num_read].pressure = ev->value;
			break;
		case ABS_MT_TRACKING_ID:
			hw->contact[hw->num_read].tracking_id = ev->value;
			break;
		}
		hw->num_abs_read++;
	}

	return 0;
}
