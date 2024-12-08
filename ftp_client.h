#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include <stddef.h>

int read_ftp_response(int sock, char *buf, size_t size);
int parse_ftp_url(const char *url, char *user, char *password, char *host, char *path);
int connect_to_server(const char *hostname, int port);
int ftp_login(int control_sock, const char *user, const char *password);
int ftp_enter_passive_mode(int control_sock, char *ip, int *port);
int ftp_retrieve_file(int control_sock, int data_sock, const char *remote_path, const char *local_filename);
void ftp_quit(int control_sock);

#endif
