#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Simplified GVDB implementation for WASM
// This provides a minimal key-value database without GLib dependencies

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} WasmBuffer;

typedef struct {
    char **keys;
    char **values;
    size_t count;
    size_t capacity;
} WasmGvdb;

// Forward declarations
WasmGvdb* gvdb_wasm_create(void);
void gvdb_wasm_free(WasmGvdb* db);
int gvdb_wasm_set(WasmGvdb* db, const char* key, const char* value);
const char* gvdb_wasm_get(WasmGvdb* db, const char* key);
int gvdb_wasm_has_key(WasmGvdb* db, const char* key);
int gvdb_wasm_remove(WasmGvdb* db, const char* key);
char** gvdb_wasm_get_keys(WasmGvdb* db, size_t* count);
size_t gvdb_wasm_size(WasmGvdb* db);
void gvdb_wasm_clear(WasmGvdb* db);
char* gvdb_wasm_serialize(WasmGvdb* db, size_t* output_size);
WasmGvdb* gvdb_wasm_deserialize(const char* data, size_t size);

EMSCRIPTEN_KEEPALIVE
extern "C" {

// Create a new GVDB instance
WasmGvdb* gvdb_wasm_create() {
    WasmGvdb* db = (WasmGvdb*)malloc(sizeof(WasmGvdb));
    if (!db) return NULL;
    
    db->capacity = 16;
    db->count = 0;
    db->keys = (char**)malloc(db->capacity * sizeof(char*));
    db->values = (char**)malloc(db->capacity * sizeof(char*));
    
    if (!db->keys || !db->values) {
        free(db->keys);
        free(db->values);
        free(db);
        return NULL;
    }
    
    return db;
}

// Free GVDB instance and all associated memory
void gvdb_wasm_free(WasmGvdb* db) {
    if (!db) return;
    
    for (size_t i = 0; i < db->count; i++) {
        free(db->keys[i]);
        free(db->values[i]);
    }
    
    free(db->keys);
    free(db->values);
    free(db);
}

// Resize the database arrays if needed
static int gvdb_wasm_resize(WasmGvdb* db) {
    if (db->count < db->capacity) return 1;
    
    size_t new_capacity = db->capacity * 2;
    char** new_keys = (char**)realloc(db->keys, new_capacity * sizeof(char*));
    char** new_values = (char**)realloc(db->values, new_capacity * sizeof(char*));
    
    if (!new_keys || !new_values) return 0;
    
    db->keys = new_keys;
    db->values = new_values;
    db->capacity = new_capacity;
    return 1;
}

// Find index of a key, returns -1 if not found
static int gvdb_wasm_find_index(WasmGvdb* db, const char* key) {
    if (!db || !key) return -1;
    
    for (size_t i = 0; i < db->count; i++) {
        if (strcmp(db->keys[i], key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Set a key-value pair
int gvdb_wasm_set(WasmGvdb* db, const char* key, const char* value) {
    if (!db || !key) return 0;
    
    int index = gvdb_wasm_find_index(db, key);
    
    if (index >= 0) {
        // Update existing key
        free(db->values[index]);
        db->values[index] = strdup(value ? value : "");
        return db->values[index] != NULL;
    }
    
    // Add new key-value pair
    if (!gvdb_wasm_resize(db)) return 0;
    
    db->keys[db->count] = strdup(key);
    db->values[db->count] = strdup(value ? value : "");
    
    if (!db->keys[db->count] || !db->values[db->count]) {
        free(db->keys[db->count]);
        free(db->values[db->count]);
        return 0;
    }
    
    db->count++;
    return 1;
}

// Get a value by key
const char* gvdb_wasm_get(WasmGvdb* db, const char* key) {
    if (!db || !key) return NULL;
    
    int index = gvdb_wasm_find_index(db, key);
    return (index >= 0) ? db->values[index] : NULL;
}

// Check if a key exists
int gvdb_wasm_has_key(WasmGvdb* db, const char* key) {
    return gvdb_wasm_find_index(db, key) >= 0;
}

// Remove a key-value pair
int gvdb_wasm_remove(WasmGvdb* db, const char* key) {
    if (!db || !key) return 0;
    
    int index = gvdb_wasm_find_index(db, key);
    if (index < 0) return 0;
    
    free(db->keys[index]);
    free(db->values[index]);
    
    // Shift remaining elements
    for (size_t i = index; i < db->count - 1; i++) {
        db->keys[i] = db->keys[i + 1];
        db->values[i] = db->values[i + 1];
    }
    
    db->count--;
    return 1;
}

// Get all keys (caller must free the returned array but not individual strings)
char** gvdb_wasm_get_keys(WasmGvdb* db, size_t* count) {
    if (!db || !count) return NULL;
    
    *count = db->count;
    if (db->count == 0) return NULL;
    
    char** keys = (char**)malloc(db->count * sizeof(char*));
    if (!keys) return NULL;
    
    for (size_t i = 0; i < db->count; i++) {
        keys[i] = db->keys[i]; // Return pointers to original strings
    }
    
    return keys;
}

// Get the number of key-value pairs
size_t gvdb_wasm_size(WasmGvdb* db) {
    return db ? db->count : 0;
}

// Clear all key-value pairs
void gvdb_wasm_clear(WasmGvdb* db) {
    if (!db) return;
    
    for (size_t i = 0; i < db->count; i++) {
        free(db->keys[i]);
        free(db->values[i]);
    }
    
    db->count = 0;
}

// Simple serialization (key1\0value1\0key2\0value2\0...)
char* gvdb_wasm_serialize(WasmGvdb* db, size_t* output_size) {
    if (!db || !output_size) return NULL;
    
    // Calculate required size
    size_t size = sizeof(size_t); // For count
    for (size_t i = 0; i < db->count; i++) {
        size += strlen(db->keys[i]) + 1;
        size += strlen(db->values[i]) + 1;
    }
    
    char* buffer = (char*)malloc(size);
    if (!buffer) return NULL;
    
    // Write count
    memcpy(buffer, &db->count, sizeof(size_t));
    size_t offset = sizeof(size_t);
    
    // Write key-value pairs
    for (size_t i = 0; i < db->count; i++) {
        size_t key_len = strlen(db->keys[i]) + 1;
        size_t value_len = strlen(db->values[i]) + 1;
        
        memcpy(buffer + offset, db->keys[i], key_len);
        offset += key_len;
        
        memcpy(buffer + offset, db->values[i], value_len);
        offset += value_len;
    }
    
    *output_size = size;
    return buffer;
}

// Deserialize data into a new GVDB instance
WasmGvdb* gvdb_wasm_deserialize(const char* data, size_t size) {
    if (!data || size < sizeof(size_t)) return NULL;
    
    WasmGvdb* db = gvdb_wasm_create();
    if (!db) return NULL;
    
    // Read count
    size_t count;
    memcpy(&count, data, sizeof(size_t));
    size_t offset = sizeof(size_t);
    
    // Read key-value pairs
    for (size_t i = 0; i < count && offset < size; i++) {
        const char* key = data + offset;
        size_t key_len = strlen(key) + 1;
        
        if (offset + key_len >= size) break;
        offset += key_len;
        
        const char* value = data + offset;
        size_t value_len = strlen(value) + 1;
        
        if (offset + value_len > size) break;
        offset += value_len;
        
        if (!gvdb_wasm_set(db, key, value)) {
            gvdb_wasm_free(db);
            return NULL;
        }
    }
    
    return db;
}

// Utility functions for memory management from JavaScript
void gvdb_wasm_free_keys_array(char** keys) {
    if (keys) free(keys);
}

void gvdb_wasm_free_buffer(char* buffer) {
    if (buffer) free(buffer);
}

// Performance test function
int gvdb_wasm_performance_test(int iterations) {
    WasmGvdb* db = gvdb_wasm_create();
    if (!db) return 0;
    
    char key[64], value[128];
    
    // Test insertions
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_for_key_%d_with_some_longer_content", i);
        if (!gvdb_wasm_set(db, key, value)) {
            gvdb_wasm_free(db);
            return 0;
        }
    }
    
    // Test lookups
    int found = 0;
    for (int i = 0; i < iterations; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        if (gvdb_wasm_get(db, key)) found++;
    }
    
    gvdb_wasm_free(db);
    return found == iterations;
}

} // extern "C"