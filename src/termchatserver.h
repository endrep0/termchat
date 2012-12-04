#define MAX_CHAT_CLIENTS 15
#define MAX_SAVED_PASSWORDS 100

#define DISCONNECTED 0
#define WAITING_FOR_NICK 1
#define HAS_NICK_WAITING_FOR_CHANNEL 2
#define CHATTING 3

// chat client data type
typedef struct {
	int socket;
	// status: DISCONNECTED, WAITING_FOR_NICK, HAS_NICK_WAITING_FOR_CHANNEL, CHATTING
	int status;
	char nickname[MAX_NICK_LENGTH];
	char channel[MAX_CHANNEL_LENGTH];
} chat_client_t;

// password pairs data type
typedef struct {
	char nickname[MAX_NICK_LENGTH];
	char password_sha512[129];
} passwords_t;

void BuildSelectList(void);
void HandleNewConnection(void);
void ProcessPendingRead(int clientindex);
void ProcessSocketsToRead(void);
int SendMsgToClient(int clientindex, const char *msg);
void ProcessClientChangeNick(int clientindex, const char *cmd_msg);
void ProcessClientChangeChan(int clientindex, const char *cmd_msg);
void BroadcastChanNicklist(const char* channel);
void ProcessClientChangePass(int clientindex, const char *cmd_msg);
void ProcessClientChanMsg(int clientindex, const char *chan_msg);
void ProcessClientPrivMsg(int clientindex, const char *priv_msg);
void QuitGracefully(int signum);
int LoadPasswordsFromDisk(void);
int SavePasswordsToDisk(void);

