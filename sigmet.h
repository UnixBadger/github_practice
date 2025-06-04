/*
 *	sigmet.h --
 *		Structures and functions that store and access Sigmet raw product files.
 *	--
 *
 *	Documentation: IRIS Programmer's Manual, February 2021.
 *
 *	Copyright (c) 2011, Gordon D. Carrie.
 *	All rights reserved.
 *	Licensed under the Academic Free License version 3.0 .
 *	See file AFL-3.0 or https://opensource.org/licenses/AFL-3.0 .
 *	Please send feedback to dev1960@polarismail.net
 */

#ifndef SIGMET_H_
#define SIGMET_H_

#include <stdio.h>
#include <time.h>
#include <stdint.h>

/* Name of environment variable that tells sigmet_raw to make more error exits. */
#define SIGMET_STRICT "SIGMET_STRICT"

/* Maximum number of sweeps in a volume, from IRIS-Programming-Guide */
#define SIGMET_MAX_SWPS 40

/* SIGMET_NUM_DATA_TYPES = number of Sigmet data types, including DB_XHDR.
 * Will require update if Sigmet/Vaisalla adds data types. */
#define SIGMET_NUM_DATA_TYPES 89

/* Number of bytes in "DB_TEMPERATURE16" NOT including nul terminator */
#define SIGMET_DATA_TYPE_LEN 16

/* Number of bytes in task name (cf. Sigmet documentation), NOT including nul terminator */
#define SIGMET_TASK_NM_LEN 12

/* Number of bytes in site name (cf. Sigmet documentation), NOT including nul terminator */
#define SIGMET_SITE_NM_LEN 16

/* Number of bytes in task descriptor (cf. Sigmet documentation), NOT including nul terminator */
#define SIGMET_TASK_DESCR_LEN 80

/* Suggested error message length. Space for
 * SIGMET_ERR_LEN1 - space for 5 lines averaging 12 words, each word averaging 8 letters + space
 * SIGMET_ERR_LEN  - space for "sigmet_raw" + SIGMET_ERR_LEN1 + volume_path + nul
 * Arbitrarily assume this is as much as anyone would read.
 */
#define SIGMET_ERR_LEN1 (size_t)(5 * (12 * (8 + 1)) + 1)
#define SIGMET_ERR_LEN (size_t)(10 + SIGMET_ERR_LEN1 + 4096 + 1)

/* Place to write pieces of error messages */
struct Sigmet_ErrMsg {
    char * str;				/* Error string */
    size_t sz;				/* Allocation at str. Must include space for nul. */
};
#define Sigmet_ErrMsg_Print(err_msg_p, ...) {			\
    if (err_msg_p != NULL) {					\
	snprintf(err_msg_p->str, err_msg_p->sz, __VA_ARGS__);	\
    }								\
}

/*
 * volume_headers --
 *
 * The following structures store headers and data for a Sigmet raw product
 * file. Sequences of members imitates sequence of data in the file, so there
 * is a some repetition, and occasional unused members with rubbist values
 * which vary with IRIS version.
 *
 * Units for members taken directly from the Sigmet volume are as indicated
 * in the IRIS Programmer Manual (i.e. nothing is converted during input).
 * Units for derived members are as indicated.  In particular, angles from
 * the volume are unsigned integer binary angles (cf. IRIS Programmer's
 * Manual, 3.1).
 */

/* Multi PRF mode flags */
enum Sigmet_MultiPRF {
    ONE_ONE, TWO_THREE, FOUR_THREE, FOUR_FIVE
};

/* Volume scan modes.  Refer to task scan info struct in IRIS Programmer's Guide */
enum Sigmet_ScanMode {
    PPI_S = 1, RHI, MAN_SCAN, PPI_C, FILE_SCAN
};

struct Sigmet_StructHdr {
    int16_t	    id;
    int16_t	    format;
    int32_t	    sz;
    int16_t	    flags;
};

