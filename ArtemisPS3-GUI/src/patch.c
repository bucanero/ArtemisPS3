#include "patch.h"
#include "../shared/stringid.h"

#include <stdint.h>
#include <stdbool.h>
#if !defined(__PRX__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include "my_string.h"

#if defined(__PRX__)
#include <sys/process.h>
#include "Memory/Memory.h"
#include "../shared/memory.h"
#include "plugins.h"
#include "lv2_stdio.h"
#include "lib/file.h"
#endif

#include "../shared/macros.h"

#if defined(__PRX__)
extern program_args* g_args;
#endif

#define PATCH_STATE_MAGIC (uint32_t)'ILNY'
#define PATCH_STATE_VERSION 1

static size_t parse_patch_entry(const char* str, PatchEntry* entry)
{
    char buffer[MAX_LINE_LENGTH + 1];
    bzero(buffer, sizeof(buffer));
    strncpy(buffer, str, _countof_1(buffer));

    char* start = strchr(buffer, '[');
    if (!start)
    {
        return 0;
    }
    start++;

    char* end = start;
    bool in_quote = false;
    while (*end)
    {
        if (*end == '\\' && *(end + 1))
        {
            end += 2;
            continue;
        }
        if (*end == '"')
        {
            in_quote = !in_quote;
        }
        if (!in_quote && *end == ']')
        {
            break;
        }
        end++;
    }

    if (*end != ']')
    {
        return 0;
    }
    *end = '\0';

    entry->param_count = 0;
    char* pos = start;

    while (*pos && entry->param_count < MAX_PATCH_PARAMS)
    {
        while (*pos && isspace(*pos))
        {
            pos++;
        }
        if (!*pos)
        {
            break;
        }

        if (*pos == '"')
        {
            char* quote_start = pos;
            pos++;

            while (*pos && !(*pos == '"' && *(pos - 1) != '\\'))
            {
                if (*pos == '\\' && *(pos + 1))
                {
                    pos += 2;
                }
                else
                {
                    pos++;
                }
            }

            if (*pos == '"')
            {
                pos++;
                size_t quote_len = pos - quote_start;
                char temp[MAX_LINE_LENGTH + 1] = {0};
                strncpy(temp, quote_start, quote_len);

                entry->params[entry->param_count] = parse_quoted_string(temp);
                if (entry->params[entry->param_count])
                {
                    entry->param_count++;
                }
            }
        }

        while (*pos && *pos != ',')
        {
            pos++;
        }
        if (*pos == ',')
        {
            pos++;
        }
    }

    return entry->param_count;
}

static uint32_t calculate_patch_hash(size_t patch_number,
                                     const char* titleid_cat,
                                     const Patch* patch,
                                     const char* app_ver)
{
    char patch_numbuf[64 + 1] = {0};
    snprintf(patch_numbuf, _countof_1(patch_numbuf), "%ld", patch_number);

    uint32_t hash = stringid(patch_numbuf, 0x811c9dc5);
    const char* hash_list[] = {
        titleid_cat,
        patch->title ? patch->title : "",
        patch->name ? patch->name : "",
        patch->author ? patch->author : "",
        patch->version ? patch->version : "",
        patch->app_bin ? patch->app_bin : "",
        app_ver ? app_ver : "",
    };
    for (size_t i = 0; i < _countof(hash_list); i++)
    {
        hash = stringid(hash_list[i], hash);
    }

    return hash;
}

static bool patch_matches_game(const GamePatchInfo* game, const char* app_ver)
{
    if (app_ver && game->app_ver[0] != '\0')
    {
        return strcmp(game->app_ver, app_ver) == 0;
    }
    return false;
}

