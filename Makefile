CC	= gcc
LD	= gcc
CFLAGS	= -Wall -Wpedantic -Wextra -Werror -g
RM	= rm

BINDIR	= bin
DEPSDIR = deps
MKDIRS	= $(CURDIR)/{$(BINDIR),$(DEPSDIR)}

TARGET	= bfint
SRCS	= main.c
OBJS	= $(addprefix $(BINDIR)/,${SRCS:.c=.o})
DEPS	= $(addprefix $(DEPSDIR)/,${SRCS:.c=.d})

.SUFFIXES :
.SUFFIXES : .o .c

$(shell `mkdir -p $(MKDIRS)`)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(LD) -o $(TARGET) $(OBJS)

$(BINDIR)/%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(DEPSDIR)/%.d : %.c
	@$(CC) -M $(CFLAGS) $< > $@.$$$$;			\
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@;	\
	rm -f $@.$$$$

TAGS :
	find . -regex ".*\.[cChH]\(pp\)?" -print | etags -

clean :
	-$(RM) -r $(TARGET) $(BINDIR) $(DEPSDIR)

-include $(DEPS)

.PHONY : clean TAGS
