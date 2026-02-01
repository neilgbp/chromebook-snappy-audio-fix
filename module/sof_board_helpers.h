/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef __SOF_BOARD_HELPERS_H
#define __SOF_BOARD_HELPERS_H

#include <sound/soc.h>
#include "sof_hdmi_common.h"

/* SSP port configuration macros */
#define SOF_SSP_PORT_CODEC_SHIFT		8
#define SOF_SSP_PORT_CODEC_MASK			(GENMASK(10, 8))
#define SOF_SSP_PORT_CODEC(quirk)		\
	(((quirk) << SOF_SSP_PORT_CODEC_SHIFT) & SOF_SSP_PORT_CODEC_MASK)

/* SSP port for amplifier */
#define SOF_SSP_PORT_AMP_SHIFT			11
#define SOF_SSP_PORT_AMP_MASK			(GENMASK(13, 11))
#define SOF_SSP_PORT_AMP(quirk)			\
	(((quirk) << SOF_SSP_PORT_AMP_SHIFT) & SOF_SSP_PORT_AMP_MASK)

/* SSP port for BT offload */
#define SOF_SSP_PORT_BT_OFFLOAD_SHIFT		14
#define SOF_SSP_PORT_BT_OFFLOAD_MASK		(GENMASK(16, 14))
#define SOF_SSP_PORT_BT_OFFLOAD(quirk)		\
	(((quirk) << SOF_SSP_PORT_BT_OFFLOAD_SHIFT) & SOF_SSP_PORT_BT_OFFLOAD_MASK)

/* BT offload present */
#define SOF_BT_OFFLOAD_PRESENT			BIT(17)

/* Number of HDMI displays */
#define SOF_NUM_IDISP_HDMI_SHIFT		18
#define SOF_NUM_IDISP_HDMI_MASK			(GENMASK(20, 18))
#define SOF_NUM_IDISP_HDMI(quirk)		\
	(((quirk) << SOF_NUM_IDISP_HDMI_SHIFT) & SOF_NUM_IDISP_HDMI_MASK)

/* iDisp codec mask */
#define IDISP_CODEC_MASK	0x4

/* DAI link order macros */
#define SOF_LINK_NONE		0
#define SOF_LINK_CODEC		1
#define SOF_LINK_AMP		2
#define SOF_LINK_DMIC01		3
#define SOF_LINK_DMIC16K	4
#define SOF_LINK_IDISP_HDMI	5
#define SOF_LINK_BT_OFFLOAD	6

#define SOF_LINK_ORDER_SHIFT	4
#define SOF_LINK_ORDER_MASK	0xF

#define SOF_LINK_ORDER(l0, l1, l2, l3, l4, l5, l6)	\
	((l0) | ((l1) << (1 * SOF_LINK_ORDER_SHIFT)) |	\
	 ((l2) << (2 * SOF_LINK_ORDER_SHIFT)) |		\
	 ((l3) << (3 * SOF_LINK_ORDER_SHIFT)) |		\
	 ((l4) << (4 * SOF_LINK_ORDER_SHIFT)) |		\
	 ((l5) << (5 * SOF_LINK_ORDER_SHIFT)) |		\
	 ((l6) << (6 * SOF_LINK_ORDER_SHIFT)))

/* Codec types */
enum {
	CODEC_NONE,
	CODEC_DA7219,
	CODEC_NAU8825,
	CODEC_RT5682,
	CODEC_RT5682S,
	CODEC_CS42L42,
	CODEC_ES8336,
};

/* Amplifier types */
enum {
	CODEC_MAX98357A,
	CODEC_MAX98360A,
	CODEC_MAX98373,
	CODEC_MAX98390,
	CODEC_RT1011,
	CODEC_RT1015,
	CODEC_RT1015P,
	CODEC_RT1019P,
	CODEC_CS35L41,
};

struct sof_da7219_dai_ops {
	int (*init)(struct snd_soc_pcm_runtime *rtd);
	void (*exit)(struct snd_soc_pcm_runtime *rtd);
	const struct snd_soc_ops *ops;
	bool mclk_en;
};

struct sof_card_private {
	struct list_head hdmi_pcm_list;
	struct snd_soc_jack headset_jack;
	struct sof_hdmi_private hdmi;
	unsigned int codec_type;
	unsigned int amp_type;
	int ssp_codec;
	int ssp_amp;
	int ssp_bt;
	int dmic_be_num;
	unsigned long link_order_overwrite;
	struct sof_da7219_dai_ops da7219_dai_ops;
};

struct sof_card_private *sof_intel_board_get_ctx(struct device *dev,
						 unsigned long board_quirk);
int sof_intel_board_set_dai_link(struct device *dev,
				 struct snd_soc_card *card,
				 struct sof_card_private *ctx);

#endif /* __SOF_BOARD_HELPERS_H */