static void process_patch_metadata_for_app_ver(PatchMetadata* meta,
                                               ParseContext* ctx,
                                               const char* app_ver,
                                               size_t app_ver_index)
{
    memset(meta, 0, sizeof(*meta));

    const size_t unique_patch_num = ctx->current_patch_number;
    meta->patch_number = (unique_patch_num * 100) + app_ver_index;

    meta->hash = calculate_patch_hash(meta->patch_number,
                                      ctx->game_info.titleid,
                                      &ctx->current_patch,
                                      app_ver);

    meta->title = str_dup(ctx->current_patch.title);
    meta->name = str_dup(ctx->current_patch.name);
    meta->author = str_dup(ctx->current_patch.author);
    meta->version = str_dup(ctx->current_patch.version);
    meta->app_bin = str_dup(ctx->current_patch.app_bin);
    meta->app_ver = str_dup(app_ver);

    meta->matches_game = patch_matches_game(&ctx->game_info, app_ver);

    char settings_buf[MAX_PATH + 1] = {0};
    snprintf(settings_buf, _countof_1(settings_buf), GAME_PATCH_SETTINGS "/%s.bin", ctx->game_info.titleid);
#if defined(__PRX__)
    const bool isExeMatched = (g_args && (strstr(g_args->argv[0].c.lo, meta->app_bin) != 0));
    const bool readEnabled = read_patch_state(settings_buf, meta->hash) == 1;
    print_bool(isExeMatched);
    print_bool(readEnabled);
    meta->enabled = readEnabled && isExeMatched;
#else
    const bool readEnabled = read_patch_state(settings_buf, meta->hash) == 1;
    meta->enabled = readEnabled;
#endif
    static const char* prx_list[] = {".prx", ".PRX", ".sprx", ".SPRX"};
    for (size_t i = 0; i < _countof(prx_list); i++)
    {
        meta->is_prx = strstr(meta->app_bin, prx_list[i]) != 0;
        if (meta->is_prx)
        {
            break;
        }
    }
}

static void reset_current_patch(ParseContext* ctx)
{
    free(ctx->current_patch.title);
    free(ctx->current_patch.name);
    free(ctx->current_patch.notes);
    free(ctx->current_patch.author);
    free(ctx->current_patch.version);
    free(ctx->current_patch.app_bin);

    for (size_t i = 0; i < ctx->current_patch.app_ver_count; i++)
    {
        free(ctx->current_patch.app_ver[i]);
    }

    memset(&ctx->current_patch, 0, sizeof(ctx->current_patch));
    ctx->current_patch.app_ver = malloc(sizeof(char*) * MAX_APP_VERS);
    ctx->in_patches_section = false;
}

static void handle_patch_complete(ParseContext* ctx)
{
    if (!ctx->current_patch.title)
    {
        return;
    }

    size_t app_ver_count = ctx->current_patch.app_ver_count;
    if (app_ver_count == 0)
    {
        app_ver_count = 1;
    }

    for (size_t av_idx = 0; av_idx < app_ver_count; av_idx++)
    {
#if !defined(__PRX__)
        const char* app_ver = (av_idx < ctx->current_patch.app_ver_count) ? ctx->current_patch.app_ver[av_idx] : NULL;
        if (ctx->mode == PARSE_MODE_ALL && strcmp(app_ver, ctx->game_info.app_ver) == 0)
        {
            PatchMetadata meta;
            process_patch_metadata_for_app_ver(&meta, ctx, app_ver, av_idx);
            // printf("ctx->title %s\n", ctx->title);
            // printf("meta.title %s\n", meta.title);
            const bool matched = ctx->title ? strcmp(ctx->title, meta.title) == 0 : false;
            if (!matched)
            {
                continue;
            }

            if (ctx->all_patches_count >= ctx->all_patches_capacity)
            {
                ctx->all_patches_capacity *= 2;
                PatchData* new_patches = realloc(ctx->all_patches,
                                                 sizeof(PatchData) * ctx->all_patches_capacity);
                if (!new_patches)
                {
                    free_patch_metadata(&meta);
                    return;
                }
                ctx->all_patches = new_patches;
            }

            ctx->all_patches[ctx->all_patches_count].metadata = meta;

            ctx->all_patches[ctx->all_patches_count].entries =
                malloc(sizeof(PatchEntry) * ctx->current_entries_count);
            ctx->all_patches[ctx->all_patches_count].entry_count = ctx->current_entries_count;

            for (size_t i = 0; i < ctx->current_entries_count; i++)
            {
                PatchEntry* dst = &ctx->all_patches[ctx->all_patches_count].entries[i];
                PatchEntry* src = &ctx->current_entries[i];
                dst->param_count = src->param_count;
                for (size_t j = 0; j < src->param_count; j++)
                {
                    dst->params[j] = str_dup(src->params[j]);
                }
            }

            ctx->all_patches_count++;
        }
        else if (ctx->mode == PARSE_MODE_METADATA)
        {
            if (ctx->metadata_count >= ctx->metadata_capacity)
            {
                ctx->metadata_capacity *= 2;
                PatchMetadata* new_metadata = realloc(ctx->metadata_array,
                                                      sizeof(PatchMetadata) * ctx->metadata_capacity);
                if (!new_metadata)
                {
                    return;
                }
                ctx->metadata_array = new_metadata;
            }

            process_patch_metadata_for_app_ver(&ctx->metadata_array[ctx->metadata_count],
                                               ctx,
                                               app_ver,
                                               av_idx);
            ctx->metadata_count++;
        }
        else
#endif
            if (ctx->mode == PARSE_MODE_LOW_MEM)
        {
            // Nothing to do
        }
    }

    for (size_t i = 0; i < ctx->current_entries_count; i++)
    {
        free_patch_entry(&ctx->current_entries[i]);
    }
    ctx->current_entries_count = 0;

    reset_current_patch(ctx);
    ctx->current_patch_number++;
}

