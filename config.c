#include "config.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *ltrim(char *s) {
    while (isspace(*s)) s++;
    return s;
}

char *rtrim(char *s) {
    char* back = s + strlen(s);
    while (isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

char *trim(char *s) {
    return rtrim(ltrim(s)); 
}

int parse_proto(char *s_proto, config_proto_t *proto, config_addr_t *addr_info) {
    if (strcmp(s_proto, "tcp") == 0) {
        *proto = TCP;
        addr_info->af = AF_INET;
        return 0;
    } else if (strcmp(s_proto, "tcp6") == 0) {
        *proto = TCP;
        addr_info->af = AF_INET6;
        return 0;
    } else if (strcmp(s_proto, "udp") == 0) {
        *proto = UDP;
        addr_info->af = AF_INET;
        return 0;
    } else if (strcmp(s_proto, "udp6") == 0) {
        *proto = UDP;
        addr_info->af = AF_INET6;
        return 0;
    } else {
        return -1;
    }
}

int parse_addr(char *s_addr, config_addr_t *addr_info) {
    int ch = ':';
    char *port_ptr = strrchr(s_addr, ch);
    if (port_ptr == NULL || s_addr + strlen(s_addr) <= port_ptr) {
        return -1;
    }

    addr_info->port = atoi(port_ptr + 1);

    if (inet_pton(addr_info->af, s_addr, &addr_info->addr) != 0) {
        return -1;
    }

    return 0;
}

config_item_t *parse_line(char *line) {
    // Make a copy for use with strtok
    char *_line_copy = malloc(strlen(line));
    if (_line_copy == NULL) return NULL;
    strcpy(_line_copy, line);
    char *line_copy = trim(_line_copy);
    
    enum {
        START, SRC_PROTO_READ, SRC_ADDR_READ, DST_PROTO_READ, FINAL
    } line_parse_state = START;
    config_item_t *ret = malloc(sizeof(config_item_t));

    char *token = strtok(line_copy, " ");
    do {
        switch (line_parse_state) {
            case START:
                if (parse_proto(token, &ret->src_proto, &ret->src_addr) != 0) {
                    printf("Invalid src protocol %s\n", token);
                    goto error_out;
                }
                line_parse_state = SRC_PROTO_READ;
                break;
            case SRC_PROTO_READ:
                if (parse_addr(token, &ret->src_addr) != 0) {
                    printf("Invalid src address %s\n", token);
                    goto error_out;
                }
                line_parse_state = SRC_ADDR_READ;
                break;
            case SRC_ADDR_READ:
                if (parse_proto(token, &ret->dst_proto, &ret->dst_addr) != 0) {
                    printf("Invalid dst protocol %s\n", token);
                    goto error_out;
                }
                line_parse_state = DST_PROTO_READ;
                break;
            case DST_PROTO_READ:
                if (parse_addr(token, &ret->dst_addr) != 0) {
                    printf("Invalid dst address %s\n", token);
                    goto error_out;
                }
                line_parse_state = FINAL;
                break;
            case FINAL:
                printf("Unexpected token: %s\n", token);
                goto error_out;
        }
    } while ((token = strtok(NULL, " ")) != NULL);

    if (line_parse_state != FINAL) {
        printf("Invalid config line %s\n", line);
        goto error_out;
    }

    free(line_copy);
    return ret;

error_out:
    free(line_copy);
    return NULL;
}

config_item_t *parse_config(const char *path, size_t *num_items) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return NULL;

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    // Count the number of lines first
    *num_items = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        (*num_items)++;
    }

    // Read config line by line
    fseek(fp, 0, SEEK_SET);
    config_item_t *items = malloc(sizeof(config_item_t) * (*num_items));
    if (items == NULL) return NULL;
    size_t i = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        config_item_t *item = parse_line(line);
        if (item == NULL) {
            printf("Invalid config\n");
            return NULL;
        }
        items[i] = *item;
        i++;
    }
    return items;
}