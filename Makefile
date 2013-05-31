 ###############################################################
 # $ID: Makefile       Sun Oct 27 13:56:17 2002 CST noclouds $ #
 #                                                             #
 # Description: A general  makefile for program.               #
 #                                                             #
 # Author:      ∑∂√¿ª‘  <mhfan@ustc.edu>                       #
 #                                                             #
 # Maintainer:  M.H. Fan  <mhfan@ustc.edu>                     #
 #              Laboratory of Structural Biology               #
 #              School of Life Science                         #
 #              Univ. of Sci.& Tech. of China (USTC)	       #
 #              People's Republic of China (PRC)               #
 #                                                             #
 # CopyRight (c)  2003  Meihui Fan                             #
 #   All rights reserved.                                      #
 #                                                             #
 # This file is free software;                                 #
 #   you are free to modify and/or redistribute it  	       #
 #   under the terms of the GNU General Public Licence.        #
 #                                                             #
 # No warranty, no liability; use this at your own risk!       #
 #                                                             #
 # Last modified: Thu, 09 Sep 2004 11:23:35 +0800      by mhfan #
 #                                                             #
 # George Foot 2001-02-16 13:56:40			       #
 # email: george.foot@merton.ox.ac.uk			       #
 #                                                             #
 # Copyright (c) 1997 George Foot			       #
 #      All rights reserved.				       #
 #                                                             #
 # No warranty,no liability; you use this at your own risk.    #
 # You are free to modify and distribute this		       #
 #  without giving credit to the original author.	       #
 ###############################################################

	# Customising

		# don't add -I and -l prefix!
INC		 =
LIBS 		 = $(CXXLIBS) $(CLIBS)
EXEC 		:= main
SONAME		:=
SRCS		:= # $(wildcard *.cpp)
SUFF		:= # already contain(.c .cpp .cxx .cc .C)
SUBD		:= 
SUBMK		:= 
DEFS		:= -O3 #-DTEST -DDEBUG -g

OBJDIR		:= .obj
LDFLAGS		:= -L$(HOME)/lib -L$(HOME)/lib/mhfan
SOFLAGS		:= -D_REENTRANT -fPIC -shared -Wl,-soname,$(SONAME)
CPPFLAGS	:= -MD
CINC		:= $(INC) $(HOME)/include $(HOME)/include/mhfan
CFLAGS		:= $(DEFS) -Wall -Wunused -Wno-implicit -march=i686 -pipe -fomit-frame-pointer -pthread
#-march=athlon -pipe -fomit-frame-pointer -ffast-math -funroll-loops -fforce-addr -falign-functions=4
CLIB		:= #l ??? # (e.g.:alleg,stdcx,iostr,etc)
CXXINC		:= $(CINC)
CXXFLAGS	:= $(CFLAGS)
CXXLIBS		:=
ASFLAGS		:= 
LFLAGS		:=
YFLAGS		:= -d
FFLAGS		:=
ARFLAGS		:= cqsruv	    # for static library

	# Now alter any implicit rules' variables if you like.

CC		:= gcc #-s
CXX		:= g++ #-s
LINKER		:= $(CXX)
LEX		:= flex
YACC		:= bison
AR		:= ar
AS		:= as
FC		:= f77

RM		:= rm -rf

	# You shouldn't need to change anything below this point.
override VPATH 	 = $(subst  ,:,$(SUBD) $(foreach file,$(SRCS),$(dir $(file))))
CFLAGS 		+= $(patsubst %,-I%,$(subst :, ,$(VPATH)))

