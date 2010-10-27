/***************************************************************************
 *
 * Multitouch X driver
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

#define MODULEVENDORSTRING "Nokia"

#include "xorg-server.h"
#include <xorg/exevents.h>
#include <xorg/xserver-properties.h>
#include <X11/Xatom.h>
#include <xf86.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>

#include "common.h"
#include "mtouch.h"

static const char* const axis_labels_str[] = {
	AXIS_LABEL_PROP_ABS_MT_POSITION_X,
	AXIS_LABEL_PROP_ABS_MT_POSITION_Y,
	AXIS_LABEL_PROP_ABS_MT_TOUCH_MAJOR,
	AXIS_LABEL_PROP_ABS_MT_TOUCH_MINOR,
	AXIS_LABEL_PROP_ABS_MT_TRACKING_ID,
};

static void pointer_control(DeviceIntPtr dev, PtrCtrl *ctrl)
{
	xf86Msg(X_INFO, "pointer_control\n");
}

static int pointer_property(DeviceIntPtr dev,
			    Atom property,
			    XIPropertyValuePtr prop,
			    BOOL checkonly)
{
	xf86Msg(X_INFO, "pointer_property\n");
	return Success;
}

static void init_axes_labels(Atom* labels, int num_labels)
{
	int i;

	for (i = 0 ; i < num_labels; i++) {
		labels[i] = MakeAtom(axis_labels_str[i % MT_AXIS_PER_FINGER],
				     strlen(axis_labels_str[i % MT_AXIS_PER_FINGER]),
				     TRUE);
	}
}

static int init_properties(DeviceIntPtr dev)
{
	static const char* const strMaxContacts = "Max Contacts";
	static const char* const strAxesPerContact = "Axes Per Contact";
	int rc;

	Atom labelMaxContacts;
	Atom labelAxesPerContact;

	int max_contacts = MT_NUM_FINGERS;
	int axes_per_contact = MT_AXIS_PER_FINGER;

	labelMaxContacts = MakeAtom(strMaxContacts,
				    strlen(strMaxContacts), TRUE);
	labelAxesPerContact = MakeAtom(strAxesPerContact,
				       strlen(strAxesPerContact), TRUE);

	rc = XIChangeDeviceProperty(dev,
				    labelMaxContacts,
				    XA_INTEGER,
				    8,
				    PropModeReplace,
				    1,
				    &max_contacts,
				    TRUE);
	if (rc != Success)
		return rc;

	XISetDevicePropertyDeletable(dev, labelMaxContacts, FALSE);


	rc = XIChangeDeviceProperty(dev,
				    labelAxesPerContact,
				    XA_INTEGER,
				    8,
				    PropModeReplace,
				    1,
				    &axes_per_contact,
				    TRUE);

	if (rc != Success)
		return rc;

	XISetDevicePropertyDeletable(dev, labelAxesPerContact, FALSE);

	return Success;
}

static int device_init(DeviceIntPtr dev, LocalDevicePtr local)
{
	struct mtev_mtouch *mt = local->private;
	Atom atom;
	int i;
	int j;
	unsigned char map[MT_NUM_BUTTONS + 1];
	Atom btn_labels[MT_NUM_BUTTONS] = { 0 };
	Atom axes_labels[MT_NUM_VALUATORS] = { 0, };
	int r;

	if (MT_NUM_VALUATORS > MAX_VALUATORS) {
		xf86Msg(X_ERROR, "MT_NUM_VALUATORS(%d) > MAX_VALUATORS(%d)\n",
			MT_NUM_VALUATORS, MAX_VALUATORS);
		return BadValue;
	}

	for (i = 0; i < MT_NUM_BUTTONS; i++)
		btn_labels[i] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_UNKNOWN);

	atom = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
	btn_labels[0] = atom;

	init_axes_labels(axes_labels, MT_NUM_VALUATORS);

	r = init_properties(dev);
	if (r != Success)
		return r;

	local->fd = xf86OpenSerial(local->options);
	if (local->fd < 0) {
		xf86Msg(X_ERROR, "mtev: cannot open device\n");
		return !Success;
	}
	if (mtouch_configure(mt, local->fd)) {
		xf86Msg(X_ERROR, "mtev: cannot configure device\n");
		return !Success;
	}
	xf86CloseSerial(local->fd);

	for (i = 0; i < MT_NUM_BUTTONS+1; i++)
		map[i] = i;

	InitPointerDeviceStruct((DevicePtr)dev,
				map,
				MT_NUM_BUTTONS,
				btn_labels,
				pointer_control,
				GetMotionHistorySize(),
				MT_NUM_VALUATORS,
				axes_labels);

	for (i = 0; i < MT_NUM_FINGERS; i++) {
		for (j = 0; j < MT_AXIS_PER_FINGER; j++) {
			const int val = (i * MT_AXIS_PER_FINGER) + j;
			int min;
			int max;

			switch (j) {
			case 0:
				min = mt->caps.abs_position_x.minimum;
				max = mt->caps.abs_position_x.maximum;
				break;
			case 1:
				min = mt->caps.abs_position_y.minimum;
				max = mt->caps.abs_position_y.maximum;
				break;
			case 2:
				min = mt->caps.abs_touch_major.minimum;
				max = mt->caps.abs_touch_major.maximum;
				break;
			case 3:
				if (mt->caps.has_touch_minor) {
					min = mt->caps.abs_touch_minor.minimum;
					max = mt->caps.abs_touch_minor.maximum;
				} else {
					min = mt->caps.abs_touch_major.minimum;
					max = mt->caps.abs_touch_major.maximum;
				}
				break;
			case 4: // Tracking id
				min = mt->caps.abs_tracking_id.minimum;
				max = mt->caps.abs_tracking_id.maximum >
					(MT_NUM_FINGERS-1) ?
					(MT_NUM_FINGERS-1) :
					mt->caps.abs_tracking_id.maximum;
				break;
			default:
				return BadValue;
			}

			xf86InitValuatorAxisStruct(dev, val, axes_labels[val],
						   min,
						   max,
						   1, 0, 1);
			xf86InitValuatorDefaults(dev, val);
		}
	}

	XIRegisterPropertyHandler(dev, pointer_property, NULL, NULL);

	return Success;
}

static int device_on(LocalDevicePtr local)
{
	struct mtev_mtouch *mt = local->private;
	local->fd = xf86OpenSerial(local->options);
	if (local->fd < 0) {
		xf86Msg(X_ERROR, "mtev: cannot open device\n");
		return !Success;
	}
	if (mtouch_open(mt, local->fd)) {
		xf86Msg(X_ERROR, "mtev: cannot grab device\n");
		return !Success;
	}
	xf86AddEnabledDevice(local);
	return Success;
}

static int device_off(LocalDevicePtr local)
{
	struct mtev_mtouch *mt = local->private;
	xf86RemoveEnabledDevice(local);
	if(mtouch_close(mt, local->fd)) {
		xf86Msg(X_WARNING, "mtev: cannot ungrab device\n");
	}
	xf86CloseSerial(local->fd);
	return Success;
}

static int device_close(LocalDevicePtr local)
{
	return Success;
}

static void process_state(LocalDevicePtr local,
			  const struct mtev_mtouch *mt)
{

	const struct mtev_touch_point *tp;
	static int pdown = 0;
	int valuators[MAX_VALUATORS];
	int down;
	int valix;
	int contacts;

	contacts = valix = down = 0;

	while ((tp = mtouch_get_contact(mt, contacts)) != NULL) {
		contacts++;

		// We don't do remapping of tracking id's so
		// make sure clients don't see too high tracking_id numbers
		if (tp->tracking_id < MT_NUM_FINGERS) {
			int x;
			int y;

			x = tp->position_x;
			y = tp->position_y;

			if (mt->invert_x)
				x = mt->caps.abs_position_x.maximum - x +
					mt->caps.abs_position_x.minimum;

			if (mt->invert_y)
				y = mt->caps.abs_position_y.maximum - y
					+ mt->caps.abs_position_y.minimum;

			if (mt->swap_xy) {
				const int tmp = y;
				y = x;
				x = tmp;
			}

			valuators[valix++] = x;
			valuators[valix++] = y;
			valuators[valix++] = tp->touch_major;

			if (mt->caps.has_touch_minor)
				valuators[valix++] = tp->touch_minor;
			else
				valuators[valix++] = tp->touch_major;

			valuators[valix++] = tp->tracking_id;

			down++;
		}

		// Don't deliver more than MaxContacts
		if (down >= MT_NUM_FINGERS)
			break;
	}

	/* Some x-clients assume they get motion events before button down */
	if (down)
		xf86PostMotionEventP(local->dev, TRUE,
				     0, down * MT_AXIS_PER_FINGER, valuators);

	if(down && pdown == 0)
		xf86PostButtonEventP(local->dev, TRUE,
				     1, 1,
				     0, down * MT_AXIS_PER_FINGER, valuators);
	else if (down == 0 && pdown)
		xf86PostButtonEvent(local->dev, TRUE, 1, 0, 0, 0);

	pdown = !!down;
}

