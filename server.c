#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#define PORT 8080     // 定义端口号为8080
#define NUM_PLAYERS 8 // 定义游戏的玩家数量为8

// 定义角色代码
#define MAFIA 0
#define CARTEL 1
#define POLICE 2
#define BEGGAR 3
#define NUM_ROLES 4 // 总共的角色数量

// 玩家信息结构体定义
typedef struct
{
    int socket;               // 玩家的socket描述符
    int playerNumber;         // 玩家编号
    int playerSpeakingNumber; // 玩家选择身份顺序
    int trueIdentity;         // 玩家的真实身份
    int declaredIdentity;     // 玩家声明的身份
    int score;                // 每个角色的得分
    int iflie;                // 表示玩家是否说谎
    int ifwin;                // 表示玩家是否赢得本局
    int perScore;             // 表示玩家本轮得分
} PlayerInfo;

PlayerInfo players[NUM_PLAYERS];     // 玩家数组
int declaredCount[4] = {0, 0, 0, 0}; // 每个角色声明的计数
int roleCounts[4] = {0, 0, 0, 0};    // 每个角色的真实选择计数

// 发送消息给所有玩家
void sendToAllPlayers(char *message)
{
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        send(players[i].socket, message, strlen(message), 0);
    }
}

// 玩家排名并发送最终排名和得分给所有玩家
void rankAndBroadcastScores(int num_players)
{
    // 简单冒泡排序玩家基于得分
    for (int i = 0; i < num_players - 1; i++)
    {
        for (int j = 0; j < num_players - i - 1; j++)
        {
            if (players[j].score < players[j + 1].score)
            {
                // 交换玩家
                PlayerInfo temp = players[j];
                players[j] = players[j + 1];
                players[j + 1] = temp;
            }
        }
    }

    // 构建排名消息
    char rankingMessage[1024];
    strcpy(rankingMessage, "最终排名和得分:\n");
    for (int i = 0; i < num_players; i++)
    {
        char scoreMessage[256];
        sprintf(scoreMessage, "第 %d 名: 玩家 %d - 总得分: %d\n", i + 1, players[i].playerNumber, players[i].score);
        strcat(rankingMessage, scoreMessage);
    }

    // 发送排名消息给所有客户端
    sendToAllPlayers(rankingMessage);
}

// 清除字符串末尾的换行符或回车符
void trimNewline(char *str)
{
    char *ptr;
    if ((ptr = strchr(str, '\n')) != NULL)
    {
        *ptr = '\0'; // 替换换行符
    }
    if ((ptr = strchr(str, '\r')) != NULL)
    {
        *ptr = '\0'; // 替换回车符
    }
}

// 每轮游戏重置玩家信息
void initInformation(int num_players)
{
    for (int i = 0; i < num_players; i++)
    {
        players[i].iflie = 0;
        players[i].ifwin = 0;
        players[i].trueIdentity = 0;
        players[i].declaredIdentity = 0;
        players[i].perScore = 0;
    }
}

// 石榴石分配
int distributeAmongRole(int num_players, int role, int totalGarnets)
{
    if (totalGarnets == 0)
    {
        return 0;
    }

    int playerCount = 0;

    // 计算指定角色的玩家数量
    for (int i = 0; i < num_players; i++)
    {
        if (players[i].trueIdentity == role)
        {
            playerCount++;
        }
    }

    // 如果玩家数量在1到4之间，则均分石榴石
    if (1 <= playerCount && playerCount <= 4)
    {
        int garnetsPerPlayer = totalGarnets / playerCount;

        // 分配石榴石给每个选定角色的玩家
        for (int i = 0; i < num_players; i++)
        {
            if (players[i].trueIdentity == role)
            {
                players[i].score += garnetsPerPlayer;
                players[i].perScore = garnetsPerPlayer;
                players[i].ifwin = 1;
            }
        }

        // 计算并返回分配后剩余的石榴石数量
        return totalGarnets - (garnetsPerPlayer * playerCount);
    }

    // 如果玩家数量不在1到4之间，则无人得到石榴石，返回所有石榴石
    return totalGarnets;
}

// 更新声明的角色计数
void updateDeclaredCount(int declaredRole)
{
    if (declaredRole >= 0 && declaredRole < 4)
    {
        declaredCount[declaredRole]++;
    }
}

// 向所有玩家广播当前真实的角色数量
void broadcastRealCounts(int num_players)
{
    char message[1024];

    sprintf(message, "当前真实的角色信息： - MAFIA: %d, CARTEL: %d, POLICE: %d, BEGGAR: %d",
            roleCounts[0], roleCounts[1], roleCounts[2], roleCounts[3]);

    for (int i = 0; i < num_players; i++)
    {
        send(players[i].socket, message, strlen(message), 0);
    }
}