static void process_line(ParseContext* ctx, char* line)
{
    if (!ctx || !line)
    {
        return;
    }
    if (is_comment_or_empty(line))
    {
        return;
    }

    size_t indent = get_indent_level(line);
    char* trimmed = trim(line);

    if (indent == 0 && strstr(trimmed, "titleid:"))
    {
        if (is_list_value(trimmed))
        {
            ctx->global_titleid_count = parse_string_list(trimmed,
                                                          ctx->global_titleids,
                                                          MAX_TITLE_IDS);
        }
        return;
    }

    if (indent == 0 && strstr(trimmed, "patch:"))
    {
        handle_patch_complete(ctx);
        return;
    }

    if (strstr(trimmed, "title:"))
    {
        ctx->current_patch.title = parse_quoted_string(trimmed);
    }
    else if (strstr(trimmed, "name:"))
    {
        ctx->current_patch.name = parse_quoted_string(trimmed);
    }
    else if (strstr(trimmed, "notes:"))
    {
        ctx->current_patch.notes = parse_quoted_string(trimmed);
    }
    else if (strstr(trimmed, "author:"))
    {
        ctx->current_patch.author = parse_quoted_string(trimmed);
    }
    else if (strstr(trimmed, "version:"))
    {
        ctx->current_patch.version = parse_quoted_string(trimmed);
    }
    else if (strstr(trimmed, "app_bin:"))
    {
        ctx->current_patch.app_bin = parse_quoted_string(trimmed);
    }
    else if (strstr(trimmed, "app_ver:"))
    {
        if (is_list_value(trimmed))
        {
            ctx->current_patch.is_app_ver_list = true;

            if (!ctx->current_patch.app_ver)
            {
                return;
            }

            ctx->current_patch.app_ver_count = parse_string_list(trimmed,
                                                                 ctx->current_patch.app_ver,
                                                                 MAX_APP_VERS);
        }
        else
        {
            ctx->current_patch.is_app_ver_list = false;

            if (!ctx->current_patch.app_ver)
            {
                return;
            }

            ctx->current_patch.app_ver[0] = parse_quoted_string(trimmed);
            ctx->current_patch.app_ver_count = ctx->current_patch.app_ver[0] ? 1 : 0;
        }
    }
    else if (strstr(trimmed, "patches:"))
    {
        ctx->in_patches_section = true;
        if (ctx->mode == PARSE_MODE_LOW_MEM)
        {
            size_t app_ver_count = ctx->current_patch.app_ver_count;
            if (app_ver_count == 0)
            {
                app_ver_count = 1;
            }

            for (size_t av_idx = 0; av_idx < app_ver_count; av_idx++)
            {
                const char* app_ver = (av_idx < ctx->current_patch.app_ver_count) ? ctx->current_patch.app_ver[av_idx] : NULL;

                PatchMetadata meta;
                process_patch_metadata_for_app_ver(&meta, ctx, app_ver, av_idx);

                if (meta.matches_game && ctx->meta_callback)
                {
                    ctx->meta_callback(&meta, ctx->user_data);
                    ctx->processing_enabled_patch = meta.matches_game && meta.enabled;
                }

                if (av_idx == 0)
                {
                    free_patch_metadata(&ctx->current_meta);
                    ctx->current_meta = meta;
                }
                else
                {
                    free_patch_metadata(&meta);
                }
            }
        }
    }
    else if (ctx->in_patches_section && trimmed[0] == '-' && trimmed[1] == ' ' && trimmed[2] == '[')
    {
        PatchEntry entry = {0};
        if (parse_patch_entry(trimmed + 2, &entry) >= MIN_PATCH_PARAMS)
        {
            if (ctx->mode == PARSE_MODE_ALL)
            {
#if !defined(__PRX__)
                if (ctx->current_entries_count >= ctx->current_entries_capacity)
                {
                    ctx->current_entries_capacity *= 2;
                    PatchEntry* new_entries = realloc(ctx->current_entries,
                                                      sizeof(PatchEntry) * ctx->current_entries_capacity);
                    if (!new_entries)
                    {
                        free_patch_entry(&entry);
                        return;
                    }
                    ctx->current_entries = new_entries;
                }
                ctx->current_entries[ctx->current_entries_count] = entry;
                ctx->current_entries_count++;
#endif
            }
            else if (ctx->mode == PARSE_MODE_LOW_MEM)
            {
                if (ctx->processing_enabled_patch && ctx->entry_callback)
                {
                    ctx->entry_callback(&ctx->current_meta, &entry, ctx->user_data);
                }
                free_patch_entry(&entry);
            }
            else
            {
                free_patch_entry(&entry);
            }
        }
    }
}