/* called for each full received packet from the touchpad */
static void read_input(LocalDevicePtr local)
{
	struct mtev_mtouch *mt = local->private;
	while (mtouch_read_synchronized_event(mt, local->fd)) {
		process_state(local, mt);
	}
}

static Bool device_control(DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = dev->public.devicePrivate;
	switch (mode) {
	case DEVICE_INIT:
		xf86Msg(X_INFO, "device control: init\n");
		return device_init(dev, local);
	case DEVICE_ON:
		xf86Msg(X_INFO, "device control: on\n");
		return device_on(local);
	case DEVICE_OFF:
		xf86Msg(X_INFO, "device control: off\n");
		return device_off(local);
	case DEVICE_CLOSE:
		xf86Msg(X_INFO, "device control: close\n");
		return device_close(local);
	default:
		xf86Msg(X_INFO, "device control: default\n");
		return BadValue;
	}
}

static InputInfoPtr preinit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	struct mtev_mtouch *mt;
	InputInfoPtr local = xf86AllocateInput(drv, 0);
	if (!local)
		goto error;
	mt = calloc(1, sizeof(struct mtev_mtouch));
	if (!mt)
		goto error;

	local->name = dev->identifier;
	local->type_name = XI_TOUCHSCREEN;
	local->device_control = device_control;
	local->read_input = read_input;
	local->private = mt;
	local->flags = XI86_POINTER_CAPABLE |
		XI86_SEND_DRAG_EVENTS;

	local->conf_idev = dev;

	xf86CollectInputOptions(local, NULL, NULL);
	//xf86OptionListReport(local->options);
	xf86ProcessCommonOptions(local, local->options);


	mt->swap_xy = xf86SetBoolOption(local->options, "SwapAxes", FALSE);
	mt->invert_x = xf86SetBoolOption(local->options, "InvertX", FALSE);
	mt->invert_y = xf86SetBoolOption(local->options, "InvertY", FALSE);

	local->flags |= XI86_CONFIGURED;

