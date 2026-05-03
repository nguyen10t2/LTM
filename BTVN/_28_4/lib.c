#include "lib.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

static size_t hash_string(const char *str, size_t len) {
    size_t hash = 5381;
    for (size_t i = 0; i < len; ++i) {
        hash = (hash << 5) + hash + (unsigned char)str[i];
    }
    return hash % MAX_TABLE_SIZE;
}

void init_hashmap(HashMapString *map) {
    for (size_t i = 0; i < MAX_TABLE_SIZE; ++i) {
        map->table[i] = NULL;
    }
}

TopicNode *find_topic(HashMapString *map, const char *topic, size_t len) {
    size_t index = hash_string(topic, len);
    for (TopicNode *node = map->table[index]; node != NULL; node = node->next) {
        if (strcmp(node->topic, topic) == 0) {
            return node;
        }
    }
    return NULL;
}

void remove_subscriber(HashMapString *map, int fd, const char *topic, size_t len) {
    size_t index = hash_string(topic, len);
    for (TopicNode *node = map->table[index];
         node != NULL;
         node = node->next) {
        if (strcmp(node->topic, topic) == 0) {
            Subscriber *prev = NULL;
            Subscriber *sub = node->subs;
            while (sub != NULL && sub->fd != fd) {
                prev = sub;
                sub = sub->next;
            }
            if (sub != NULL) {
                if (prev != NULL) {
                    prev->next = sub->next;
                } else {
                    node->subs = sub->next;
                }
                free(sub);
                sub = NULL;
                if (node->subs == NULL) {
                    remove_topic(map, topic, len);
                }
            }
            return;
        }
    }
}

void remove_topic(HashMapString *map, const char *topic, size_t len) {
    size_t index = hash_string(topic, len);
    TopicNode *prev = NULL;
    TopicNode *node = map->table[index];
    while (node != NULL) {
        if (strcmp(node->topic, topic) == 0) {
            if (prev != NULL) {
                prev->next = node->next;
            } else {
                map->table[index] = node->next;
            }
            Subscriber *sub = node->subs;
            while (sub != NULL) {
                Subscriber *tmp = sub;
                sub = sub->next;
                free(tmp);
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

void remove_client_from_all_topics(HashMapString *map, int fd) {
    for (size_t i = 0; i < MAX_TABLE_SIZE; ++i) {
        TopicNode *prev_node = NULL;
        TopicNode *node = map->table[i];
        while (node != NULL) {
            Subscriber *prev_sub = NULL;
            Subscriber *sub = node->subs;
            while (sub != NULL) {
                if (sub->fd == fd) {
                    Subscriber *next_sub = sub->next;
                    if (prev_sub != NULL) {
                        prev_sub->next = next_sub;
                    } else {
                        node->subs = next_sub;
                    }
                    free(sub);
                    sub = next_sub;
                } else {
                    prev_sub = sub;
                    sub = sub->next;
                }
            }
            if (node->subs == NULL) {
                TopicNode *next_node = node->next;
                if (prev_node != NULL) {
                    prev_node->next = next_node;
                } else {
                    map->table[i] = next_node;
                }
                free(node);
                node = next_node;
            } else {
                prev_node = node;
                node = node->next;
            }
        }
    }
}

void add_subscriber(HashMapString *map, int fd, const char *topic, size_t len) {
    TopicNode *node = find_topic(map, topic, len);
    if (node == NULL) {
        node = malloc(sizeof(TopicNode));
        strncpy(node->topic, topic, len);
        node->topic[len] = '\0';
        node->subs = NULL;
        size_t index = hash_string(topic, len);
        node->next = map->table[index];
        map->table[index] = node;
    }
    Subscriber *sub = node->subs;
    while (sub != NULL) {
        if (sub->fd == fd) {
            return;
        }
        sub = sub->next;
    }
    Subscriber *new_sub = malloc(sizeof(Subscriber));
    new_sub->fd = fd;
    new_sub->next = node->subs;
    node->subs = new_sub;
}

void free_hashmap(HashMapString *map) {
    for (size_t i = 0; i < MAX_TABLE_SIZE; ++i) {
        TopicNode *node = map->table[i];
        while (node != NULL) {
            Subscriber *sub = node->subs;
            while (sub != NULL) {
                Subscriber *tmp = sub;
                sub = sub->next;
                free(tmp);
            }
            TopicNode *tmp = node;
            node = node->next;
            free(tmp);
        }
        map->table[i] = NULL;
    }
}