void create_parse_context(ParseContext* ctx, const GamePatchInfo* game_info, ParseMode mode)
{
    if (!ctx)
    {
        return;
    }

    ctx->mode = mode;

    if (game_info)
    {
        ctx->game_info = *game_info;
    }

    ctx->global_titleids = malloc(sizeof(char*) * MAX_TITLE_IDS);
    if (!ctx->global_titleids)
    {
        return;
    }

    ctx->current_patch.app_ver = malloc(sizeof(char*) * MAX_APP_VERS);
    if (!ctx->current_patch.app_ver)
    {
        free(ctx->global_titleids);
        ctx->global_titleids = NULL;
        return;
    }

    if (mode == PARSE_MODE_ALL)
    {
        ctx->all_patches_capacity = 8;
        ctx->all_patches = malloc(sizeof(PatchData) * ctx->all_patches_capacity);
        if (!ctx->all_patches)
        {
            return;
        }

        ctx->current_entries_capacity = 16;
        ctx->current_entries = malloc(sizeof(PatchEntry) * ctx->current_entries_capacity);
        if (!ctx->current_entries)
        {
            return;
        }
    }
    else if (mode == PARSE_MODE_METADATA)
    {
        ctx->metadata_capacity = 8;
        ctx->metadata_array = malloc(sizeof(PatchMetadata) * ctx->metadata_capacity);
        if (!ctx->metadata_array)
        {
            return;
        }
    }
}

void free_parse_context_data(ParseContext* ctx)
{
    if (!ctx)
    {
        return;
    }

    if (ctx->global_titleids)
    {
        for (size_t i = 0; i < ctx->global_titleid_count; i++)
        {
            free(ctx->global_titleids[i]);
        }
        free(ctx->global_titleids);
    }

    free(ctx->current_patch.title);
    free(ctx->current_patch.name);
    free(ctx->current_patch.notes);
    free(ctx->current_patch.author);
    free(ctx->current_patch.version);
    free(ctx->current_patch.app_bin);

    if (ctx->current_patch.app_ver)
    {
        for (size_t i = 0; i < ctx->current_patch.app_ver_count; i++)
        {
            free(ctx->current_patch.app_ver[i]);
        }
        free(ctx->current_patch.app_ver);
    }

    if (ctx->all_patches)
    {
        for (size_t i = 0; i < ctx->all_patches_count; i++)
        {
            free_patch_metadata(&ctx->all_patches[i].metadata);
            if (ctx->all_patches[i].entries)
            {
                for (size_t j = 0; j < ctx->all_patches[i].entry_count; j++)
                {
                    free_patch_entry(&ctx->all_patches[i].entries[j]);
                }
                free(ctx->all_patches[i].entries);
            }
        }
        free(ctx->all_patches);
    }

    if (ctx->current_entries)
    {
        for (size_t i = 0; i < ctx->current_entries_count; i++)
        {
            free_patch_entry(&ctx->current_entries[i]);
        }
        free(ctx->current_entries);
    }

    if (ctx->metadata_array)
    {
        for (size_t i = 0; i < ctx->metadata_count; i++)
        {
            free_patch_metadata(&ctx->metadata_array[i]);
        }
        free(ctx->metadata_array);
    }

    free_patch_metadata(&ctx->current_meta);
}

