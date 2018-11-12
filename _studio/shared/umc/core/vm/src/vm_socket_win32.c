// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined(_SOCKET_SUPPORT)

#include "vm_socket.h"
#include "vm_time.h"
#include "vm_debug.h"

#if defined(_WIN32) || defined(_WIN64)

#if defined (__ICL) || defined(_MSVC_LANG)
/* non-pointer conversion from "unsigned __int64" to "int32_t={signed int}" may lose significant bits */
#pragma warning(disable:2259)
#endif /* __ICL || _MSVC_LANG */

#define LINE_SIZE 128

static
int32_t fill_sockaddr(struct sockaddr_in *iaddr, vm_socket_host *host)
{
    struct hostent *ent;
    int8_t hostname[LINE_SIZE];

    memset(iaddr, 0, sizeof(*iaddr));

    iaddr->sin_family      = AF_INET;
    iaddr->sin_port        = (u_short) (host ? htons(host->port) : 0);
    iaddr->sin_addr.s_addr = htons(INADDR_ANY);

    if (!host)
        return 0;
    if (!host->hostname)
        return 0;

#if defined(UNICODE) || defined(_UNICODE)
    wcstombs(hostname, host->hostname, LINE_SIZE);
#else /* defined(UNICODE) || defined(_UNICODE) */
    strncpy(hostname, host->hostname, LINE_SIZE);
#endif /* defined(UNICODE) || defined(_UNICODE) */
    iaddr->sin_addr.s_addr = inet_addr(hostname);
    if (iaddr->sin_addr.s_addr != INADDR_NONE)
        return 0;

    ent = gethostbyname(hostname);
    if (!ent)
        return 1;
    iaddr->sin_addr.s_addr = *((u_long *)ent->h_addr_list[0]);

    return 0;

} /* int32_t fill_sockaddr(struct sockaddr_in *iaddr, vm_socket_host *host) */


vm_status vm_socket_init(vm_socket *hd,
                         vm_socket_host *local,
                         vm_socket_host *remote,
                         int32_t flags)
{
    int32_t i;
    int32_t error = 0;
    int32_t connection_attempts = 7, reattempt = 0;
      WSADATA wsaData;

    if (WSAStartup(MAKEWORD(1,1), &wsaData))
        return VM_OPERATION_FAILED;

    /* clean everything */
    for (i = 0; i <= VM_SOCKET_QUEUE; i++)hd->chns[i] = INVALID_SOCKET;

    FD_ZERO(&hd->r_set);
    FD_ZERO(&hd->w_set);

    /* flags shaping */
    if (flags & VM_SOCKET_MCAST)
    {    flags |= VM_SOCKET_UDP; }

    hd->flags=flags;

    /* create socket */
    hd->chns[0] = socket(AF_INET,
                         (flags&VM_SOCKET_UDP) ? SOCK_DGRAM : SOCK_STREAM,
                         0);

    if (hd->chns[0] == INVALID_SOCKET)
        return VM_OPERATION_FAILED;

    if (fill_sockaddr(&hd->sal, local))
        return VM_OPERATION_FAILED;

    if (fill_sockaddr(&hd->sar, remote))
        return VM_OPERATION_FAILED;

    if (bind(hd->chns[0], (struct sockaddr*)&hd->sal, sizeof(hd->sal)))
        return VM_OPERATION_FAILED;

    if (flags & VM_SOCKET_SERVER)
    {
        if (flags & VM_SOCKET_UDP)
            return VM_OK;
        if (listen(hd->chns[0], VM_SOCKET_QUEUE))
            return VM_OPERATION_FAILED;

        return VM_OK;
    }

    /* network client */
    if (flags & VM_SOCKET_MCAST)
    {
        struct ip_mreq imr;

        imr.imr_multiaddr.s_addr = hd->sar.sin_addr.s_addr;
        imr.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(hd->chns[0],
            IPPROTO_IP,
            IP_ADD_MEMBERSHIP,
            (int8_t *) &imr,
                    sizeof(imr))==SOCKET_ERROR)
              return VM_OPERATION_FAILED;
    }

    if (flags & VM_SOCKET_UDP)
        return VM_OK;

    while (connect(hd->chns[0], (struct sockaddr *)&hd->sar, sizeof(hd->sar)) &&
        reattempt < connection_attempts )

    {
        error = WSAGetLastError();

        if (error ==  WSAENETUNREACH || error == WSAECONNREFUSED
            || error ==  WSAEADDRINUSE || error ==  WSAETIMEDOUT)
        {
            reattempt++;
            vm_debug_trace1(-1, VM_STRING("%d"), reattempt);
        }
        else
        {
            vm_debug_trace1(-1, VM_STRING("%d"), reattempt);
            return VM_OPERATION_FAILED;
        }
        vm_time_sleep(1);
    }
    return VM_OK;
} /* vm_status vm_socket_init(vm_socket *hd, */

