#ifndef LIB_H
#define LIB_H

#include <stddef.h>

#define MAX_TOPICS 64

#define MAX_TABLE_SIZE 997

typedef struct Subscriber {
    int fd;
    struct Subscriber *next;
} Subscriber;

typedef struct TopicNode {
    char topic[MAX_TOPICS];
    Subscriber *subs;

    struct TopicNode *next;
} TopicNode;

typedef struct {
    TopicNode *table[MAX_TABLE_SIZE];
} HashMapString;

static size_t hash_string(const char *str, size_t len);

void init_hashmap(HashMapString *map);

TopicNode *find_topic(HashMapString *map, const char *topic, size_t len);

void remove_subscriber(HashMapString *map, int fd, const char *topic, size_t len);

void remove_topic(HashMapString *map, const char *topic, size_t len);

void remove_client_from_all_topics(HashMapString *map, int fd);

void add_subscriber(HashMapString *map, int fd, const char *topic, size_t len);

void free_hashmap(HashMapString *map);

#endif // LIB_H
