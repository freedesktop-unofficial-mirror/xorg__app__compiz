INCLUDES =			     \
	@COMPIZ_CFLAGS@		     \
	@GL_CFLAGS@		     \
	-I$(top_srcdir)/include	     \
	-I$(top_builddir)/include    \
	-DPLUGINDIR=\"$(plugindir)\" \
	-DIMAGEDIR=\"$(imagedir)\"   \
	-DMETADATADIR=\"$(metadatadir)\"

bin_PROGRAMS = compiz

compiz_LDADD = @COMPIZ_LIBS@ @GL_LIBS@ -lm
compiz_LDFLAGS = -export-dynamic
compiz_SOURCES =	  \
	main.c		  \
	error.c		  \
	privates.c	  \
	object.c	  \
	c-object.c	  \
	p-object.c	  \
	signal-arg-map.c  \
	signal-match.c	  \
	delegate.c	  \
	timer.c		  \
	file-descriptor.c \
	root.c		  \
	branch.c	  \
	box.c		  \
	prop.c		  \
	output.c	  \
	input.c		  \
	pointer.c	  \
	keyboard.c	  \
	core.c		  \
	texture.c	  \
	display.c	  \
	screen.c	  \
	window.c	  \
	event.c		  \
	paint.c		  \
	option.c	  \
	plugin.c	  \
	session.c	  \
	fragment.c	  \
	matrix.c	  \
	cursor.c	  \
	match.c		  \
	metadata.c	  \
	marshal.c