/* Time as represented in various Sigmet raw headers. */
struct Sigmet_YMDS_Tm {
    int32_t	    sec;		/* Seconds since midnight */
    uint16_t	    msec;		/* Milliseconds */
    _Bool	    dst;		/* If true, time is daylight savings */
    _Bool	    utc;		/* If true, time is UTC */
    _Bool	    ldst;		/* If true, local time is daylight savings */
    int16_t	    yr;
    int16_t	    mon;
    int16_t	    day;
};

/* volume_hdr.product_hdr.product_configuration.product_specific_info
 * For raw volume, product_specific_info is raw_psi struct. See IRIS Programmer's Manual, 4.3.28. */
struct Sigmet_ProdSpecificInfo {
    uint32_t	    dat_typ_mask;
    int32_t	    rng_last_bin;
    uint32_t	    format_conv_flag;
    uint32_t	    flag;
    int32_t	    sweep_num;
    uint32_t	    xhdr_type;
    uint32_t	    dat_typ_mask1;
    uint32_t	    dat_typ_mask2;
    uint32_t	    dat_typ_mask3;
    uint32_t	    dat_typ_mask4;
    uint32_t	    playback_vsn;
};

/* volume_hdr.product_hdr.product_configuration.color_scale_def */
struct Sigmet_ColorScaleDef {
    uint32_t	    flags;
    int32_t	    istart;
    int32_t	    istep;
    int16_t	    icolcnt;
    uint16_t	    iset_and_scale;
    uint16_t	    ilevel_seams[16];
};

/* volume_hdr.product_hdr.product_configuration */
struct Sigmet_ProdCfg {
    struct Sigmet_StructHdr struct_hdr;
    uint16_t	    type;
    uint16_t	    schedule;
    int32_t	    skip;
    struct Sigmet_YMDS_Tm gen_tm;
    struct Sigmet_YMDS_Tm ingst_swp_tm;
    struct Sigmet_YMDS_Tm ingst_fl_tm;
    char	    cfg_fl[13];		/* string */
    char	    task_nm[SIGMET_TASK_NM_LEN + 1];	/* Add 1 for null. */
    uint16_t	    flag;
    int32_t	    x_scale;
    int32_t	    y_scale;
    int32_t	    z_scale;
    int32_t	    x_size;
    int32_t	    y_size;
    int32_t	    z_size;
    int32_t	    x_loc;
    int32_t	    y_loc;
    int32_t	    z_loc;
    int32_t	    max_rng;
    uint16_t	    data_type;
    char	    proj[13];		/* string */
    uint16_t	    inp_data_type;
    uint8_t	    proj_type;
    int16_t	    rad_smoother;
    int16_t	    num_runs;
    int32_t	    zr_const;
    int32_t	    zr_exp;
    int16_t	    x_smooth;
    int16_t	    y_smooth;
    struct Sigmet_ProdSpecificInfo prod_specific_info;
    char	    suffixes[17];	/* string */
    struct Sigmet_ColorScaleDef color_scale_def;
};

