#define MAX_SOCKET_BUF 1024
// 50= 80 (column terminal) - 2 (chat window borders) - 9 (timestamp+space) - 9 (nick+space) - 8 (nick in nick window) - 2 (nick window borders)
#define MAX_MSG_LENGTH 50
#define MAX_NICK_LENGTH 8
#define MAX_PASS_LENGTH 12
#define MAX_CHANNEL_LENGTH 12

#define TRUE 1
#define FALSE 0
#define DEBUG

void SetNonblocking(int sock);
int StrBegins(const char *haystack, const char *beginning);
int CountParams(const char *cmd);
