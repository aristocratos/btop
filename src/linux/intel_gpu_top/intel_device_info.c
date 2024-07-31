#include "intel_chipset.h"
#include "i915_pciids.h"
#include "i915_pciids_local.h"
#include "xe_pciids.h"

#include <strings.h> /* ffs() */

// from pciaccess.h
#define PCI_MATCH_ANY  (~0U)

// from pciaccess.h
struct pci_id_match {
    /**
     * \name Device / vendor matching controls
     *
     * Control the search based on the device, vendor, subdevice, or subvendor
     * IDs.  Setting any of these fields to \c PCI_MATCH_ANY will cause the
     * field to not be used in the comparison.
     */
    /*@{*/
    uint32_t    vendor_id;
    uint32_t    device_id;
    uint32_t    subvendor_id;
    uint32_t    subdevice_id;
    /*@}*/


    /**
     * \name Device class matching controls
     *
     */
    /*@{*/
    uint32_t    device_class;
    uint32_t    device_class_mask;
    /*@}*/

    intptr_t    match_data;
};

static const struct intel_device_info intel_generic_info = {
	.graphics_ver = 0,
	.display_ver = 0,
};

static const struct intel_device_info intel_i810_info = {
	.graphics_ver = 1,
	.display_ver = 1,
	.is_whitney = true,
	.codename = "solano" /* 815 == "whitney" ? or vice versa? */
};

static const struct intel_device_info intel_i815_info = {
	.graphics_ver = 1,
	.display_ver = 1,
	.is_whitney = true,
	.codename = "whitney"
};

static const struct intel_device_info intel_i830_info = {
	.graphics_ver = 2,
	.display_ver = 2,
	.is_almador = true,
	.codename = "almador"
};
static const struct intel_device_info intel_i845_info = {
	.graphics_ver = 2,
	.display_ver = 2,
	.is_brookdale = true,
	.codename = "brookdale"
};
static const struct intel_device_info intel_i855_info = {
	.graphics_ver = 2,
	.display_ver = 2,
	.is_mobile = true,
	.is_montara = true,
	.codename = "montara"
};
static const struct intel_device_info intel_i865_info = {
	.graphics_ver = 2,
	.display_ver = 2,
	.is_springdale = true,
	.codename = "spingdale"
};

static const struct intel_device_info intel_i915_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_grantsdale = true,
	.codename = "grantsdale"
};
static const struct intel_device_info intel_i915m_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_mobile = true,
	.is_alviso = true,
	.codename = "alviso"
};
static const struct intel_device_info intel_i945_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_lakeport = true,
	.codename = "lakeport"
};
static const struct intel_device_info intel_i945m_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_mobile = true,
	.is_calistoga = true,
	.codename = "calistoga"
};

static const struct intel_device_info intel_g33_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_bearlake = true,
	.codename = "bearlake"
};

static const struct intel_device_info intel_pineview_g_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_pineview = true,
	.codename = "pineview"
};

static const struct intel_device_info intel_pineview_m_info = {
	.graphics_ver = 3,
	.display_ver = 3,
	.is_mobile = true,
	.is_pineview = true,
	.codename = "pineview"
};

static const struct intel_device_info intel_i965_info = {
	.graphics_ver = 4,
	.display_ver = 4,
	.is_broadwater = true,
	.codename = "broadwater"
};

static const struct intel_device_info intel_i965m_info = {
	.graphics_ver = 4,
	.display_ver = 4,
	.is_mobile = true,
	.is_crestline = true,
	.codename = "crestline"
};

static const struct intel_device_info intel_g45_info = {
	.graphics_ver = 4,
	.display_ver = 4,
	.is_eaglelake = true,
	.codename = "eaglelake"
};
static const struct intel_device_info intel_gm45_info = {
	.graphics_ver = 4,
	.display_ver = 4,
	.is_mobile = true,
	.is_cantiga = true,
	.codename = "cantiga"
};

static const struct intel_device_info intel_ironlake_info = {
	.graphics_ver = 5,
	.display_ver = 5,
	.is_ironlake = true,
	.codename = "ironlake" /* clarkdale? */
};
static const struct intel_device_info intel_ironlake_m_info = {
	.graphics_ver = 5,
	.display_ver = 5,
	.is_mobile = true,
	.is_arrandale = true,
	.codename = "arrandale"
};

