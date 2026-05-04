#ifndef PACKET_PARSE_H
#define PACKET_PARSE_H

#include <pcap.h>
#include <stdint.h>

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint8_t  flags;
    const uint8_t *payload;
    uint32_t payload_len;
} packet_info_t;

int packet_parse(const struct pcap_pkthdr *hdr,
                 const uint8_t            *pkt,
                 int                       datalink,
                 packet_info_t            *info);

#endif /* PACKET_PARSE_H */