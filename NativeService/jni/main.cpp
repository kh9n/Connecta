#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <unistd.h>

#include <android/log.h>

#define PORT 27015
#define MAX_CLIENTS 5
#define BUF_SIZE 256
#define SCREENSHOT_CMD "SCREEN"

#define PRODUCT_NAME "Connecta"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, PRODUCT_NAME, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , PRODUCT_NAME, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , PRODUCT_NAME, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , PRODUCT_NAME, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , PRODUCT_NAME, __VA_ARGS__)

volatile sig_atomic_t service_quit_flag = 1;

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

ssize_t Receive(int sfd, char* buf, size_t count, int flags)
{
	int c;
	size_t len = 0;

	do
	{
		c = recv(sfd, buf, count, flags);
		if (c < 0)	return c;
		if (c == 0)	return len;

		buf += c;
		len += c;
		count -= c;
	} while (count > 0);

	return len;
}

ssize_t Send(int sfd, const char* buf, ssize_t count, int flags)
{
	int c;
	size_t len = 0;

	do
	{
		c = send(sfd, buf, count, flags);
		if (c < 0)	return c;

		buf += c;
		len += c;
		count -= c;

//#ifdef DEBUG
//		char msg[BUF_SIZE];
//		snprintf (msg, BUF_SIZE, "-- Sent %d bytes (%d total, %d remaining)", c, len, count);
//		Log (msg);
//#endif
	} while (count > 0);

	return len;
}

int accept_client(int servfd, int** client_fd, int* client_count)
{
	LOGV ("Incoming client connection");

	int cfd = accept(servfd, NULL, NULL);
	if (cfd < 0)	return -1;
	LOGV ("- Connection accepted");

	/* check whether the client comes from local system; detach if not */
	struct sockaddr_in client_addr;
	socklen_t ca_len = sizeof(struct sockaddr_in);
	if (getpeername(cfd, (struct sockaddr*)&client_addr, &ca_len) < 0)	return -1;
	if (strcmp(inet_ntoa(client_addr.sin_addr), "127.0.0.1") != 0) {
		LOGV ("- Remote client detected -- closing connection.");
		shutdown (cfd, SHUT_RDWR);
		close (cfd);
		return 0;
	}

	*client_fd = (int*)realloc(*client_fd, sizeof(int) * (*client_count + 1));
	(*client_fd)[(*client_count)++] = cfd;	// (*client_fd)[...] != *client_fd[...] -- f'kin precedence ;/

	return cfd;
}

int handle_client_input(int cfd)
{
	LOGV ("Client socket signaled for input");
	// struct picture pict;
	char buf[BUF_SIZE];
	int c;

	/* read input and parse it */
	LOGV ("- Retreiving data");
	c = Receive(cfd, buf, strlen(SCREENSHOT_CMD), 0);
	if (c == 0 || (c < 0 && errno == EINTR))	return 0;
	if (c < 0)	return -1;
	if (c >= (signed)strlen(SCREENSHOT_CMD) && (buf[strlen(SCREENSHOT_CMD)] = '\0', strcmp(buf, SCREENSHOT_CMD) == 0))
	{
		LOGV ("- Command identified as " SCREENSHOT_CMD);

		/* screenshot command read; take screenshot and post it through socket */
		LOGV ("- Taking screenshot");
		// if (TakeScreenshot(fddev, &pict) < 0)	return -1;
		LOGV ("- Screenshot taken");

		/* header: width height BPP */
		memset (buf, 0, BUF_SIZE * sizeof(char));
		//snprintf (buf, BUF_SIZE, "%d %d %d", pict.xres, pict.yres, pict.bps);
		if (Send(cfd, buf, (strlen(buf) + 1) * sizeof(char), 0) < 0)	/* incl. \0 */
			return -1;

		LOGV ("- Response header sent.");

		/* content */
		//if (Send(cfd, pict.buffer, pict.xres * pict.yres * pict.bps / 8, 0) < 0)
		//	return -1;
		LOGV ("- Screenshot sent");
	}

	return c;
}

int cleanup(int servfd, int* client_fd, int client_count)
{
	LOGV ("Shutdown");
	int i;
	for (i = 0; i < client_count; ++i)
		if (close(client_fd[i]) < 0)	return -1;
	free (client_fd);

	LOGV ("- Closing server socket");
	if (close(servfd) < 0)	return -1;

	LOGV ("Shutdown complete");

#ifdef DEBUG
	if (logFile != stderr)	fclose (logFile);
#endif
	return 0;
}


int start_main_loop(int servfd)
{
    int* client_fd = NULL;
    int client_count = 0;
    int max_fd;
    fd_set readfs;
    int i = 0;
    int c = 0;

    while(service_quit_flag > 0)
    {
        FD_ZERO(&readfs);
        FD_SET(servfd, &readfs);
        max_fd = 0;

        if (servfd > max_fd)
        {
            max_fd = servfd;
        }

        for (i = 0; i < client_count; i++)
        {
            if (client_fd[i] < 0)
            {
                continue;
            }

            FD_SET(client_fd[i], &readfs);
            if (client_fd[i] > max_fd)
            {
                max_fd = client_fd[i];
            }
        }

        if (select(max_fd + 1, &readfs, NULL, NULL, NULL) == -1)
        {
            if (errno == EINTR)
            {
                errno = 0;
                continue;
            }

            return -1;
        }

        for (i = 0; i < client_count; i++)
        {
            if (client_fd[i] < 0)
            {
                continue;
            }

            if (FD_ISSET(client_fd[i], &readfs))
            {
                // handler for data
                c = handle_client_input(client_fd[i]);

                if (c < 0 && errno != EPIPE && errno != ECONNRESET)
                {
                    return -1;
                }

                close(client_fd[i]);
                client_fd[i] = -1;
            }
        }

        if (FD_ISSET(servfd, &readfs))
        {
			if (accept_client (servfd, &client_fd, &client_count) < 0)
            {
				return -1;
            }
        }
    }

    return cleanup(servfd, client_fd, client_count);
}

int main(int argc, char* argv [])
{
    int server_socket = start_server();
    if (server_socket < 0)
    {
        LOGE("Failed to start socket server. Error code: %d", server_socket);
    }
}
