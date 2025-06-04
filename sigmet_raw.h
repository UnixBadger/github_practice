/*
 *	sigmet_raw.h --
 *		Global variables and functions for sigmet_raw applications.
 * 	--
 *
 *	Copyright (c) 2020, Gordon D. Carrie. All rights reserved.
 *	Licensed under the Academic Free License version 3.0. See file AFL-3.0 or
 *	https://opensource.org/licenses/AFL-3.0.
 *	Please send feedback to dev1960@polarismail.net
 */

#ifndef SIGMET_RAW_H_
#define SIGMET_RAW_H_

#include <limits.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include "sigmet.h"

/* Daemon subcommand specifiers */
enum SigmetRaw_SubCmdN {
    SigmetRawExit, SigmetRawVolumeHeaders, SigmetRawSwpHeaders, SigmetRawRayHeaders,
    SigmetRawData, SigmetRawCorx
};

/* Daemon status codes */
enum SigmetRaw_Status { SigmetRawError, SigmetRawOkay };

/* Order of parameters in client-to-daemon requests */
#define SIGMETRAW_RQST_IOVLEN 3
enum { SigmetRawRqstSubCmd, SigmetRawRqstDataType, SigmetRawRqstSwpIdx };

/* Order of shared file descriptors in client-to-daemon requests */
enum {SigmetRawErrFD, SigmetRawHdrDataFD};

/* Client to daemon requests. */
struct SigmetRaw_Rqst {
    enum SigmetRaw_SubCmdN sub_cmd_n;	/* Subcommand. Always used. */
    char abbrv[SIGMET_DATA_TYPE_LEN];	/* Data type abbreviation. Sometimes used. */
    int s;				/* Sweep index. Sometimes used. */
    int hd_fd;				/* Shared file descriptor for headers or data. */
    int err_fd;				/* Error message channel */
};

struct SigmetRaw_Rqst SigmetRaw_Rqst_Init(void);
void SigmetRaw_Rqst_Set_SubCmd(struct SigmetRaw_Rqst *, enum SigmetRaw_SubCmdN);
void SigmetRaw_Rqst_Set_DataType(struct SigmetRaw_Rqst *, const char *);
void SigmetRaw_Rqst_Set_Swp(struct SigmetRaw_Rqst *, unsigned);
void SigmetRaw_Rqst_Set_ShFD(struct SigmetRaw_Rqst *, int);
void SigmetRaw_Rqst_Set_ErrFD(struct SigmetRaw_Rqst *, int);
int SigmetRaw_Rqst_Send(int, struct SigmetRaw_Rqst *, struct Sigmet_ErrMsg *);

/* Daemons call sendmsg to respond to client subcommand requests. Clients call recvmsg to receive
 * the reponse. The msghdr.msg_iov array provides command status and metadata the client might
 * need to manipulate headers and data in subcommand output. This enumerator gives descriptive
 * indeces for the array elements. All array elements are in all responses, although not all may
 * be used. */
#define SIGMETRAW_RPS_IOVLEN 7
enum { SigmetRawRpsStatus, SigmetRawRpsNumSwps, SigmetRawRpsNumRays, SigmetRawRpsNumSwpBins,
    SigmetRawRpsSwpTm, SigmetRawRpsTZ, SigmetRawRpsErr };

/* Sigmet raw header appended with extended header time, if available */
struct SigmetRaw_RayHdr {
    struct Sigmet_RayHdr ray_hdr;
    double tm;				/* Sweep time + (ray_hdr time OR extended header time) or NAN */
};

/* Functions */
static inline _Bool SigmetRaw_GetAllSwps(unsigned i_swp) { return i_swp == UINT_MAX; }
int SigmetRaw_DmnConnect(const char *, struct Sigmet_ErrMsg *);
int SigmetRaw_Rqst(const char *, enum SigmetRaw_SubCmdN, const struct Sigmet_DataType *, int, int,
	enum SigmetRaw_Status *, int *, int *, int *, double *, char * tz, char *, struct Sigmet_ErrMsg *);
int SigmetRaw_Dmn_VolHdr(int, struct Sigmet_VolHdr *, struct Sigmet_ErrMsg *);
int SigmetRaw_Rqst_RayHdrs(unsigned *, unsigned *, double [SIGMET_MAX_SWPS], char [SIGMET_TZ_STRLEN],
	const char *, const struct Sigmet_DataType *, unsigned, struct Sigmet_ErrMsg *);

#endif

