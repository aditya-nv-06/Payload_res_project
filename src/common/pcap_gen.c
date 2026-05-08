#include "pcap_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* PCAP global header format */
typedef struct {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
} pcap_global_hdr_t;

/* PCAP packet header format */
typedef struct {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
} pcap_packet_hdr_t;

/**
 * Simple PRNG for generating test queries.
 */
static uint32_t simple_rand(uint32_t *seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

/**
 * Generate a clean SQL query.
 */
static const char *get_clean_query(uint32_t *seed) {
    static const char *queries[] = {
        "SELECT id, username, email FROM users WHERE id = 1",
        "INSERT INTO logs (timestamp, message) VALUES (NOW(), 'login')",
        "UPDATE user_profiles SET last_seen = NOW() WHERE user_id = 42",
        "SELECT COUNT(*) FROM transactions WHERE status = 'completed'",
        "DELETE FROM temp_cache WHERE created_at < NOW() - INTERVAL 1 DAY",
        "SELECT * FROM orders WHERE customer_id = 123 LIMIT 10",
        "SELECT password FROM admin WHERE username = 'admin'",
        "SELECT * FROM users WHERE email LIKE '%@example.com'",
        "UPDATE settings SET value = 'enabled' WHERE key = 'feature_flag'",
        "SELECT SUM(amount) FROM transactions WHERE date > NOW() - INTERVAL 30 DAY",
    };
    int idx = simple_rand(seed) % 10;
    return queries[idx];
}

/**
 * Generate a SQL injection payload query.
 */
static const char *get_injection_query(uint32_t *seed) {
    static const char *payloads[] = {
        "SELECT * FROM users WHERE id = ' OR '1'='1",
        "SELECT * FROM users WHERE username = 'admin' --",
        "SELECT * FROM users WHERE id = 1; DROP TABLE users; --",
        "SELECT * FROM users WHERE id = 1' UNION SELECT * FROM passwords --",
        "UPDATE users SET admin = 1 WHERE id = 1 OR 1=1 --",
        "SELECT * FROM users WHERE id = 1' OR 'x'='x",
        "INSERT INTO users (username, password) VALUES ('hacker', 'password'); --",
        "SELECT * FROM users WHERE name LIKE '%' OR '1'='1",
        "DELETE FROM logs WHERE id > 0 OR 1=1 --",
        "SELECT * FROM users WHERE id = (SELECT MAX(id) FROM (SELECT * FROM users) a) --",
    };
    int idx = simple_rand(seed) % 10;
    return payloads[idx];
}

/**
 * Write a PostgreSQL Simple Query message (format: 'Q' + length + query + '\0').
 */
static int write_query_packet(FILE *fp, const char *query, time_t ts_sec, uint32_t ts_usec) {
    pcap_packet_hdr_t pkt_hdr;
    
    /* Build the PostgreSQL message: 'Q' + 4-byte length + query + '\0' */
    size_t query_len = strlen(query);
    size_t msg_len = 1 + 4 + query_len + 1;  /* 'Q' + length + query + null */
    
    unsigned char *msg = malloc(msg_len);
    if (!msg) return -1;
    
    msg[0] = 'Q';  /* Simple Query message type */
    
    /* Encode 4-byte big-endian length (includes the 4 bytes for length itself) */
    uint32_t len_be = query_len + 1 + 4;  /* query + null + 4-byte length field */
    msg[1] = (len_be >> 24) & 0xFF;
    msg[2] = (len_be >> 16) & 0xFF;
    msg[3] = (len_be >> 8) & 0xFF;
    msg[4] = len_be & 0xFF;
    
    /* Copy query and null terminator */
    memcpy(msg + 5, query, query_len);
    msg[msg_len - 1] = '\0';
    
    /* Write packet header */
    pkt_hdr.ts_sec = ts_sec;
    pkt_hdr.ts_usec = ts_usec;
    pkt_hdr.incl_len = msg_len;
    pkt_hdr.orig_len = msg_len;
    
    if (fwrite(&pkt_hdr, sizeof(pkt_hdr), 1, fp) != 1) {
        free(msg);
        return -1;
    }
    
    /* Write packet payload */
    if (fwrite(msg, msg_len, 1, fp) != 1) {
        free(msg);
        return -1;
    }
    
    free(msg);
    return 0;
}

/**
 * Generate a PCAP file with synthetic PostgreSQL traffic.
 */
int pcap_gen_write(const char *filename, const pcap_gen_opts_t *opts) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    /* Write PCAP global header */
    pcap_global_hdr_t global_hdr = {
        .magic_number = 0xa1b2c3d4,  /* Magic number (little-endian PCAP) */
        .version_major = 2,
        .version_minor = 4,
        .thiszone = 0,
        .sigfigs = 0,
        .snaplen = 65535,
        .network = 1,  /* Ethernet */
    };
    
    if (fwrite(&global_hdr, sizeof(global_hdr), 1, fp) != 1) {
        perror("fwrite global header");
        fclose(fp);
        return -1;
    }
    
    /* Initialize PRNG */
    uint32_t seed = opts->seed ? opts->seed : (uint32_t)time(NULL);
    time_t ts_sec = time(NULL);
    uint32_t packet_count = 0;
    
    /* Write clean queries */
    for (int i = 0; i < opts->num_clean; i++) {
        const char *query = get_clean_query(&seed);
        if (write_query_packet(fp, query, ts_sec, i * 100000) < 0) {  /* 100ms intervals */
            perror("write_query_packet (clean)");
            fclose(fp);
            return -1;
        }
        packet_count++;
    }
    
    /* Write injection queries */
    for (int i = 0; i < opts->num_injection; i++) {
        const char *query = get_injection_query(&seed);
        if (write_query_packet(fp, query, ts_sec, (opts->num_clean + i) * 100000) < 0) {
            perror("write_query_packet (injection)");
            fclose(fp);
            return -1;
        }
        packet_count++;
    }
    
    fclose(fp);
    
    fprintf(stdout, "[✓] Generated PCAP: %s\n", filename);
    fprintf(stdout, "    Packets: %u (clean=%d, injection=%d)\n", 
            packet_count, opts->num_clean, opts->num_injection);
    
    return 0;
}