/* WE HAD TO REDEFINE THIS DEFINE TO ELIMINATE THE WARNINGS */
#undef FD_SET
#define FD_SET(fd, set) \
{ \
    if (((fd_set FAR *)(set))->fd_count < FD_SETSIZE) \
    { \
        ((fd_set FAR *)(set))->fd_array[((fd_set FAR *)(set))->fd_count++]=(fd); \
    } \
}

int32_t vm_socket_select(vm_socket *hds, int32_t nhd, int32_t masks)
{
    int32_t i, j;

    FD_ZERO(&hds->r_set);
    FD_ZERO(&hds->w_set);

    for (i = 0; i < nhd; i++)
    {
        int32_t flags = hds[i].flags;
        if (hds[i].chns[0] == INVALID_SOCKET)
            continue;

        if (masks & VM_SOCKET_ACCEPT ||
                (masks & VM_SOCKET_READ &&
                 (flags & VM_SOCKET_UDP || !(flags & VM_SOCKET_SERVER))))
            FD_SET(hds[i].chns[0], &hds->r_set);

        if (masks & VM_SOCKET_WRITE &&
                (flags & VM_SOCKET_UDP || !(flags & VM_SOCKET_SERVER)))
            FD_SET(hds[i].chns[0], &hds->w_set);

        for (j = 1; j <= VM_SOCKET_QUEUE; j++)
        {
            if (hds[i].chns[j] == INVALID_SOCKET)
                continue;

            if (masks & VM_SOCKET_READ)
                FD_SET(hds[i].chns[j], &hds->r_set);
            if (masks & VM_SOCKET_WRITE)
                FD_SET(hds[i].chns[j], &hds->w_set);
        }
    }

    i = select(0, &hds->r_set, &hds->w_set, 0, 0);

    return (i < 0) ? -1 : i;

} /* int32_t vm_socket_select(vm_socket *hds, int32_t nhd, int32_t masks) */

/* WE HAD TO REDEFINE THIS DEFINE TO ELIMINATE THE WARNINGS */
#undef FD_CLR
#define FD_CLR(fd, set) \
{ \
    u_int __i; \
    for (__i = 0; __i < ((fd_set FAR *)(set))->fd_count ; __i++) { \
        if (((fd_set FAR *)(set))->fd_array[__i] == fd) { \
            while (__i < ((fd_set FAR *)(set))->fd_count-1) { \
                ((fd_set FAR *)(set))->fd_array[__i] = \
                    ((fd_set FAR *)(set))->fd_array[__i+1]; \
                __i++; \
            } \
            ((fd_set FAR *)(set))->fd_count--; \
            break; \
        } \
    } \
}

int32_t vm_socket_next(vm_socket *hds, int32_t nhd, int32_t *idx, int32_t *chn, int32_t *type)
{
    int32_t i, j;

    for (i = 0; i < nhd; i++)
    {
        for (j = 0; j <= VM_SOCKET_QUEUE; j++)
        {
            int32_t flags = hds[i].flags;
            if (hds[i].chns[j] == INVALID_SOCKET)
                continue;

            if (FD_ISSET(hds[i].chns[j], &hds->r_set))
            {
                FD_CLR(hds[i].chns[j],&hds->r_set);

                if (idx)
                    *idx=i;
                if (chn)
                    *chn=j;
                if (type)
                {
                    *type = VM_SOCKET_READ;
                    if (j > 0)
                        return 1;
                    if (flags&VM_SOCKET_UDP)
                        return 1;
                    if (flags&VM_SOCKET_SERVER)
                        *type=VM_SOCKET_ACCEPT;
                }
                return 1;
            }

            if (FD_ISSET(hds[i].chns[j],&hds->w_set))
            {
                FD_CLR(hds[i].chns[j],&hds->w_set);

                if (idx)
                    *idx=i;
                if (chn)
                    *chn=j;
                if (type)
                    *type=VM_SOCKET_WRITE;
                return 1;
            }
        }
    }

    return 0;

} /* int32_t vm_socket_next(vm_socket *hds, int32_t nhd, int32_t *idx, int32_t *chn, int32_t *type) */