#if defined(__PRX__)
FileStatus
#else
int
#endif
parse_patch_file(ParseContext* ctx, const char* filename)
{
    if (!ctx || !filename)
    {
#if defined(__PRX__)
        return FILE_STATUS_OPEN_FAILED;
#else
        return -1;
#endif
    }

    if (ctx->mode == PARSE_MODE_LOW_MEM)
    {
#if defined(__PRX__)
        return FILE_STATUS_OPEN_FAILED;
#else
        return -1;
#endif
    }

#if !defined(__PRX__)
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        perror("Failed to open file");
        return -1;
    }
#else
    FileHandle h = 0;
    FileStatus ret = fileOpen(&h, filename, FILE_MODE_READ);
    if (ret != FILE_STATUS_OK)
    {
        printf("Failed to open file: %s\n", filename);
        return ret;
    }

    uint64_t fsz = 0;
    if (fileSize(h, &fsz) != FILE_STATUS_OK)
    {
        fileClose(h);
        return ret;
    }
#endif

#if !defined(__PRX__)
    char line[MAX_LINE_LENGTH + 1];
    size_t line_num = 0;
    while (fgets(line, _countof_1(line), file))
    {
        line_num++;
        process_line(ctx, line);
    }
#else
    uint64_t bufread_add = 0;
    uint64_t bufread = 0, bufread2 = 0;

    while (bufread_add < fsz)
    {
        char line[MAX_LINE_LENGTH + 1];
        bzero(line, sizeof(line));

        FileStatus readret = fileReadLine(h, line, _countof_1(line), &bufread, &bufread2);
        if (readret != FILE_STATUS_OK || bufread == 0)
        {
            break;
        }

        bufread_add += bufread;
        process_line(ctx, line);
    }
#endif

    handle_patch_complete(ctx);

#if !defined(__PRX__)
    fclose(file);
#else
    fileClose(h);
#endif

#if defined(__PRX__)
    return FILE_STATUS_OK;
#else
    return 0;
#endif
}

int parse_patch_file_low_mem(ParseContext* ctx, const ParseContext* input)
{
    if (!ctx || !input || !input->filename)
    {
        return -1;
    }

    if (ctx->mode != PARSE_MODE_LOW_MEM)
    {
        return -1;
    }

    ctx->meta_callback = input->meta_callback;
    ctx->entry_callback = input->entry_callback;
    ctx->user_data = input->user_data;

#if !defined(__PRX__)
    FILE* file = fopen(input->filename, "r");
    if (!file)
    {
        perror("Failed to open file");
        return -1;
    }
#else
    FileHandle h = 0;
    FileStatus ret = fileOpen(&h, input->filename, FILE_MODE_READ);
    if (ret != FILE_STATUS_OK)
    {
        printf("Failed to open file: %s\n", input->filename);
        return -1;
    }

    uint64_t fsz = 0;
    if (fileSize(h, &fsz) != FILE_STATUS_OK)
    {
        fileClose(h);
        return -1;
    }
#endif

#if !defined(__PRX__)
    char line[MAX_LINE_LENGTH + 1];
    while (fgets(line, _countof_1(line), file))
    {
        process_line(ctx, line);
    }
#else
    uint64_t bufread_add = 0;
    uint64_t bufread = 0, bufread2 = 0;

    while (bufread_add < fsz)
    {
        char line[MAX_LINE_LENGTH + 1] = {0};
        FileStatus readret = fileReadLine(h, line, _countof_1(line), &bufread, &bufread2);
        if (readret != FILE_STATUS_OK || bufread == 0)
        {
            break;
        }
        bufread_add += bufread;
        process_line(ctx, line);
    }
#endif

    handle_patch_complete(ctx);

#if !defined(__PRX__)
    fclose(file);
#else
    fileClose(h);
#endif

    return 0;
}

