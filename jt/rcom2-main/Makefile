CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = download

SRC = main.c ftp_client.c
OBJ = $(SRC:.c=.o)

PREFIX = /usr/local/bin

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	@echo "Installing $(TARGET) to $(PREFIX)..."
	@sudo cp $(TARGET) $(PREFIX)
	@sudo chmod 755 $(PREFIX)/$(TARGET)
	@echo "Installation complete. You can now run 'download {ftp address}'."

uninstall:
	@echo "Uninstalling $(TARGET) from $(PREFIX)..."
	@sudo rm -f $(PREFIX)/$(TARGET)
	@echo "Uninstallation complete."

clean:
	rm -f $(OBJ) $(TARGET)
