
all: gloriousctl

gloriousctl: gloriousctl.c
	$(CC) -Wall -Wextra gloriousctl.c -lhidapi-hidraw -o gloriousctl -g

.PHONY: clean

clean:
	rm -f gloriousctl
