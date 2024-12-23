#include "ftp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>



// Parses the FTP URL and extracts user, password, host, and path
int parse_ftp_url(const char *url, char *user, char *password, char *host, char *path)
{
  // Try full format: ftp://user:pass@host/path
  if (sscanf(url, "ftp://%[^:]:%[^@]@%[^/]/%s", user, password, host, path) == 4)
  {
    return 0;
  }
  // Try anonymous format: ftp://host/path
  if (sscanf(url, "ftp://%[^/]/%s", host, path) == 2)
  {
    strcpy(user, "anonymous");
    strcpy(password, "guest");
    return 0;
  }
  return -1;
}

// Connects to the server and returns the socket descriptor
int connect_to_server(const char *hostname, int port)
{
  struct sockaddr_in server_addr;
  struct hostent *host_entry;

  if ((host_entry = gethostbyname(hostname)) == NULL)
  {
    perror("gethostbyname");
    return -1;
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("socket");
    return -1;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy(&server_addr.sin_addr, host_entry->h_addr, host_entry->h_length);
  server_addr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("connect");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

// Reads a complete FTP response, handling multi-line replies.
// FTP multi-line replies format:
// xyz-... (one or more lines)
// xyz ... (final line)
int read_ftp_response(int sock, char *buf, size_t size)
{
  memset(buf, 0, size);
  int total = 0;
  char line[1024];
  bool multiline = false;
  char code[4] = {0};

  while (1)
  {
    memset(line, 0, sizeof(line));
    int n = 0;
    // Read one line at a time
    while (n < (int)sizeof(line) - 1)
    {
      int r = read(sock, &line[n], 1);
      if (r <= 0)
      {
        // Connection closed or error
        break;
      }
      if (line[n] == '\n')
      {
        n++;
        break;
      }
      n++;
    }
    if (n == 0)
    {
      // No data read, possibly connection closed
      break;
    }
    line[n] = '\0';

    // Append this line to buf if space allows
    if ((int)strlen(buf) + n < (int)size - 1)
    {
      strcat(buf, line);
    }

    // If this is the first line, determine if it's multiline
    if (total == 0 && strlen(line) >= 4)
    {
      strncpy(code, line, 3);
      code[3] = '\0';
      // Check if multiline
      if (line[3] == '-')
      {
        multiline = true;
      }
    }

    total += n;

    // Check for end of multiline
    if (strlen(code) == 3 && !multiline)
    {
      // Single line response
      break;
    }

    if (multiline && strncmp(line, code, 3) == 0 && line[3] == ' ')
    {
      // End of multiline response
      break;
    }

    if (!multiline)
    {
      // If not multiline, we read only one line
      break;
    }
  }
  return total;
}

// Logs in to the FTP server
int ftp_login(int control_sock, const char *user, const char *password)
{
  char buffer[2048];

  // If user is anonymous and password is "guest", try a common anonymous password
  char actual_password[256];
  if (strcmp(user, "anonymous") == 0 && strcmp(password, "guest") == 0)
  {
    strcpy(actual_password, "anonymous@example.com");
  }
  else
  {
    strcpy(actual_password, password);
  }

  snprintf(buffer, sizeof(buffer), "USER %s\r\n", user);
  write(control_sock, buffer, strlen(buffer));
  read_ftp_response(control_sock, buffer, sizeof(buffer));
  
  // Expect a 331 code here
  if (strncmp(buffer, "331", 3) != 0)
  {
    fprintf(stderr, "Login failed (USER step): %s", buffer);
    return -1;
  }

  snprintf(buffer, sizeof(buffer), "PASS %s\r\n", actual_password);
  write(control_sock, buffer, strlen(buffer));
  read_ftp_response(control_sock, buffer, sizeof(buffer));

  // Expect a 230 code here for success
  if (strncmp(buffer, "230", 3) != 0)
  {
    fprintf(stderr, "Login failed (PASS step): %s", buffer);
    return -1;
  }

  return 0;
}

// Enters passive mode and retrieves IP and port for data connection
int ftp_enter_passive_mode(int control_sock, char *ip, int *port)
{
  char buffer[2048];
  char *start;
  int ip1, ip2, ip3, ip4, p1, p2;

  snprintf(buffer, sizeof(buffer), "PASV\r\n");
  write(control_sock, buffer, strlen(buffer));
  read_ftp_response(control_sock, buffer, sizeof(buffer));
  if (strncmp(buffer, "227", 3) != 0)
  {
    fprintf(stderr, "Passive mode failed: %s", buffer);
    return -1;
  }

  // Parse the response to extract IP and port
  start = strchr(buffer, '(');
  if (!start || sscanf(start, "(%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &p1, &p2) != 6)
  {
    fprintf(stderr, "Failed to parse passive mode response: %s", buffer);
    return -1;
  }

  snprintf(ip, 16, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
  *port = p1 * 256 + p2;
  return 0;
}

// Retrieves a file from the server
int ftp_retrieve_file(int control_sock, int data_sock, const char *remote_path, const char *local_filename)
{
  char buffer[2048];
  FILE *file;

  // Set binary mode using TYPE I command
  snprintf(buffer, sizeof(buffer), "TYPE I\r\n");
  write(control_sock, buffer, strlen(buffer));
  read_ftp_response(control_sock, buffer, sizeof(buffer));
  if (strncmp(buffer, "200", 3) != 0)
  {
    fprintf(stderr, "Failed to set binary mode: %s", buffer);
    return -1;
  }

  // Send RETR command to retrieve the file
  snprintf(buffer, sizeof(buffer), "RETR %s\r\n", remote_path);
  write(control_sock, buffer, strlen(buffer));
  read_ftp_response(control_sock, buffer, sizeof(buffer));

  // Expecting 150 (File status okay; about to open data connection)
  if (strncmp(buffer, "150", 3) != 0 && strncmp(buffer, "125", 3) != 0)
  {
    fprintf(stderr, "Failed to retrieve file: %s", buffer);
    return -1;
  }

  // Open file for writing
  if ((file = fopen(local_filename, "wb")) == NULL)
  {
    perror("fopen");
    return -1;
  }

  // Read data from the data socket and write to file
  int bytes;
  char data_buf[1024];
  while ((bytes = read(data_sock, data_buf, sizeof(data_buf))) > 0)
  {
    if (fwrite(data_buf, 1, bytes, file) != (size_t)bytes)
    {
      perror("fwrite");
      fclose(file);
      return -1;
    }
  }

  // Check if reading from data socket encountered an error
  if (bytes < 0)
  {
    perror("read");
    fclose(file);
    return -1;
  }

  fclose(file);

  // After data transfer, server should send a 226 response (Transfer complete)
  read_ftp_response(control_sock, buffer, sizeof(buffer));
  if (strncmp(buffer, "226", 3) != 0)
  {
    fprintf(stderr, "File transfer incomplete: %s", buffer);
    return -1;
  }

  return 0;
}

// Sends QUIT command to close the session
void ftp_quit(int control_sock)
{
  char buffer[2048];
  snprintf(buffer, sizeof(buffer), "QUIT\r\n");
  write(control_sock, buffer, strlen(buffer));
  read_ftp_response(control_sock, buffer, sizeof(buffer));
  printf("Server response: %s", buffer);
}
