// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
//
// This file is provided under a dual BSD/GPLv2 license.  When using or
// redistributing this file, you may do so under either license.
//
// Copyright(c) 2021-2023 Intel Corporation
//
// Modified to add support for BXT/APL Chromebooks (bxt_da7219_mx98357a)
//

#include <linux/input.h>
#include <linux/module.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include <sound/sof.h>
#include "da7219.h"
#include "sof_board_helpers.h"
#include "sof_maxim_common.h"

#define SOF_DA7219_GLK_BOARD			BIT(0)
#define SOF_DA7219_CML_BOARD			BIT(1)
#define SOF_DA7219_JSL_BOARD			BIT(2)
#define SOF_DA7219_MCLK_EN			BIT(3)
#define SOF_DA7219_BXT_BOARD			BIT(4)

#define DIALOG_CODEC_DAI	"da7219-hifi"

static int platform_clock_control(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *k, int  event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	int ret = 0;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		ret = snd_soc_dai_set_sysclk(snd_soc_card_get_codec_dai(card, DIALOG_CODEC_DAI),
					     0, 24576000, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			dev_err(card->dev, "fail to set da7219 sysclk: %d\n", ret);
			return ret;
		}
	} else {
		ret = snd_soc_dai_set_sysclk(snd_soc_card_get_codec_dai(card, DIALOG_CODEC_DAI),
					     0, 0, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			dev_err(card->dev, "fail to stop da7219 sysclk: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static const struct snd_soc_dapm_widget dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			    platform_clock_control, SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* HP jack connectors */
	{"Headphone Jack", NULL, "HPL"},
	{"Headphone Jack", NULL, "HPR"},

	{"MIC", NULL, "Headset Mic"},

	/* other jacks */
	{"Line Out", NULL, "HPL"},
	{"Line Out", NULL, "HPR"},

	/* digital mics */
	{"DMic", NULL, "SoC DMIC"},

	/* CODEC BE connections */
	{"HPOL", NULL, "Headphones"},
	{"HPOR", NULL, "Headphones"},
	{"MIC", NULL, "Capture"},

	/* Platform clock */
	{"Headphone Jack", NULL, "Platform Clock"},
	{"Headset Mic", NULL, "Platform Clock"},
	{"Line Out", NULL, "Platform Clock"},
};

static struct snd_soc_jack_pin naau8825_pins[] = {
	{
		.pin    = "Headphone Jack",
		.mask   = SND_JACK_HEADPHONE,
	},
	{
		.pin    = "Headset Mic",
		.mask   = SND_JACK_MICROPHONE,
	},
};

static int codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct sof_card_private *ctx = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	struct snd_soc_component *component = codec_dai->component;
	struct snd_soc_jack *jack = &ctx->headset_jack;
	int ret;

	ret = snd_soc_dapm_add_routes(&rtd->card->dapm, audio_map,
				      ARRAY_SIZE(audio_map));
	if (ret) {
		dev_err(rtd->dev, "fail to add dapm routes: %d\n", ret);
		return ret;
	}

	/*
	 * Headset buttons map to the google Reference Design
	 * The first 8 buttons are for playpause, volume up/down, mute,
	 * previous track, next track, launch assistant and voice activate.
	 */
	ret = snd_soc_card_jack_new_pins(rtd->card, "Headset Jack",
					 SND_JACK_HEADSET | SND_JACK_BTN_0 |
					 SND_JACK_BTN_1 | SND_JACK_BTN_2 |
					 SND_JACK_BTN_3 | SND_JACK_LINEOUT,
					 jack,
					 naau8825_pins,
					 ARRAY_SIZE(naau8825_pins));
	if (ret) {
		dev_err(rtd->dev, "Headset Jack creation failed: %d\n", ret);
		return ret;
	}

	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_PLAYPAUSE);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEDOWN);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOICECOMMAND);

	ret = snd_soc_component_set_jack(component, jack, NULL);
	if (ret) {
		dev_err(rtd->dev, "fail to set da7219 jack: %d\n", ret);
		return ret;
	}

	return 0;
}

static void codec_exit(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	struct snd_soc_component *component = codec_dai->component;

	snd_soc_component_set_jack(component, NULL, NULL);
}

static int codec_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);
	int ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
				     24576000, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(rtd->dev, "fail to set da7219 sysclk: %d\n", ret);

	return ret;
}