SUFF		+= .c .cpp .cxx .cc .C 
SUBSRC		:= $(filter %$(SUFF),$(foreach dir,$(SUBD),$(wildcard $(dir)/*))) # ???
SRCS		+= $(SUBSRC) $(foreach suff,$(SUFF),$(wildcard *$(suff)))
OBJS   		:= $(addprefix $(OBJDIR)/,$(addsuffix .o, $(basename $(notdir $(SRCS)))))
DEPS		:= $(patsubst %.o,%.d,$(OBJS))
MISS_DEPS	:= $(filter-out $(wildcard $(DEPS)),$(DEPS))
MISS_DEPS_SRCS 	:= $(wildcard $$(patsubst %.d,%.c, $(MISS_DEPS)) $(patsubst %.d,%.cpp,$(MISS_DEPS))) # ???

ifeq (,$(findstring .cpp,$(SRCS)))
    LINKER	:=	$(CC)
endif

ifneq (,$(SUBMK))
    SUBS	:= submake
endif

$(OBJDIR)/%.o	: %.c 	; @mkdir -p $(OBJDIR)
	$(CC)  -o $@  -c $(CFLAGS) $(CPPFLAGS)   $(addprefix -I,$(CINC))    $<
$(OBJDIR)/%.o	: %.cpp ; @mkdir -p $(OBJDIR)
	$(CXX) -o $@  -c $(CXXFLAGS) $(CPPFLAGS) $(addprefix -I,$(CXXINC))  $<
$(OBJDIR)/%.o	: %.cxx ; @mkdir -p $(OBJDIR)
	$(CXX) -o $@  -c $(CXXFLAGS) $(CPPFLAGS) $(addprefix -I,$(CXXINC))  $<
$(OBJDIR)/%.o	: %.cc 	; @mkdir -p $(OBJDIR)
	$(CXX) -o $@  -c $(CXXFLAGS) $(CPPFLAGS) $(addprefix -I,$(CXXINC))  $<
$(OBJDIR)/%.o	: %.C 	; @mkdir -p $(OBJDIR)
	$(CXX) -o $@  -c $(CXXFLAGS) $(CPPFLAGS) $(addprefix -I,$(CXXINC))  $<

#lib%.a		: $(OBJDIR)/%.o

lib%.so		: $(OBJDIR)/%.o
	$(LINKER) -o $@  $(SOFLAGS) $(LDFLAGS) $(addprefix -l,$(LIBS))  $(OBJS)
ifeq (,$(SONAME))
    $(SOFLAGS)	= $(SOFLAGS)$@
endif

%		: $(OBJDIR)/%.o
	$(LINKER) -o $@    $(LDFLAGS)  $(addprefix -l,$(LIBS))    $< #$(OBJS)

.PHONY: all deps objs clean distclean rebuild install strip $(SUBS)

all		: $(SUBS) $(EXEC) ;	@mkdir -p $(OBJDIR)
deps		: $(DEPS)
objs		: $(OBJS)
strip		: ;		strip --strip-all $(EXEC)
rebuild		: distclean all
submake		: ; 		@for dir in $(SUBMK); do $(MAKE) -C $$dir; done 
distclean	: ; 		@$(RM) $(OBJDIR) $(EXEC)
ifneq (,$(SUBMK))
	@for dir in $(SUBMK); do $(MAKE) $@ -C $(dir); done 
endif
clean		: ;		@$(RM) $(OBJDIR)/*.o
ifneq (,$(SUBMK))
	@for dir in $(SUBMK); do $(MAKE) $@ -C $(dir); done
endif
$(EXEC)		: $(OBJS)
	$(LINKER) -o $(EXEC)    $(LDFLAGS)  $(addprefix -l,$(LIBS))    $(OBJS)
install		:

.SUFFIXES: .tex .dvi .pdf .ps

%.pdf		: %.tex;	pdflatex $< && pdflatex $< > /dev/null 2>&1
%.dvi		: %.tex;	latex    $< && latex    $< > /dev/null 2>&1
%.ps		: %.tex;	dvips    $< && dvips    $< > /dev/null 2>&1
%.bbl		: %.aux;	bibtex   $(subst .aux,,$<)

clean-doc	:
	@find -regex ".*\(chk\|log\|aux\|bbl\|blg\|ilg\|toc\|lof\|lot\|idx\|ind\|out\|lol\|thm\|hdir\|t2p\|bak\)" -print0 | xargs -0r rm -f

ifneq ($(MISS_DEPS),)
    $(MISS_DEPS): ; 		@$(RM) $(patsubst %.d,%.o,$@)
endif

ifneq (,$(DEPS))
    -include $(DEPS)
endif

############################ End Of File: Makefile ###########################
