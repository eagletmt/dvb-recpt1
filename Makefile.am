bin_PROGRAMS = recpt1 recpt1ctl
recpt1_SOURCES = record.cc tuner.cc oneshot_timer.cc
recpt1ctl_SOURCES = recpt1ctl.c

confdir = $(sysconfdir)/$(PACKAGE)
conf_DATA = adapters.conf channels.conf

AM_CPPFLAGS = -DSYSCONFDIR="\"${sysconfdir}\""