// 向所有玩家广播当前声明的角色数量
void broadcastDeclaredCounts(int num_players)
{
    char message[1024];

    sprintf(message, "当前声明的角色信息： - MAFIA: %d, CARTEL: %d, POLICE: %d, BEGGAR: %d\n",
            declaredCount[0], declaredCount[1], declaredCount[2], declaredCount[3]);

    for (int i = 0; i < num_players; i++)
    {
        send(players[i].socket, message, strlen(message), 0);
    }
}

// 初始化玩家信息
void initializePlayerInfo()
{
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        players[i].socket = -1;
        players[i].playerNumber = -1;
        players[i].playerSpeakingNumber = -1;
        players[i].trueIdentity = -1;
        players[i].declaredIdentity = -1;
        players[i].score = 0;
        players[i].iflie = 0;
        players[i].ifwin = 0;
        players[i].perScore = 0;
    }
}

// 解析角色
int parseRole(char *buffer)
{
    if (strcmp(buffer, "MAFIA") == 0)
        return MAFIA;
    if (strcmp(buffer, "CARTEL") == 0)
        return CARTEL;
    if (strcmp(buffer, "POLICE") == 0)
        return POLICE;
    if (strcmp(buffer, "BEGGAR") == 0)
        return BEGGAR;
    return -1; // 如果没有匹配，返回-1或其他错误代码
}

// 获取角色名称
const char *getRoleName(int role)
{
    switch (role)
    {
    case MAFIA:
        return "MAFIA";
    case CARTEL:
        return "CARTEL";
    case POLICE:
        return "POLICE";
    case BEGGAR:
        return "BEGGAR";
    default:
        return "UNKNOWN";
    }
}

// 分配石榴石
void distributeGarnets(int num_players)
{
    int totalGarnets = 4; // 总共有4颗石榴石

    // 黑手党和卡特尔分配逻辑
    int mafiaCount = roleCounts[MAFIA];
    int cartelCount = roleCounts[CARTEL];

    if (mafiaCount > cartelCount)
    {
        totalGarnets = distributeAmongRole(num_players, MAFIA, totalGarnets);
    }
    else if (cartelCount > mafiaCount)
    {
        totalGarnets = distributeAmongRole(num_players, CARTEL, totalGarnets);
    }

    // 警察分配逻辑
    if (mafiaCount == 0 && cartelCount == 0 || mafiaCount == cartelCount)
    {
        totalGarnets = distributeAmongRole(num_players, POLICE, totalGarnets);
    }

    // 乞丐分配逻辑
    distributeAmongRole(num_players, BEGGAR, totalGarnets);
}

// 扣分逻辑
void decreaseScore(int num_players)
{
    for (int i = 0; i < num_players; i++)
    {
        if (players[i].iflie == 1 && players[i].ifwin == 0)
        {
            players[i].score--;
            players[i].perScore--;
        }
    }
}

// 随机分配玩家编号
void assignPlayerNumbers(int num_players)
{
    int numbers[num_players];
    for (int i = 0; i < num_players; i++)
    {
        numbers[i] = i + 1; // 初始化编号数组
    }

    // 使用Fisher-Yates洗牌算法随机分配编号
    for (int i = num_players - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int temp = numbers[i];
        numbers[i] = numbers[j];
        numbers[j] = temp;
    }

    // 分配编号给玩家并发送编号信息
    char message[1024];
    for (int i = 0; i < num_players; i++)
    {
        players[i].playerNumber = numbers[i];

        // 构造包含玩家编号的消息
        sprintf(message, "您的玩家编号是: %d", players[i].playerNumber);

        // 向该玩家发送消息
        send(players[i].socket, message, strlen(message), 0);
    }
}

// 初始化玩家发言顺序
void initSpeakingOrder(int num_players)
{

    for (int i = 0; i < num_players; i++)
    {
        players[i].playerSpeakingNumber = players[i].playerNumber;
    }
}

// 等待用户输入
int waitForInput(int socket)
{
    struct pollfd fd;
    int ret;

    // 设置pollfd结构体
    fd.fd = socket;     // 监听的socket文件描述符
    fd.events = POLLIN; // 报告ready-to-read事件

    // 使用 poll 等待用户输入
    ret = poll(&fd, 1, 300000); // 超时时间300000毫秒（5分钟）

    if (ret == -1)
    {
        perror("poll error");
        return -1; // 如果poll调用错误，返回-1
    }
    else if (ret == 0)
    {
        printf("超时，没有接收到客户端输入\n");
        return 0; // 如果超时，返回0
    }
    else
    {
        if (fd.revents & POLLIN)
        {
            return 1; // 输入就绪，返回1
        }
    }
    return 0;
}

