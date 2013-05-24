################################################################################
## -------------------------------------------------------------------------- ##
##                                                                            ##
##                          Copyright (C) 2012-2013                           ##
##                            github.com/dkrutsko                             ##
##                            github.com/Harrold                              ##
##                            github.com/AbsMechanik                          ##
##                                                                            ##
##                        See LICENSE.md for copyright                        ##
##                                                                            ##
## -------------------------------------------------------------------------- ##
################################################################################

##----------------------------------------------------------------------------##
## Build                                                                      ##
##----------------------------------------------------------------------------##

.PHONY: build clean

build: NDP.h NDP.c Main.c
	gcc -Wall NDP.c Main.c -o Metropolis -lncurses -pthread

clean:
	$(RM) Metropolis