/* volume_hdr.product_hdr.product_end */
struct Sigmet_ProdEnd {
    char	    site_nm_prod[SIGMET_SITE_NM_LEN + 1]; /* Add 1 for nul terminator */
    char	    iris_prod_vsn[9];	/* string */
    char	    iris_ing_vsn[9];	/* string */
    int16_t	    local_wgmt;
    char	    hw_nm[SIGMET_SITE_NM_LEN + 1];	 /* Add 1 for nul terminator */
    char	    site_nm_ing[SIGMET_SITE_NM_LEN + 1]; /* Add 1 for nul terminator */
    int16_t	    rec_wgmt;
    uint32_t	    ctr_lat;
    uint32_t	    ctr_lon;
    int16_t	    ground_elev;
    int16_t	    radar_ht;
    int32_t	    prf;
    int32_t	    pulse_w;
    uint16_t	    proc_type;
    uint16_t	    trgr_rate_scheme;
    int16_t	    num_samples;
    char	    clutter_filter[13];	/* string */
    uint16_t	    lin_filter;
    int32_t	    wave_len;
    int32_t	    trunc_ht;
    int32_t	    rng_bin0;
    int32_t	    rng_last_bin;
    int32_t	    num_bins_out;
    uint16_t	    flag;
    uint16_t	    polzn;
    int16_t	    h_pol_io_cal;
    int16_t	    h_pol_cal_noise;
    int16_t	    h_pol_radar_const;
    uint16_t	    recv_bandw;
    int16_t	    h_pol_noise;
    int16_t	    v_pol_noise;
    int16_t	    ldr_offset;
    int16_t	    zdr_offset;
    uint16_t	    tcf_cal_flags;
    uint16_t	    tcf_cal_flags2;
    uint32_t	    std_parallel1;
    uint32_t	    std_parallel2;
    uint32_t	    rearth;
    uint32_t	    flatten;
    uint32_t	    fault;
    uint32_t	    insites_mask;
    uint16_t	    log_filter_num;
    uint16_t	    clutter_map_used;
    uint32_t	    proj_lat;
    uint32_t	    proj_lon;
    int16_t	    i_prod;
    int16_t	    melt_lvl;
    int16_t	    radar_ht_ref;
    int16_t	    num_elem;
    uint8_t	    wind_spd;
    uint8_t	    wind_dir;
    char	    tz[9];		/* string */
    uint32_t        off_xph;
};

/* volume_hdr.product_hdr */
struct Sigmet_ProdHdr {
    struct Sigmet_StructHdr struct_hdr;
    struct Sigmet_ProdCfg   prod_cfg;
    struct Sigmet_ProdEnd   prod_end;
};

/* volume_hdr.ingest_header.ingest_configuration */
struct Sigmet_IngstCfg {
    char		  file_nm[81];	/* string */
    int16_t		  num_assoc_fls;
    int16_t		  num_swps;
    int32_t		  size_fls;
    struct Sigmet_YMDS_Tm vol_start_tm;
    int16_t		  ray_hdr_sz;
    int16_t		  ext_ray_hdr_sz;
    int16_t		  task_cfg_tbl_num;
    int16_t		  playback_vsn;
    char		  IRISVsn[9];	/* string */
    char		  hw_site_nm[SIGMET_SITE_NM_LEN + 1]; /* Add 1 for nul terminator */
    int16_t		  local_wgmt;
    char		  su_site_nm[SIGMET_SITE_NM_LEN + 1]; /* Add 1 for nul terminator */
    int16_t		  rec_wgmt;
    uint32_t		  lat;
    uint32_t		  lon;
    int16_t		  ground_elev;
    int16_t		  radar_ht;
    uint16_t		  resolution;
    uint16_t		  index_first_ray;
    uint16_t		  num_rays;
    int16_t		  num_bytes_g_param;
    int32_t		  altitude;
    int32_t		  velocity[3];
    int32_t		  offset_inu[3];
    uint32_t		  fault;
    int16_t		  melt_lvl;
    char		  tz[9];	/* string */
    uint32_t		  flags;
    char		  cfg_nm[17];	/* string */
};

/* volume_hdr.ingest_header.task_configuration.task_sched_info */
struct Sigmet_TaskSchedInfo {
    int32_t	    start_tm;
    int32_t	    stop_tm;
    int32_t	    skip;
    int32_t	    tm_last_run;
    int32_t	    tm_used_last_run;
    int32_t	    rel_day_last_run;
    uint16_t	    flag;
};

/* volume_hdr.ingest_header.task_configuration.task_dsp_info.dsp_data_mask */
#define SIGMET_NUM_MASK_WORDS 5
struct Sigmet_DSPDataMask {
    uint32_t	    mask_wd0;
    uint32_t	    ext_hdr_type;
    uint32_t	    mask_wd1;
    uint32_t	    mask_wd2;
    uint32_t	    mask_wd3;
    uint32_t	    mask_wd4;
};