int main()
{
    int server_fd, new_socket;
    int i;
    int max_sd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    int connected_players = 0; // 已连接的玩家数

    struct pollfd fds[NUM_PLAYERS + 1];

    // 创建一个socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置socket选项，使端口号和地址可以复用
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址信息
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 绑定socket到地址
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 监听网络连接
    if (listen(server_fd, NUM_PLAYERS) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // 初始化玩家信息
    initializePlayerInfo();

    // 初始化pollfd结构体
    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;  // 第一个pollfd用于监听server socket
    fds[0].events = POLLIN; // 报告ready-to-read事件

    printf("等待所有玩家加入...\n");
    char joinMsg[] = "请等待其他玩家进入游戏...\n";
    sendToAllPlayers(joinMsg);

    // 主循环，等待玩家连接
    while (1)
    {
        // 设置pollfd结构体
        for (int i = 0; i <= connected_players; i++)
        {
            fds[i].fd = (i == 0) ? server_fd : players[i - 1].socket;
            fds[i].events = POLLIN;
        }

        int poll_count = poll(fds, connected_players + 1, -1);
        if (poll_count < 0)
        {
            perror("poll error");
            exit(EXIT_FAILURE);
        }

        // 检查新连接
        if (fds[0].revents & POLLIN)
        {
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            if (new_socket < 0)
            {
                perror("接听失败");
                exit(EXIT_FAILURE);
            }
            printf("有玩家加入游戏，位于端口: %d\n", ntohs(address.sin_port));

            // 发送欢迎消息给新连接
            const char *hello = "欢迎您加入游戏，请等待其他玩家加入游戏....\n";
            send(new_socket, hello, strlen(hello), 0);

            // 将新玩家添加到玩家列表
            for (int i = 0; i < NUM_PLAYERS; i++)
            {
                if (players[i].socket == -1)
                {
                    players[i].socket = new_socket;
                    connected_players++;
                    break;
                }
            }

            // 如果已经连接足够数量的玩家，就fork一个新进程来处理游戏
            if (connected_players == NUM_PLAYERS)
            {
                pid_t pid = fork();
                // 子进程
                if (pid == 0)
                {
                    // 子进程
                    printf("游戏开始，子进程ID: %d\n", getpid());
 
                    char beginMsg[] = "所有玩家均加入游戏，游戏开始！\n";
                    sendToAllPlayers(beginMsg);

                    // 分配编号给玩家
                    assignPlayerNumbers(NUM_PLAYERS);

                    // 初始化玩家的身份选择顺序
                    initSpeakingOrder(NUM_PLAYERS);

                    for (int round = 0; round < 8; round++)
                    {
                        // 游戏主循环

                        // 重置角色计数器
                        memset(declaredCount, 0, sizeof(declaredCount));
                        memset(roleCounts, 0, sizeof(roleCounts));

                        // 玩家进行身份选择：真实身份和宣布的身份
                        for (int i = 0; i < NUM_PLAYERS; i++)
                        {
                            for (int j = 0; j < NUM_PLAYERS; j++)
                            {
                                if (players[j].playerSpeakingNumber == i + 1)
                                {
                                    int trueRole;
                                    int declaredRole;

                                    // 玩家选择真实身份
                                    do
                                    {
                                        char trueIdentityMessage[1024];
                                        sprintf(trueIdentityMessage, "玩家%d，请选择您的真实身份（MAFIA/CARTEL/POLICE/BEGGAR）:\n", players[j].playerNumber);
                                        send(players[j].socket, trueIdentityMessage, strlen(trueIdentityMessage), 0);
                                        // 等待用户输入
                                        int isValid = waitForInput(players[j].socket);
                                        if (isValid == -1)
                                        {
                                            // 处理错误情况
                                            return -1;
                                        }
                                        else if (isValid == 0)
                                        {
                                            // 处理超时情况
                                            // 继续等待或采取其他操作
                                            continue;
                                        }
                                        else
                                        {
                                            char buffer[1024] = {0};
                                            int received = read(players[j].socket, buffer, sizeof(buffer));
                                            if (received <= 0)
                                            {
                                                perror("read error");
                                                return -1;
                                            }
                                            buffer[received] = '\0';
                                            printf("接收到客户端发送的消息: %s\n", buffer);
                                            trimNewline(buffer); // 清除换行符或回车符
                                            trueRole = parseRole(buffer);
                                            if (trueRole >= 0 && trueRole < 4)
                                            {
                                                players[j].trueIdentity = trueRole;
                                                roleCounts[trueRole]++;
                                            }
                                            else
                                            {
                                                char errorMsg[1024];
                                                sprintf(errorMsg, "输入错误，请重新输入！\n");
                                                send(players[j].socket, errorMsg, strlen(errorMsg), 0);
                                            }
                                        }
                                    } while (trueRole < 0 || trueRole >= 4);

                                    // 玩家选择宣布身份
                                    do
                                    {
                                        char declareIdentityMessage[1024];
                                        sprintf(declareIdentityMessage, "玩家%d，请选择您要宣布的身份（MAFIA/CARTEL/POLICE/BEGGAR）:\n", players[j].playerNumber);
                                        send(players[j].socket, declareIdentityMessage, strlen(declareIdentityMessage), 0);
                                        // 等待用户输入
                                        int isValid = waitForInput(players[j].socket);
                                        if (isValid == -1)
                                        {
                                            // 处理错误情况
                                            return -1;
                                        }
                                        else if (isValid == 0)
                                        {
                                            // 处理超时情况
                                            // 继续等待或采取其他操作
                                            continue;
                                        }
                                        else
                                        {
                                            char buffer[1024] = {0};
                                            int received = read(players[j].socket, buffer, sizeof(buffer));
                                            if (received <= 0)
                                            {
                                                perror("read error");
                                                return -1;
                                            }
                                            buffer[received] = '\0';
                                            printf("接收到客户端发送的消息: %s\n", buffer);
                                            trimNewline(buffer); // 清除换行符或回车符
                                            declaredRole = parseRole(buffer);
                                            if (declaredRole >= 0 && declaredRole < 4)
                                            {
                                                players[j].declaredIdentity = declaredRole;
                                                declaredCount[declaredRole]++;
                                            }
                                            else
                                            {
                                                char errorMsg[1024];
                                                sprintf(errorMsg, "输入错误，请重新输入！\n");
                                                send(players[j].socket, errorMsg, strlen(errorMsg), 0);
                                            }
                                        }
                                    } while (declaredRole < 0 || declaredRole >= 4);

                                    // 判断玩家是否说谎
                                    if (players[j].trueIdentity != players[j].declaredIdentity)
                                    {
                                        players[j].iflie = 1;
                                    }

                                    // 广播当前宣布的角色数量
                                    broadcastDeclaredCounts(NUM_PLAYERS);
                                }
                            }
                        }

                        // 广播本轮真实身份选择角色数量
                        broadcastRealCounts(NUM_PLAYERS);

                        // 给胜者加分
                        distributeGarnets(NUM_PLAYERS);

                        // 给败者且说谎的人扣分
                        decreaseScore(NUM_PLAYERS);

                        // 广播本轮得分
                        for (int i = 0; i < NUM_PLAYERS; i++)
                        {
                            char scoreMessage[1024];
                            sprintf(scoreMessage, "身份为 %s 的玩家 %d 本轮得分 %d \n", getRoleName(players[i].trueIdentity), players[i].playerNumber, players[i].perScore);

                            // 发送得分消息给所有客户端
                            for (int j = 0; j < NUM_PLAYERS; j++)
                            {
                                send(players[j].socket, scoreMessage, strlen(scoreMessage), 0);
                            }
                        }

                        // 游戏进入下一轮
                        // speakNumber调整
                        for (int i = 0; i < NUM_PLAYERS; i++)
                        {
                            if (players[i].playerSpeakingNumber == 1)
                            {
                                players[i].playerSpeakingNumber = NUM_PLAYERS;
                            }
                            else
                            {
                                players[i].playerSpeakingNumber--;
                            }
                        }
                        initInformation(NUM_PLAYERS);
                    }

                    char endGameMsg[] = "游戏结束!";
                    sendToAllPlayers(endGameMsg);

                    // 统计排名得分信息并发送给所有客户端
                    rankAndBroadcastScores(NUM_PLAYERS);

                    exit(0); // 游戏结束后子进程退出
                }
                else if (pid > 0)
                {
                    // 父进程
                    printf("主服务器继续监听新的玩家连接，父进程ID: %d\n", getpid());

                    // 重置玩家信息和连接玩家计数，为下一轮游戏准备
                    initializePlayerInfo();
                    connected_players = 0;
                }
                else
                {
                    // fork失败
                    perror("fork failed");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}
