#define MAX_SOCKET_BUF 1024
#define MAX_MSG_LENGTH 64
#define MAX_NICK_LENGTH 8
#define MAX_PASS_LENGTH 12
#define MAX_CHANNEL_LENGTH 12

#define TRUE 1
#define FALSE 0
#define DEBUG

void SetNonblocking(int sock);
int StrBegins(const char *haystack, const char *beginning);
int CountParams(const char *cmd);
