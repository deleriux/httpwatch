SUBDIRS = ini

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = httpwatch
httpwatch_SOURCES = \
    config.c \
    curl_event.c \
    event.c \
    main.c \
    run_script.c \
    urlchecker.c

httpwatch_CFLAGS = $(CURL_CFLAGS) $(CAPNG_CFLAGS) -I ini
httpwatch_LDFLAGS = $(CURL_LIBS) $(CAPNG_LIBS) ini/libini.la
