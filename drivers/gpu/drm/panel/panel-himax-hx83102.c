// SPDX-License-Identifier: GPL-2.0
/*
 * DRM driver for Himax HX83102 based MIPI DSI panels
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct hx83102_panel_desc {
	const struct drm_display_mode *mode;
	int (*init)(struct mipi_dsi_device *dsi);
};

struct hx83102 {
	struct device *dev;
	struct drm_panel panel;
	struct regulator *supply;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *enable_gpio;
	struct mipi_dsi_device *dsi;
	const struct hx83102_panel_desc *desc;
	bool prepared;
};

static inline struct hx83102 *to_hx83102(struct drm_panel *panel)
{
	return container_of(panel, struct hx83102, panel);
}

static int productshop_init(struct mipi_dsi_device *dsi)
{
	int ret;
	u8 id_data[3];
	
	dev_info(&dsi->dev, "ProductShop HX83102E: Starting initialization sequence\n");
	
	/* ProductShop 12" manufacturer initialization sequence */
	
	/* Test DSI write command validity by checking return codes */
	dev_info(&dsi->dev, "Testing DSI write command responses...\n");
	
	/* Test 1: Valid extension command - should succeed */
	ret = mipi_dsi_dcs_write(dsi, 0xB9, "\x83\x10\x2E", 3);
	dev_info(&dsi->dev, "Valid extension command (0xB9): ret=%d\n", ret);
	
	/* Test 2: Standard DCS command - should succeed */
	ret = mipi_dsi_dcs_write(dsi, 0x11, NULL, 0); // Sleep out
	dev_info(&dsi->dev, "Standard DCS sleep out (0x11): ret=%d\n", ret);
	
	/* Test 3: Another standard command - should succeed */
	ret = mipi_dsi_dcs_write(dsi, 0x10, NULL, 0); // Sleep in
	dev_info(&dsi->dev, "Standard DCS sleep in (0x10): ret=%d\n", ret);
	
	/* Test 4: Extension command again to verify it's working */
	ret = mipi_dsi_dcs_write(dsi, 0xB9, "\x83\x10\x2E", 3);
	dev_info(&dsi->dev, "Extension command again (0xB9): ret=%d\n", ret);
	
	dev_info(&dsi->dev, "Extension command access test completed\n");
	
	/* Try multiple read commands to check what data we can get */
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x04, id_data, 3);
	dev_info(&dsi->dev, "DCS Read 0x04 (Display ID): ret=%d, data=[%02x %02x %02x]\n", 
		 ret, id_data[0], id_data[1], id_data[2]);
	
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x0A, id_data, 1);
	dev_info(&dsi->dev, "DCS Read 0x0A (Power Mode): ret=%d, data=[%02x]\n", 
		 ret, id_data[0]);
	
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x0B, id_data, 1);
	dev_info(&dsi->dev, "DCS Read 0x0B (Address Mode): ret=%d, data=[%02x]\n", 
		 ret, id_data[0]);
	
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x0C, id_data, 1);
	dev_info(&dsi->dev, "DCS Read 0x0C (Pixel Format): ret=%d, data=[%02x]\n", 
		 ret, id_data[0]);
	
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x0D, id_data, 1);
	dev_info(&dsi->dev, "DCS Read 0x0D (Display Mode): ret=%d, data=[%02x]\n", 
		 ret, id_data[0]);
	
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x0E, id_data, 1);
	dev_info(&dsi->dev, "DCS Read 0x0E (Signal Mode): ret=%d, data=[%02x]\n", 
		 ret, id_data[0]);
	
	memset(id_data, 0, sizeof(id_data));
	ret = mipi_dsi_dcs_read(dsi, 0x0F, id_data, 1);
	dev_info(&dsi->dev, "DCS Read 0x0F (Diagnostic): ret=%d, data=[%02x]\n", 
		 ret, id_data[0]);
	mipi_dsi_dcs_write_seq(dsi, 0xE9, 0xCD);
	mipi_dsi_dcs_write_seq(dsi, 0xBB, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xE9, 0x00);
	
	/* Power control and timing */
	mipi_dsi_dcs_write_seq(dsi, 0xD1, 0x67, 0x0C, 0xFF, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xB1, 0x10, 0xFA, 0xAF, 0xAF, 0x29, 0x29, 0xC2, 0x6C, 0x43, 0x36, 0x36, 0x36, 0x36, 0x22, 0x21, 0x15, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xB2, 0x00, 0xB0, 0x47, 0xD0, 0x00, 0x26, 0xE0, 0x26, 0x13, 0x03, 0x00, 0x00, 0x15, 0x20, 0xD7, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xB4, 0x40, 0x48, 0x10, 0x28, 0x10, 0x78, 0x10, 0x50, 0x01, 0x9F, 0x01, 0x58, 0x00, 0xFF, 0x00, 0xFF);
	mipi_dsi_dcs_write_seq(dsi, 0xD2, 0x29, 0x29);
	mipi_dsi_dcs_write_seq(dsi, 0xBF, 0xFC, 0x85, 0x80, 0x9C, 0x36, 0x00, 0x04);
	
	/* Display configuration */
	mipi_dsi_dcs_write_seq(dsi, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x3C, 0xE8, 0x00, 0x00, 0x00, 0x37, 0x47, 0x44, 0x4F, 0x26, 0x26, 0x00, 0x00, 0x65, 0x10, 0x1C, 0x00, 0x1C, 0x32, 0x17, 0xF8, 0x07, 0xF8, 0x32, 0x17, 0xF8, 0x07, 0xF8, 0x00, 0x00, 0x2B, 0x50, 0xB6, 0xC8, 0x2C, 0x4F, 0xB6, 0xC8, 0x0F);
	
	/* Bank selection and configuration */
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xCB, 0x80, 0x36, 0x12, 0x16, 0xC0, 0x28, 0x54, 0x84, 0x02, 0x34);
	mipi_dsi_dcs_write_seq(dsi, 0xD3, 0x01, 0x40, 0xFC, 0x00, 0x00, 0x11, 0x10, 0x00, 0x0A, 0x00, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xB4, 0x4E, 0x00, 0x33, 0x11, 0x33, 0x88);
	mipi_dsi_dcs_write_seq(dsi, 0xBF, 0xF2, 0x00, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x00);
	
	/* Color and gamma settings */
	mipi_dsi_dcs_write_seq(dsi, 0xC0, 0x33, 0x33, 0x22, 0x11, 0xA2, 0x17, 0x00, 0x80, 0x00, 0x00, 0x08, 0x00, 0x63, 0x63);
	mipi_dsi_dcs_write_seq(dsi, 0xD5, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x28, 0x29, 0x20, 0x21, 0x1B, 0x1B, 0x1A, 0x1A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x22, 0x23, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0xD6, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x23, 0x22, 0x21, 0x20, 0x1B, 0x1A, 0x1A, 0x1B, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x29, 0x28, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0xD0, 0x07, 0x04, 0x05);
	
	/* Extended registers */
	mipi_dsi_dcs_write_seq(dsi, 0xE7, 0x12, 0x16, 0x02, 0x02, 0x55, 0x00, 0x0E, 0x0E, 0x00, 0x26, 0x29, 0x72, 0x1C, 0x72, 0x01, 0x27, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x68);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xE7, 0x01, 0x30, 0x01, 0x94, 0x0D, 0xB5, 0x0E);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xE7, 0xFF, 0x01, 0xFD, 0x01, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0x02, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x00);
	
	/* Gamma correction */
	mipi_dsi_dcs_write_seq(dsi, 0xE0, 0x00, 0x08, 0x15, 0x1E, 0x27, 0x48, 0x62, 0x6A, 0x71, 0x6D, 0x86, 0x8A, 0x8E, 0x9B, 0x97, 0x9C, 0xA4, 0xB5, 0xB2, 0xD7, 0xDE, 0xE8, 0x73, 0x00, 0x08, 0x15, 0x1E, 0x27, 0x48, 0x62, 0x6A, 0x71, 0x6D, 0x86, 0x8A, 0x8E, 0x9B, 0x97, 0x9C, 0xA4, 0xB5, 0xB2, 0xD7, 0xDE, 0xE8, 0x73);
	
	/* Final configuration */
	mipi_dsi_dcs_write_seq(dsi, 0xCC, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xBA, 0x70, 0x03, 0xA8, 0x83, 0xF2, 0x80, 0xC0, 0x0D);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xB2, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xBD, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xC8, 0x00, 0x04, 0x04, 0x00, 0x00, 0x02, 0x13, 0xFF);
	mipi_dsi_dcs_write_seq(dsi, 0xD1, 0x67, 0x0C, 0x0F);
	mipi_dsi_dcs_write_seq(dsi, 0xE1, 0x00, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xB9, 0x00, 0x00, 0x00);

	/* Sleep out and display on with manufacturer timing */
	dev_info(&dsi->dev, "Sending sleep out command\n");
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(&dsi->dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(800);  /* 800ms delay as specified by manufacturer */

	dev_info(&dsi->dev, "Sending display on command\n");
	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(&dsi->dev, "Failed to turn on display: %d\n", ret);
		return ret;
	}
	msleep(120);  /* 120ms delay as specified by manufacturer */

	dev_info(&dsi->dev, "ProductShop HX83102E: Initialization completed successfully\n");
	return 0;
}