PatchData* get_all_patches(ParseContext* ctx, size_t* count)
{
    if (!ctx || !count)
    {
        return NULL;
    }

    *count = ctx->all_patches_count;
    return ctx->all_patches;
}

PatchMetadata* get_metadata(ParseContext* ctx, size_t* count)
{
    if (!ctx || !count)
    {
        return NULL;
    }

    *count = ctx->metadata_count;
    return ctx->metadata_array;
}

void free_patch_metadata(PatchMetadata* meta)
{
    if (!meta)
    {
        return;
    }

    free(meta->title);
    free(meta->name);
    free(meta->author);
    free(meta->version);
    free(meta->app_bin);
    free(meta->app_ver);
}

void free_patch_entry(PatchEntry* entry)
{
    if (!entry)
    {
        return;
    }

    for (size_t i = 0; i < entry->param_count; i++)
    {
        free(entry->params[i]);
    }
}

void free_patch_data(PatchData* patches, size_t count)
{
    if (!patches)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        free_patch_metadata(&patches[i].metadata);
        for (size_t j = 0; j < patches[i].entry_count; j++)
        {
            free_patch_entry(&patches[i].entries[j]);
        }
        free(patches[i].entries);
    }
}

#if !defined(__PRX__)
static int write_patch_states_internal(const char* filename, const PatchState* states, size_t count)
{
    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        perror("Failed to open file for writing");
        return -1;
    }

    PatchStateFileHeader header = {
        .magic = PATCH_STATE_MAGIC,
        .version = PATCH_STATE_VERSION,
        .count = (uint32_t)count};

    if (fwrite(&header, sizeof(header), 1, f) != 1)
    {
        fclose(f);
        return -1;
    }

    if (count > 0 && fwrite(states, sizeof(PatchState), count, f) != count)
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}
#endif

static PatchState* read_patch_states_internal(const char* filename, size_t* count)
{
#if !defined(__PRX__)
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        *count = 0;
        return NULL;
    }

    PatchStateFileHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1)
    {
        fclose(f);
        *count = 0;
        return NULL;
    }

    if (header.magic != PATCH_STATE_MAGIC)
    {
        printf("Invalid patch state file magic %x == %x\n", header.magic, PATCH_STATE_MAGIC);
        fclose(f);
        *count = 0;
        return NULL;
    }

    if (header.version != PATCH_STATE_VERSION)
    {
        printf("Unsupported patch state file version\n");
        fclose(f);
        *count = 0;
        return NULL;
    }

    *count = header.count;

    if (*count == 0)
    {
        fclose(f);
        return NULL;
    }

    PatchState* states = malloc(sizeof(PatchState) * header.count);
    if (!states)
    {
        fclose(f);
        *count = 0;
        return NULL;
    }

    if (fread(states, sizeof(PatchState), header.count, f) != header.count)
    {
        free(states);
        fclose(f);
        *count = 0;
        return NULL;
    }

    fclose(f);
    return states;
#else
    FileHandle h = 0;
    FileStatus ret = fileOpen(&h, filename, FILE_MODE_READ);
    uint64_t fsz = 0;
    PatchStateFileHeader header;
    PatchState* states = NULL;
    *count = 0;

    if (ret == FILE_STATUS_OK && fileSize(h, &fsz) == FILE_STATUS_OK && fsz > 0)
    {
        uint64_t readcount = 0;
        FileStatus read = fileRead(h, &header, sizeof(header), &readcount);
        if (read == FILE_STATUS_OK && readcount > 0)
        {
            if (header.magic == PATCH_STATE_MAGIC && header.version == PATCH_STATE_VERSION)
            {
                *count = header.count;
                if (*count > 0)
                {
                    const size_t nbytes = sizeof(PatchState) * header.count;
                    states = malloc(nbytes);
                    if (states)
                    {
                        uint64_t statesBytes = 0;
                        FileStatus read2 = fileRead(h, states, nbytes, &statesBytes);
                        if (read2 != FILE_STATUS_OK || statesBytes != nbytes)
                        {
                            free(states);
                            states = NULL;
                            *count = 0;
                        }
                    }
                }
            }
        }
    }
    fileClose(h);
    return states;