/* volume_hdr.ingest_header.task_configuration.task_dsp_info.task_dsp_mode_batch */
struct Sigmet_TaskDSPModeBatch {
    uint16_t	    lo_prf;
    uint16_t	    lo_prf_frac;
    int16_t	    lo_prf_sampl;
    int16_t	    lo_prf_avg;
    int16_t	    dz_unfold_thresh;
    int16_t	    vr_unfold_thresh;
    int16_t	    sw_unfold_thresh;
};

/* volume_hdr.ingest_header.task_configuration.task_dsp_info */
struct Sigmet_TaskDSPInfo {
    uint16_t			  major_mode;
    uint16_t			  dsp_type;
    struct Sigmet_DSPDataMask	  curr_data_mask;
    struct Sigmet_DSPDataMask	  orig_data_mask;
    struct Sigmet_TaskDSPModeBatch task_dsp_mode_batch;
    int32_t			  prf;
    int32_t			  pulse_w;
    enum Sigmet_MultiPRF	  multi_prf_mode;
    int16_t			  dual_prf;
    uint16_t			  agc_feebk;
    int16_t			  sampl_sz;
    uint16_t			  gain_flag;
    char			  clutter_fl[13];	/* string */
    uint8_t			  lin_filter_num;
    uint8_t			  log_filter_num;
    int16_t			  attn;
    uint16_t			  gas_attn;
    _Bool			  clutter_flag;
    uint16_t			  xmt_phase;
    uint32_t			  ray_hdr_mask;
    uint16_t			  tm_series_flag;
    char			  custom_ray_hdr[17];	/* string */
};

/* volume_hdr.ingest_header.task_configuration.task_calib_info */
struct Sigmet_TaskCalibInfo {
    int16_t	    dbz_slope;
    int16_t	    dbz_noise_thresh;
    int16_t	    clutter_corr_thesh;
    int16_t	    sqi_thresh;
    int16_t	    pwr_thresh;
    int16_t	    cal_dbz;
    uint16_t	    dbt_flags;
    uint16_t	    dbz_flags;
    uint16_t	    vel_flags;
    uint16_t	    sw_flags;
    uint16_t	    zdr_flags;
    uint16_t	    flags;
    int16_t	    ldr_bias;
    int16_t	    zdr_bias;
    int16_t	    nx_clutter_thresh;
    uint16_t	    nx_clutter_skip;
    int16_t	    h_pol_io_cal;
    int16_t	    v_pol_io_cal;
    int16_t	    h_pol_noise;
    int16_t	    v_pol_noise;
    int16_t	    h_pol_radar_const;
    int16_t	    v_pol_radar_const;
    uint16_t	    bandwidth;
    uint16_t	    flags2;
};