static const struct drm_display_mode productshop_mode = {
	.clock = 171000,  /* 171 MHz PCLK as specified by manufacturer */
	.hdisplay = 1200,  /* HAdr (Horizontal Active Width) */
	.hsync_start = 1200 + 24,  /* HAdr + HFP (Horizontal Front Porch) */
	.hsync_end = 1200 + 24 + 12,  /* HAdr + HFP + Hsync (Horizontal Sync Pulse Width) */
	.htotal = 1200 + 24 + 12 + 24,  /* HAdr + HFP + Hsync + HBP (Horizontal Back Porch) */
	.vdisplay = 2000,  /* VAdr (Vertical Active Height) */
	.vsync_start = 2000 + 224,  /* VAdr + VFP (Vertical Front Porch) */
	.vsync_end = 2000 + 224 + 12,  /* VAdr + VFP + Vsync (Vertical Sync Pulse Width) */
	.vtotal = 2000 + 224 + 12 + 28,  /* VAdr + VFP + Vsync + VBP (Vertical Back Porch) */
	.width_mm = 135,  /* 12" display physical width */
	.height_mm = 216, /* 12" display physical height */
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct hx83102_panel_desc productshop_desc = {
	.mode = &productshop_mode,
	.init = productshop_init,
};

static int hx83102_disable(struct drm_panel *panel)
{
	struct hx83102 *ctx = to_hx83102(panel);

	mipi_dsi_dcs_set_display_off(ctx->dsi);
	mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	msleep(120);

	return 0;
}

static int hx83102_unprepare(struct drm_panel *panel)
{
	struct hx83102 *ctx = to_hx83102(panel);

	if (!ctx->prepared)
		return 0;

	gpiod_set_value_cansleep(ctx->enable_gpio, 0);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	if (ctx->supply)
		regulator_disable(ctx->supply);
	ctx->prepared = false;

	return 0;
}

static int hx83102_prepare(struct drm_panel *panel)
{
	struct hx83102 *ctx = to_hx83102(panel);
	int ret;

	dev_info(ctx->dev, "Panel prepare called\n");

	if (ctx->prepared) {
		dev_info(ctx->dev, "Panel already prepared, skipping\n");
		return 0;
	}

	if (ctx->supply) {
		ret = regulator_enable(ctx->supply);
		if (ret < 0)
			return ret;
	}

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(25);
	gpiod_set_value_cansleep(ctx->enable_gpio, 1);
	msleep(200);

	ret = ctx->desc->init(ctx->dsi);
	if (ret < 0) {
		dev_err(ctx->dev, "Panel init failed: %d\n", ret);
		goto poweroff;
	}

	ctx->prepared = true;
	return 0;

poweroff:
	gpiod_set_value_cansleep(ctx->enable_gpio, 0);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	if (ctx->supply)
		regulator_disable(ctx->supply);
	return ret;
}

static int hx83102_enable(struct drm_panel *panel)
{
	return 0;
}

static int hx83102_get_modes(struct drm_panel *panel,
			      struct drm_connector *connector)
{
	struct hx83102 *ctx = to_hx83102(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ctx->desc->mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs hx83102_panel_funcs = {
	.disable = hx83102_disable,
	.unprepare = hx83102_unprepare,
	.prepare = hx83102_prepare,
	.enable = hx83102_enable,
	.get_modes = hx83102_get_modes,
};

static int hx83102_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct hx83102 *ctx;
	int ret;

	dev_info(dev, "ProductShop HX83102E panel probe started\n");

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->desc = of_device_get_match_data(dev);
	if (!ctx->desc)
		return -ENODEV;

	ctx->supply = devm_regulator_get_optional(dev, "power");
	if (IS_ERR(ctx->supply))
		ctx->supply = NULL;

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return PTR_ERR(ctx->reset_gpio);

	ctx->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->enable_gpio))
		return PTR_ERR(ctx->enable_gpio);

	ctx->dsi = dsi;
	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &hx83102_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return ret;

	drm_panel_add(&ctx->panel);

	mipi_dsi_set_drvdata(dsi, ctx);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		if (ret == -EBUSY || ret == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		return ret;
	}

	dev_info(dev, "ProductShop HX83102E panel probe completed successfully\n");
	return 0;
}

static void hx83102_remove(struct mipi_dsi_device *dsi)
{
	struct hx83102 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id hx83102_of_match[] = {
	{ .compatible = "himax,hx83102e", .data = &productshop_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, hx83102_of_match);

static struct mipi_dsi_driver hx83102_driver = {
	.probe = hx83102_probe,
	.remove = hx83102_remove,
	.driver = {
		.name = "panel-himax-hx83102",
		.of_match_table = hx83102_of_match,
	},
};
module_mipi_dsi_driver(hx83102_driver);

MODULE_AUTHOR("Ahmed Hadjeres <ahmed@productshop.io>");
MODULE_DESCRIPTION("DRM driver for Himax HX83102 based DSI panels");
MODULE_LICENSE("GPL");