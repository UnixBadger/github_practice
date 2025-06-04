/*
 *	ray_headers.c --
 *		Print list of ray headers in raw product file. See sigmet_raw (1).
 *	--
 *
 *	Copyright (c) 2022, Gordon D. Carrie. All rights reserved.
 *	Licensed under the Academic Free License version 3.0
 *	See file AFL-3.0 or https://opensource.org/licenses/AFL-3.0.
 *	Send feedback to dev1960@polarismail.net
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include "sigmet.h"
#include "sigmet_raw.h"

#define DEG_PER_RAD ((double)57.29577951308232087648)
#define RAY_HDR_FMT "%2d %4d    time    %04d/%02d/%02d %02d:%02d:%06.3lf    az    %7.1f %7.1f" \
    "    tilt %6.1f %6.1f    num_bins    %4d \n"

static void ray_hdrs_fm_fl(const char *, const struct Sigmet_DataType *, unsigned, const char *);
static void ray_hdrs_fm_skt(const char *, const struct Sigmet_DataType *, unsigned, const char *);

int main(int argc, char *argv[])
{
    /* If set, use $APP_NAME in error messages instead of argv[0]. */
    char *cmd = getenv("APP_NAME") ? getenv("APP_NAME") : basename(argv[0]);

    if ( !(argc == 3 || argc == 4) ) {
	fprintf(stderr, "Usage: %s sweep_index [data_type] raw_product_file|socket\n", cmd);
	exit(EXIT_FAILURE);
    }
    char *s_swp = argv[1];		/* Sweep index, integer or "all" */
    char *path = argv[argc - 1];	/* Path to Sigmet raw file or daemon socket */
    char *abbrv = NULL;			/* Data type abbreviation, e.g. "DB_DBZ" */
    const struct Sigmet_DataType * type = NULL;
    if (argc == 4) {
	abbrv = argv[2];
	type = Sigmet_DataTypeGet(abbrv);
	if (type == NULL) {
	    fprintf(stderr, "%s: %s is not a Sigmet data type.\n", cmd, abbrv);
	    exit(EXIT_FAILURE);
	}
    }
    unsigned i_swp = 0;
    if (strcmp(s_swp, "all") == 0) {
	i_swp = UINT_MAX;
    } else if (sscanf(s_swp, "%u", &i_swp) != 1) {
	fprintf(stderr, "%s: expected integer or \"all\" for sweep index, got %s\n", cmd, s_swp);
	exit(EXIT_FAILURE);
    }
    struct stat st_buf;
    errno = 0;
    if (stat(path, &st_buf) == -1) {
	fprintf(stderr, "%s: could not get information about %s. %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (S_ISREG(st_buf.st_mode) || S_ISFIFO(st_buf.st_mode)) {
	/* path must be Sigmet raw product file */
	ray_hdrs_fm_fl(path, type, i_swp, cmd);
    } else if (S_ISSOCK(st_buf.st_mode)) {
	ray_hdrs_fm_skt(path, type, i_swp, cmd);
    } else {
	fprintf(stderr, "%s: %s must be a file, fifo, or socket.", cmd, path);
	exit(EXIT_FAILURE);
    }
}

/* Obtain ray headers for data type with abbreviation abbrv, sweep i_swp from Sigmet raw product file at
 * path, print, and exit. If type is NULL, use first data type in volume. cmd is for error messages. */