static const struct snd_soc_ops codec_ops = {
	.hw_params = codec_hw_params,
};

static int ssp1_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_dai *codec_dai;
	int i, ret;

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		if (strcmp(codec_dai->component->name,
			   "da7219.5-001a") == 0) {
			ret = snd_soc_dai_set_sysclk(codec_dai, 0,
						     24576000,
						     SND_SOC_CLOCK_IN);
			if (ret < 0) {
				dev_err(rtd->dev,
					"fail to set da7219 sysclk: %d\n",
					ret);
				return ret;
			}
		}
	}

	return 0;
}

static const struct snd_soc_ops ssp1_ops = {
	.hw_params = ssp1_hw_params,
};

static struct sof_dai_link_ops codec_dai_ops = {
	.init = codec_init,
	.exit = codec_exit,
	.ops = &codec_ops,
};

static struct snd_soc_card card_da7219 = {
	.name = "da7219max",
	.owner = THIS_MODULE,
	.dapm_widgets = dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dapm_widgets),
	.fully_routed = true,
};

#define GLK_LINK_ORDER	SOF_LINK_ORDER(SOF_LINK_AMP,         \
					SOF_LINK_CODEC,      \
					SOF_LINK_DMIC01,     \
					SOF_LINK_IDISP_HDMI, \
					SOF_LINK_NONE,       \
					SOF_LINK_NONE,       \
					SOF_LINK_NONE)

#define CML_LINK_ORDER	SOF_LINK_ORDER(SOF_LINK_AMP,         \
					SOF_LINK_CODEC,      \
					SOF_LINK_DMIC01,     \
					SOF_LINK_IDISP_HDMI, \
					SOF_LINK_DMIC16K,    \
					SOF_LINK_NONE,       \
					SOF_LINK_NONE)

#define JSL_LINK_ORDER	SOF_LINK_ORDER(SOF_LINK_AMP,         \
					SOF_LINK_CODEC,      \
					SOF_LINK_DMIC01,     \
					SOF_LINK_IDISP_HDMI, \
					SOF_LINK_DMIC16K,    \
					SOF_LINK_NONE,       \
					SOF_LINK_NONE)

static int audio_probe(struct platform_device *pdev)
{
	struct snd_soc_acpi_mach *mach = pdev->dev.platform_data;
	struct sof_card_private *ctx;
	char *card_name;
	unsigned long board_quirk = 0;
	int ret;

	if (pdev->id_entry && pdev->id_entry->driver_data)
		board_quirk = (unsigned long)pdev->id_entry->driver_data;

	dev_dbg(&pdev->dev, "board_quirk = %lx\n", board_quirk);

	/* initialize ctx with board quirk */
	ctx = sof_intel_board_get_ctx(&pdev->dev, board_quirk);
	if (!ctx)
		return -ENOMEM;

	if (mach->mach_params.codec_mask & IDISP_CODEC_MASK)
		ctx->hdmi.idisp_codec = true;

	if (board_quirk & SOF_DA7219_BXT_BOARD) {
		/* dmic16k not supported on BXT */
		ctx->dmic_be_num = 1;
		/* overwrite the DAI link order for BXT boards */
		ctx->link_order_overwrite = GLK_LINK_ORDER;
		switch (ctx->amp_type) {
		case CODEC_MAX98357A:
			card_name = devm_kstrdup(&pdev->dev, "bxtda7219max",
						 GFP_KERNEL);
			if (!card_name)
				return -ENOMEM;
			card_da7219.name = card_name;
			break;
		default:
			break;
		}
	} else if (board_quirk & SOF_DA7219_GLK_BOARD) {
		/* dmic16k not support */
		ctx->dmic_be_num = 1;
		/* overwrite the DAI link order for GLK boards */
		ctx->link_order_overwrite = GLK_LINK_ORDER;
		/* backward-compatible with existing devices */
		switch (ctx->amp_type) {
		case CODEC_MAX98357A:
			card_name = devm_kstrdup(&pdev->dev, "glkda7219max",
						 GFP_KERNEL);
			if (!card_name)
				return -ENOMEM;
			card_da7219.name = card_name;
			break;
		default:
			break;
		}
	} else if (board_quirk & SOF_DA7219_CML_BOARD) {
		/* overwrite the DAI link order for CML boards */
		ctx->link_order_overwrite = CML_LINK_ORDER;
		/* backward-compatible with existing devices */
		switch (ctx->amp_type) {
		case CODEC_MAX98357A:
			card_name = devm_kstrdup(&pdev->dev, "cmlda7219max",
						 GFP_KERNEL);
			if (!card_name)
				return -ENOMEM;
			card_da7219.name = card_name;
			break;
		default:
			break;
		}
	} else if (board_quirk & SOF_DA7219_JSL_BOARD) {
		/* overwrite the DAI link order for JSL boards */
		ctx->link_order_overwrite = JSL_LINK_ORDER;
		/* backward-compatible with existing devices */
		switch (ctx->amp_type) {
		case CODEC_MAX98357A:
			card_name = devm_kstrdup(&pdev->dev, "jslda7219max",
						 GFP_KERNEL);
			if (!card_name)
				return -ENOMEM;
			card_da7219.name = card_name;
			break;
		default:
			break;
		}
	}

	if (board_quirk & SOF_DA7219_MCLK_EN)
		ctx->da7219_dai_ops.mclk_en = true;

	ctx->codec_type = CODEC_DA7219;
	ctx->da7219_dai_ops.init = codec_init;
	ctx->da7219_dai_ops.exit = codec_exit;
	ctx->da7219_dai_ops.ops = &codec_ops;

	card_da7219.dev = &pdev->dev;

	ret = sof_intel_board_set_dai_link(&pdev->dev, &card_da7219, ctx);
	if (ret)
		return ret;

	snd_soc_card_set_drvdata(&card_da7219, ctx);
	return devm_snd_soc_register_card(&pdev->dev, &card_da7219);
}

