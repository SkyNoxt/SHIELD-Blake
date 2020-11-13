
#include <linux/hid.h>
#include <linux/module.h>

#define USB_VENDOR_ID_NVIDIA 0x0955
#define USB_DEVICE_ID_NVIDIA_BLAKE 0x7210

#define TP_REPORT_ID 2

#define TP_DEFAULT_X 128
#define TP_DEFAULT_Y 128

#define TP_SPEED_X 1
#define TP_SPEED_Y 1

struct blake_state
{
	u8 x;
	u8 y;
	u8 action;
};

static int blake_raw_event(struct hid_device* device, struct hid_report* report, u8* data, int size)
{
	struct blake_state* state = (struct blake_state*)hid_get_drvdata(device);

	u8 x, y;
	s16 *dx, *dy;

	if (report->id != TP_REPORT_ID)
		return 0;

	x = data[2];
	y = data[4];

	dx = (s16*)&data[2];
	dy = (s16*)&data[4];

	if (data[1] & 0x08)
	{
		if (state->action)
		{
			s16 sqrX = ((s16)x - (s16)state->x) * TP_SPEED_X;
			s16 sqrY = ((s16)y - (s16)state->y) * TP_SPEED_Y;

			*dx = (sqrX > 0 ? sqrX : -sqrX) * sqrX;
			*dy = (sqrY > 0 ? sqrY : -sqrY) * sqrY;
		}
		else
		{
			state->action = 1;
			*dx = 0;
			*dy = 0;
		}
		state->x = x;
		state->y = y;
	}
	else
	{
		state->action = 0;
		*dx = 0;
		*dy = 0;
	}

	return 0;
}

static int blake_upload(struct input_dev* dev, struct ff_effect* effect, struct ff_effect* old)
{
	return 0;
}

static int blake_playback(struct input_dev* dev, int index, int value)
{
	struct hid_device* device = input_get_drvdata(dev);
	struct hid_report* report = list_entry(device->report_enum[HID_OUTPUT_REPORT].report_list.next, struct hid_report, list);

	struct ff_effect* effect = dev->ff->effects + index;
	int* data = report->field[0]->value;

	if (value)
	{
		data[0] = effect->u.rumble.strong_magnitude;
		data[1] = effect->u.rumble.weak_magnitude;
		data[2] = effect->replay.length;
	}
	else
		data[0] = data[1] = data[2] = 0;

	hid_hw_request(device, report, HID_REQ_SET_REPORT);

	return 0;
}

static int blake_probe(struct hid_device* device, const struct hid_device_id* device_id)
{
	struct blake_state* state = devm_kzalloc(&device->dev, sizeof(struct blake_state), GFP_KERNEL);
	struct hid_input* input;

	state->x = TP_DEFAULT_X;
	state->y = TP_DEFAULT_Y;

	hid_set_drvdata(device, state);

	hid_parse(device);
	hid_hw_start(device, HID_CONNECT_DEFAULT);

	input = list_entry(device->inputs.next, struct hid_input, list);

	set_bit(FF_RUMBLE, input->input->ffbit);
	input_ff_create(input->input, 0x10);

	input->input->ff->playback = blake_playback;
	input->input->ff->upload = blake_upload;

	kobject_uevent(&device->dev.kobj, KOBJ_CHANGE);

	return 0;
}

static void blake_remove(struct hid_device* device)
{
	hid_hw_stop(device);
}

static const struct hid_device_id blake_device[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_NVIDIA, USB_DEVICE_ID_NVIDIA_BLAKE) },
	{}
};
MODULE_DEVICE_TABLE(hid, blake_device);

static struct hid_driver blake_driver = {
	.name = "SHIELD-Blake",
	.id_table = blake_device,
	.probe = blake_probe,
	.raw_event = blake_raw_event,
	.remove = blake_remove,
};
module_hid_driver(blake_driver);

MODULE_AUTHOR("Sky Noxt <skynoxt@gmail.com>");
MODULE_LICENSE("GPL");
