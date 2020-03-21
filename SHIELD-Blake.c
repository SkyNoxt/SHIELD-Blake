
#include <linux/hid.h>
#include <linux/module.h>

#define USB_VENDOR_ID_NVIDIA 0x0955
#define USB_DEVICE_ID_NVIDIA_BLAKE 0x7210

#define TP_REPORT_ID 2

#define TP_DEFAULT_X 128
#define TP_DEFAULT_Y 128

#define TP_SPEED_X 1
#define TP_SPEED_Y 1

struct blake_tp
{
	u8 x;
	u8 y;
	u8 action;
};

static int blake_raw_event(struct hid_device* device, struct hid_report* report, u8* data, int size)
{
	struct blake_tp* tp = (struct blake_tp*)hid_get_drvdata(device);

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
		if (tp->action)
		{
			s16 sqrX = ((s16)x - (s16)tp->x) * TP_SPEED_X;
			s16 sqrY = ((s16)y - (s16)tp->y) * TP_SPEED_Y;

			*dx = (sqrX > 0 ? sqrX : -sqrX) * sqrX;
			*dy = (sqrY > 0 ? sqrY : -sqrY) * sqrY;
		}
		else
		{
			tp->action = 1;
			*dx = 0;
			*dy = 0;
		}
		tp->x = x;
		tp->y = y;
	}
	else
	{
		tp->action = 0;
		*dx = 0;
		*dy = 0;
	}

	return 0;
}

static int blake_ff_play(struct input_dev* dev, void* data, struct ff_effect* effect)
{
	struct hid_device* device = input_get_drvdata(dev);
	struct hid_report* report = list_entry(device->report_enum[HID_OUTPUT_REPORT].report_list.next, struct hid_report, list);

	report->field[0]->value[0] = effect->u.rumble.strong_magnitude;
	report->field[0]->value[1] = effect->u.rumble.weak_magnitude;
	report->field[0]->value[2] = effect->replay.length;

	hid_hw_request(device, report, HID_REQ_SET_REPORT);

	return 0;
}

static int blake_init_ff(struct hid_device* device)
{
	struct hid_input* input = list_entry(device->inputs.next, struct hid_input, list);

	set_bit(FF_RUMBLE, input->input->ffbit);
	input_ff_create_memless(input->input, NULL, blake_ff_play);

	return 0;
}

static int blake_probe(struct hid_device* device, const struct hid_device_id* device_id)
{
	struct blake_tp* tp = devm_kzalloc(&device->dev, sizeof(struct blake_tp), GFP_KERNEL);

	tp->x = TP_DEFAULT_X;
	tp->y = TP_DEFAULT_Y;
	tp->action = 0;

	hid_set_drvdata(device, tp);

	hid_parse(device);
	hid_hw_start(device, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	blake_init_ff(device);

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

MODULE_AUTHOR("Artem Pyx <artempyx@gmail.com>");
MODULE_LICENSE("GPL");
