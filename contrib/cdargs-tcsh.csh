#!/bin/csh

# (C) 2002-2005 Stefan Kamphausen

# (T)CShell extensions written by
# Stefan Kamphausen
# Ruediger Goetz <www.r-goetz.de>

# Note: this is currently BY FAR not as advanced as the bash stuff
# written by Dan!

# Just the basic cdargs functionality for tcshell
alias cv 'cdargs \!* && cd `cat $HOME/.cdargsresult`'

# add current directory with description:
alias ca 'cdargs --add=:\!*":"$cwd'
# don't blame me if this is wrong! It took me quite a while to find in
# st**id tcsh docs *grrr* (insert "sorry-to-the-developers" here). Too
# few examples and too much fuss with the colons (is there any reason
# that just escaping the colon with a backslash doesn't work?!? That
# would be very intuitive and common practice).


# Simple completion (thanks RG)
complete cv 'n@*@`cat -s ~/.cdargs | sed -e "s/ .*//"`@'

