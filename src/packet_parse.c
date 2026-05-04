#include "packet_parse.h"

#include <arpa/inet.h>
#include <netinet/in.h>

static uint16_t read_u16_be(const uint8_t *ptr)
{
    return (uint16_t)(((uint16_t)ptr[0] << 8) | (uint16_t)ptr[1]);
}

static uint32_t read_u32_be(const uint8_t *ptr)
{
    return ((uint32_t)ptr[0] << 24) |
           ((uint32_t)ptr[1] << 16) |
           ((uint32_t)ptr[2] << 8)  |
           ((uint32_t)ptr[3]);
}

int packet_parse(const struct pcap_pkthdr *hdr,
                 const uint8_t            *pkt,
                 int                       datalink,
                 packet_info_t            *info)
{
    if (!hdr || !pkt || !info) return 0;

    const uint8_t *ip_start = NULL;
    uint32_t caplen = hdr->caplen;

    switch (datalink) {
    case DLT_EN10MB:
        if (caplen < 14) return 0;
        if (pkt[12] != 0x08 || pkt[13] != 0x00) return 0;
        ip_start = pkt + 14;
        caplen -= 14;
        break;
    case DLT_LINUX_SLL:
        if (caplen < 16) return 0;
        if (pkt[14] != 0x08 || pkt[15] != 0x00) return 0;
        ip_start = pkt + 16;
        caplen -= 16;
        break;
    case DLT_RAW:
        ip_start = pkt;
        break;
    default:
        return 0;
    }

    if (!ip_start || caplen < 20) return 0;

    uint8_t ip_v = (uint8_t)(ip_start[0] >> 4);
    uint8_t ip_hl = (uint8_t)(ip_start[0] & 0x0F);
    if (ip_v != 4) return 0;
    if (ip_start[9] != IPPROTO_TCP) return 0;

    uint32_t ip_hlen = (uint32_t)ip_hl * 4;
    if (ip_hlen < 20 || ip_hlen > caplen) return 0;

    const uint8_t *tcp_start = ip_start + ip_hlen;
    uint32_t remaining = caplen - ip_hlen;
    if (remaining < 20) return 0;

    uint8_t tcp_off = (uint8_t)(tcp_start[12] >> 4);
    uint32_t tcp_hlen = (uint32_t)tcp_off * 4;
    if (tcp_hlen < 20 || tcp_hlen > remaining) return 0;

    info->src_ip = read_u32_be(ip_start + 12);
    info->dst_ip = read_u32_be(ip_start + 16);
    info->src_port = read_u16_be(tcp_start + 0);
    info->dst_port = read_u16_be(tcp_start + 2);
    info->seq = read_u32_be(tcp_start + 4);
    info->flags = tcp_start[13];
    info->payload = tcp_start + tcp_hlen;
    info->payload_len = remaining - tcp_hlen;
    return 1;
}