int32_t vm_socket_accept(vm_socket *hd)
{
    int32_t psize, chn;

    if (hd->chns[0] == INVALID_SOCKET)
        return -1;
    if (hd->flags & VM_SOCKET_UDP)
        return 0;
    if (!(hd->flags & VM_SOCKET_SERVER))
        return 0;

    for (chn = 1; chn <= VM_SOCKET_QUEUE; chn++)
    {
        if (hd->chns[chn] == INVALID_SOCKET)
            break;
    }
    if (chn > VM_SOCKET_QUEUE)
        return -1;

    psize = sizeof(hd->peers[chn]);
    hd->chns[chn] = accept(hd->chns[0], (struct sockaddr *)&(hd->peers[chn]), &psize);
    if (hd->chns[chn] == INVALID_SOCKET)
    {
        memset(&(hd->peers[chn]), 0, sizeof(hd->peers[chn]));
        return -1;
    }

    return chn;

} /* int32_t vm_socket_accept(vm_socket *hd) */

int32_t vm_socket_read(vm_socket *hd, int32_t chn, void *buffer, int32_t nbytes)
{
    int32_t retr;

    if (chn < 0 || chn > VM_SOCKET_QUEUE)
        return -1;
    if (hd->chns[chn] == INVALID_SOCKET)
        return -1;

    if (hd->flags & VM_SOCKET_UDP)
    {
        int32_t ssize = sizeof(hd->sar);
        retr = recvfrom(hd->chns[chn],
                        buffer,
                        nbytes,
                        0,
                        (struct sockaddr *)&hd->sar,
                        &ssize);
    }
    else
    {
        int32_t time_out_count = 0;

        retr = recv(hd->chns[chn], buffer, nbytes, 0);

        /* Process 2 seconds time out */
        while (retr < 0 && WSAEWOULDBLOCK == WSAGetLastError() && ++time_out_count < 20000)
        {
            vm_time_sleep(1);
            retr = recv(hd->chns[chn], buffer, nbytes, 0);
        }
    }

    if (retr < 1)
        vm_socket_close_chn(hd, chn);

    return retr;

} /* int32_t vm_socket_read(vm_socket *hd, int32_t chn, void *buffer, int32_t nbytes) */

int32_t vm_socket_write(vm_socket *hd, int32_t chn, void *buffer, int32_t nbytes)
{
    int32_t retw;

    if (chn < 0 || chn > VM_SOCKET_QUEUE)
        return -1;
    if (hd->chns[chn] == INVALID_SOCKET)
        return -1;

    if (hd->flags & VM_SOCKET_UDP)
    {
        retw = sendto(hd->chns[chn],
                      buffer,
                      nbytes,
                      0,
                      (struct sockaddr *)&hd->sar,
                      sizeof(hd->sar));
    }
    else
    {
        retw = send(hd->chns[chn], buffer, nbytes, 0);
    }

    if (retw < 1)
        vm_socket_close_chn(hd, chn);

    return retw;

} /* int32_t vm_socket_write(vm_socket *hd, int32_t chn, void *buffer, int32_t nbytes) */

void vm_socket_close_chn(vm_socket *hd, int32_t chn)
{
    if (hd->chns[chn] != INVALID_SOCKET)
        closesocket(hd->chns[chn]);

    hd->chns[chn] = INVALID_SOCKET;

} /* void vm_socket_close_chn(vm_socket *hd, int32_t chn) */

void vm_socket_close(vm_socket *hd)
{
    int32_t i;

    if ((hd->flags & VM_SOCKET_MCAST) && !(hd->flags&VM_SOCKET_SERVER))
    {
        struct ip_mreq imr;

        imr.imr_multiaddr.s_addr = hd->sar.sin_addr.s_addr;
        imr.imr_interface.s_addr = INADDR_ANY;
        setsockopt(hd->chns[0],
                IPPROTO_IP,
                IP_DROP_MEMBERSHIP,
                (int8_t *) &imr,
                sizeof(imr));
    }
    for (i= VM_SOCKET_QUEUE; i >= 0; i--)
        vm_socket_close_chn(hd, i);

    WSACleanup();
} /* void vm_socket_close(vm_socket *hd) */

