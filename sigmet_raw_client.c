/*
 *	sigmet_raw_client.c --
 *		Common functions for sigmet_raw subcommands.
 *	--
 *
 *	Copyright (c) 2021, Gordon D. Carrie. All rights reserved.
 *	Licensed under the Academic Free License version 3.0
 *	See file AFL-3.0 or https://opensource.org/licenses/AFL-3.0.
 *	Send feedback to dev1960@polarismail.net
 */

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include "sigmet.h"
#include "sigmet_raw.h"

struct SigmetRaw_Rqst SigmetRaw_Rqst_Init(void)
{
    struct SigmetRaw_Rqst rqst = (struct SigmetRaw_Rqst){
	.sub_cmd_n = -1, .s = -1, .hd_fd = -1, .err_fd = -1
    };
    memset(rqst.abbrv, 0, SIGMET_DATA_TYPE_LEN);
    return rqst;
}
void SigmetRaw_Rqst_Set_SubCmd(struct SigmetRaw_Rqst * rqst_p, enum SigmetRaw_SubCmdN sub_cmd_n)
{
    rqst_p->sub_cmd_n = sub_cmd_n;
}
void SigmetRaw_Rqst_Set_DataType(struct SigmetRaw_Rqst * rqst_p, const char * abbrv)
{
    snprintf(rqst_p->abbrv, SIGMET_DATA_TYPE_LEN, "%s", abbrv);
}
void SigmetRaw_Rqst_Set_Swp(struct SigmetRaw_Rqst * rqst_p, unsigned s)
{
    rqst_p->s = s;
}
void SigmetRaw_Rqst_Set_ShFD(struct SigmetRaw_Rqst * rqst_p, int hd_fd)
{
    rqst_p->hd_fd = hd_fd;
}
void SigmetRaw_Rqst_Set_ErrFD(struct SigmetRaw_Rqst * rqst_p, int err_fd)
{
    rqst_p->err_fd = err_fd;
}

/* Popluate a msghdr struct with contents of client-to-daemon request at rqst_p and send it to
 * socket at skt_path, which must be a socket created by and being monitored by a daemon spawned
 * with a call to "sigmet_raw daemon skt_path ..." Return 1/0 on success/failure. On failure,
 * err_msg_p will contain error information. */
int SigmetRaw_Rqst_Send(int skt_fd, struct SigmetRaw_Rqst * rqst_p, struct Sigmet_ErrMsg * err_msg_p)
{
    struct msghdr rqst_msg = {
	.msg_iov = (struct iovec [SIGMETRAW_RQST_IOVLEN]){
	    [SigmetRawRqstSubCmd] = {
		.iov_base = &rqst_p->sub_cmd_n,
		.iov_len = sizeof rqst_p->sub_cmd_n
	    },
	    [SigmetRawRqstDataType] = {
		.iov_base = &rqst_p->abbrv,
		.iov_len = SIGMET_DATA_TYPE_LEN
	    },
	    [SigmetRawRqstSwpIdx] = {
		.iov_base = &rqst_p->s,
		.iov_len = sizeof rqst_p->s
	    }
	},
	.msg_iovlen = SIGMETRAW_RQST_IOVLEN
    };

    /* Put shared file descriptors for headers/data and error info into ancillary data.
     * If client does not want to share a file descriptor (e.g. exit command) share a placeholder.
     * This keeps message size consistent so daemon knows size of incoming message. */
    int fd0 = -1;
    if (rqst_p->err_fd < 0 || rqst_p->hd_fd < 0) {
	char * p0 = "/dev/null";
	fd0 = open(p0, O_RDONLY);
	if (fd0 == -1) {
	    Sigmet_ErrMsg_Print(err_msg_p, "%s coud not open placeholder file %s. %s.",
		    __func__, p0, strerror(errno));
	    return 0;
	}
    }
    int fd[2] = {-1};
    fd[SigmetRawErrFD] = (rqst_p->err_fd >= 0) ? rqst_p->err_fd : fd0;
    fd[SigmetRawHdrDataFD] = (rqst_p->hd_fd >= 0) ? rqst_p->hd_fd : fd0;
    /* Wrap cmsghdr in union in order to ensure it is suitably aligned. See CMSG_LEN(3). */
    union {
	char buf[CMSG_SPACE(sizeof fd)];
	struct cmsghdr align;
    } cmsgbuf = {0};
    rqst_msg.msg_control = cmsgbuf.buf;
    rqst_msg.msg_controllen = sizeof(cmsgbuf.buf);
    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&rqst_msg);
    if (cmsg == NULL) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s could not obtain memory for request message control.",
		__func__);
	return 0;
    }
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof fd);
    memcpy(CMSG_DATA(cmsg), fd, sizeof(fd));

    /* Send request */
    errno = 0;
    ssize_t w = sendmsg(skt_fd, &rqst_msg, 0);
    if (w == -1) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s failed to send message to daemon. %s.",
		__func__, strerror(errno));
	return 0;
    }
    close(fd0);
    return 1;
}

