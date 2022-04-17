#ifndef MAP_H
#define MAP_H

#include "common.h"

typedef struct Bucket {
	char *key;
	void *data;
} Bucket;

typedef struct Map {
	u32 size;
	u32 capacity;
	Bucket *m;
} Map;

Map *map_init() {
	Map *map = (Map *)malloc(sizeof(Map));
	map->size = 0;
	map->capacity = 20;
	map->m = (Bucket *)calloc(map->capacity, sizeof(Bucket));

	return map;
}

u32 map_hash(const char *key) {
	u32 hash = 0;
	for (u32 i = 0; i < strlen(key); i++) {
		hash = hash ^ key[i];
	}

	return hash;
}

void map_print(Map *map) {
	printf("map->m: %p, map->size: %d, map->capacity: %d\n", map->m, map->size, map->capacity);
	for (u32 i = 0; i < map->capacity; i++) {
		Bucket b = map->m[i];
		if (b.key != NULL) {
			printf("[%d] key: %s\n", i, b.key);
		}
	}
}

void map_insert(Map *map, char *key, void *data);

void map_grow(Map *map) {
	Bucket *new_buckets = calloc(map->capacity * 2, sizeof(Bucket));

	Map tmp_map;
	tmp_map.m = new_buckets;
	tmp_map.capacity = map->capacity * 2;
	tmp_map.size = 0;

	for (u32 i = 0; i < map->capacity; i++) {
		Bucket b = map->m[i];
		if (b.key != NULL) {
			map_insert(&tmp_map, b.key, b.data);
		}
	}

	free(map->m);

	map->m = new_buckets;
	map->capacity = tmp_map.capacity;
	map->size = tmp_map.size;
}

void map_insert(Map *map, char *key, void *data) {
	if (map->size >= (map->capacity / 2) + (map->capacity / 4)) {
		map_grow(map);
	}

	u32 hash = map_hash(key) % map->capacity;

	for (u32 i = 0; i < map->capacity; i++) {
		Bucket b = map->m[hash];
		if (b.key == NULL) {
			map->m[hash].key = key;
			map->m[hash].data = data;
			map->size += 1;
			return;
		} else {
			hash = (hash + 1) % map->capacity;
		}
	}

	printf("Failed to insert into map!\n");
}

Bucket map_get(Map *map, char *key) {
	u32 hash = map_hash(key) % map->capacity;

	Bucket b;
	for (u32 i = 0; i < map->capacity; i++) {
		b = map->m[hash];
		if (b.key != NULL) {
			if (strcmp(b.key, key) == 0) {
				return map->m[hash];
			}
		}
		hash = (hash + 1) % map->capacity;
	}

	// debug("Failed to get %s\n", key);
	b.key = NULL;
	return b;
}

#endif
