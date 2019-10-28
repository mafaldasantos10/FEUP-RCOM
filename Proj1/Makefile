.PHONY: all clean
all: appll

CFLAGS = -Wall -Wextra -Werror
OBJECTS = app.o ll.o

appll: $(OBJECTS)
	@gcc $(CFLAGS) -pedantic $(OBJECTS) -o appll

app.o: app.h ll_app.h
ll.o: ll.h ll_app.h

clean:
	@rm -f appll $(OBJECTS)