int SigmetRaw_DmnConnect(const char * skt_path, struct Sigmet_ErrMsg * err_msg_p)
{
    errno = 0;
    int skt_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (skt_fd == -1) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s could not create socket. %s.",
		__func__, strerror(errno));
	return -1;
    }
    struct sockaddr_un sa_un = { .sun_family = AF_UNIX, .sun_path = {'\0'} };
    size_t plen = sizeof sa_un.sun_path;
    int path_len = snprintf(sa_un.sun_path, plen, "%s", skt_path);
    if ( path_len >= (int)plen ) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s: path too big for unix socket address."
		"System limit is %zu characters.", __func__, plen);
	close(skt_fd);
	return -1;
    }
    /* Assume sun_path is last member of sockaddr_un */
    socklen_t len = offsetof(struct sockaddr_un, sun_path) + strlen(skt_path);
    errno = 0;
    if (connect(skt_fd, (struct sockaddr *)&sa_un, len) != 0) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s could not connect to daemon. %s.",
		__func__, strerror(errno));
	close(skt_fd);
	return -1;
    }
    return skt_fd;
}

/* Obtain volume headers from sigmet_raw daemon connection at skt_fd. Put the volume headers at
 * vol_hdr_p. Return 1/0 on success/failure. On failure, error information will be in err_msg_p,
 * which must point to storage for SIGMET_ERR_LEN bytes. */
int SigmetRaw_Dmn_VolHdr(int skt_fd, struct Sigmet_VolHdr * vol_hdr_p, struct Sigmet_ErrMsg * err_msg_p)
{
    /* Daemon will write volume headers to pipe */
    int p[2];
    if (pipe(p) == -1) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s could not create pipe to daemon. %s.",
		__func__, strerror(errno));
	return 0;
    }
    FILE *vol_hdr_fl = fdopen(p[0], "r");
    if (vol_hdr_fl == NULL) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s could not configure pipe to daemon. %s.",
		__func__, strerror(errno));
	return 0;
    }
    /* Send request, get response. */
    struct SigmetRaw_Rqst rqst = SigmetRaw_Rqst_Init();
    SigmetRaw_Rqst_Set_SubCmd(&rqst, SigmetRawVolumeHeaders);
    SigmetRaw_Rqst_Set_ShFD(&rqst, p[1]);
    if ( !SigmetRaw_Rqst_Send(skt_fd, &rqst, err_msg_p) ) {
	fclose(vol_hdr_fl);
	return 0;
    }
    /* Read volume headers from the pipe */
    if (fread(vol_hdr_p, sizeof *vol_hdr_p, 1, vol_hdr_fl) != 1) {
	Sigmet_ErrMsg_Print(err_msg_p, "%s could not read of volume headers from daemon.", __func__);
	fclose(vol_hdr_fl);
	return 0;
    }
    fclose(vol_hdr_fl);
    close(p[1]);			/* Only daemon writes to pipe. */
    return 1;
}

