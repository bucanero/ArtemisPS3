#pragma once

#if !defined(PATCH_H)
#define PATCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../shared/GamePatchInfo.h"

#if defined(__PRX__)
#include "lib/file.h"
#endif

#define MIN_PATCH_PARAMS 3
#define MAX_PATCH_PARAMS 8

typedef struct __attribute__((packed))
{
    uint32_t hash;
    uint8_t enabled : 1;
} PatchState;

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t version;
    uint32_t count;
} PatchStateFileHeader;

typedef struct
{
    uint32_t hash;
    size_t patch_number;
    char* title;
    char* name;
    char* author;
    char* version;
    char* app_bin;
    char* app_ver;
    bool matches_game : 1;
    bool enabled : 1;
    bool is_prx : 1;
} PatchMetadata;

typedef struct
{
    char* params[MAX_PATCH_PARAMS];
    size_t param_count;
} PatchEntry;

typedef struct
{
    PatchMetadata metadata;
    PatchEntry* entries;
    size_t entry_count;
} PatchData;

typedef enum
{
    PARSE_MODE_ALL,      // Return all patches
    PARSE_MODE_LOW_MEM,  // Process per line
    PARSE_MODE_METADATA  // Only extract metadata and hashes
} ParseMode;

typedef void (*PatchMetadataCallback)(const PatchMetadata* meta, void* user_data);
typedef void (*PatchEntryCallback)(const PatchMetadata* meta, const PatchEntry* entry, void* user_data);

typedef struct
{
    char* title;
    char* name;
    char* notes;
    char* author;
    char* version;
    char* app_bin;
    char** app_ver;
    size_t app_ver_count;
    bool is_app_ver_list : 1;
} Patch;

typedef struct ParseContext
{
    ParseMode mode;
    GamePatchInfo game_info;
    char** global_titleids;
    size_t global_titleid_count;
    size_t current_patch_number;
    Patch current_patch;
    bool in_patches_section;

    // For PARSE_MODE_ALL
    char* title;
    PatchData* all_patches;
    size_t all_patches_capacity;
    size_t all_patches_count;
    PatchEntry* current_entries;
    size_t current_entries_capacity;
    size_t current_entries_count;

    // For PARSE_MODE_METADATA
    PatchMetadata* metadata_array;
    size_t metadata_capacity;
    size_t metadata_count;

    // For PARSE_MODE_LOW_MEM
    const char* filename;
    PatchMetadataCallback meta_callback;
    PatchEntryCallback entry_callback;
    void* user_data;
    PatchMetadata current_meta;
    bool processing_enabled_patch;
} ParseContext;

void create_parse_context(ParseContext* ctx, const GamePatchInfo* game_info, ParseMode mode);
void free_parse_context_data(ParseContext* ctx);

int parse_patch_file_low_mem(ParseContext* ctx, const ParseContext* input);

#if defined(__PRX__)
FileStatus
#else
int
#endif
parse_patch_file(ParseContext* ctx, const char* filename);

PatchData* get_all_patches(ParseContext* ctx, size_t* count);
PatchMetadata* get_metadata(ParseContext* ctx, size_t* count);

int read_patch_state(const char* filename, uint32_t hash);
int toggle_patch_state(const char* filename, uint32_t hash);

void free_patch_data(PatchData* patches, size_t count);
void free_patch_metadata(PatchMetadata* meta);
void free_patch_entry(PatchEntry* entry);

#endif
