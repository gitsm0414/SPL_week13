#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 80
#define MAX_USERNAME 20

#ifdef __GNUC__
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#else
#define UNUSED(x) UNUSED_##x
#endif

enum ExitCode {
  EXIT_CODE_SUCCESS,
  EXIT_CODE_SOCKET_CREATION_FAILURE,
  EXIT_CODE_CONNECTION_FAILURE,
  EXIT_CODE_SELECT_FAILURE,
  EXIT_CODE_WRITE_FAILURE,
  EXIT_CODE_READ_FAILURE,
  EXIT_CODE_INVALID_ARGUMENTS,
  EXIT_CODE_INVALID_PORT_NUMBER,
  EXIT_CODE_GETADDRINFO_FAILURE,
  EXIT_CODE_SERVER_DOWN,
  // feel free to add more
};

#define SAFELY_RUN(call, exit_code) \
  if ((call) < 0) {                 \
    perror(#call);                  \
    exit(exit_code);                \
  }

int conn_fd;

// TODO: You can add enums and structures here if you'd like

/* Helper functions */
// You can remove these functions if you'd like
// I just added these functions to lessen the errors.
void sigint_handler(int UNUSED(sig));
void handle_server_termination(int UNUSED(sig));
long get_port(int argc, char* argv[]);
struct in_addr get_socket_address(char* host);

/* Functions you might have to modify */

// When exiting, remember that the server has to send
// "<user> has left the chat" to every client.
// This can be done in the client or the server
// depending on your implementation
void exit_handler(void) {
  printf("Bye!\n");
  close(conn_fd);
}

void handle_input(int conn_fd) {
  char buf[MAXLINE];
  ssize_t bytes_read;

  while (1) {
    switch (bytes_read = read(STDIN_FILENO, buf, MAXLINE)) {
      case -1:
        perror("read");
        exit(EXIT_CODE_READ_FAILURE);
      case 0:
        return;
      default:
        SAFELY_RUN(write(conn_fd, buf, bytes_read), EXIT_CODE_WRITE_FAILURE)
        if (bytes_read < MAXLINE)
          return;
        break;
    }
  }
}

void handle_server_response(int conn_fd) {
  char buf[MAXLINE];
  ssize_t bytes_read;

  switch (read(conn_fd, buf, MAXLINE)) {
    case -1:
      perror("read");
      exit(EXIT_CODE_READ_FAILURE);
    case 0:
      puts("Connection with the server has been terminated.");
      exit(EXIT_CODE_SERVER_DOWN);
    default:
      write(STDOUT_FILENO, buf, bytes_read);
      if (bytes_read < MAXLINE)
        return;
      break;
  }
}

int main(int argc, char* argv[]) {
  signal(SIGPIPE, handle_server_termination);
  signal(SIGINT, sigint_handler);
  struct sockaddr_in socket_address;

  SAFELY_RUN(conn_fd = socket(AF_INET, SOCK_STREAM, 0),
             EXIT_CODE_SOCKET_CREATION_FAILURE)
  atexit(exit_handler);

  memset((char*)&socket_address, 0, sizeof(socket_address));
  socket_address.sin_family = AF_INET;
  socket_address.sin_addr = get_socket_address(argv[1]);
  socket_address.sin_port = htons(get_port(argc, argv));

  SAFELY_RUN(connect(conn_fd, (struct sockaddr*)&socket_address,
                     sizeof(socket_address)),
             EXIT_CODE_CONNECTION_FAILURE)

  // TODO: Get username
  // TODO: Remember that the server should notify each client
  //       when a new user joins the chat room.
  //       This can be done in the client or the server

  fd_set readset, copyset;
  FD_ZERO(&readset);  // initialize socket set
  FD_SET(STDIN_FILENO,
         &readset);  // Checks if the user entered something
  FD_SET(conn_fd,
         &readset);  // Checks if the server sent something
  int fdnum;

  while (1) {
    copyset = readset;
    SAFELY_RUN((fdnum = select(conn_fd + 1, &copyset, NULL, NULL, NULL)),
               EXIT_CODE_SELECT_FAILURE)
    // + 1 is here because the first fd is 0 (STDIN)

    // TODO: If the user entered something, send it to the server
    if (FD_ISSET(STDIN_FILENO, &copyset)) {
      handle_input(conn_fd);
    }

    // TODO: If the server sent something, print it
    if (FD_ISSET(conn_fd, &copyset)) {
      handle_server_response(conn_fd);
    }
  }

  close(conn_fd);
  return EXIT_CODE_SUCCESS;
}

// Helper functions

void sigint_handler(int UNUSED(sig)) {
  exit(EXIT_CODE_SUCCESS);
}

void handle_server_termination(int UNUSED(sig)) {
  write(STDOUT_FILENO, "Server was terminated!\n", 24);
  exit(EXIT_CODE_SERVER_DOWN);
}

long get_port(int argc, char* argv[]) {
  char* endptr;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
    exit(EXIT_CODE_INVALID_ARGUMENTS);
  }

  long port = strtol(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    printf("Invalid port number.\n");
    exit(EXIT_CODE_INVALID_PORT_NUMBER);
  }

  return port;
}

struct in_addr get_socket_address(char* host) {
  struct addrinfo hints, *res;
  int err;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  if ((err = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    exit(EXIT_CODE_GETADDRINFO_FAILURE);
  }

  struct in_addr addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;

  freeaddrinfo(res);
  return addr;
}