static const struct platform_device_id board_ids[] = {
	{
		.name = "bxt_da7219_mx98357a",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_BXT_BOARD |
					SOF_SSP_PORT_CODEC(1) |
					SOF_SSP_PORT_AMP(5)),
	},
	{
		.name = "glk_da7219_def",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_GLK_BOARD |
					SOF_SSP_PORT_CODEC(2) |
					SOF_SSP_PORT_AMP(1)),
	},
	{
		.name = "cml_da7219_def",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_CML_BOARD |
					SOF_SSP_PORT_CODEC(0) |
					SOF_SSP_PORT_AMP(1)),
	},
	{
		.name = "jsl_da7219_def",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_JSL_BOARD |
					SOF_SSP_PORT_CODEC(0) |
					SOF_SSP_PORT_AMP(1)),
	},
	{
		.name = "adl_da7219_def",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_MCLK_EN |
					SOF_SSP_PORT_CODEC(0) |
					SOF_SSP_PORT_AMP(1) |
					SOF_NUM_IDISP_HDMI(4) |
					SOF_SSP_PORT_BT_OFFLOAD(2) |
					SOF_BT_OFFLOAD_PRESENT),
	},
	{
		.name = "rpl_da7219_def",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_MCLK_EN |
					SOF_SSP_PORT_CODEC(0) |
					SOF_SSP_PORT_AMP(1) |
					SOF_NUM_IDISP_HDMI(4) |
					SOF_SSP_PORT_BT_OFFLOAD(2) |
					SOF_BT_OFFLOAD_PRESENT),
	},
	{
		.name = "mtl_da7219_def",
		.driver_data = (kernel_ulong_t)(SOF_DA7219_MCLK_EN |
					SOF_SSP_PORT_CODEC(2) |
					SOF_SSP_PORT_AMP(0) |
					SOF_SSP_PORT_BT_OFFLOAD(1) |
					SOF_BT_OFFLOAD_PRESENT),
	},
	{ }
};
MODULE_DEVICE_TABLE(platform, board_ids);

static struct platform_driver audio = {
	.probe = audio_probe,
	.driver = {
		.name = "sof_da7219",
		.pm = &snd_soc_pm_ops,
	},
	.id_table = board_ids,
};

module_platform_driver(audio);

MODULE_DESCRIPTION("ASoC Intel(R) SOF + DA7219 Machine driver");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_IMPORT_NS(SND_SOC_INTEL_SOF_BOARD_HELPERS);
MODULE_IMPORT_NS(SND_SOC_INTEL_SOF_MAXIM_COMMON);
