/*
 *	data.c --
 *		Print data from volume as text. See sigmet_raw (1).
 *	--
 *
 *	Copyright (c) 2022, Gordon D. Carrie. All rights reserved.
 *	Licensed under the Academic Free License version 3.0
 *	See file AFL-3.0 or https://opensource.org/licenses/AFL-3.0.
 *	Send feedback to dev1960@polarismail.net
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <math.h>
#include <libgen.h>
#include "sigmet.h"
#include "sigmet_raw.h"

static void data_fm_fl(const char *, const struct Sigmet_DataType *, int, _Bool, const char *);
static void skt_to_txt(const char *, const struct Sigmet_DataType *, int, const char *);
static void skt_to_bin(const char *, const struct Sigmet_DataType *, int, const char *);

int main(int argc, char *argv[])
{
    /* If set, use $APP_NAME in error messages instead of argv[0]. */
    char *cmd = getenv("APP_NAME") ? getenv("APP_NAME") : basename(argv[0]);
    char * abbrv = NULL;		/* Data type abbreviation, e.g. "DB_DBZ" */
    char * s_s = NULL;			/* Sweep index */
    char * path = NULL;			/* Volume file or socket */
    _Bool txt = true;			/* true => print data as text. false => send native binary. */
    if (argc == 4) {
	abbrv = argv[1];
	s_s = argv[2];
	path = argv[3];
    } else if (argc == 5 && strcmp(argv[1], "-b") == 0) {
	abbrv = argv[2];
	s_s = argv[3];
	path = argv[4];
	txt = false;
    } else {
	fprintf(stderr, "Usage: %s [-b] data_type sweep_index raw_product_file|socket\n", cmd);
	exit(EXIT_FAILURE);
    }
    const struct Sigmet_DataType * type = Sigmet_DataTypeGet(abbrv);
    if (type == NULL) {
	fprintf(stderr, "%s: %s is not a Sigmet data type.\n", cmd, abbrv);
	exit(EXIT_FAILURE);
    }
    int s;
    if (sscanf(s_s, "%d", &s) != 1) {
	fprintf(stderr, "%s: expected integer for sweep index, got %s\n", cmd, s_s);
	exit(EXIT_FAILURE);
    }
    struct stat st_buf;
    if (stat(path, &st_buf) == -1) {
	fprintf(stderr, "sigmet_raw %s %s %s: could not get information about %s. %s\n", 
		cmd, abbrv, s_s, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (S_ISREG(st_buf.st_mode) || S_ISFIFO(st_buf.st_mode)) {
	/* path must specify a Sigmet raw product file */
	data_fm_fl(path, type, s, txt, cmd);
    } else if (S_ISSOCK(st_buf.st_mode)) {
	/* path must be sigmet_raw daemon socket. */
	if (txt) {
	    skt_to_txt(path, type, s, cmd);
	} else {
	    skt_to_bin(path, type, s, cmd);
	}

    } else {
	fprintf(stderr, "%s: %s must be a file, fifo, or socket.", cmd, path);
	exit(EXIT_FAILURE);
    }
}

/* Obtain data for data type type, sweep s from Sigmet raw product file at path, print, and exit.
 * If txt is true, print data as text, otherwise print native binary. cmd is for error messages. */
static void data_fm_fl(const char * path, const struct Sigmet_DataType * type, int s, _Bool txt,
	const char * cmd)
{
    struct Sigmet_ErrMsg err_msg = { .str = (char[SIGMET_ERR_LEN]){ '\0' }, .sz = SIGMET_ERR_LEN};
    FILE *vol_fl = fopen(path, "r");
    if (vol_fl == NULL) {
	fprintf(stderr, "%s: could not open file. %s\n", cmd, strerror(errno));
	exit(EXIT_FAILURE);
    }
    struct Sigmet_VolHdr vol_hdr;
    memset(&vol_hdr, 0, sizeof vol_hdr);
    if ( !Sigmet_VolReadVHdr(vol_fl, &vol_hdr, &err_msg) ) {
	fclose(vol_fl);
	fprintf(stderr, "%s: could not read volume headers from %s. %s\n", cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    int num_swps = Sigmet_VolNumSwps(&vol_hdr);
    if (s < 0 || s >= num_swps) {
	fprintf(stderr, "%s: sweep index %d out of range. Volume has %d sweeps.\n",
		cmd, s, num_swps);
	exit(EXIT_FAILURE);
    }
    int num_rays = Sigmet_VolNumRays(&vol_hdr);
    int num_types = Sigmet_VolNumTypes(&vol_hdr);
    int y = (type != NULL) ? Sigmet_VolTypeIdx(type, &vol_hdr) : -1;
    if (y == -1) {
	fprintf(stderr, "%s: %s data type is not in volume at %s.\n",
		cmd, Sigmet_DataTypeAbbrv(type), path);
	exit(EXIT_FAILURE);
    }
    int num_bins = Sigmet_VolNumBins(&vol_hdr);
    if (num_bins <= 0) {
	fprintf(stderr, "%s: %s corrupt, claims %d bins per ray.\n", cmd, path, num_bins);
	exit(EXIT_FAILURE);
    }
    /* Rays, dimensioned [num_swps][num_rays][num_types], per raw product format. */
    struct Sigmet_Ray (*rays)[num_rays][num_types] = NULL;
    rays = calloc(num_swps, sizeof *rays);
    if (rays == NULL) {
	fprintf(stderr, "%s could not allocate memory for array of  %d by %d ray structures "
		"from raw product file %s\n", cmd, num_swps, num_rays, path);
	exit(EXIT_FAILURE);
    }
    /* dat_buf will receive storage values from raw product file in file order, without structure.
     * Bin counts will come from ray headers which will be read from the raw product file along with
     * the data. */
    size_t dat_buf_sz = Sigmet_VolIDatSz(&vol_hdr, &err_msg);
    if (dat_buf_sz == 0) {
	fprintf(stderr, "%s: could not determine size of input data buffer. %s\n", cmd, err_msg.str);
	exit(EXIT_FAILURE);
    }
    void * dat_buf = malloc(dat_buf_sz);
    if (dat_buf == NULL) {
	fprintf(stderr, "%s: could not allocate memory for %zu bytes of data.\n", cmd, dat_buf_sz);
	exit(EXIT_FAILURE);
    }
    /* Read volume data headers and data values. */
    int rd = Sigmet_VolReadDat(vol_fl, &vol_hdr, num_swps, num_rays, num_types, NULL,
	    rays, dat_buf_sz, dat_buf, &err_msg);
    fclose(vol_fl);
    if (rd == 0) {
	fprintf(stderr, "%s: volume at %s has no data. %s\n", cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Allocate ray with maximum bin count from ray headers, not volume headers. */
    int num_bins_max = 0;
    for (int r = 0; r < num_rays; r++) {
	if (rays[s][r][y].ray_hdr.num_bins > num_bins_max) {
	    num_bins_max = rays[s][r][y].ray_hdr.num_bins;
	}
    }
    if (num_bins_max == 0) {
	fprintf(stderr, "%s: raw product file %s has no data.\n", cmd, path);
	exit(EXIT_FAILURE);
    }
    /* Convert file representation to float for bins in each ray and send. */
    float * dat = calloc(num_bins_max, sizeof *dat);
    if (dat == NULL) {
	fprintf(stderr, "%s: could not allocate memory for %d x %d output array.\n",
		cmd, num_rays, num_bins_max);
	exit(EXIT_FAILURE);
    }
    if (txt) {
	/* Text output */
	const char * fmt = Sigmet_DataType_PrintFmt(type);
	if (fmt == NULL) {
	    fprintf(stderr, "%s: could not obtain print format for data type %s "
		    "in raw product file %s.\n", cmd, Sigmet_DataTypeAbbrv(type), path);
	    exit(EXIT_FAILURE);
	}
	for (int r = 0; r < num_rays; r++) {
	    for (int b = 0; b < num_bins_max; b++) {
		dat[b] = NAN;
	    }
	    void * idat = rays[s][r][y].dat;
	    if (idat != NULL) {
		int nb = rays[s][r][y].ray_hdr.num_bins;
		Sigmet_DataTypeStorToVal(type, nb, dat, idat, &vol_hdr);
	    }
	    for (int b = 0; b < num_bins_max; b++) {
		printf(fmt, dat[b]);
	    }
	    printf("\n");
	}
    } else {
	/* Assume native binary output */
	for (int r = 0; r < num_rays; r++) {
	    void * idat = rays[s][r][y].dat;
	    if (idat != NULL) {
		int nb = rays[s][r][y].ray_hdr.num_bins;
		Sigmet_DataTypeStorToVal(type, nb, dat, idat, &vol_hdr);
		fwrite(dat, sizeof *dat, nb, stdout);
	    }
	}
    }
    exit(EXIT_SUCCESS);
}

/* Obtain ray headers and sweep data for data type type, sweep s, from sigmet_raw daemon monitoring
 * socket at path. Ray headers provide data dimensions. Print the sweep data to standard output as text.
 * cmd is for error messages. */
static void skt_to_txt(const char * path, const struct Sigmet_DataType * type, int s, const char * cmd)
{
    struct Sigmet_ErrMsg err_msg = { .str = (char[SIGMET_ERR_LEN]){ '\0' }, .sz = SIGMET_ERR_LEN};
    errno = 0;
    const char * abbrv = Sigmet_DataTypeAbbrv(type);
    /* Obtain ray headers - needed for bin counts. Note: binary output skips empty rays. Text output prints
     * them as num_bins*"NAN". */
    int rh_skt_fd = SigmetRaw_DmnConnect(path, &err_msg);
    if (rh_skt_fd == -1) {
	fprintf(stderr, "%s failed to connect to sigmet_raw daemon at %s. %s\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    int rh_pipe[2];			/* Ray headers will appear here. */
    if (pipe(rh_pipe) == -1) {
	fprintf(stderr, "%s could not create pipe to read ray headers from daemon at socket %s."
		" %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    FILE *rh_fl = fdopen(rh_pipe[0], "r");
    if (rh_fl == NULL) {
	fprintf(stderr, "%s could not configure pipe to read ray headers from daemon "
		"at socket %s. %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    int rh_err_pipe[2];			/* Ray header transfer error channel */
    if (pipe(rh_err_pipe) == -1) {
	fprintf(stderr, "%s could not create pipe to read ray header error information from "
		"daemon at socket %s. %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* Send request message for ray headers */
    struct SigmetRaw_Rqst rh_rqst = SigmetRaw_Rqst_Init();
    SigmetRaw_Rqst_Set_SubCmd(&rh_rqst, SigmetRawRayHeaders);
    SigmetRaw_Rqst_Set_Swp(&rh_rqst, s);
    SigmetRaw_Rqst_Set_DataType(&rh_rqst, abbrv);
    SigmetRaw_Rqst_Set_ShFD(&rh_rqst, rh_pipe[1]);
    SigmetRaw_Rqst_Set_ErrFD(&rh_rqst, rh_err_pipe[1]);
    if ( !SigmetRaw_Rqst_Send(rh_skt_fd, &rh_rqst, &err_msg) ) {
	fprintf(stderr, "%s failed to request ray headers from daemon at socket %s. %s.\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Get daemon response, which should provide status, ray count, type count, and type index. */
    enum SigmetRaw_Status rh_stat = SigmetRawError;
    int num_swps = -1;			/* Not used in this subcommand. Daemon sends it anyway. */
    int num_rays = -1;
    double swp_tm = NAN;
    char tz[SIGMET_TZ_STRLEN] = {'\0'};
    struct msghdr rh_rps = {
	.msg_iov = (struct iovec [5]){
	    [0] = { .iov_base = &rh_stat,   .iov_len = sizeof rh_stat },
	    [1] = { .iov_base = &num_swps,  .iov_len = sizeof num_swps },
	    [2] = { .iov_base = &num_rays,  .iov_len = sizeof num_rays },
	    [3] = { .iov_base = &swp_tm,    .iov_len = sizeof swp_tm },
	    [4] = { .iov_base = tz,         .iov_len = SIGMET_TZ_STRLEN }
	},
	.msg_iovlen = 5
    };
    if (recvmsg(rh_skt_fd, &rh_rps, 0) == -1) {
	fprintf(stderr, "%s: when requesting ray headers could not get response from daemon "
		"at %s. %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    close(rh_pipe[1]);		/* Daemon writes to pipe. This process reads from pipe. */
    close(rh_err_pipe[1]);
    close(rh_skt_fd);		/* Done with socket */
    if (rh_stat != SigmetRawOkay) {
	fprintf(stderr, "%s failed for %s. ", cmd, path);
	FILE * err = fdopen(rh_err_pipe[0], "r");
	if (err == NULL) {
	    fprintf(stderr, "%s when requesting ray headers could not configure pipe to read "
		    "error information from daemon at socket %s. %s.\n",
		    cmd, path, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	/* Copy error information from error channel to stderr. */
	for (int c = fgetc(err); c != EOF; c = fgetc(err)) {
	    fputc(c, stderr);
	}
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
    }
    close(rh_err_pipe[0]);
    if (num_rays <= 0) {
	fprintf(stderr, "%s: got impossible ray count (%d) from daemon at socket %s.\n",
		cmd, num_rays, path);
	exit(EXIT_FAILURE);
    }
    /* Allocate ray headers and read from daemon via pipe. */
    struct SigmetRaw_RayHdr * wray_hdrs = NULL;
    wray_hdrs = calloc(num_rays, sizeof *wray_hdrs);
    if (wray_hdrs == NULL) {
	fprintf(stderr, "%s could not allocate memory for %d rays headers from daemon at socket %s\n",
		cmd, num_rays, path);
	exit(EXIT_FAILURE);
    }
    if (fread(wray_hdrs, sizeof wray_hdrs[0], num_rays, rh_fl) != (size_t)num_rays) {
	fprintf(stderr, "%s: could not read ray headers from daemon at socket %s\n.", cmd, path);
	exit(EXIT_FAILURE);
    }
    fclose(rh_fl);
    /* Allocate data array with space for all of the data bins in sweep. */
    size_t num_bins_tot = 0;
    for (int r = 0; r < num_rays; r++) {
	num_bins_tot += wray_hdrs[r].ray_hdr.num_bins;
    }
    float * dat = calloc(num_bins_tot, sizeof(float));
    if (dat == NULL) {
	fprintf(stderr, "%s %s %d: could not allocate memory for array of %zu data values "
		"from daemon at socket %s.", cmd, abbrv, s, num_bins_tot, path);
	exit(EXIT_FAILURE);
    }
    for (size_t b = 0; b < num_bins_tot; b++) {
	dat[b] = NAN;
    }
    /* Obtain data from pipe shared with daemon */
    int dat_skt_fd = SigmetRaw_DmnConnect(path, &err_msg);
    if (dat_skt_fd == -1) {
	fprintf(stderr, "%s failed to connect to sigmet_raw daemon at %s. %s\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    int dat_pipe[2];
    if (pipe(dat_pipe) == -1) {
	fprintf(stderr, "%s could not create pipe to daemon at socket %s. %s.\n",
		cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    FILE * dat_fl = fdopen(dat_pipe[0], "r");
    if (dat_fl == NULL) {
	fprintf(stderr, "%s could not configure pipe to read from daemon at socket %s. %s.\n",
		cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* Error channel for data. Daemon will write error messages to it. */
    int dat_err_fd[2];
    if (pipe(dat_err_fd) == -1) {
	fprintf(stderr, "%s could not create pipe to read error information from daemon at "
		"socket %s. %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* Send request message for data */
    struct SigmetRaw_Rqst dat_rqst = SigmetRaw_Rqst_Init();
    SigmetRaw_Rqst_Set_SubCmd(&dat_rqst, SigmetRawData);
    SigmetRaw_Rqst_Set_DataType(&dat_rqst, abbrv);
    SigmetRaw_Rqst_Set_Swp(&dat_rqst, s);
    SigmetRaw_Rqst_Set_ShFD(&dat_rqst, dat_pipe[1]);
    SigmetRaw_Rqst_Set_ErrFD(&dat_rqst, dat_err_fd[1]);
    if ( !SigmetRaw_Rqst_Send(dat_skt_fd, &dat_rqst, &err_msg) ) {
	fprintf(stderr, "%s failed to request ray headers from daemon at socket %s. %s.\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Get daemon response. */
    enum SigmetRaw_Status dat_stat = SigmetRawError;
    struct msghdr dat_rps = {
	.msg_iov = (struct iovec [1]){
	    [0] = { .iov_base = &dat_stat, .iov_len = sizeof dat_stat }
	},
	.msg_iovlen = 1
    };
    if (recvmsg(dat_skt_fd, &dat_rps, 0) == -1) {
	fprintf(stderr, "%s: could not get response from daemon at %s. %s.\n",
		cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    close(dat_pipe[1]);		/* Daemon writes to pipe. This process only reads from it. */
    close(dat_err_fd[1]);
    close(dat_skt_fd);		/* Done with socket. */
    /* Check status for data read request */
    if (dat_stat != SigmetRawOkay) {
	fprintf(stderr, "%s failed for daemon at socket %s. ", cmd, path);
	/* Request failed. Copy error information from error channel to stderr. */
	FILE * err = fdopen(dat_err_fd[0], "r");
	if (err == NULL) {
	    fprintf(stderr, "%s could not configure pipe to read error information from daemon "
		    "at socket %s. %s.\n", cmd, path, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	for (int c = fgetc(err); c != EOF; c = fgetc(err)) {
	    fputc(c, stderr);
	}
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
    }
    close(dat_err_fd[0]);
    /* Read the sweep data */
    if (fread(dat, sizeof *dat, num_bins_tot, dat_fl) != num_bins_tot) {
	fprintf(stderr, "%s: could not read %s data for sweep %d from sigmet_raw daemon "
		"at socket %s.\n", cmd, abbrv, s, path);
	exit(EXIT_FAILURE);
    }
    fclose(dat_fl);
    /* Write the data */
    int num_bins_max = 0;		/* Use bin count for sweep, not volume. */
    for (int r = 0; r < num_rays; r++) {
	if (wray_hdrs[r].ray_hdr.num_bins > num_bins_max) {
	    num_bins_max = wray_hdrs[r].ray_hdr.num_bins;
	}
    }
    const char * fmt = Sigmet_DataType_PrintFmt(type);
    if (fmt == NULL) {
	fprintf(stderr, "%s: could not obtain print format for data type %s in daemon at socket %s.\n",
		cmd, abbrv, path);
	exit(EXIT_FAILURE);
    }
    for (int r = 0; r < num_rays; r++) {
	int b = 0;
	for ( ; b < wray_hdrs[r].ray_hdr.num_bins; b++) {
	    printf(fmt, *dat++);
	}
	for ( ; b < num_bins_max; b++) {
	    printf(fmt, NAN);
	}
	printf("\n");
    }

    exit(EXIT_SUCCESS);
}

/* Request sigmet_raw daemon at path send sweep data for data type type, sweep s to standard output
   of this process in native binary. */
static void skt_to_bin(const char * path, const struct Sigmet_DataType * type, int s, const char * cmd)
{
    struct Sigmet_ErrMsg err_msg = { .str = (char[SIGMET_ERR_LEN]){ '\0' }, .sz = SIGMET_ERR_LEN};
    errno = 0;
    const char * abbrv = Sigmet_DataTypeAbbrv(type);
    int skt_fd = SigmetRaw_DmnConnect(path, &err_msg);
    if (skt_fd == -1) {
	fprintf(stderr, "%s failed to connect to sigmet_raw daemon at %s. %s\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Error channel. */
    int err_pipe[2];
    if (pipe(err_pipe) == -1) {
	fprintf(stderr, "%s could not create pipe to read error information from daemon at "
		"socket %s. %s.\n", cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* Send request message */
    struct SigmetRaw_Rqst rqst = SigmetRaw_Rqst_Init();
    SigmetRaw_Rqst_Set_SubCmd(&rqst, SigmetRawData);
    SigmetRaw_Rqst_Set_DataType(&rqst, abbrv);
    SigmetRaw_Rqst_Set_Swp(&rqst, s);
    SigmetRaw_Rqst_Set_ShFD(&rqst, STDOUT_FILENO);
    SigmetRaw_Rqst_Set_ErrFD(&rqst, err_pipe[1]);
    if ( !SigmetRaw_Rqst_Send(skt_fd, &rqst, &err_msg) ) {
	fprintf(stderr, "%s failed to request ray headers from daemon at socket %s. %s.\n",
		cmd, path, err_msg.str);
	exit(EXIT_FAILURE);
    }
    /* Get daemon response, which will provide status. */
    enum SigmetRaw_Status status = SigmetRawError;
    struct msghdr rps = {
	.msg_iov = (struct iovec [1]){
	    [0] = { .iov_base = &status, .iov_len = sizeof status },
	},
	.msg_iovlen = 1
    };
    if (recvmsg(skt_fd, &rps, 0) == -1) {
	fprintf(stderr, "%s: could not get response from daemon at socket %s. %s.\n",
		cmd, path, strerror(errno));
	exit(EXIT_FAILURE);
    }
    close(err_pipe[1]);
    err_pipe[1] = -1;
    if (status != SigmetRawOkay) {
	fprintf(stderr, "%s failed for daemon at socket %s. ", cmd, path);
	FILE * err = fdopen(err_pipe[0], "r");
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
    /* Daemon is writing data to standard output of this process. All done here. */
    exit(EXIT_SUCCESS);
}