static const struct intel_device_info intel_sandybridge_info = {
	.graphics_ver = 6,
	.display_ver = 6,
	.is_sandybridge = true,
	.codename = "sandybridge"
};
static const struct intel_device_info intel_sandybridge_m_info = {
	.graphics_ver = 6,
	.display_ver = 6,
	.is_mobile = true,
	.is_sandybridge = true,
	.codename = "sandybridge"
};

static const struct intel_device_info intel_ivybridge_info = {
	.graphics_ver = 7,
	.display_ver = 7,
	.is_ivybridge = true,
	.codename = "ivybridge"
};
static const struct intel_device_info intel_ivybridge_m_info = {
	.graphics_ver = 7,
	.display_ver = 7,
	.is_mobile = true,
	.is_ivybridge = true,
	.codename = "ivybridge"
};

static const struct intel_device_info intel_valleyview_info = {
	.graphics_ver = 7,
	.display_ver = 7,
	.is_valleyview = true,
	.codename = "valleyview"
};

#define HASWELL_FIELDS \
	.graphics_ver = 7, \
	.display_ver = 7, \
	.is_haswell = true, \
	.codename = "haswell"

static const struct intel_device_info intel_haswell_gt1_info = {
	HASWELL_FIELDS,
	.gt = 1,
};

static const struct intel_device_info intel_haswell_gt2_info = {
	HASWELL_FIELDS,
	.gt = 2,
};

static const struct intel_device_info intel_haswell_gt3_info = {
	HASWELL_FIELDS,
	.gt = 3,
};

#define BROADWELL_FIELDS \
	.graphics_ver = 8, \
	.display_ver = 8, \
	.is_broadwell = true, \
	.codename = "broadwell"

static const struct intel_device_info intel_broadwell_gt1_info = {
	BROADWELL_FIELDS,
	.gt = 1,
};

static const struct intel_device_info intel_broadwell_gt2_info = {
	BROADWELL_FIELDS,
	.gt = 2,
};

static const struct intel_device_info intel_broadwell_gt3_info = {
	BROADWELL_FIELDS,
	.gt = 3,
};

static const struct intel_device_info intel_broadwell_unknown_info = {
	BROADWELL_FIELDS,
};

static const struct intel_device_info intel_cherryview_info = {
	.graphics_ver = 8,
	.display_ver = 8,
	.is_cherryview = true,
	.codename = "cherryview"
};

#define SKYLAKE_FIELDS \
	.graphics_ver = 9, \
	.display_ver = 9, \
	.codename = "skylake", \
	.is_skylake = true

static const struct intel_device_info intel_skylake_gt1_info = {
	SKYLAKE_FIELDS,
	.gt = 1,
};

static const struct intel_device_info intel_skylake_gt2_info = {
	SKYLAKE_FIELDS,
	.gt = 2,
};

static const struct intel_device_info intel_skylake_gt3_info = {
	SKYLAKE_FIELDS,
	.gt = 3,
};

static const struct intel_device_info intel_skylake_gt4_info = {
	SKYLAKE_FIELDS,
	.gt = 4,
};

static const struct intel_device_info intel_broxton_info = {
	.graphics_ver = 9,
	.display_ver = 9,
	.is_broxton = true,
	.codename = "broxton"
};

#define KABYLAKE_FIELDS \
	.graphics_ver = 9, \
	.display_ver = 9, \
	.is_kabylake = true, \
	.codename = "kabylake"

static const struct intel_device_info intel_kabylake_gt1_info = {
	KABYLAKE_FIELDS,
	.gt = 1,
};

static const struct intel_device_info intel_kabylake_gt2_info = {
	KABYLAKE_FIELDS,
	.gt = 2,
};

static const struct intel_device_info intel_kabylake_gt3_info = {
	KABYLAKE_FIELDS,
	.gt = 3,
};

static const struct intel_device_info intel_kabylake_gt4_info = {
	KABYLAKE_FIELDS,
	.gt = 4,
};

static const struct intel_device_info intel_geminilake_info = {
	.graphics_ver = 9,
	.display_ver = 9,
	.is_geminilake = true,
	.codename = "geminilake"
};

#define COFFEELAKE_FIELDS \
	.graphics_ver = 9, \
	.display_ver = 9, \
	.is_coffeelake = true, \
	.codename = "coffeelake"

static const struct intel_device_info intel_coffeelake_gt1_info = {
	COFFEELAKE_FIELDS,
	.gt = 1,
};

static const struct intel_device_info intel_coffeelake_gt2_info = {
	COFFEELAKE_FIELDS,
	.gt = 2,
};

