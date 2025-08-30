#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_MESSAGE_SIZE 8192

int main()
{
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char message[MAX_MESSAGE_SIZE] = {0};
    char buffer[MAX_MESSAGE_SIZE] = {0};

    // 创建套接字
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        return -1;
    }

    // 配置服务器信息
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 转换IP地址从文本到二进制
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // 连接到服务器
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        return -1;
    }

    while (1)
    {
        // 从服务器接收响应
        valread = read(sock, buffer, MAX_MESSAGE_SIZE);

        if (valread > 0)
        {
            printf("Server response: \n%s\n", buffer);

            if (strstr(buffer, "请选择您的真实身份") != NULL || strstr(buffer, "请选择您要宣布的身份") != NULL)
            {
                // 读取用户输入
                printf("Enter your choice (or 'quit' to exit): ");
                if (fgets(message, MAX_MESSAGE_SIZE, stdin) == NULL) {
                    perror("Error reading user input");
                    break;
                }


                // 发送消息给服务器
                if (send(sock, message, strlen(message), 0) == -1) {
                    perror("Error sending message to server");
                    break;
                }
                printf("Message sent: %s\n", message);
            }
        }
        else if (valread == 0)
        {
            // 服务器连接关闭
            printf("Server connection closed.\n");
            break;
        }
        else
        {
            // 读取错误
            perror("Error reading from server");
            break;
        }

        // 清空缓冲区
        memset(buffer, 0, sizeof(buffer));
    }

    // 关闭socket
    close(sock);
    return 0;
}