static void ray_hdrs_fm_fl(const char * path, const struct Sigmet_DataType * type, unsigned i_swp,
	const char * cmd)
{
    struct Sigmet_ErrMsg err_msg = { .str = (char[SIGMET_ERR_LEN]){ '\0' }, .sz = SIGMET_ERR_LEN};
    FILE *vol_fl = fopen(path, "r");
    if (vol_fl == NULL) {
	fprintf(stderr, "%s: could not open raw product file %s. %s\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    struct Sigmet_VolHdr vol_hdr = {0};
    if ( !Sigmet_VolReadVHdr(vol_fl, &vol_hdr, &err_msg) ) {
	fclose(vol_fl);
	fprintf(stderr, "%s: could not read volume headers from %s. %s\n", cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    unsigned num_swps = Sigmet_VolNumSwps(&vol_hdr);
    if ( !SigmetRaw_GetAllSwps(i_swp) && i_swp >= num_swps) {
	fprintf(stderr, "%s: sweep index %d out of range. Volume %s has %d sweeps.\n",
		cmd, i_swp, path, num_swps);
	exit(EXIT_FAILURE);
    }
    _Bool hav_xhdr = Sigmet_VolXHdr(&vol_hdr);
    unsigned y = hav_xhdr;		/* Type index. Default points to first actual data type in
					 * product. Extended header, which is stored as data, is
					 * not actually a data type. */
    if (type != NULL) {
	int y_ = Sigmet_VolTypeIdx(type, &vol_hdr);
	if (y_ == -1) {
	    fprintf(stderr, "%s: %s data type is not in volume at %s.\n",
		    cmd, Sigmet_DataTypeAbbrv(type), path);
	    exit(EXIT_FAILURE);
	}
	y = y_;
    }
    /* Need sweep headers because ray time is based on sweep time. */
    struct Sigmet_SwpHdr * swp_hdrs = calloc(num_swps, sizeof *swp_hdrs);
    if (swp_hdrs == NULL) {
	fprintf(stderr, "%s could not allocate memory for %d sweep headers from raw product file %s\n",
		cmd, num_swps, path);
	exit(EXIT_FAILURE);
    }
    /* Rays, dimensioned [num_swps][num_rays][num_types], per raw product format */
    unsigned num_rays = Sigmet_VolNumRays(&vol_hdr);
    unsigned num_types = Sigmet_VolNumTypes(&vol_hdr);
    struct Sigmet_Ray (*rays)[num_rays][num_types] = NULL;
    rays = calloc(num_swps, sizeof *rays);
    if (rays == NULL) {
	fprintf(stderr, "%s could not allocate memory for array of  %d by %d ray structures "
		"from raw product file %s\n", cmd, num_swps, num_rays, path);
	exit(EXIT_FAILURE);
    }
    void * dat_buf = NULL;		/* Raw product data. Might be needed for extended headers */
    size_t dat_buf_sz = 0;
    if (hav_xhdr) {
	/* If available, obtain high resolution time from extended headers, which are part of the data,
	 * which means reading all of the data.
	 *
	 * dat_buf will receive storage values from raw product file in file order, without structure.
	 * Bin counts will come from ray headers which will be read from the raw product file along with
	 * the data. */
	dat_buf_sz = Sigmet_VolIDatSz(&vol_hdr, &err_msg);
	if (dat_buf_sz == 0) {
	    fprintf(stderr, "%s: could not determine size of input data buffer. %s\n", cmd, err_msg.str);
	    exit(EXIT_FAILURE);
	}
	dat_buf = malloc(dat_buf_sz);
	if (dat_buf == NULL) {
	    fprintf(stderr, "%s: could not allocate memory for %zu bytes of data.\n", cmd, dat_buf_sz);
	    exit(EXIT_FAILURE);
	}
    }
    int rd = Sigmet_VolReadDat(vol_fl, &vol_hdr, num_swps, num_rays, num_types, swp_hdrs, rays,
	    dat_buf_sz, dat_buf, &err_msg);
    fclose(vol_fl);
    if (rd == 0) {
	fprintf(stderr, "%s: raw product file %s has no data. %s\n", cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    if ( !Sigmet_VolTZSet(&vol_hdr, &err_msg) ) {
	fprintf(stderr, "%s: could not set time zone from %s.\n", cmd, path);
	exit(EXIT_FAILURE);
    }
    unsigned s0 = SigmetRaw_GetAllSwps(i_swp) ? 0        : i_swp;
    unsigned s1 = SigmetRaw_GetAllSwps(i_swp) ? num_swps : i_swp + 1;
    const struct Sigmet_DataType * xhdr = Sigmet_DataTypeGet("DB_XHDR");
    int y_hxdr = Sigmet_VolTypeIdx(xhdr, &vol_hdr); /* 0 in current raw product format */
    for (unsigned s = s0; s < s1; s++) {
	double swp_tm = Sigmet_DTime(&swp_hdrs[s].tm);
	for (unsigned r = 0; r < num_rays; r++) {
	    struct Sigmet_RayHdr ray_hdr = rays[s][r][y].ray_hdr;
	    float ray_tm = NAN;
	    if (hav_xhdr) {
		Sigmet_DataTypeStorToVal(xhdr, 1, &ray_tm, rays[s][r][y_hxdr].dat, &vol_hdr);
	    } else {
		ray_tm = ray_hdr.tm;
	    }
	    int yr, mon, day, hr, min;
	    float sec;
	    if ( !Sigmet_BkTime(swp_tm + ray_tm, &yr, &mon, &day, &hr, &min, &sec) ) {
		yr = mon = day = hr = min = sec = 0;
	    }
	    printf(RAY_HDR_FMT, s, r, yr, mon, day, hr, min, sec,
		    ray_hdr.az0 * DEG_PER_RAD, ray_hdr.az1 * DEG_PER_RAD,
		    ray_hdr.tilt0 * DEG_PER_RAD, ray_hdr.tilt1 * DEG_PER_RAD,
		    ray_hdr.num_bins);
	}
    }
}

/* Obtain ray headers for data type with abbreviation abbrv, sweep i_swp from sigmet_raw daemon
 * monitoring socket at path, print, and exit. If type is NULL, daemon will set a default. cmd is
 * for error messages. */
static void ray_hdrs_fm_skt(const char * path, const struct Sigmet_DataType * type, unsigned i_swp,
	const char * cmd)
{
    /* path must be sigmet_raw daemon socket. */
    struct Sigmet_ErrMsg err_msg = { .str = (char[SIGMET_ERR_LEN]){ '\0' }, .sz = SIGMET_ERR_LEN};
    errno = 0;
    int skt_fd = SigmetRaw_DmnConnect(path, &err_msg);
    if (skt_fd == -1) {
	fprintf(stderr, "%s failed to connect to sigmet_raw daemon at %s. %s\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Will read ray headers from pipe shared with daemon socket. */
    int ray_hdr_fd[2];
    if (pipe(ray_hdr_fd) == -1) {
	fprintf(stderr, "%s could not create pipe to read ray headers from daemon at socket %s. "
		"%s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    FILE *ray_hdr_fl = fdopen(ray_hdr_fd[0], "r");
    if (ray_hdr_fl == NULL) {
	fprintf(stderr, "%s could not configure pipe to read from daemon at socket %s. %s.\n",
		cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* Error channel. */
    int err_fd[2];
    if (pipe(err_fd) == -1) {
	fprintf(stderr, "%s could not create pipe to read error information from daemon at socket %s."
		" %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* Send request message */
    struct SigmetRaw_Rqst rqst = SigmetRaw_Rqst_Init();
    SigmetRaw_Rqst_Set_SubCmd(&rqst, SigmetRawRayHeaders);
    SigmetRaw_Rqst_Set_Swp(&rqst, i_swp);
    /* Requesting "" should obtain default data type */
    SigmetRaw_Rqst_Set_DataType(&rqst, (type != NULL) ? Sigmet_DataTypeAbbrv(type) : "");
    SigmetRaw_Rqst_Set_ShFD(&rqst, ray_hdr_fd[1]);
    SigmetRaw_Rqst_Set_ErrFD(&rqst, err_fd[1]);
    if ( !SigmetRaw_Rqst_Send(skt_fd, &rqst, &err_msg) ) {
	fprintf(stderr, "%s failed to request ray headers from daemon at socket %s. %s.\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Get daemon response, which will provide status, ray count, sweep time, and time zone. */
    enum SigmetRaw_Status status = SigmetRawError;
    unsigned num_swps = 0;
    unsigned num_rays = 0;
    char tz[SIGMET_TZ_STRLEN] = {'\0'};
    struct msghdr rps = {
	.msg_iov = (struct iovec [4]){
	    [0] = { .iov_base = &status,	.iov_len = sizeof status },
	    [1] = { .iov_base = &num_swps,      .iov_len = sizeof num_swps },
	    [2] = { .iov_base = &num_rays,      .iov_len = sizeof num_rays },
	    [3] = { .iov_base = tz,		.iov_len = SIGMET_TZ_STRLEN }
	},
	.msg_iovlen = 4
    };
    if (recvmsg(skt_fd, &rps, 0) == -1) {
	fprintf(stderr, "%s: could not get response from daemon at socket %s. %s.\n",
		cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    close(ray_hdr_fd[1]);
    close(err_fd[1]);
    ray_hdr_fd[1] = err_fd[1] = -1;
    if (status != SigmetRawOkay) {
	fprintf(stderr, "%s failed for daemon at socket %s. ", cmd, path);
	close(ray_hdr_fd[0]);
	FILE * err = fdopen(err_fd[0], "r");
	if (err == NULL) {
	    fprintf(stderr, "%s could not configure pipe to read error information from daemon "
		    "at socket %s. %s.\n", cmd, path, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	/* Copy error information from error channel to stderr. */
	for (int c = fgetc(err); c != EOF; c = fgetc(err)) {
	    fputc(c, stderr);
	}
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
    }
    if (num_swps == 0) {
	fprintf(stderr, "%s: got impossible sweep count (%d) from daemon at socket %s.\n",
		cmd, num_rays, path);
	exit(EXIT_FAILURE);
    }
    if (num_rays == 0) {
	fprintf(stderr, "%s: got impossible ray count (%d) from daemon at socket %s.\n",
		cmd, num_rays, path);
	exit(EXIT_FAILURE);
    }
    /* Print ray headers with time zone from Sigmet volume, not local time. */
    if ( setenv("TZ", tz, 1) == -1 ) {
	fprintf(stderr, "%s could not set TZ environment variable to %s for daemon at socket %s. %s.",
		cmd, tz, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    tzset();
    unsigned num_wray_hdrs = num_swps * num_rays;
    struct SigmetRaw_RayHdr * wray_hdrs = calloc(num_wray_hdrs, sizeof(struct SigmetRaw_RayHdr));
    if (wray_hdrs == NULL) {
	fprintf(stderr, "%s could not allocate memory for %d * %d ray headers from "
		"daemon at socket %s\n", cmd, num_swps, num_rays, path);
	exit(EXIT_FAILURE);
    }
    size_t num_rd = fread(wray_hdrs, sizeof *wray_hdrs, num_wray_hdrs, ray_hdr_fl);
    fclose(ray_hdr_fl);
    if (num_rd == 0) {
	fprintf(stderr, "%s: could not read ray headers from daemon at socket %s.\n", cmd, path);
	exit(EXIT_FAILURE);
    }
    num_wray_hdrs = num_rd;
    for (unsigned n = 0, s = 0; s < num_swps; s++) {
	for (unsigned r = 0; r < num_rays && n < num_wray_hdrs; r++, n++) {
	    int yr, mon, day, hr, min;
	    float sec;
	    if ( !Sigmet_BkTime(wray_hdrs[n].tm, &yr, &mon, &day, &hr, &min, &sec) ) {
		yr = mon = day = hr = min = sec = 0;
	    }
	    printf(RAY_HDR_FMT, s, r, yr, mon, day, hr, min, sec,
		    wray_hdrs[n].ray_hdr.az0 * DEG_PER_RAD, wray_hdrs[n].ray_hdr.az1 * DEG_PER_RAD,
		    wray_hdrs[n].ray_hdr.tilt0 * DEG_PER_RAD, wray_hdrs[n].ray_hdr.tilt1 * DEG_PER_RAD,
		    wray_hdrs[n].ray_hdr.num_bins);
	}
    }
}

