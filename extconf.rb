require 'mkmf'
dir_config("GBP")
have_library("GBP", "TL_Open")
create_makefile("chipcard/gbp")