#endif /* defined(_WIN32) || defined(_WIN64) */

/* Old part */
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

int32_t vm_sock_startup(vm_char a, vm_char b)
{
    uint16_t wVersionRequested = MAKEWORD(a, b);
    WSADATA wsaData;
    int32_t nRet;

    /*
    // Initialize WinSock and check version
    */
    nRet = WSAStartup(wVersionRequested, &wsaData);
    if ((0 != nRet) ||
        (wsaData.wVersion != wVersionRequested))
    {
        return 1;
    }

    return 0;

} /* int32_t vm_sock_startup(vm_char a, vm_char b) */

int32_t vm_sock_cleanup(void)
{
    return WSACleanup();

} /* int32_t vm_sock_cleanup(void) */

static
uint32_t vm_inet_addr(const vm_char *szName)
{
    uint32_t ulRes;

#if defined(UNICODE)||defined(_UNICODE)

    int32_t iStrSize = (int32_t) vm_string_strlen(szName);
    int8_t *szAnciStr = malloc(iStrSize + 2);

    if (NULL == szAnciStr)
        return INADDR_NONE;
    if (0 == WideCharToMultiByte(CP_ACP, 0, szName, iStrSize, szAnciStr,
                                 iStrSize + 2, 0, NULL))
    {
        free(szAnciStr);
        return INADDR_NONE;
    }
    ulRes = inet_addr(szAnciStr);
    free(szAnciStr);

#else /* defined(UNICODE)||defined(_UNICODE) */

    ulRes = inet_addr(szName);

#endif /* defined(UNICODE)||defined(_UNICODE) */

    return ulRes;

} /* uint32_t vm_inet_addr(const vm_char *szName) */

static
struct hostent *vm_gethostbyname(const vm_char *szName)
{
    struct hostent *pRes = NULL;

#if defined(UNICODE)||defined(_UNICODE)

    int32_t iStrSize = (int32_t) vm_string_strlen(szName);
    int8_t *szAnciStr = malloc(iStrSize + 2);

    if (NULL == szAnciStr)
        return NULL;
    if (0 == WideCharToMultiByte(CP_ACP, 0, szName, iStrSize, szAnciStr,
                                 iStrSize + 2, 0, NULL))
    {
        free(szAnciStr);
        return NULL;
    }

    pRes = gethostbyname(szAnciStr);
    free(szAnciStr);

#else /* defined(UNICODE)||defined(_UNICODE) */

    pRes = gethostbyname(szName);

#endif /* defined(UNICODE)||defined(_UNICODE) */

    return pRes;

} /* struct hostent *vm_gethostbyname(const vm_char *szName) */

int32_t vm_sock_host_by_name(vm_char * name, struct in_addr * paddr)
{
    LPHOSTENT lpHostEntry;

    paddr->S_un.S_addr = vm_inet_addr(name);

    if (paddr->S_un.S_addr == INADDR_NONE)
    {
        lpHostEntry = vm_gethostbyname(name);
        if (NULL == lpHostEntry)
            return 1;
        else
            *paddr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
    }

    return 0;

} /* int32_t vm_sock_host_by_name(vm_char * name,struct in_addr * paddr) */

int32_t vm_socket_get_client_ip(vm_socket *hd, uint8_t* buffer, int32_t len)
{
    int32_t iplen;
    if (hd == NULL)
        return -1;
    if (hd->chns[0] == INVALID_SOCKET)
        return -1;

    iplen = (int32_t) strlen(inet_ntoa(hd->sal.sin_addr));

    if (iplen >= len || buffer == NULL)
        return -1;

    MFX_INTERNAL_CPY(buffer,inet_ntoa(hd->sal.sin_addr),iplen);
    buffer[iplen]='\0';

    return 0;
}
#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */

#else /* defined(_SOCKET_SUPPORT) */
#if defined(_WIN32) || defined(_WIN64)
/* nonstandard extension used : translation unit is empty */
#pragma warning( disable: 4206 )
#endif
#endif /* defined(_SOCKET_SUPPORT) */