static const struct intel_device_info intel_coffeelake_gt3_info = {
	COFFEELAKE_FIELDS,
	.gt = 3,
};

#define COMETLAKE_FIELDS \
	.graphics_ver = 9, \
	.display_ver = 9, \
	.is_cometlake = true, \
	.codename = "cometlake"

static const struct intel_device_info intel_cometlake_gt1_info = {
	COMETLAKE_FIELDS,
	.gt = 1,
};

static const struct intel_device_info intel_cometlake_gt2_info = {
	COMETLAKE_FIELDS,
	.gt = 2,
};

static const struct intel_device_info intel_cannonlake_info = {
	.graphics_ver = 10,
	.display_ver = 10,
	.is_cannonlake = true,
	.codename = "cannonlake"
};

static const struct intel_device_info intel_icelake_info = {
	.graphics_ver = 11,
	.display_ver = 11,
	.is_icelake = true,
	.codename = "icelake"
};

static const struct intel_device_info intel_elkhartlake_info = {
	.graphics_ver = 11,
	.display_ver = 11,
	.is_elkhartlake = true,
	.codename = "elkhartlake"
};

static const struct intel_device_info intel_jasperlake_info = {
	.graphics_ver = 11,
	.display_ver = 11,
	.is_jasperlake = true,
	.codename = "jasperlake"
};

static const struct intel_device_info intel_tigerlake_gt1_info = {
	.graphics_ver = 12,
	.display_ver = 12,
	.is_tigerlake = true,
	.codename = "tigerlake",
	.gt = 1,
};

static const struct intel_device_info intel_tigerlake_gt2_info = {
	.graphics_ver = 12,
	.display_ver = 12,
	.is_tigerlake = true,
	.codename = "tigerlake",
	.gt = 2,
};

static const struct intel_device_info intel_rocketlake_info = {
	.graphics_ver = 12,
	.display_ver = 12,
	.is_rocketlake = true,
	.codename = "rocketlake"
};

static const struct intel_device_info intel_dg1_info = {
	.graphics_ver = 12,
	.graphics_rel = 10,
	.display_ver = 12,
	.is_dg1 = true,
	.codename = "dg1"
};

static const struct intel_device_info intel_dg2_info = {
	.graphics_ver = 12,
	.graphics_rel = 55,
	.display_ver = 13,
	.has_4tile = true,
	.is_dg2 = true,
	.codename = "dg2",
	.has_flatccs = true,
};

static const struct intel_device_info intel_alderlake_s_info = {
	.graphics_ver = 12,
	.display_ver = 12,
	.is_alderlake_s = true,
	.codename = "alderlake_s"
};

static const struct intel_device_info intel_raptorlake_s_info = {
	.graphics_ver = 12,
	.display_ver = 12,
	.is_raptorlake_s = true,
	.codename = "raptorlake_s"
};

static const struct intel_device_info intel_alderlake_p_info = {
	.graphics_ver = 12,
	.display_ver = 13,
	.is_alderlake_p = true,
	.codename = "alderlake_p"
};

static const struct intel_device_info intel_alderlake_n_info = {
	.graphics_ver = 12,
	.display_ver = 13,
	.is_alderlake_n = true,
	.codename = "alderlake_n"
};

static const struct intel_device_info intel_ats_m_info = {
	.graphics_ver = 12,
	.graphics_rel = 55,
	.display_ver = 0, /* no display support */
	.is_dg2 = true,
	.has_4tile = true,
	.codename = "ats_m",
	.has_flatccs = true,
};

static const struct intel_device_info intel_meteorlake_info = {
	.graphics_ver = 12,
	.graphics_rel = 70,
	.display_ver = 14,
	.has_4tile = true,
	.has_oam = true,
	.is_meteorlake = true,
	.codename = "meteorlake",
};

static const struct intel_device_info intel_pontevecchio_info = {
	.graphics_ver = 12,
	.graphics_rel = 60,
	.is_pontevecchio = true,
	.codename = "pontevecchio",
};

static const struct intel_device_info intel_lunarlake_info = {
	.graphics_ver = 20,
	.graphics_rel = 4,
	.display_ver = 20,
	.has_4tile = true,
	.has_flatccs = true,
	.has_oam = true,
	.is_lunarlake = true,
	.codename = "lunarlake",
};

static const struct intel_device_info intel_battlemage_info = {
	.graphics_ver = 20,
	.graphics_rel = 1,
	.display_ver = 14,
	.has_4tile = true,
	.has_flatccs = true,
	.is_battlemage = true,
	.codename = "battlemage",
};

