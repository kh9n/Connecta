#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <android/log.h>

#define PORT 27015
#define MAX_CLIENTS 5

#define PRODUCT_NAME "Connecta"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, PRODUCT_NAME, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , PRODUCT_NAME, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , PRODUCT_NAME, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , PRODUCT_NAME, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , PRODUCT_NAME, __VA_ARGS__)

int start_server()
{
    int ret;

    LOGV("Starting server...");
    int sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sfd < 0)
    {
        LOGE("Failed to create socket.");
        return sfd;
    }

    struct sockaddr_in sin;
    sin.sin_family = PF_INET;
    sin.sin_port = htons(PORT); // convert bytes to big-endian
    sin.sin_addr.s_addr = htons(INADDR_ANY);

    ret = bind(sfd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        LOGE("Socket failed to bind to local.");
        return ret;
    }
    LOGV("Socket binding");

    ret = listen(sfd, MAX_CLIENTS);
    if (ret < 0)
    {
        LOGE("Socket failed to listen on local port.");
        return ret;
    }
    LOGV("Socket listening");

    // Reference to http://www.cnblogs.com/hateislove214/archive/2010/11/05/1869886.html
    struct linger l = {0, 0};
    ret = setsockopt(sfd, SOL_SOCKET, SO_LINGER, (const void*)&l, sizeof(struct linger));
    if (ret < 0)
    {
        LOGE("Socket failed to set listening linger mode.");
        return ret;
    }
    LOGV("Server started");

    return sfd;
}

int main(int argc, char* argv [])
{
    int server_socket = start_server();
    if (server_socket < 0)
    {
        LOGE("Failed to start socket server. Error code: %d", server_socket);
    }
}
