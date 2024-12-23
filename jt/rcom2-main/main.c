#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <libgen.h>
#include "ftp_client.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        return -1;
    }

    char user[64], password[64], host[256], path[256];
    if (parse_ftp_url(argv[1], user, password, host, path) < 0) {
        fprintf(stderr, "Error: Invalid FTP URL format.\n");
        return -1;
    }

    printf("Starting FTP download...\n");

    // Connect to FTP server
    int control_sock = connect_to_server(host, 21);
    if (control_sock < 0) {
        fprintf(stderr, "Error: Unable to connect to server.\n");
        return -1;
    }
    printf("Connected to server: %s\n", host);
    
    // Read initial greeting
    char greeting[2048];
    if (read_ftp_response(control_sock, greeting, sizeof(greeting)) <= 0) {
        fprintf(stderr, "Error: Failed to read server greeting.\n");
        close(control_sock);
        return -1;
    }
    printf("Server greeting: %s", greeting);

    // Login to FTP server
    if (ftp_login(control_sock, user, password) < 0) {
        fprintf(stderr, "Error: Login failed.\n");
        close(control_sock);
        return -1;
    }
    printf("Logged in as: %s\n", user);

    // Enter passive mode
    char ip[16];
    int port;
    if (ftp_enter_passive_mode(control_sock, ip, &port) < 0) {
        fprintf(stderr, "Error: Failed to enter passive mode.\n");
        close(control_sock);
        return -1;
    }
    printf("Entered passive mode: %s:%d\n", ip, port);

    // Connect to data socket
    int data_sock = connect_to_server(ip, port);
    if (data_sock < 0) {
        fprintf(stderr, "Error: Unable to establish data connection.\n");
        close(control_sock);
        return -1;
    }
    printf("Data connection established.\n");
    char *filename = basename(path);

    // Retrieve file
    if (ftp_retrieve_file(control_sock, data_sock, path, filename) < 0) {
        fprintf(stderr, "Error: Failed to retrieve file.\n");
        close(data_sock);
        close(control_sock);
        return -1;
    }
    printf("File downloaded successfully: %s\n", filename);

    // Close connections
    close(data_sock);
    ftp_quit(control_sock);
    close(control_sock);
    printf("FTP session closed.\n");

    return 0;
}
