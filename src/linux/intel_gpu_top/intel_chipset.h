/*
 * Copyright © 2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef _INTEL_CHIPSET_H
#define _INTEL_CHIPSET_H

#include <stdbool.h>
#include <stdint.h>

#define BIT(x) (1ul <<(x))

struct intel_device_info {
	unsigned graphics_ver;
	unsigned graphics_rel;
	unsigned display_ver;
	unsigned gt; /* 0 if unknown */
	bool has_4tile : 1;
	bool has_flatccs : 1;
	bool has_oam : 1;
	bool is_mobile : 1;
	bool is_whitney : 1;
	bool is_almador : 1;
	bool is_brookdale : 1;
	bool is_montara : 1;
	bool is_springdale : 1;
	bool is_grantsdale : 1;
	bool is_alviso : 1;
	bool is_lakeport : 1;
	bool is_calistoga : 1;
	bool is_bearlake : 1;
	bool is_pineview : 1;
	bool is_broadwater : 1;
	bool is_crestline : 1;
	bool is_eaglelake : 1;
	bool is_cantiga : 1;
	bool is_ironlake : 1;
	bool is_arrandale : 1;
	bool is_sandybridge : 1;
	bool is_ivybridge : 1;
	bool is_valleyview : 1;
	bool is_haswell : 1;
	bool is_broadwell : 1;
	bool is_cherryview : 1;
	bool is_skylake : 1;
	bool is_broxton : 1;
	bool is_kabylake : 1;
	bool is_geminilake : 1;
	bool is_coffeelake : 1;
	bool is_cometlake : 1;
	bool is_cannonlake : 1;
	bool is_icelake : 1;
	bool is_elkhartlake : 1;
	bool is_jasperlake : 1;
	bool is_tigerlake : 1;
	bool is_rocketlake : 1;
	bool is_dg1 : 1;
	bool is_dg2 : 1;
	bool is_alderlake_s : 1;
	bool is_raptorlake_s : 1;
	bool is_alderlake_p : 1;
	bool is_alderlake_n : 1;
	bool is_meteorlake : 1;
	bool is_pontevecchio : 1;
	bool is_lunarlake : 1;
	bool is_battlemage : 1;
	const char *codename;
};

const struct intel_device_info *intel_get_device_info(uint16_t devid) __attribute__((pure));

extern enum pch_type intel_pch;

enum pch_type {
	PCH_NONE,
	PCH_IBX,
	PCH_CPT,
	PCH_LPT,
};

void intel_check_pch(void);

#define HAS_IBX (intel_pch == PCH_IBX)
#define HAS_CPT (intel_pch == PCH_CPT)
#define HAS_LPT (intel_pch == PCH_LPT)

#define IP_VER(ver, rel)		((ver) << 8 | (rel))

/* Exclude chipset #defines, they just add noise */
#ifndef __GTK_DOC_IGNORE__

#define PCI_CHIP_I810			0x7121
#define PCI_CHIP_I810_DC100		0x7123
#define PCI_CHIP_I810_E			0x7125
#define PCI_CHIP_I815			0x1132

#define PCI_CHIP_I830_M			0x3577
#define PCI_CHIP_845_G			0x2562
#define PCI_CHIP_I854_G			0x358e
#define PCI_CHIP_I855_GM		0x3582
#define PCI_CHIP_I865_G			0x2572

#define PCI_CHIP_I915_G			0x2582
#define PCI_CHIP_E7221_G		0x258A
#define PCI_CHIP_I915_GM		0x2592
#define PCI_CHIP_I945_G			0x2772
#define PCI_CHIP_I945_GM		0x27A2
#define PCI_CHIP_I945_GME		0x27AE

#define PCI_CHIP_I965_G			0x29A2
#define PCI_CHIP_I965_Q			0x2992
#define PCI_CHIP_I965_G_1		0x2982
#define PCI_CHIP_I946_GZ		0x2972
#define PCI_CHIP_I965_GM		0x2A02
#define PCI_CHIP_I965_GME		0x2A12

#define PCI_CHIP_GM45_GM		0x2A42

#define PCI_CHIP_Q45_G			0x2E12
#define PCI_CHIP_G45_G			0x2E22
#define PCI_CHIP_G41_G			0x2E32

#endif /* __GTK_DOC_IGNORE__ */

#define IS_915G(devid)		(intel_get_device_info(devid)->is_grantsdale)
#define IS_915GM(devid)		(intel_get_device_info(devid)->is_alviso)

#define IS_915(devid)		(IS_915G(devid) || IS_915GM(devid))

#define IS_945G(devid)		(intel_get_device_info(devid)->is_lakeport)
#define IS_945GM(devid)		(intel_get_device_info(devid)->is_calistoga)

#define IS_945(devid)		(IS_945G(devid) || \
				 IS_945GM(devid) || \
				 IS_G33(devid))

#define IS_PINEVIEW(devid)	(intel_get_device_info(devid)->is_pineview)
#define IS_G33(devid)		(intel_get_device_info(devid)->is_bearlake || \
				 intel_get_device_info(devid)->is_pineview)

#define IS_BROADWATER(devid)	(intel_get_device_info(devid)->is_broadwater)
#define IS_CRESTLINE(devid)	(intel_get_device_info(devid)->is_crestline)