#endif
}

#if !defined(__PRX__)
int write_patch_state(const char* filename, uint32_t hash, bool enabled)
{
    size_t count = 0;
    PatchState* states = read_patch_states_internal(filename, &count);

    bool found = false;
    for (size_t i = 0; i < count; i++)
    {
        if (states[i].hash == hash)
        {
            states[i].enabled = enabled;
            found = true;
            printf("Updated hash 0x%08x to %s\n", hash, enabled ? "enabled" : "disabled");
            break;
        }
    }

    if (!found)
    {
        PatchState* new_states = realloc(states, sizeof(PatchState) * (count + 1));
        if (!new_states)
        {
            free(states);
            printf("Failed to allocate memory for new patch state\n");
            return -1;
        }

        states = new_states;
        states[count].hash = hash;
        states[count].enabled = enabled;
        count++;

        printf("Added hash 0x%08x as %s\n", hash, enabled ? "enabled" : "disabled");
    }

    const int result = write_patch_states_internal(filename, states, count);
    free(states);

    return result;
}
#endif

int read_patch_state(const char* filename, uint32_t hash)
{
    size_t count = 0;
    PatchState* states = read_patch_states_internal(filename, &count);

    if (!states && count == 0)
    {
        return -1;
    }

    int result = -1;
    for (size_t i = 0; i < count; i++)
    {
        if (states[i].hash == hash)
        {
            result = states[i].enabled;
            break;
        }
    }

    free(states);
    return result;
}

#if !defined(__PRX__)
int toggle_patch_state(const char* filename, uint32_t hash)
{
    const int current_state = read_patch_state(filename, hash);

    if (current_state == -1)
    {
        if (write_patch_state(filename, hash, true) == 0)
        {
            return 1;
        }
        return -1;
    }

    const bool new_state = (current_state == 0);
    if (write_patch_state(filename, hash, new_state) == 0)
    {
        return new_state;
    }

    return -1;
}
#endif

#if defined(__PRX__)

static void metadata_callback(const PatchMetadata* meta, void* user_data)
{
    size_t* count = (size_t*)user_data;
    (*count)++;
    printf("Patch %ld (#%ld) (Hash: 0x%08x)\n", *count, meta->patch_number, meta->hash);
    printf("  Title: %s\n", meta->title ? meta->title : "N/A");
    printf("  Name: %s\n", meta->name ? meta->name : "N/A");
    printf("  Author: %s\n", meta->author ? meta->author : "N/A");
    printf("  Version: %s\n", meta->version ? meta->version : "N/A");
    printf("  App Binary: %s\n", meta->app_bin ? meta->app_bin : "N/A");
    printf("  App Version: %s\n", meta->app_ver ? meta->app_ver : "N/A");
    printf("  Matches: %s, Enabled: %s\n",
           meta->matches_game ? "Yes" : "No",
           meta->enabled ? "Yes" : "No");
}