/* volume_hdr.ingest_header.task_configuration.task_range_info */
struct Sigmet_TaskRngInfo {
    int32_t	    rng_1st_bin;
    int32_t	    rng_last_bin;
    int16_t	    num_bins_in;
    int16_t	    num_bins_out;
    int32_t	    step_in;
    int32_t	    step_out;
    uint16_t	    flag;
    int16_t	    rng_avg_flag;
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info */
struct Sigmet_TaskRHI_ScanInfo {
    uint16_t	    lo_elev;
    uint16_t	    hi_elev;
    uint16_t	    az[SIGMET_MAX_SWPS];
    uint8_t	    start;
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info */
struct Sigmet_TaskPPI_ScanInfo {
    uint16_t	    left_az;
    uint16_t	    right_az;
    uint16_t	    elev[SIGMET_MAX_SWPS];
    uint8_t	    start;
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info */
struct Sigmet_TaskFlScanInfo {
    uint16_t	    az0;
    uint16_t	    elev0;
    char	    ant_ctrl[13];	/* string */
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info */
struct Sigmet_TaskManualScanInfo {
    uint16_t	    flags;
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info */
union Sigmet_ScanInfo {
    struct Sigmet_TaskRHI_ScanInfo   task_rhi_scan_info;
    struct Sigmet_TaskPPI_ScanInfo   task_ppi_scan_info;
    struct Sigmet_TaskFlScanInfo     task_fl_scan_info;
    struct Sigmet_TaskManualScanInfo task_manual_scan_info;
};
struct Sigmet_TaskScanInfo {
    enum Sigmet_ScanMode  scan_mode;
    int16_t		  resoln;
    int16_t		  num_swps;
    union Sigmet_ScanInfo scan_info;
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info.task_misc_info */
struct Sigmet_TaskMiscInfo {
    int32_t	    wave_len;
    char	    tr_ser[17];		/* string */
    int32_t	    power;
    uint16_t	    flags;
    uint16_t	    polzn;
    int32_t	    trunc_ht;
    int16_t	    comment_sz;
    uint32_t	    horiz_beam_width;
    uint32_t	    vert_beam_width;
    uint32_t	    custom[10];
};

/* volume_hdr.ingest_header.task_configuration.task_scan_info.task_end_info */
struct Sigmet_TaskEndInfo {
    int16_t		  task_major;
    int16_t		  task_minor;
    char		  task_cfg[13];		/* string */
    char		  task_descr[SIGMET_TASK_DESCR_LEN + 1];	/* Add 1 for nul */
    int32_t		  hybrid_ntasks;
    uint16_t		  task_state;
    struct Sigmet_YMDS_Tm data_tm;
};

/* volume_hdr.ingest_header.task_configuration */
struct Sigmet_TaskCfg {
    struct Sigmet_StructHdr struct_hdr;
    struct Sigmet_TaskSchedInfo task_sched_info;
    struct Sigmet_TaskDSPInfo task_dsp_info;
    struct Sigmet_TaskCalibInfo task_calib_info;
    struct Sigmet_TaskRngInfo task_rng_info;
    struct Sigmet_TaskScanInfo task_scan_info;
    struct Sigmet_TaskMiscInfo task_misc_info;
    struct Sigmet_TaskEndInfo task_end_info;
};

/* volume_hdr.ingest_header */
struct Sigmet_IngstHdr {
    struct Sigmet_StructHdr struct_hdr;
    struct Sigmet_IngstCfg  ingst_cfg;
    struct Sigmet_TaskCfg   task_cfg;
};

/* Volume header. */
struct Sigmet_DataType;
struct Sigmet_VolHdr {
    struct Sigmet_ProdHdr  prod_hdr;	/* Record #1 of raw product file */
    struct Sigmet_IngstHdr ingst_hdr;	/* Record #2 of raw product file */
    unsigned num_types;
    const struct Sigmet_DataType * types[SIGMET_NUM_DATA_TYPES];
};

/* End of declarations for volume_headers -- */

/* Sweep header */
struct Sigmet_SwpHdr {
    struct Sigmet_YMDS_Tm tm;		/* Sweep start time */
    double angl;			/* Sweep angle, radians */
};

/* Ray header */
struct Sigmet_RayHdr {
    float az0;				/* Azimuth at start of ray, radians */
    float tilt0;			/* Elevation at start of ray, radians */
    float az1;				/* Azimuth at end of ray, radians */
    float tilt1;			/* Elevation at end of ray, radians, */
    int   num_bins;			/* Number of bins in ray */
    unsigned tm;			/* Time from start of sweep, seconds */
};

/* Sigmet ray - header and data for one data type in one ray */
struct Sigmet_Ray {
    struct Sigmet_RayHdr ray_hdr;
    void * dat;				/* hdr.num_bins data values in file representation, or NULL */
};

/* Space for "UTC-11:-59" */
#define SIGMET_TZ_STRLEN 11

/* These functions access radar volumes in Sigmet raw product files. */
const struct Sigmet_DataType * Sigmet_DataTypeGet(const char *);
const char * Sigmet_DataTypeAbbrv(const struct Sigmet_DataType *);
const char * Sigmet_DataType_PrintFmt(const struct Sigmet_DataType *);
int Sigmet_DataType_DatumSz(const struct Sigmet_DataType *, const struct Sigmet_VolHdr *,
	struct Sigmet_ErrMsg *);
void Sigmet_DataTypeStorToVal(const struct Sigmet_DataType *, int, float *, const void *,
	const struct Sigmet_VolHdr *);
unsigned Sigmet_DataTypes_FmMask(const struct Sigmet_DataType * [SIGMET_NUM_DATA_TYPES],
	const struct Sigmet_VolHdr *);
double Sigmet_Bin4Rad(uint32_t);
double Sigmet_Bin2Rad(uint16_t);

inline unsigned Sigmet_VolNumSwps(const struct Sigmet_VolHdr * vol_hdr_p)
{
    return vol_hdr_p->ingst_hdr.task_cfg.task_scan_info.num_swps;
}
inline unsigned Sigmet_VolNumRays(const struct Sigmet_VolHdr * vol_hdr_p)
{
    return vol_hdr_p->ingst_hdr.ingst_cfg.num_rays;
}
inline unsigned Sigmet_VolNumTypes(const struct Sigmet_VolHdr * vol_hdr_p)
{
    return vol_hdr_p->num_types;
}
inline unsigned Sigmet_VolNumBins(const struct Sigmet_VolHdr * vol_hdr_p)
{
    return vol_hdr_p->ingst_hdr.task_cfg.task_rng_info.num_bins_out;
}
int Sigmet_VolReadVHdr(FILE *, struct Sigmet_VolHdr *, struct Sigmet_ErrMsg *);
int Sigmet_VolTypeIdx(const struct Sigmet_DataType *, const struct Sigmet_VolHdr *);
size_t Sigmet_VolIDatSz(const struct Sigmet_VolHdr *, struct Sigmet_ErrMsg *);
int Sigmet_DataType_MaxRayDatSz(const struct Sigmet_DataType *, const struct Sigmet_VolHdr *,
	struct Sigmet_ErrMsg *);
unsigned Sigmet_VolReadDat(FILE *, const struct Sigmet_VolHdr *,
	unsigned num_swps, unsigned num_rays, unsigned num_types,
	struct Sigmet_SwpHdr [num_swps], struct Sigmet_Ray (*)[num_rays][num_types],
	size_t, void *, struct Sigmet_ErrMsg *);
void Sigmet_Vol_TZ_Str(char *, int);
int Sigmet_VolTZSet(const struct Sigmet_VolHdr *, struct Sigmet_ErrMsg *);
double Sigmet_VolVNyquist(const struct Sigmet_VolHdr *);
int Sigmet_VolPrintVHdr(FILE *, const struct Sigmet_VolHdr *, struct Sigmet_ErrMsg *);
void Sigmet_VolPrintSwpHdrs(FILE *, unsigned num_swps, struct Sigmet_SwpHdr *);
void Sigmet_VolPrintRayHdrs(FILE *, double, unsigned, const struct Sigmet_RayHdr *);
int Sigmet_VolScanVHdr(FILE *, struct Sigmet_VolHdr *, struct Sigmet_ErrMsg *);
double Sigmet_DTime(const struct Sigmet_YMDS_Tm *);
int Sigmet_BkTime(double, int *, int *, int *, int *, int *, float *);
/* Return true if volume has extended ray header "data type" */
static inline _Bool Sigmet_VolXHdr(const struct Sigmet_VolHdr * vol_hdr_p)
{
    return vol_hdr_p->ingst_hdr.task_cfg.task_dsp_info.curr_data_mask.mask_wd0 & 1;
}

#endif

