#define MAX_IGNORES 10
#define CHAT_WINDOW_BUFFER_MAX_LINES 100

#define SCROLL_DIRECTION_UP -1
#define SCROLL_DIRECTION_DOWN 1


WINDOW *create_newwin(int height, int width, int starty, int startx);
void HandleKeypress(void);
void HandleMessageFromServer(char *message_from_server);
void SetNonblocking(int sock);
void AddMsgToChatWindow(const char* msg, int timestamped);
void ScrollChatWindow(int direction);
void UpdateNicklist(char* nicklist);
char password_sha512[129];
void SHA512(char *source_string, char *hash_in_hex_string);