static bool isHex(const char* s)
{
    return s && (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
}

static int isHexBase(const char* s)
{
    return isHex(s) ? 16 : 10;
}

static void write_patch(void* addr, const void* val, const size_t valsz)
{
    static sys_pid_t current_pid = 0;
    if (!current_pid)
    {
        current_pid = sys_process_getpid();
    }
    if (current_pid)
    {
        hex_dump((void*)addr, valsz, (uintptr_t)addr);
        WriteProcessMemory(current_pid, addr, val, valsz);
        hex_dump((void*)addr, valsz, (uintptr_t)addr);
    }
}

static int64_t string_to_int(const char* str)
{
    return strtoll(str, NULL, isHexBase(str));
}

static uint64_t string_to_uint(const char* str)
{
    return strtoull(str, NULL, isHexBase(str));
}

#if 0 // how should this be ~~copied~~ added?
static double string_to_double(const char* str)
{
    return strtod(str, NULL);
}
#endif

static void apply_patch(const PatchEntry* entry)
{
    const size_t params = entry->param_count;

    if (params >= MIN_PATCH_PARAMS)
    {
        const char* type = entry->params[0];
        const char* address = entry->params[1];
        const char* value = entry->params[2];
        uintptr_t base = 0;  // TODO: Support prx

        if (isZero(strncmp2(type, "bytes8")) || isZero(strncmp2(type, "byte")))
        {
            const uintptr_t addr = string_to_uint(address);
            const int8_t val = string_to_int(value);
            write_patch((void*)(base + addr), &val, sizeof(val));
        }
        else if (isZero(strncmp2(type, "bytes16")) || isZero(strncmp2(type, "be16")))
        {
            const uintptr_t addr = string_to_uint(address);
            const int16_t val = string_to_int(value);
            write_patch((void*)(base + addr), &val, sizeof(val));
        }
        else if (isZero(strncmp2(type, "bytes32")) || isZero(strncmp2(type, "be32")))
        {
            const uintptr_t addr = string_to_uint(address);
            const int32_t val = string_to_int(value);
            write_patch((void*)(base + addr), &val, sizeof(val));
        }
        else if (isZero(strncmp2(type, "bytes64")) || isZero(strncmp2(type, "be64")))
        {
            const uintptr_t addr = string_to_uint(address);
            const int64_t val = string_to_int(value);
            write_patch((void*)(base + addr), &val, sizeof(val));
        }
        else if (g_args && strcmp(type, "append_arg") == 0)
        {
            // starting from `"type"`. up to 7 args per entry
            for (size_t i = 1; i < params; i++)
            {
                if (entry->params[i] && entry->params[i][0])
                {
                    if (append_arg(g_args, entry->params[i]))
                    {
                        printf("appended \"%s\" okay!\n", entry->params[i]);
                    }
                    else
                    {
                        printf("couldn't append %ld arguement.\n", i);
                    }
                }
            }
        }
#if 0
        else if (strcmp(type, "float32") == 0)
        {
            const float val = (float)string_to_double(value);
            write_patch((void*)(base + addr), &val, sizeof(val));
        }
        else if (strcmp(type, "float64") == 0)
        {
            const double val = string_to_double(value);
            write_patch((void*)(base + addr), &val, sizeof(val));
        }
#endif
    }
}

static void entry_callback(const PatchMetadata* meta, const PatchEntry* entry, void* user_data)
{
    here();
    if (meta->enabled)
    {
        apply_patch(entry);
    }
    printf("- [ ");
    for (size_t i = 0; i < entry->param_count; i++)
    {
        printf("\"%s\"", entry->params[i]);
        if (i < entry->param_count - 1)
        {
            printf(", ");
        }
    }
    printf(" ]\n");
}

int run_patch(GamePatchInfo* game_info)
{
    char path[MAX_PATH + 1] = {0};
    snprintf(path, _countof_1(path), GAME_PATCH_FILES_PATH "/%s.yml", game_info->titleid);
    ParseContext ctx;
    bzero(&ctx, sizeof(ctx));
    create_parse_context(&ctx, game_info, PARSE_MODE_LOW_MEM);
    size_t count = 0;
    ParseContext input;
    bzero(&input, sizeof(input));

    input.filename = path;
    input.meta_callback = metadata_callback;
    input.entry_callback = entry_callback;
    input.user_data = &count;
    const int ret = parse_patch_file_low_mem(&ctx, &input);
    if (ret == 0 && count > 0)
    {
        char buf[64 + 1] = {0};
        snprintf(buf, _countof_1(buf), "Applied %ld patch%s", count, count > 1 ? "es" : "");
        uint64_t write_count = 0;
        fileWrite2(GAME_PATCH_NOTIFY_MSG_FILE, buf, strlen(buf), &write_count);
        printf("%s\n", buf);
    }
    else if (count == 0)
    {
        printf("no patches, deleting notification file!\n");
        fileDelete(GAME_PATCH_NOTIFY_MSG_FILE);
    }

    free_parse_context_data(&ctx);
    return count > 0 ? 0 : 1;
}

#endif