#define IS_GM45(devid)		(intel_get_device_info(devid)->is_cantiga)
#define IS_G45(devid)		(intel_get_device_info(devid)->is_eaglelake)
#define IS_G4X(devid)		(IS_G45(devid) || IS_GM45(devid))

#define IS_IRONLAKE(devid)	(intel_get_device_info(devid)->is_ironlake)
#define IS_ARRANDALE(devid)	(intel_get_device_info(devid)->is_arrandale)
#define IS_SANDYBRIDGE(devid)	(intel_get_device_info(devid)->is_sandybridge)
#define IS_IVYBRIDGE(devid)	(intel_get_device_info(devid)->is_ivybridge)
#define IS_VALLEYVIEW(devid)	(intel_get_device_info(devid)->is_valleyview)
#define IS_HASWELL(devid)	(intel_get_device_info(devid)->is_haswell)
#define IS_BROADWELL(devid)	(intel_get_device_info(devid)->is_broadwell)
#define IS_CHERRYVIEW(devid)	(intel_get_device_info(devid)->is_cherryview)
#define IS_SKYLAKE(devid)	(intel_get_device_info(devid)->is_skylake)
#define IS_BROXTON(devid)	(intel_get_device_info(devid)->is_broxton)
#define IS_KABYLAKE(devid)	(intel_get_device_info(devid)->is_kabylake)
#define IS_GEMINILAKE(devid)	(intel_get_device_info(devid)->is_geminilake)
#define IS_COFFEELAKE(devid)	(intel_get_device_info(devid)->is_coffeelake)
#define IS_COMETLAKE(devid)	(intel_get_device_info(devid)->is_cometlake)
#define IS_CANNONLAKE(devid)	(intel_get_device_info(devid)->is_cannonlake)
#define IS_ICELAKE(devid)	(intel_get_device_info(devid)->is_icelake)
#define IS_TIGERLAKE(devid)	(intel_get_device_info(devid)->is_tigerlake)
#define IS_ROCKETLAKE(devid)	(intel_get_device_info(devid)->is_rocketlake)
#define IS_DG1(devid)		(intel_get_device_info(devid)->is_dg1)
#define IS_DG2(devid)		(intel_get_device_info(devid)->is_dg2)
#define IS_ALDERLAKE_S(devid)	(intel_get_device_info(devid)->is_alderlake_s)
#define IS_RAPTORLAKE_S(devid)	(intel_get_device_info(devid)->is_raptorlake_s)
#define IS_ALDERLAKE_P(devid)	(intel_get_device_info(devid)->is_alderlake_p)
#define IS_ALDERLAKE_N(devid)	(intel_get_device_info(devid)->is_alderlake_n)
#define IS_METEORLAKE(devid)	(intel_get_device_info(devid)->is_meteorlake)
#define IS_PONTEVECCHIO(devid)	(intel_get_device_info(devid)->is_pontevecchio)
#define IS_LUNARLAKE(devid)	(intel_get_device_info(devid)->is_lunarlake)
#define IS_BATTLEMAGE(devid)	(intel_get_device_info(devid)->is_battlemage)

#define IS_GEN(devid, x)	(intel_get_device_info(devid)->graphics_ver == x)
#define AT_LEAST_GEN(devid, x)	(intel_get_device_info(devid)->graphics_ver >= x)
#define AT_LEAST_DISPLAY(devid, x) (intel_get_device_info(devid)->display_ver >= x)

#define IS_GEN2(devid)		IS_GEN(devid, 2)
#define IS_GEN3(devid)		IS_GEN(devid, 3)
#define IS_GEN4(devid)		IS_GEN(devid, 4)
#define IS_GEN5(devid)		IS_GEN(devid, 5)
#define IS_GEN6(devid)		IS_GEN(devid, 6)
#define IS_GEN7(devid)		IS_GEN(devid, 7)
#define IS_GEN8(devid)		IS_GEN(devid, 8)
#define IS_GEN9(devid)		IS_GEN(devid, 9)
#define IS_GEN10(devid)		IS_GEN(devid, 10)
#define IS_GEN11(devid)		IS_GEN(devid, 11)
#define IS_GEN12(devid)		IS_GEN(devid, 12)

#define IS_MOBILE(devid)	(intel_get_device_info(devid)->is_mobile)
#define IS_965(devid)		AT_LEAST_GEN(devid, 4)

#define HAS_BSD_RING(devid)	AT_LEAST_GEN(devid, 5)
#define HAS_BLT_RING(devid)	AT_LEAST_GEN(devid, 6)

#define HAS_PCH_SPLIT(devid)	(AT_LEAST_GEN(devid, 5) && \
				 !(IS_VALLEYVIEW(devid) || \
				   IS_CHERRYVIEW(devid) || \
				   IS_BROXTON(devid)))

#define HAS_4TILE(devid)	(intel_get_device_info(devid)->has_4tile)

#define HAS_FLATCCS(devid)	(intel_get_device_info(devid)->has_flatccs)

#define HAS_OAM(devid)		(intel_get_device_info(devid)->has_oam)

#endif /* _INTEL_CHIPSET_H */
