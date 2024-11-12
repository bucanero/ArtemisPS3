#ifdef ARTEMIS_ENABLE_LOGGING
#include <dbglogger.h>
#define LOG dbglogger_log
#else
#define LOG(...)
#define dbglogger_init(...)
#endif

#define ARTEMIS_PATH    "/dev_hdd0/game/ARTPS3001/USRDIR/"

#define USERLIST_PATH_USB		"/dev_usb000/USERLIST/"
#define USERLIST_PATH_HDD		ARTEMIS_PATH "USERLIST/"

#define ONLINE_URL				"https://bucanero.github.io/ArtemisPS3/"
#define ONLINE_LOCAL_CACHE		ARTEMIS_PATH "CACHE/"
#define ARTEMIS_UPDATE_URL		"https://api.github.com/repos/bucanero/ArtemisPS3/releases/latest"

#define ONLINE_CACHE_TIMEOUT    24*3600     // 1-day local cache

struct option_entry
{
    char * line;
    char * * name;
    char * * value;
    int id;
    int size;
    int sel;
};

struct code_entry
{
    char * name;
    int cwrite;
    int activated;
    int options_count;
    char * codes;
    struct option_entry * options;
};

struct game_entry
{
    char * name;
	char * version;
	char * title_id;
	char * path;
    int code_count;
    int code_sorted;
    struct code_entry * codes;
};

struct game_entry * ReadUserList(int * gmc);
struct game_entry * ReadOnlineList(int * gmc);
void UnloadGameList(struct game_entry * list, int count);
int isGameActivated(struct game_entry game);
char * ParseActivatedGameList(struct game_entry * list, int count);
void writeFile(const char * path, char * a, char * b);
char * readFile(const char * path);
void readFileBuffered(const char * path, char * buffer);
void QSortGameList(struct game_entry * games, int count);
struct game_entry QSortCodeList(struct game_entry game);
int isCodeLineValid(char * line);
long getFileSize(const char * path);
struct option_entry * ReadOptions(struct code_entry code, int * count);
struct code_entry * ReadNCL(const char * path, int * _code_count);
struct code_entry * ReadOnlineNCL(const char * path, int * _code_count);
int FilterInstalledGameList(struct game_entry * games, int count, char ** installed_titleids, int installed_count);

int http_init(void);
void http_end(void);
int http_download(const char* url, const char* filename, const char* local_dst, int show_progress);

int extract_zip(const char* zip_file, const char* dest_path);

int show_dialog(int tdialog, const char * str);
void init_progress_bar(const char* progress_bar_title, const char* msg);
void update_progress_bar(long unsigned int* progress, const long unsigned int total_size, const char* msg);
void end_progress_bar(void);