static const struct pci_id_match intel_device_match[] = {
	INTEL_I810_IDS(INTEL_VGA_DEVICE, &intel_i810_info),
	INTEL_I815_IDS(INTEL_VGA_DEVICE, &intel_i815_info),

	INTEL_I830_IDS(INTEL_VGA_DEVICE, &intel_i830_info),
	INTEL_I845G_IDS(INTEL_VGA_DEVICE, &intel_i845_info),
	INTEL_I85X_IDS(INTEL_VGA_DEVICE, &intel_i855_info),
	INTEL_I865G_IDS(INTEL_VGA_DEVICE, &intel_i865_info),

	INTEL_I915G_IDS(INTEL_VGA_DEVICE, &intel_i915_info),
	INTEL_I915GM_IDS(INTEL_VGA_DEVICE, &intel_i915m_info),
	INTEL_I945G_IDS(INTEL_VGA_DEVICE, &intel_i945_info),
	INTEL_I945GM_IDS(INTEL_VGA_DEVICE, &intel_i945m_info),

	INTEL_G33_IDS(INTEL_VGA_DEVICE, &intel_g33_info),
	INTEL_PNV_G_IDS(INTEL_VGA_DEVICE, &intel_pineview_g_info),
	INTEL_PNV_M_IDS(INTEL_VGA_DEVICE, &intel_pineview_m_info),

	INTEL_I965G_IDS(INTEL_VGA_DEVICE, &intel_i965_info),
	INTEL_I965GM_IDS(INTEL_VGA_DEVICE, &intel_i965m_info),

	INTEL_G45_IDS(INTEL_VGA_DEVICE, &intel_g45_info),
	INTEL_GM45_IDS(INTEL_VGA_DEVICE, &intel_gm45_info),

	INTEL_ILK_D_IDS(INTEL_VGA_DEVICE, &intel_ironlake_info),
	INTEL_ILK_M_IDS(INTEL_VGA_DEVICE, &intel_ironlake_m_info),

	INTEL_SNB_D_IDS(INTEL_VGA_DEVICE, &intel_sandybridge_info),
	INTEL_SNB_M_IDS(INTEL_VGA_DEVICE, &intel_sandybridge_m_info),

	INTEL_IVB_D_IDS(INTEL_VGA_DEVICE, &intel_ivybridge_info),
	INTEL_IVB_M_IDS(INTEL_VGA_DEVICE, &intel_ivybridge_m_info),

	INTEL_HSW_GT1_IDS(INTEL_VGA_DEVICE, &intel_haswell_gt1_info),
	INTEL_HSW_GT2_IDS(INTEL_VGA_DEVICE, &intel_haswell_gt2_info),
	INTEL_HSW_GT3_IDS(INTEL_VGA_DEVICE, &intel_haswell_gt3_info),

	INTEL_VLV_IDS(INTEL_VGA_DEVICE, &intel_valleyview_info),

	INTEL_BDW_GT1_IDS(INTEL_VGA_DEVICE, &intel_broadwell_gt1_info),
	INTEL_BDW_GT2_IDS(INTEL_VGA_DEVICE, &intel_broadwell_gt2_info),
	INTEL_BDW_GT3_IDS(INTEL_VGA_DEVICE, &intel_broadwell_gt3_info),
	INTEL_BDW_RSVD_IDS(INTEL_VGA_DEVICE, &intel_broadwell_unknown_info),

	INTEL_CHV_IDS(INTEL_VGA_DEVICE, &intel_cherryview_info),

	INTEL_SKL_GT1_IDS(INTEL_VGA_DEVICE, &intel_skylake_gt1_info),
	INTEL_SKL_GT2_IDS(INTEL_VGA_DEVICE, &intel_skylake_gt2_info),
	INTEL_SKL_GT3_IDS(INTEL_VGA_DEVICE, &intel_skylake_gt3_info),
	INTEL_SKL_GT4_IDS(INTEL_VGA_DEVICE, &intel_skylake_gt4_info),

	INTEL_BXT_IDS(INTEL_VGA_DEVICE, &intel_broxton_info),

	INTEL_KBL_GT1_IDS(INTEL_VGA_DEVICE, &intel_kabylake_gt1_info),
	INTEL_KBL_GT2_IDS(INTEL_VGA_DEVICE, &intel_kabylake_gt2_info),
	INTEL_KBL_GT3_IDS(INTEL_VGA_DEVICE, &intel_kabylake_gt3_info),
	INTEL_KBL_GT4_IDS(INTEL_VGA_DEVICE, &intel_kabylake_gt4_info),
	INTEL_AML_KBL_GT2_IDS(INTEL_VGA_DEVICE, &intel_kabylake_gt2_info),

	INTEL_GLK_IDS(INTEL_VGA_DEVICE, &intel_geminilake_info),

	INTEL_CFL_S_GT1_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt1_info),
	INTEL_CFL_S_GT2_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt2_info),
	INTEL_CFL_H_GT1_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt1_info),
	INTEL_CFL_H_GT2_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt2_info),
	INTEL_CFL_U_GT2_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt2_info),
	INTEL_CFL_U_GT3_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt3_info),
	INTEL_WHL_U_GT1_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt1_info),
	INTEL_WHL_U_GT2_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt2_info),
	INTEL_WHL_U_GT3_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt3_info),
	INTEL_AML_CFL_GT2_IDS(INTEL_VGA_DEVICE, &intel_coffeelake_gt2_info),

	INTEL_CML_GT1_IDS(INTEL_VGA_DEVICE, &intel_cometlake_gt1_info),
	INTEL_CML_GT2_IDS(INTEL_VGA_DEVICE, &intel_cometlake_gt2_info),
	INTEL_CML_U_GT1_IDS(INTEL_VGA_DEVICE, &intel_cometlake_gt1_info),
	INTEL_CML_U_GT2_IDS(INTEL_VGA_DEVICE, &intel_cometlake_gt2_info),

	INTEL_CNL_IDS(INTEL_VGA_DEVICE, &intel_cannonlake_info),

	INTEL_ICL_IDS(INTEL_VGA_DEVICE, &intel_icelake_info),

	INTEL_EHL_IDS(INTEL_VGA_DEVICE, &intel_elkhartlake_info),
	INTEL_JSL_IDS(INTEL_VGA_DEVICE, &intel_jasperlake_info),

	INTEL_TGL_GT1_IDS(INTEL_VGA_DEVICE, &intel_tigerlake_gt1_info),
	INTEL_TGL_GT2_IDS(INTEL_VGA_DEVICE, &intel_tigerlake_gt2_info),
	INTEL_RKL_IDS(INTEL_VGA_DEVICE, &intel_rocketlake_info),

	INTEL_DG1_IDS(INTEL_VGA_DEVICE, &intel_dg1_info),
	INTEL_DG2_IDS(INTEL_VGA_DEVICE, &intel_dg2_info),

	INTEL_ADLS_IDS(INTEL_VGA_DEVICE, &intel_alderlake_s_info),
	INTEL_RPLS_IDS(INTEL_VGA_DEVICE, &intel_raptorlake_s_info),
	INTEL_ADLP_IDS(INTEL_VGA_DEVICE, &intel_alderlake_p_info),
	INTEL_RPLU_IDS(INTEL_VGA_DEVICE, &intel_alderlake_p_info),
	INTEL_RPLP_IDS(INTEL_VGA_DEVICE, &intel_alderlake_p_info),
	INTEL_ADLN_IDS(INTEL_VGA_DEVICE, &intel_alderlake_n_info),

	INTEL_ATS_M_IDS(INTEL_VGA_DEVICE, &intel_ats_m_info),

	INTEL_MTL_IDS(INTEL_VGA_DEVICE, &intel_meteorlake_info),

	INTEL_PVC_IDS(INTEL_VGA_DEVICE, &intel_pontevecchio_info),

	XE_LNL_IDS(INTEL_VGA_DEVICE, &intel_lunarlake_info),

	XE_BMG_IDS(INTEL_VGA_DEVICE, &intel_battlemage_info),

	INTEL_VGA_DEVICE(PCI_MATCH_ANY, &intel_generic_info),
};

/**
 * intel_get_device_info:
 * @devid: pci device id
 *
 * Looks up the Intel GFX device info for the given device id.
 *
 * Returns:
 * The associated intel_get_device_info
 */
const struct intel_device_info *intel_get_device_info(uint16_t devid)
{
	static const struct intel_device_info *cache = &intel_generic_info;
	static uint16_t cached_devid;
	int i;

	if (cached_devid == devid)
		goto out;

	/* XXX Presort table and bsearch! */
	for (i = 0; intel_device_match[i].device_id != PCI_MATCH_ANY; i++) {
		if (devid == intel_device_match[i].device_id)
			break;
	}

	cached_devid = devid;
	cache = (void *)intel_device_match[i].match_data;

out:
	return cache;
}