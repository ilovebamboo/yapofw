#include "stats.h"
#include "loop.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

yapofw_stats_t *stats;
size_t stats_len = 0;
const char *stats_file_name;
time_t last_save_time = 0;

void stats_serialize() {
    char **buffers = malloc(stats_len * sizeof(char *));
    int *buf_len = malloc(stats_len * sizeof(int));
    if (buffers == NULL || buf_len == NULL) return;
    memset(buffers, 0, stats_len * sizeof(char *));
    memset(buf_len, 0, stats_len * sizeof(int));
    int total_len = 0;
    for (int i = 0; i < stats_len; i++) {
        buffers[i] = malloc(255);
        int len = snprintf(buffers[i], 255, "listen %s proto %s trans %llu recv %llu\n",
            stats[i].listen_addr_key, stats[i].type_key, stats[i].bytes_transmitted, stats[i].bytes_received);
        buf_len[i] = len;
        total_len += len;
    }

    char *out_buffer = malloc(total_len + 1);
    int pos = 0;
    for (int i = 0; i < stats_len; i++) {
        if (out_buffer == NULL) {
            free(buffers[i]);
            continue;
        }

        memcpy(&out_buffer[pos], buffers[i], buf_len[i]);
        pos += buf_len[i];
        free(buffers[i]);
    }

    if (out_buffer == NULL) {
        free(buffers);
        free(buf_len);
        return;
    }

    FILE *fp = fopen(stats_file_name, "w");
    fwrite(out_buffer, 1, total_len, fp);
    fclose(fp);

    free(out_buffer);
    free(buffers);
    free(buf_len);
}

void stats_deserialize() {
    FILE *fp = fopen(stats_file_name, "r");
    if (fp == NULL) return;

    yapofw_stats_t tmp_stat;
    tmp_stat.type_key = malloc(255); // For reading, will be freed in this function
    while (fscanf(fp, "listen %s proto %s trans %llu recv %llu\n",
        tmp_stat.listen_addr_key, tmp_stat.type_key, &tmp_stat.bytes_transmitted,
        &tmp_stat.bytes_received) != EOF) {
        for (int i = 0; i < stats_len; i++) {
            if (strcmp(tmp_stat.listen_addr_key, stats[i].listen_addr_key) != 0) continue;
            if (strcmp(tmp_stat.type_key, stats[i].type_key) != 0) continue;
            stats[i].bytes_transmitted = tmp_stat.bytes_transmitted;
            stats[i].bytes_received = tmp_stat.bytes_received;
        }
    }

    fclose(fp);
    free(tmp_stat.type_key);
}

void stats_after_poll() {
    // We always save after a poll because the stats will only ever change
    // when something happens (i.e. poll() returns)
    time_t now = time(NULL);
    if (now - last_save_time < 10) return; // We only save every 10 seconds
    last_save_time = now;
    stats_serialize();
}

int stats_init_from_config(config_item_t *config, size_t config_len, const char *persist_file) {
    stats_file_name = persist_file;
    // Initialize the stats array
    stats = malloc(config_len * sizeof(yapofw_stats_t));
    if (stats == NULL) {
        return -1;
    }
    memset(stats, 0, config_len * sizeof(yapofw_stats_t));
    stats_len = config_len;

    char ip_str[255];
    for (int i = 0; i < config_len; i++) {
        config_addr_to_str(&config[i].src_addr, ip_str, 255);
        sprintf(stats[i].listen_addr_key, "%s:%d", ip_str, config[i].src_addr.port);

        if (config[i].src_proto == TCP) {
            stats[i].type_key = "TCP";
        } else if (config[i].src_proto == UDP) {
            stats[i].type_key = "UDP";
        }
    }

    stats_deserialize();

    last_save_time = time(NULL);
    
    event_loop_register_hook_after_poll(&stats_after_poll);

    return 0;
}

void stats_add_bytes(size_t cfg_idx, unsigned long bytes, int direction) {
    if (direction == STATS_DIRECTION_SRC_DST) {
        stats[cfg_idx].bytes_transmitted += bytes;
    } else if (direction == STATS_DIRECTION_DST_SRC) {
        stats[cfg_idx].bytes_received += bytes;
    }
}