error:
	return local;
}

static void uninit(InputDriverPtr drv, InputInfoPtr local, int flags)
{
	free(local->private);
	local->private = NULL;
	xf86DeleteInput(local, 0);
}

static InputDriverRec MTEV = {
	.driverVersion = 1,
	.driverName = "mtev",
	.Identify = NULL,
	.PreInit = preinit,
	.UnInit = uninit,
	.module = NULL,
	.refCount = 0
};

static XF86ModuleVersionInfo VERSION = {
	.modname = "mtev",
	.vendor = "Nokia",
	._modinfo1_ = MODINFOSTRING1,
	._modinfo2_ = MODINFOSTRING2,
	.xf86version = XORG_VERSION_CURRENT,
	.majorversion = 0,
	.minorversion = 1,
	.patchlevel = 11,
	.abiclass = ABI_CLASS_XINPUT,
	.abiversion = ABI_XINPUT_VERSION,
	.moduleclass = MOD_CLASS_XINPUT,
	.checksum = {0, 0, 0, 0}
};

static pointer setup(pointer module, pointer options, int *errmaj, int *errmin)
{
	xf86AddInputDriver(&MTEV, module, 0);
	return module;
}

XF86ModuleData mtevModuleData = {
	.vers = &VERSION,
	.setup = &setup,
	.teardown = NULL
};
